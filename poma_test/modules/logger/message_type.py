from enum import Enum


class MessageType(Enum):
    """Types of logable messages."""
    DEBUG = "DEBUG"
    INFO = "INFO"
    WARNING = "WARNING"
    ERROR = "ERROR"
    SUCCESS = "SUCCESS"
    SENT = "SENT"
    RECEIVED = "RECEIVED"
    WAITING = "WAITING"
    SEPARATOR = "SEPARATOR"
