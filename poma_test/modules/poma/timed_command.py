class TimedCommand:
    """Representa un comando con su timestamp de envío"""

    def __init__(self, timestamp: float, command: str):
        self.timestamp = timestamp
        self.command = command

    def __repr__(self):
        return f"TimedCommand(t={self.timestamp}, msg='{self.command}')"
