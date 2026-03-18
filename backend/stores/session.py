from dataclasses import dataclass, field
from datetime import datetime
from typing import Optional
from backend.models import ClaudeEvent, HookEventType


@dataclass
class Session:
    id: str
    cwd: Optional[str] = None
    model: Optional[str] = None
    start_time: datetime = field(default_factory=datetime.now)
    end_time: Optional[datetime] = None
    event_count: int = 0
    terminal_pid: Optional[int] = None
    shell_pid: Optional[int] = None
    phase: str = "working"


class SessionStore:
    def __init__(self):
        self._sessions: dict[str, Session] = {}

    def handle_event(self, event: ClaudeEvent):
        sid = event.session_id
        if not sid:
            return

        if sid not in self._sessions:
            self._sessions[sid] = Session(
                id=sid,
                cwd=event.cwd,
                model=event.model,
                terminal_pid=event.terminal_pid,
                shell_pid=event.shell_pid,
            )

        session = self._sessions[sid]
        session.event_count += 1

        if event.cwd and not session.cwd:
            session.cwd = event.cwd
        if event.model and not session.model:
            session.model = event.model
        if event.terminal_pid and not session.terminal_pid:
            session.terminal_pid = event.terminal_pid

        et = event.event_type
        if et == HookEventType.SESSION_END:
            session.phase = "ended"
            session.end_time = datetime.now()
        elif et == HookEventType.PERMISSION_REQUEST:
            session.phase = "permission_pending"
        elif et == HookEventType.SESSION_START:
            session.phase = "working"
        elif session.phase == "permission_pending" and et in (
            HookEventType.PRE_TOOL_USE,
            HookEventType.POST_TOOL_USE,
        ):
            session.phase = "working"

    def get(self, session_id: str) -> Optional[Session]:
        return self._sessions.get(session_id)

    def active_sessions(self) -> list[Session]:
        return [s for s in self._sessions.values() if s.phase != "ended"]

    def all_sessions(self) -> list[Session]:
        return list(self._sessions.values())
