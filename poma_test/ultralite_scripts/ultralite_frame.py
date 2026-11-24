#!/usr/bin/env python3
"""
Generador (y verificador) de tramas PoMA sobre UltraLite de 20 bytes.

Uso:
    python3 ultralite_frame.py query Period set Period 3600 set DeepSleep 10
    python3 ultralite_frame.py set Period 0x0E10
    python3 ultralite_frame.py set Period 7:4        # el valor 7 usando N=4 bytes
    python3 ultralite_frame.py --list
    python3 ultralite_frame.py --selftest
    python3 ultralite_frame.py set Period 10 --tpoma tests/ultralite.tpoma

Formato (ver documento "PoMA Command Packing"):
    query : 1 byte  -> (0<<7) | indice
    set   : 2 bytes de header + N parametros -> (1<<7) | indice, N, params...
            (N = 1..max_size del topico; por defecto se usa el N MINIMO que alcanza)
    trama : exactamente 20 bytes
            bytes 0..18 -> comandos uno detras de otro + relleno 0x00
            byte  19    -> CANTIDAD de comandos de la trama (y nada mas)

Errores:
    - de TRAMA  (estructura rota, cantidad que no coincide...) -> no se ejecuta nada
    - de COMANDO (topico desconocido, N=0, ...)                -> se saltea ese comando,
                                                                 los demas se ejecutan igual
"""

import argparse
import sys

FRAME_LEN = 20
CMD_AREA = 19              # bytes 0..18: comandos + relleno
COUNT_POS = 19             # byte 19: cantidad de comandos
MAX_N = 17                 # un set: 2 bytes de header + N <= 19
QS_QUERY = 0               # Punto abierto del doc: confirmar polaridad del bit Q/S
QS_SET = 1
BYTEORDER = "big"          # orden de bytes por defecto del proyecto (regla 7)

# ---------------------------------------------------------------------------
# Tabla de topicos: nombre -> indice (1..127), tamano MAXIMO en bytes, con signo, acceso
#   El byte N de un set puede valer de 1 hasta max_size (se omiten los ceros de la izquierda).
#   acceso: "rw" = tiene getter y setter, "wo" = solo setter, "ro" = solo getter
# TOPICS tiene que ser IGUAL a la tabla topic_table[] de ultralite_frame.c (ESP32).
# ---------------------------------------------------------------------------
TOPICS = {
    "Period":    {"index": 1, "max_size": 4, "signed": False, "access": "rw"},
    "DeepSleep": {"index": 2, "max_size": 4, "signed": False, "access": "wo"},
}

# Topicos de EJEMPLO del documento (solo se usan en --selftest para reproducir sus ejemplos).
DOC_EXAMPLE_TOPICS = {
    "report_interval": {"index": 3,  "max_size": 2, "signed": False, "access": "rw"},
    "relay_state":     {"index": 7,  "max_size": 1, "signed": False, "access": "rw"},
    "battery_voltage": {"index": 12, "max_size": 2, "signed": False, "access": "ro"},
    "alarm_threshold": {"index": 20, "max_size": 4, "signed": True,  "access": "rw"},
}
BY_INDEX = {t["index"]: name for name, t in TOPICS.items()}


def set_table(table: dict):
    """Cambia la tabla de topicos activa (usado por el autotest)."""
    global TOPICS, BY_INDEX
    TOPICS = table
    BY_INDEX = {t["index"]: name for name, t in table.items()}


class FrameError(Exception):
    """Trama o comando invalido."""


# ---------------------------------------------------------------------------
# Codificacion
# ---------------------------------------------------------------------------
def encode_query(topic: str) -> bytes:
    t = _topic(topic)
    if t["access"] == "wo":
        raise FrameError(f"el topico '{topic}' es de solo escritura, no admite query")
    return bytes([(QS_QUERY << 7) | (t["index"] & 0x7F)])


def min_bytes(value: int, signed: bool) -> int:
    """Cantidad minima de bytes para representar 'value' (con signo: complemento a 2)."""
    if signed:
        bits = (value if value >= 0 else ~value).bit_length() + 1   # +1 = bit de signo
    else:
        if value < 0:
            raise FrameError(f"valor negativo ({value}) en un topico sin signo")
        bits = max(value.bit_length(), 1)
    return (bits + 7) // 8


