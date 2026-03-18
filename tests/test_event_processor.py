import json
from pathlib import Path
from backend.models import ClaudeEvent, HookEventType
from backend.stores.event import EventStore
from backend.stores.session import SessionStore
from backend.stores.notification import NotificationStore
from backend.state_machine import StateMachine, ConditionValue
from backend.event_processor import EventProcessor

MASCOT_DIR = Path(__file__).parent.parent / "resources" / "mascots"

def make_processor():
    config = json.loads((MASCOT_DIR / "masko.json").read_text())
    sm = StateMachine(config)
    sm.start()
    return EventProcessor(
        event_store=EventStore(),
        session_store=SessionStore(),
        notification_store=NotificationStore(),
        state_machine=sm,
    )

def test_session_start_sets_working():
    ep = make_processor()
    ev = ClaudeEvent(hook_event_name="SessionStart", session_id="s1", cwd="/project")
    ep.process(ev)
    assert ep.state_machine.inputs["claudeCode::isWorking"] == ConditionValue(True)
    ep.state_machine.handle_video_ended()
    assert ep.state_machine.current_node_name == "Working"

def test_session_end_sets_idle():
    ep = make_processor()
    ep.process(ClaudeEvent(hook_event_name="SessionStart", session_id="s1"))
    ep.process(ClaudeEvent(hook_event_name="SessionEnd", session_id="s1"))
    assert ep.state_machine.inputs["claudeCode::isIdle"] == ConditionValue(True)

def test_permission_request_sets_alert():
    ep = make_processor()
    ep.process(ClaudeEvent(hook_event_name="SessionStart", session_id="s1"))
    ep.process(ClaudeEvent(hook_event_name="PermissionRequest", session_id="s1", tool_name="Bash"))
    assert ep.state_machine.inputs["claudeCode::isAlert"] == ConditionValue(True)

def test_notification_generated():
    ep = make_processor()
    ep.process(ClaudeEvent(hook_event_name="Notification", session_id="s1",
                           title="Done", message="Task complete"))
    assert ep.notification_store.unread_count() == 1

def test_precompact_sets_compacting():
    ep = make_processor()
    ep.process(ClaudeEvent(hook_event_name="SessionStart", session_id="s1"))
    ep.process(ClaudeEvent(hook_event_name="PreCompact", session_id="s1"))
    assert ep.state_machine.inputs["claudeCode::isCompacting"] == ConditionValue(True)
