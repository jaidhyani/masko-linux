import asyncio
from backend.stores.permission import PendingPermissionStore, PendingPermission

def test_add_pending():
    store = PendingPermissionStore()
    future = asyncio.Future()
    store.add("tu-1", PendingPermission(
        tool_use_id="tu-1", tool_name="Bash",
        tool_input={"command": "ls"}, session_id="s1",
        response_future=future,
    ))
    assert store.get("tu-1") is not None
    assert len(store.all_pending()) == 1

def test_resolve_approve():
    loop = asyncio.new_event_loop()
    future = loop.create_future()
    store = PendingPermissionStore()
    store.add("tu-1", PendingPermission(
        tool_use_id="tu-1", tool_name="Bash",
        tool_input={}, session_id="s1",
        response_future=future,
    ))
    store.resolve("tu-1", "approve")
    assert future.result() == "approve"
    assert store.get("tu-1") is None
    loop.close()

def test_resolve_deny():
    loop = asyncio.new_event_loop()
    future = loop.create_future()
    store = PendingPermissionStore()
    store.add("tu-1", PendingPermission(
        tool_use_id="tu-1", tool_name="Bash",
        tool_input={}, session_id="s1",
        response_future=future,
    ))
    store.resolve("tu-1", "deny")
    assert future.result() == "deny"
    loop.close()

def test_resolve_nonexistent():
    store = PendingPermissionStore()
    store.resolve("nonexistent", "approve")  # should not raise

def test_deny_all():
    loop = asyncio.new_event_loop()
    f1 = loop.create_future()
    f2 = loop.create_future()
    store = PendingPermissionStore()
    store.add("tu-1", PendingPermission(tool_use_id="tu-1", tool_name="Bash", tool_input={}, session_id="s1", response_future=f1))
    store.add("tu-2", PendingPermission(tool_use_id="tu-2", tool_name="Write", tool_input={}, session_id="s1", response_future=f2))
    store.deny_all()
    assert f1.result() == "deny"
    assert f2.result() == "deny"
    assert len(store.all_pending()) == 0
    loop.close()