def encode_set(topic: str, value: int, n: int = None) -> bytes:
    """Arma un set. Si n es None usa el N minimo que alcanza para el valor;
    si se pasa n, usa exactamente ese N (rellena con ceros a la izquierda)."""
    t = _topic(topic)
    if t["access"] == "ro":
        raise FrameError(f"el topico '{topic}' es de solo lectura, no admite set")
    needed = min_bytes(value, t["signed"])
    if n is None:
        n = needed
    if not 1 <= n <= MAX_N:
        raise FrameError(f"N={n} invalido (1..{MAX_N})")
    if n > t["max_size"]:
        raise FrameError(f"N={n} supera el maximo de '{topic}' ({t['max_size']} byte(s))")
    if n < needed:
        raise FrameError(f"el valor {value} necesita {needed} byte(s), no entra en N={n}")
    params = value.to_bytes(n, BYTEORDER, signed=t["signed"])
    header = bytes([(QS_SET << 7) | (t["index"] & 0x7F), n])
    return header + params


def _topic(name: str) -> dict:
    # Busqueda sin distinguir mayusculas: "period" == "Period"
    match = [k for k in TOPICS if k.lower() == name.lower()]
    if not match:
        raise FrameError(f"topico desconocido: '{name}' (usar --list)")
    idx = TOPICS[match[0]]["index"]
    if not 1 <= idx <= 127:
        raise FrameError(f"indice {idx} de '{name}' fuera de 1..127 (0 esta reservado)")
    return TOPICS[match[0]]


def pack_frames(records: list) -> list:
    """Empaqueta registros (bytes) en tramas de 20 bytes.
    Cada trama: hasta 19 bytes de comandos, relleno 0x00 y en el byte 19 la cantidad
    de comandos. Un registro nunca se parte entre tramas: si no entra, va a la proxima."""
    frames, current, n_cmds = [], bytearray(), 0

    def close():
        frames.append(bytes(current).ljust(CMD_AREA, b"\x00") + bytes([n_cmds]))

    for rec in records:
        if len(rec) > CMD_AREA:
            raise FrameError(f"un comando de {len(rec)} bytes no entra en {CMD_AREA}")
        if len(current) + len(rec) > CMD_AREA:
            close()
            current, n_cmds = bytearray(), 0
        current += rec
        n_cmds += 1
    if current:
        close()
    return frames


# ---------------------------------------------------------------------------
# Decodificacion byte a byte (misma logica que el C del ESP32)
# ---------------------------------------------------------------------------
def check_frame(frame: bytes) -> int:
    """PRIMER PARSEO: solo ubica donde empieza y termina cada comando y los cuenta.
    No mira topicos ni valores. Lanza FrameError si la trama esta rota.
    Devuelve la cantidad de comandos encontrados."""
    if len(frame) != FRAME_LEN:
        raise FrameError(f"la trama mide {len(frame)} bytes, tiene que medir {FRAME_LEN}")
    pos, found = 0, 0
    while pos < CMD_AREA and frame[pos] != 0x00:
        if frame[pos] & 0x80 == 0:              # query: 1 byte
            pos += 1
        else:                                   # set: header, N, N bytes
            if pos + 1 >= CMD_AREA:
                raise FrameError(f"byte {pos}: header de set truncado (falta N)")
            n = frame[pos + 1]
            if pos + 2 + n > CMD_AREA:
                raise FrameError(f"byte {pos}: el set no entra en los 19 bytes de comandos")
            pos += 2 + n
        found += 1
    if any(x != 0 for x in frame[pos:CMD_AREA]):
        raise FrameError(f"padding sucio a partir del byte {pos}")
    if found != frame[COUNT_POS]:
        raise FrameError(f"se encontraron {found} comandos pero el byte 19 dice {frame[COUNT_POS]}")
    return found


