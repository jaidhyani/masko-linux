class SessionFinishedStore:
    def __init__(self, default_duration=5000):
        self.default_duration = default_duration

    def make_toast_data(self, session):
        name = session.cwd.rstrip("/").rsplit("/", 1)[-1] if session.cwd else "Session"
        return {"text": f"{name} session finished", "duration": self.default_duration}
