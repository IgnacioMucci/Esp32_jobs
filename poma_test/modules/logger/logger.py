from datetime import datetime

from modules.logger.message_type import MessageType


class Logger:
    """Class to handle all log logic."""

    def __init__(self, use_colors: bool = True, use_timestamps: bool = False):
        """
        Initializes the Logger.

        Args:
            use_colors: whether ANSI colors should be used in the output.
            use_timestamps: whether timestamps should be included in messages,
        """
        self.use_colors = use_colors
        self.use_timestamps = use_timestamps

        # ANSI Color Codes:
        self.colors = {
            MessageType.DEBUG: '\033[97m',  # White.
            MessageType.INFO: '\033[94m',  # Blue.
            MessageType.WARNING: '\033[93m',  # Yellow.
            MessageType.ERROR: '\033[91m',  # Red.
            MessageType.SUCCESS: '\033[92m',  # Green.
            MessageType.SENT: '\033[96m',  # Cyan.
            MessageType.RECEIVED: '\033[95m',  # Purple.
            MessageType.WAITING: '\033[90m',  # Grey.
        }
        self.reset = '\033[0m'

        # Prefixes for each type of message:
        self.prefixes = {
            MessageType.DEBUG: '[?]',
            MessageType.INFO: '[*]',
            MessageType.WARNING: '[!]',
            MessageType.ERROR: '[-]',
            MessageType.SUCCESS: '[+]',
            MessageType.SENT: '[>]',
            MessageType.RECEIVED: '[<]',
            MessageType.WAITING: '[~]',
        }

    def _format_message(self, message_type: MessageType, message: str) -> str:
        """Formats a message with its prefix, color, and timestamp."""
        parts = []

        # Optional timestamp.
        if self.use_timestamps:
            timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
            parts.append(f"[{timestamp}]")

        # Message type prefix.
        prefix = self.prefixes.get(message_type, '')
        if prefix:
            parts.append(prefix)

        # Message.
        parts.append(message)

        formatted = ' '.join(parts)

        # Apply color if enabled.
        if self.use_colors and message_type in self.colors:
            color = self.colors[message_type]
            formatted = f"{color}{formatted}{self.reset}"

        return formatted

    def debug(self, message: str):
        """Logs a debug message."""
        print(self._format_message(MessageType.DEBUG, message))

    def info(self, message: str):
        """Logs an informational message."""
        print(self._format_message(MessageType.INFO, message))

    def warning(self, message: str):
        """Logs a warning message."""
        print(self._format_message(MessageType.WARNING, message))

    def error(self, message: str):
        """Logs an error message."""
        print(self._format_message(MessageType.ERROR, message))

    def success(self, message: str):
        """Logs a success message."""
        print(self._format_message(MessageType.SUCCESS, message))

    def sent(self, message: str):
        """Logs a sent message."""
        print(self._format_message(MessageType.SENT, f"Sent: {self.unhide_especial_chars(message)}"))

    def received(self, message: str):
        """Logs a received message."""
        print(self._format_message(MessageType.RECEIVED, f"Recv: {self.unhide_especial_chars(message)}"))

    def waiting(self, message: str):
        """Logs a waiting message."""
        print(self._format_message(MessageType.WAITING, message))

    def separator(self, length: int = 60, char: str = '='):
        """Logs a separator line."""
        print(char * length)

    def section(self, title: str, length: int = 60, char: str = '-'):
        """Logs a section title with separators."""
        title = f" {title} "
        if len(title) >= length:
            print(f"{title[:length]}")
            return
        total_fill = length - len(title)
        left = total_fill // 2
        right = total_fill - left
        print(f"{char * left}{title}{char * right}")

    def blank_line(self):
        """Logs a blank line."""
        print()

    def unhide_especial_chars(self, message: str):
        """Returns the message, replacing the special characters so they are visible."""
        return ((message.replace('\r', '␍')
                 .replace('\n', '␊'))
                .replace('\x00', '␀'))
