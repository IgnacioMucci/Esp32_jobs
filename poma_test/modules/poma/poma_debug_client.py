import socket

from modules.logger.logger import Logger


class PomaDebugClient:
    """TCP/PoMA client for debugging with timed PoMA commands"""

    def __init__(self, host: str, port: int, logger: Logger, buffer_size: int = 4096):
        self.host = host
        self.port = port
        self.buffer_size = buffer_size
        self.socket = None
        self.logger = logger
        self.connected = False

    def connect(self) -> bool:
        """Establishes a TCP connection with the PoMA server."""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            self.logger.info(f"Connecting to '{self.host}:{self.port}'...")
            self.socket.connect((self.host, self.port))
            self.connected = True
            self.logger.success(f"Connection established with '{self.host}:{self.port}'.")
            return True
        except socket.timeout:
            self.logger.error(f"Timeout when connecting to '{self.host}:{self.port}'.")
        except socket.error as e:
            self.logger.error(f"Connection error: '{e}'.")
        except Exception as e:
            self.logger.error(f"Unexpected error: '{e}'.")
        self.connected = False
        return False

    def send_command(self, command: str) -> bool:
        """Sends a command to the server."""
        if not self.is_connected():
            self.logger.error("Cannot send command: not connected to server.")
            return False

        try:
            # Ensure that the command ends correctly so that it can be understood by PoMA.
            if not command.endswith('\x00'):
                command += '\x00'

            self.socket.sendall(command.encode('utf-8'))
            self.logger.sent(command)
            return True
        except socket.error as e:
            self.logger.error(f"Error sending command: '{e}'.")
            return False

    def receive_response(self, timeout: float = 2.0) -> str:
        """Receives a response from the server."""
        try:
            self.socket.settimeout(timeout)
            accumulated_data = ""

            # 1) Wait for the first complete line (same as before).
            while '\n' not in accumulated_data:
                chunk = self.socket.recv(self.buffer_size)
                if not chunk:
                    self.logger.warning("Connection closed by server.")
                    self.connected = False
                    return accumulated_data
                accumulated_data += chunk.decode('utf-8', errors='ignore')

            # 2) Some commands (like Ultralite) answer with several lines:
            #    keep reading until the server stays quiet for a short time.
            self.socket.settimeout(0.3)
            try:
                while True:
                    chunk = self.socket.recv(self.buffer_size)
                    if not chunk:
                        self.connected = False
                        break
                    accumulated_data += chunk.decode('utf-8', errors='ignore')
            except socket.timeout:
                pass   # nothing more arrived: the response is complete

            for line in accumulated_data.splitlines():
                self.logger.received(line + '\n')
            return accumulated_data

        except socket.timeout:
            self.logger.warning("Timeout esperando respuesta")
        except socket.error as e:
            self.logger.error(f"Error al recibir respuesta: {e}")
        return ""

    def is_connected(self) -> bool:
        """Returns True if the client is connected to the server."""
        return self.connected

    def close(self):
        """Closes the connection."""
        if self.socket:
            try:
                self.socket.close()
                self.logger.info("TCP Connection closed.")
            except:
                pass
        self.connected = False
