class SessionSwitcherStore:
    def __init__(self):
        self.is_open = False
        self.selected = 0
        self.sessions = []

    def open(self, sessions):
        self.sessions = sessions
        self.selected = 0
        self.is_open = True

    def close(self):
        self.is_open = False
        self.sessions = []

    def nav(self, direction):
        if not self.sessions:
            return
        if direction == "next":
            self.selected = (self.selected + 1) % len(self.sessions)
        elif direction == "prev":
            self.selected = (self.selected - 1) % len(self.sessions)
