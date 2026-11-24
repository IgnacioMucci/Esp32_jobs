#!/usr/bin/env python3
"""
Genera los archivos .tpoma de la Fase 3 del topico "ultralite".

Uso:
    python3 make_ultralite_tests.py            # escribe en ./tests
    python3 make_ultralite_tests.py -o salida  # otra carpeta

Requiere ultralite_frame.py en la misma carpeta (usa su tabla de topicos y sus
funciones para armar las tramas, asi nunca se calculan bytes a mano).

Convencion de los archivos (igual que tus .tpoma de siempre):
    TIMESTAMP|COMANDO        -> se envia
    # expect: ...            -> primera linea de respuesta esperada (se compara a ojo)
    # also: ...              -> lineas siguientes de la respuesta (se ven en idf monitor)
"""

import argparse
import json
from pathlib import Path

from ultralite_frame import (FRAME_LEN, FrameError, decode_frame, encode_query,
                             encode_set, pack_frames, spaced, to_hex)

STEP = 2.0                      # segundos entre comandos
MAX_TIME = 2529200              # tope que acepta setterPeriod (string_check.h)
BASE = 10                       # valor conocido de Period antes de cada caso


def pad(b: bytes) -> bytes:
    return b.ljust(FRAME_LEN, b"\x00")


def q(name):
    return encode_query(name)


def s(name, value, n=None):
    """Set. Sin n usa el N minimo que alcanza; con n fuerza esa cantidad de bytes."""
    return encode_set(name, value, n)


def frame(*records) -> bytes:
    """Arma UNA trama de 20 bytes con los registros dados (sin partirlos)."""
    frames = pack_frames(list(records))
    assert len(frames) == 1, "los registros no entran en una sola trama"
    return frames[0]


def ok(n):
    return f"ACK: ok: {n} command(s)"


def err(msg):
    return f"ACK: error: {msg}"


# Cada caso: id, titulo, argumento (hex o texto crudo), esperado (1a linea),
# lineas siguientes, setup y verificaciones posteriores.
CASES = []


def case(file, cid, title, arg, expect, also=(), setup=True, after=(), note=None,
         frame_desc=None, is_frame=True):
    CASES.append(dict(file=file, id=cid, title=title, arg=arg, expect=expect,
                      also=list(also), setup=setup, after=list(after), note=note,
                      frame_desc=frame_desc, is_frame=is_frame))


def hexf(b: bytes) -> str:
    assert len(b) == FRAME_LEN
    return to_hex(b)


# ---------------------------------------------------------------------------
# A. Validas
# ---------------------------------------------------------------------------
V = "ultralite_valid"
case(V, "A1", "un solo query", hexf(frame(q("Period"))), ok(1), also=[str(BASE)],
     frame_desc="query Period")
case(V, "A2", "un solo set, verificado con ?Period", hexf(frame(s("Period", 3600))), ok(1),
     also=["done"], after=[("?Period", "ACK: 3600")], frame_desc="set Period 3600")
case(V, "A3", "query, set, query: se ejecutan en orden",
     hexf(frame(q("Period"), s("Period", 120), q("Period"))), ok(3),
     also=[str(BASE), "done", "120"], after=[("?Period", "ACK: 120")],
     frame_desc="query Period | set Period 120 | query Period",
     note="el segundo query tiene que devolver el valor NUEVO (120)")
case(V, "A4", "hex en minusculas",
     hexf(frame(s("Period", 3600))).lower(), ok(1), also=["done"],
     after=[("?Period", "ACK: 3600")], frame_desc="set Period 3600 (hex en minusculas)",
     is_frame=False)
case(V, "A6", "longitud variable: valores 1,2,3,4 usando N = 1,2,3,4 (ceros a la izquierda)",
     hexf(frame(s("Period", 1, 1), s("Period", 2, 2), s("Period", 3, 3), s("Period", 4, 4), q("Period"), q("Period"))),
     ok(6), also=["done"] * 4 + ["4", "4"], after=[("?Period", "ACK: 4")],
     frame_desc="set 1 (N=1) | set 2 (N=2) | set 3 (N=3) | set 4 (N=4) | query | query",
     note="el byte N manda: cada set trae solo los bytes que dice N")
case(V, "A5", "trama vacia (todo padding)", hexf(pad(b"")), ok(0),
     frame_desc="(solo padding)", note="PENDIENTE: confirmar con el jefe si una trama vacia es valida")