def _classify(is_set: bool, index: int, n: int):
    """Error de UN comando (None si esta bien) y nombre del topico."""
    if index == 0:
        return "indice 0 reservado", None
    if index not in BY_INDEX:
        return f"indice {index} no esta en la tabla", None
    name = BY_INDEX[index]
    t = TOPICS[name]
    if not is_set:
        return ("el topico no tiene getter" if t["access"] == "wo" else None), name
    if t["access"] == "ro":
        return "el topico no tiene setter", name
    if n == 0:
        return "set vacio (N=0)", name
    if n > t["max_size"]:
        return f"N={n} supera el maximo del topico ({t['max_size']})", name
    return None, name


def decode_frame(frame: bytes) -> list:
    """Primer parseo (check_frame) + SEGUNDO PARSEO: arma la lista de comandos.
    Cada comando trae 'error' = None (se ejecuta) o el motivo por el que se saltea."""
    check_frame(frame)
    cmds, pos = [], 0
    while pos < CMD_AREA and frame[pos] != 0x00:
        is_set, index = bool(frame[pos] & 0x80), frame[pos] & 0x7F
        n = frame[pos + 1] if is_set else 0
        params = frame[pos + 2: pos + 2 + n] if is_set else b""
        error, name = _classify(is_set, index, n)
        cmd = {"op": "set" if is_set else "query", "index": index, "topic": name,
               "params": params, "error": error}
        if is_set and error is None:
            cmd["value"] = int.from_bytes(params, BYTEORDER, signed=TOPICS[name]["signed"])
        cmds.append(cmd)
        pos += 2 + n if is_set else 1
    return cmds


# ---------------------------------------------------------------------------
# Presentacion
# ---------------------------------------------------------------------------
def to_hex(frame: bytes) -> str:
    return frame.hex().upper()


def spaced(frame: bytes) -> str:
    return " ".join(f"{b:02X}" for b in frame)


def describe(cmds: list) -> str:
    lines = []
    for i, c in enumerate(cmds, 1):
        who = c["topic"] or f"indice {c['index']}"
        if c["error"]:
            lines.append(f"  #{i} {c['op']:<5} {who}  -> SE SALTEA: {c['error']}")
        elif c["op"] == "query":
            lines.append(f"  #{i} query {who}")
        else:
            lines.append(f"  #{i} set   {who} = {c['value']}  (params {c['params'].hex().upper()})")
    return "\n".join(lines)


def list_topics():
    print(f"{'topico':<18}{'indice':>7}{'maxN':>7}  {'signo':<11}acceso")
    for name, t in sorted(TOPICS.items(), key=lambda kv: kv[1]["index"]):
        sg = "con signo" if t["signed"] else "sin signo"
        print(f"{name:<18}{t['index']:>7}{t['max_size']:>7}  {sg:<11}{t['access']}")


