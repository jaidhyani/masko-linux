from backend.models import ClaudeEvent
from backend.stores.session import SessionStore, Session

def test_session_start():
    store = SessionStore()
    ev = ClaudeEvent(hook_event_name="SessionStart", session_id="s1", cwd="/project", model="opus")
    store.handle_event(ev)
    s = store.get("s1")
    assert s is not None
    assert s.cwd == "/project"
    assert s.model == "opus"
    assert s.phase == "working"

def test_session_end():
    store = SessionStore()
    store.handle_event(ClaudeEvent(hook_event_name="SessionStart", session_id="s1"))
    store.handle_event(ClaudeEvent(hook_event_name="SessionEnd", session_id="s1"))
    s = store.get("s1")
    assert s.phase == "ended"
    assert s.end_time is not None

def test_permission_sets_phase():
    store = SessionStore()
    store.handle_event(ClaudeEvent(hook_event_name="SessionStart", session_id="s1"))
    store.handle_event(ClaudeEvent(hook_event_name="PermissionRequest", session_id="s1"))
    assert store.get("s1").phase == "permission_pending"

def test_event_count():
    store = SessionStore()
    store.handle_event(ClaudeEvent(hook_event_name="SessionStart", session_id="s1"))
    store.handle_event(ClaudeEvent(hook_event_name="PreToolUse", session_id="s1"))
    store.handle_event(ClaudeEvent(hook_event_name="PostToolUse", session_id="s1"))
    assert store.get("s1").event_count == 3

def test_active_sessions():
    store = SessionStore()
    store.handle_event(ClaudeEvent(hook_event_name="SessionStart", session_id="s1"))
    store.handle_event(ClaudeEvent(hook_event_name="SessionStart", session_id="s2"))
    store.handle_event(ClaudeEvent(hook_event_name="SessionEnd", session_id="s1"))
    active = store.active_sessions()
    assert len(active) == 1
    assert active[0].id == "s2"