# ---------------------------------------------------------------------------
# B. Limites
# ---------------------------------------------------------------------------
B = "ultralite_boundary"
case(B, "B1", "maximo de comandos: 20 queries, sin padding",
     hexf(frame(*[q("Period")] * 20)), ok(20), also=[str(BASE)] * 20,
     frame_desc="20 x query Period")
case(B, "B2", "trama llena: 3 sets de N=4 + 2 queries (20 bytes exactos, sin padding)",
     hexf(frame(s("Period", 1, 4), s("Period", 2, 4), s("Period", 3, 4), q("Period"), q("Period"))),
     ok(5), also=["done", "done", "done", "3", "3"], after=[("?Period", "ACK: 3")],
     frame_desc="set 1 (N=4) | set 2 (N=4) | set 3 (N=4) | query | query")
case(B, "B3", "un set que termina justo en el byte 19",
     hexf(frame(*[q("Period")] * 14, s("Period", 7, 4))), ok(15),
     also=[str(BASE)] * 14 + ["done"], after=[("?Period", "ACK: 7")],
     frame_desc="14 x query | set Period 7 (N=4)")
case(B, "B4", "un query que ocupa justo el byte 19",
     hexf(frame(s("Period", 8, 4), *[q("Period")] * 14)), ok(15),
     also=["done"] + ["8"] * 14, after=[("?Period", "ACK: 8")],
     frame_desc="set Period 8 (N=4) | 14 x query")
case(B, "B4b", "capacidad con valores chicos: 6 sets de 1 byte + 2 queries (20 bytes)",
     hexf(frame(*[s("Period", v) for v in range(1, 7)], q("Period"), q("Period"))), ok(8),
     also=["done"] * 6 + ["6", "6"], after=[("?Period", "ACK: 6")],
     frame_desc="set 1 | set 2 | ... | set 6 (cada set = 3 bytes) | query | query",
     note="con N variable entran 6 sets por trama; con N fijo de 4 bytes entrarian 3")
case(B, "B5", "valor minimo de Period (1)", hexf(frame(s("Period", 1))), ok(1),
     also=["done"], after=[("?Period", "ACK: 1")], frame_desc="set Period 1")
case(B, "B6", "valor maximo que acepta Period (MAX_TIME)", hexf(frame(s("Period", MAX_TIME))),
     ok(1), also=["done"], after=[("?Period", f"ACK: {MAX_TIME}")],
     frame_desc=f"set Period {MAX_TIME}")
case(B, "B7", "Period por encima del maximo: la trama es valida pero el setter lo ignora",
     hexf(frame(s("Period", MAX_TIME + 1))), ok(1), also=["done"],
     after=[("?Period", f"ACK: {BASE}")], frame_desc=f"set Period {MAX_TIME + 1}")
case(B, "B8", "Period = 0xFFFFFFFF: el setter lo ignora", hexf(frame(s("Period", 0xFFFFFFFF))),
     ok(1), also=["done"], after=[("?Period", f"ACK: {BASE}")],
     frame_desc="set Period 4294967295")
case(B, "B9", "Period = 0: el setter lo ignora", hexf(frame(s("Period", 0))), ok(1),
     also=["done"], after=[("?Period", f"ACK: {BASE}")], frame_desc="set Period 0")

# ---------------------------------------------------------------------------
# C. Invalidas (nada de esto debe ejecutar nada: Period tiene que seguir en BASE)
# ---------------------------------------------------------------------------
I = "ultralite_invalid"
KEEP = [("?Period", f"ACK: {BASE}")]


def bad(cid, title, raw: bytes, expect, desc, note=None):
    case(I, cid, title, hexf(pad(raw)), err(expect), after=KEEP, frame_desc=desc, note=note)


