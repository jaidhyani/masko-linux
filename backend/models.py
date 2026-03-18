import json
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Optional


class HookEventType(Enum):
    PRE_TOOL_USE = "PreToolUse"
    POST_TOOL_USE = "PostToolUse"
    POST_TOOL_USE_FAILURE = "PostToolUseFailure"
    STOP = "Stop"
    NOTIFICATION = "Notification"
    SESSION_START = "SessionStart"
    SESSION_END = "SessionEnd"
    TASK_COMPLETED = "TaskCompleted"
    PERMISSION_REQUEST = "PermissionRequest"
    USER_PROMPT_SUBMIT = "UserPromptSubmit"
    SUBAGENT_START = "SubagentStart"
    SUBAGENT_STOP = "SubagentStop"
    PRE_COMPACT = "PreCompact"
    CONFIG_CHANGE = "ConfigChange"
    TEAMMATE_IDLE = "TeammateIdle"
    WORKTREE_CREATE = "WorktreeCreate"
    WORKTREE_REMOVE = "WorktreeRemove"


@dataclass
class ClaudeEvent:
    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    hook_event_name: str = ""
    session_id: Optional[str] = None
    cwd: Optional[str] = None
    permission_mode: Optional[str] = None
    transcript_path: Optional[str] = None
    tool_name: Optional[str] = None
    tool_input: Optional[dict[str, Any]] = None
    tool_response: Optional[dict[str, Any]] = None
    tool_use_id: Optional[str] = None
    message: Optional[str] = None
    title: Optional[str] = None
    notification_type: Optional[str] = None
    source: Optional[str] = None
    reason: Optional[str] = None
    model: Optional[str] = None
    stop_hook_active: Optional[bool] = None
    last_assistant_message: Optional[str] = None
    agent_id: Optional[str] = None
    agent_type: Optional[str] = None
    task_id: Optional[str] = None
    task_subject: Optional[str] = None
    permission_suggestions: Optional[list] = None
    terminal_pid: Optional[int] = None
    shell_pid: Optional[int] = None
    received_at: datetime = field(default_factory=datetime.now)

    @property
    def event_type(self) -> Optional[HookEventType]:
        try:
            return HookEventType(self.hook_event_name)
        except ValueError:
            return None

    @property
    def project_name(self) -> Optional[str]:
        if self.cwd:
            return self.cwd.rstrip("/").rsplit("/", 1)[-1]
        return None

    @classmethod
    def from_json(cls, raw: str) -> "ClaudeEvent":
        return cls.from_dict(json.loads(raw))

    @classmethod
    def from_dict(cls, data: dict) -> "ClaudeEvent":
        return cls(
            hook_event_name=data.get("hook_event_name", ""),
            session_id=data.get("session_id"),
            cwd=data.get("cwd"),
            permission_mode=data.get("permission_mode"),
            transcript_path=data.get("transcript_path"),
            tool_name=data.get("tool_name"),
            tool_input=data.get("tool_input"),
            tool_response=data.get("tool_response"),
            tool_use_id=data.get("tool_use_id"),
            message=data.get("message"),
            title=data.get("title"),
            notification_type=data.get("notification_type"),
            source=data.get("source"),
            reason=data.get("reason"),
            model=data.get("model"),
            stop_hook_active=data.get("stop_hook_active"),
            last_assistant_message=data.get("last_assistant_message"),
            agent_id=data.get("agent_id"),
            agent_type=data.get("agent_type"),
            task_id=data.get("task_id"),
            task_subject=data.get("task_subject"),
            permission_suggestions=data.get("permission_suggestions"),
            terminal_pid=data.get("terminal_pid"),
            shell_pid=data.get("shell_pid"),
        )
