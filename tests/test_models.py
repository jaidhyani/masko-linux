import json
from backend.models import ClaudeEvent, HookEventType

def test_parse_event_from_json():
    raw = json.dumps({
        "hook_event_name": "SessionStart",
        "session_id": "abc-123",
        "cwd": "/home/user/project",
        "model": "opus",
    })
    event = ClaudeEvent.from_json(raw)
    assert event.hook_event_name == "SessionStart"
    assert event.session_id == "abc-123"
    assert event.event_type == HookEventType.SESSION_START

def test_parse_tool_event():
    raw = json.dumps({
        "hook_event_name": "PreToolUse",
        "session_id": "abc-123",
        "tool_name": "Bash",
        "tool_input": {"command": "ls"},
        "tool_use_id": "tu-456",
    })
    event = ClaudeEvent.from_json(raw)
    assert event.tool_name == "Bash"
    assert event.tool_input == {"command": "ls"}

def test_parse_permission_event():
    raw = json.dumps({
        "hook_event_name": "PermissionRequest",
        "session_id": "abc-123",
        "tool_name": "Bash",
        "tool_input": {"command": "rm -rf /"},
        "tool_use_id": "tu-789",
    })
    event = ClaudeEvent.from_json(raw)
    assert event.event_type == HookEventType.PERMISSION_REQUEST
    assert event.tool_use_id == "tu-789"

def test_hook_event_types():
    assert HookEventType.SESSION_START.value == "SessionStart"
    assert HookEventType.STOP.value == "Stop"
    assert HookEventType.PRE_TOOL_USE.value == "PreToolUse"

def test_from_dict():
    data = {"hook_event_name": "Stop", "session_id": "s1"}
    event = ClaudeEvent.from_dict(data)
    assert event.hook_event_name == "Stop"
    assert event.session_id == "s1"