# ---------------------------------------------------------------------------
# Autotest con los ejemplos del documento
# ---------------------------------------------------------------------------
def selftest() -> bool:
    ok = True

    def check(label, got, expected):
        nonlocal ok
        good = got == expected
        ok &= good
        print(f"[{'OK ' if good else 'FAIL'}] {label}")
        if not good:
            print(f"       esperado: {expected}\n       obtenido: {got}")

    def rejected(raw: bytes) -> bool:
        try:
            decode_frame(raw)
            return False
        except FrameError:
            return True

    def frame_of(body: bytes, count: int) -> bytes:
        return body.ljust(CMD_AREA, b"\x00") + bytes([count])

    # Codificacion
    f = pack_frames([encode_query("Period"), encode_set("Period", 3600)])[0]
    check("query + set Period 3600 (byte 19 = 2)", to_hex(f), "0181020E10" + "00" * 14 + "02")
    check("set Period 7 usa 1 byte", encode_set("Period", 7).hex(), "810107")
    check("19 queries entran en una trama (byte 19 = 0x13)",
          to_hex(pack_frames([encode_query("Period")] * 19)[0]), "01" * 19 + "13")
    check("20 queries se reparten en 2 tramas", len(pack_frames([encode_query("Period")] * 20)), 2)
    check("con signo: -250 usa 2 bytes", min_bytes(-250, True), 2)

    # Errores de TRAMA: se rechaza todo
    check("rechaza: cantidad que no coincide", rejected(frame_of(b"\x01", 2)), True)
    check("rechaza: padding sucio", rejected(frame_of(b"\x01\x00\xAA", 1)), True)
    check("rechaza: set truncado en el byte 18", rejected(bytes([1] * 18 + [0x81, 19])), True)
    check("rechaza: set que no entra (N=17 en el byte 1)", rejected(frame_of(b"\x01\x81\x11", 2)), True)
    check("rechaza: largo != 20", rejected(bytes(19)), True)

    # Errores de COMANDO: se saltea solo ese, los demas siguen
    cmds = decode_frame(frame_of(bytes([0x01, 0x03, 0x81, 0x01, 0x07]), 3))
    check("comando malo en el medio: 3 comandos", len(cmds), 3)
    check("  #1 query Period OK", cmds[0]["error"], None)
    check("  #2 topico desconocido se saltea", cmds[1]["error"] is not None, True)
    check("  #3 set Period 7 OK", (cmds[2]["error"], cmds[2]["value"]), (None, 7))
    errs = [c["error"] is not None for c in decode_frame(frame_of(bytes([0x02, 0x81, 0x00, 0x81, 0x05, 0, 0, 0, 0, 1]), 3))]
    check("query sin getter, N=0 y N>max se saltean", errs, [True, True, True])
    check("trama vacia (byte 19 = 0) es valida", decode_frame(bytes(20)), [])
    return ok


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def parse_commands(tokens: list) -> list:
    records, i = [], 0
    while i < len(tokens):
        op = tokens[i].lower()
        if op == "query":
            if i + 1 >= len(tokens):
                raise FrameError("falta el topico despues de 'query'")
            records.append(encode_query(tokens[i + 1]))
            i += 2
        elif op == "set":
            if i + 2 >= len(tokens):
                raise FrameError("'set' necesita topico y valor: set <topico> <valor>")
            tok, nbytes = tokens[i + 2], None
            try:
                if ":" in tok:                          # "7:4" = valor 7 usando N=4 bytes
                    tok, nb = tok.split(":", 1)
                    nbytes = int(nb, 0)
                value = int(tok, 0)                     # acepta 3600, -250, 0x0E10
            except ValueError:
                raise FrameError(f"valor no numerico: '{tokens[i + 2]}'")
            records.append(encode_set(tokens[i + 1], value, nbytes))
            i += 3
        else:
            raise FrameError(f"operacion desconocida: '{tokens[i]}' (usar set o query)")
    return records


def main():
    ap = argparse.ArgumentParser(description="Generador de tramas UltraLite de 20 bytes")
    ap.add_argument("commands", nargs="*", help="query <topico> | set <topico> <valor> (se pueden encadenar)")
    ap.add_argument("--list", action="store_true", help="muestra la tabla de topicos")
    ap.add_argument("--selftest", action="store_true", help="prueba el script con los ejemplos del doc")
    ap.add_argument("--tpoma", metavar="ARCHIVO", help="agrega las lineas '=ultralite <hex>' a un .tpoma")
    ap.add_argument("--step", type=float, default=2.0, help="segundos entre comandos en el .tpoma (default 2.0)")
    args = ap.parse_args()

    if args.list:
        list_topics()
        return 0
    if args.selftest:
        return 0 if selftest() else 1
    if not args.commands:
        ap.print_help()
        return 1

    try:
        frames = pack_frames(parse_commands(args.commands))
    except FrameError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 2

    for n, fr in enumerate(frames, 1):
        print(f"Trama {n}/{len(frames)}")
        print(f"  bytes : {spaced(fr)}")
        print(f"  hex   : {to_hex(fr)}")
        print(f"  poma  : =ultralite {to_hex(fr)}")
        print(describe(decode_frame(fr)))          # verificacion de ida y vuelta

    if args.tpoma:
        with open(args.tpoma, "a", encoding="utf-8") as f:
            for n, fr in enumerate(frames):
                f.write(f"# {' '.join(args.commands)} (trama {n + 1}/{len(frames)})\n")
                f.write(f"{n * args.step:.1f}|=ultralite {to_hex(fr)}\n")
        print(f"Agregado a {args.tpoma}")
    return 0


if __name__ == "__main__":
    sys.exit(main())