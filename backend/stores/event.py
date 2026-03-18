from collections import deque
from backend.models import ClaudeEvent


class EventStore:
    def __init__(self, max_events=1000):
        self._events = deque(maxlen=max_events)

    def add(self, event: ClaudeEvent):
        self._events.append(event)

    def recent(self, limit=None) -> list[ClaudeEvent]:
        events = list(self._events)
        if limit:
            return events[-limit:]
        return events

    def for_session(self, session_id: str) -> list[ClaudeEvent]:
        return [e for e in self._events if e.session_id == session_id]
