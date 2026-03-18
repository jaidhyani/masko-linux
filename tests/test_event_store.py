from backend.models import ClaudeEvent
from backend.stores.event import EventStore

def test_add_and_retrieve():
    store = EventStore(max_events=100)
    ev = ClaudeEvent(hook_event_name="SessionStart", session_id="s1")
    store.add(ev)
    assert len(store.recent()) == 1
    assert store.recent()[0].session_id == "s1"

def test_max_events_eviction():
    store = EventStore(max_events=3)
    for i in range(5):
        store.add(ClaudeEvent(hook_event_name="PreToolUse", session_id=f"s{i}"))
    assert len(store.recent()) == 3
    assert store.recent()[0].session_id == "s2"

def test_filter_by_session():
    store = EventStore()
    store.add(ClaudeEvent(hook_event_name="SessionStart", session_id="s1"))
    store.add(ClaudeEvent(hook_event_name="PreToolUse", session_id="s2"))
    store.add(ClaudeEvent(hook_event_name="Stop", session_id="s1"))
    assert len(store.for_session("s1")) == 2
    assert len(store.for_session("s2")) == 1