bad("C1", "indice 0 reservado (set 0x80)", bytes([0x80, 0x04, 0, 0, 0, 5]), "reserved index 0", "80 04 00 00 00 05")
bad("C2", "topico desconocido (query indice 3)", bytes([0x03]), "unknown topic", "03")
bad("C3", "topico desconocido (set indice 3)", bytes([0x83, 0x04, 0, 0, 0, 1]), "unknown topic", "83 04 00 00 00 01")
bad("C4", "indice maximo 127 (query 0x7F)", bytes([0x7F]), "unknown topic", "7F")
bad("C5", "indice maximo 127 (set 0xFF)", bytes([0xFF, 0x04, 0, 0, 0, 1]), "unknown topic", "FF 04 00 00 00 01")
bad("C6", "query sobre DeepSleep (no tiene getter)", bytes([0x02]), "topic has no getter", "02")
bad("C7", "set con N = 0", bytes([0x81, 0x00]), "bad set length", "81 00")
bad("C8", "set con N = 19 (mayor que 18)", bytes([0x81, 19]), "bad set length", "81 13 ...")
bad("C9", "set con N = 255", bytes([0x81, 0xFF]), "bad set length", "81 FF ...")
bad("C11", "N = 5, mayor que el maximo del topico (4)", bytes([0x81, 5, 0, 0, 0, 0, 5]), "length exceeds topic size", "81 05 00 00 00 00 05")
bad("C12", "N = 18 (maximo del formato, pero Period admite hasta 4)", bytes([0x81, 18]), "length exceeds topic size", "81 12 ...")
bad("C13", "set que se pasa del byte 20 (empieza en el byte 16)", bytes([0x01] * 16 + [0x81, 0x04, 0, 0]),
    "record overruns frame", "16 x 01 | 81 04 00 00")
bad("C13b", "set que se pasa por EXACTAMENTE 1 byte (empieza en el byte 15)", bytes([0x01] * 15 + [0x81, 0x04, 0, 0, 0]),
    "record overruns frame", "15 x 01 | 81 04 00 00 00", note="caso limite: el registro necesita 21 bytes")
bad("C14", "set que se pasa del byte 20 (empieza en el byte 18)", bytes([0x01] * 18 + [0x81, 0x04]),
    "record overruns frame", "18 x 01 | 81 04")
bad("C15", "header de set truncado en el byte 19", bytes([0x01] * 19 + [0x81]),
    "truncated set header", "19 x 01 | 81")
bad("C16", "padding sucio: byte distinto de 0 al final", bytes([0x01]) + bytes(18) + bytes([0x01]),
    "dirty padding", "01 00 ... 00 01")
bad("C17", "padding sucio: justo despues de un registro", bytes([0x01, 0x00, 0x00, 0xAA]),
    "dirty padding", "01 00 00 AA")
# Atomicidad: un set valido seguido de algo malo. NO debe ejecutarse ni el set valido.
bad("C18", "ATOMICIDAD: set valido + topico desconocido", s("Period", 99) + bytes([0x63]),
    "unknown topic", "set Period 99 | 63", note="Period NO debe pasar a 99")
bad("C19", "ATOMICIDAD: set valido + query sin getter", s("Period", 99) + bytes([0x02]),
    "topic has no getter", "set Period 99 | 02", note="Period NO debe pasar a 99")
bad("C20", "ATOMICIDAD: set valido + registro que se pasa de 20",
    s("Period", 99) + bytes([0x01] * 13) + bytes([0x81, 0x04, 0, 0]),
    "record overruns frame", "set Period 99 | 13 x 01 | 81 04 00 00", note="Period NO debe pasar a 99")
bad("C21", "ATOMICIDAD: set valido + padding sucio", s("Period", 99) + bytes([0, 0, 0xAA]),
    "dirty padding", "set Period 99 | 00 00 AA", note="Period NO debe pasar a 99")
bad("C22", "ATOMICIDAD: set valido + set con N mayor que el maximo",
    s("Period", 99) + bytes([0x81, 5, 0, 0, 0, 0, 5]), "length exceeds topic size",
    "set Period 99 | 81 05 00 00 00 00 05", note="Period NO debe pasar a 99")

# Transporte: el argumento ni siquiera llega a ser una trama de 20 bytes
H = hexf(frame(s("Period", 99)))
T = "bad hex frame"


def transport(cid, title, arg, desc):
    case(I, cid, title, arg, err(T), after=KEEP, frame_desc=desc, is_frame=False)


transport("T1", "sin argumento", "", "=ultralite")
transport("T2", "38 caracteres (19 bytes)", H[:-2], "38 hex")
transport("T3", "42 caracteres (21 bytes)", H + "00", "42 hex")
transport("T4", "un caracter no hexadecimal (G)", "G" + H[1:], "G + 39 hex")
transport("T5", "espacios dentro de la trama", " ".join(H[i:i + 2] for i in range(0, 40, 2)), "bytes separados por espacio")
transport("T6", "prefijo 0x", "0x" + H[2:], "0x + 38 hex")

# ---------------------------------------------------------------------------
# Escritura de archivos
# ---------------------------------------------------------------------------
HEADERS = {
    V: "Tramas VALIDAS: operaciones y parametros admitidos.",
    B: "LIMITES: maximo de comandos, tramas llenas, valores min/max.",
    I: "INVALIDAS: nada de lo de este archivo debe ejecutar ningun comando.\n"
       "# Despues de cada trama se consulta ?Period: tiene que seguir valiendo %d." % BASE,
}


def write_files(out: Path):
    out.mkdir(parents=True, exist_ok=True)
    manifest = []
    for fname in (V, B, I):
        lines = [f"# {HEADERS[fname]}",
                 "# Generado por make_ultralite_tests.py (no editar a mano: regenerar).",
                 "# Formato:  TIMESTAMP|COMANDO   ('# expect:' = primera linea esperada).",
                 ""]
        t = 0.0

        def send(cmd, expect=None, also=()):
            nonlocal t
            lines.append(f"{t:.1f}|{cmd}")
            if expect is not None:
                lines.append(f"# expect: {expect}")
            for a in also:
                lines.append(f"# also:   {a}")
            t += STEP

        for c in [c for c in CASES if c["file"] == fname]:
            lines.append(f"# ---- {c['id']}: {c['title']}")
            if c["frame_desc"]:
                lines.append(f"# trama: {c['frame_desc']}")
            if c["note"]:
                lines.append(f"# nota:  {c['note']}")
            steps = []
            if c["setup"]:
                send(f"=Period {BASE}", "ACK: done")
                steps.append(("=Period %d" % BASE, ["ACK: done"]))
            cmd = f"=ultralite {c['arg']}".rstrip() if c["arg"] else "=ultralite"
            send(cmd, c["expect"], c["also"])
            steps.append((cmd, [c["expect"]] + c["also"]))
            for acmd, aexp in c["after"]:
                send(acmd, aexp)
                steps.append((acmd, [aexp]))
            lines.append("")
            manifest.append(dict(file=fname, id=c["id"], steps=steps))
        (out / f"{fname}.tpoma").write_text("\n".join(lines), encoding="utf-8")

    # Un test de DeepSleep MANUAL (el ESP32 se duerme y se reinicia: la conexion se corta).
    ds = frame(s("DeepSleep", 5), s("Period", 77))
    (out / "ultralite_deepsleep.tpoma").write_text("\n".join([
        "# MANUAL. El ESP32 se duerme 5 s y se reinicia: la conexion TCP se corta.",
        "# La trama es: set DeepSleep 5 | set Period 77",
        "# Esperado: se duerme, y el 'set Period 77' (que viene DESPUES) NO se ejecuta.",
        f"0.0|=Period {BASE}",
        "# expect: ACK: done",
        f"2.0|=ultralite {to_hex(ds)}",
        "# expect: ACK: ok: 2 command(s)",
        "# (la segunda respuesta ya no llega: el dispositivo esta durmiendo)",
        ""]), encoding="utf-8")
    (out / "ultralite_after_wake.tpoma").write_text("\n".join([
        "# Correr DESPUES de que el ESP32 se desperto y reconecto a la red.",
        "0.0|?Period",
        f"# expect: ACK: {BASE}    (si dice 77, el set posterior al DeepSleep se ejecuto: BUG)",
        ""]), encoding="utf-8")
    (out / "cases.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")

    # Autochequeo con el decodificador de referencia en Python:
    # las tramas "validas" tienen que decodificar y las "invalidas" tienen que fallar.
    for c in CASES:
        if not c["is_frame"]:
            continue
        raw = bytes.fromhex(c["arg"])
        is_error = c["expect"].startswith("ACK: error")
        try:
            decode_frame(raw)
            decoded_ok = True
        except FrameError:
            decoded_ok = False
        assert decoded_ok != is_error, f"{c['id']}: el decodificador Python no coincide con lo esperado"
    print(f"{len(CASES)} casos escritos en {out}/ (autochequeo con decode_frame: OK)")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", default="tests")
    write_files(Path(ap.parse_args().out))