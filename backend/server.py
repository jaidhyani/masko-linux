import json
import weakref
from dataclasses import asdict
from datetime import datetime
from pathlib import Path
from aiohttp import web
from backend.models import ClaudeEvent
from backend.stores.event import EventStore
from backend.stores.session import SessionStore
from backend.stores.notification import NotificationStore
from backend.stores.permission import PendingPermissionStore
from backend.config import Config

DASHBOARD_DIR = Path(__file__).resolve().parent.parent / "dashboard"
MASCOTS_DIR = Path(__file__).resolve().parent.parent / "resources" / "mascots"


def _json_serializer(obj):
    if isinstance(obj, datetime):
        return obj.isoformat()
    raise TypeError(f"Not serializable: {type(obj)}")


def _json_response(data):
    body = json.dumps(data, default=_json_serializer)
    return web.Response(text=body, content_type="application/json")


def create_app(config=None):
    app = web.Application()
    app["config"] = config or Config()
    app["event_store"] = EventStore()
    app["session_store"] = SessionStore()
    app["notification_store"] = NotificationStore()
    app["permission_store"] = PendingPermissionStore()
    app["state_machine"] = None
    app["websockets"] = weakref.WeakSet()

    app.router.add_get("/health", handle_health)
    app.router.add_post("/hook", handle_hook)
    app.router.add_post("/input", handle_input)

    app.router.add_get("/api/sessions", handle_api_sessions)
    app.router.add_get("/api/events", handle_api_events)
    app.router.add_get("/api/notifications", handle_api_notifications)
    app.router.add_get("/api/config", handle_api_config)
    app.router.add_post("/api/notifications/{id}/read", handle_notif_read)
    app.router.add_post("/api/notifications/clear", handle_notif_clear)
    app.router.add_get("/ws", handle_websocket)

    app.router.add_get("/", handle_dashboard)
    app.router.add_static("/dashboard/", DASHBOARD_DIR)

    return app


def create_dashboard_app(hook_app, orchestrator=None):
    """Build a dashboard app that shares stores with the hook app."""
    app = web.Application()
    app["config"] = hook_app["config"]
    app["event_store"] = hook_app["event_store"]
    app["session_store"] = hook_app["session_store"]
    app["notification_store"] = hook_app["notification_store"]
    app["permission_store"] = hook_app["permission_store"]
    app["state_machine"] = hook_app.get("state_machine")
    app["websockets"] = weakref.WeakSet()
    if orchestrator:
        app["actions"] = orchestrator.actions
        app["orchestrator"] = orchestrator

    app.router.add_get("/health", handle_health)
    app.router.add_get("/api/sessions", handle_api_sessions)
    app.router.add_get("/api/events", handle_api_events)
    app.router.add_get("/api/notifications", handle_api_notifications)
    app.router.add_get("/api/config", handle_api_config)
    app.router.add_post("/api/config", handle_api_config_post)
    app.router.add_get("/api/actions", handle_api_actions)
    app.router.add_post("/api/actions/{action_id}", handle_api_actions_execute)
    app.router.add_get("/api/mascots", handle_api_mascots)
    app.router.add_post("/api/notifications/{id}/read", handle_notif_read)
    app.router.add_post("/api/notifications/clear", handle_notif_clear)
    app.router.add_get("/ws", handle_websocket)
    app.router.add_get("/", handle_dashboard)
    app.router.add_static("/dashboard/", DASHBOARD_DIR)

    return app


async def handle_health(request):
    return web.Response(text="ok")


async def handle_hook(request):
    try:
        data = await request.json()
    except json.JSONDecodeError:
        return web.Response(status=400, text="invalid json")

    event = ClaudeEvent.from_dict(data)
    app = request.app
    app["event_store"].add(event)
    app["session_store"].handle_event(event)

    await _broadcast(app, {"type": "event", "event_name": event.hook_event_name})

    return web.Response(text="ok")


async def handle_input(request):
    try:
        data = await request.json()
    except json.JSONDecodeError:
        return web.Response(status=400, text="invalid json")

    sm = request.app.get("state_machine")
    if sm:
        from backend.state_machine import ConditionValue
        name = data.get("name", "")
        value = data.get("value")
        sm.set_input(name, ConditionValue(value))

    return web.Response(text="ok")


# --- Dashboard API ---

async def handle_api_sessions(request):
    sessions = request.app["session_store"].all_sessions()
    data = [asdict(s) for s in sessions]
    return _json_response(data)


async def handle_api_events(request):
    events = request.app["event_store"].recent(limit=200)
    data = []
    for e in events:
        data.append({
            "id": e.id,
            "hook_event_name": e.hook_event_name,
            "session_id": e.session_id,
            "tool_name": e.tool_name,
            "received_at": e.received_at.isoformat() if e.received_at else None,
        })
    return _json_response(data)


async def handle_api_notifications(request):
    notifications = request.app["notification_store"].all()
    data = [asdict(n) for n in notifications]
    return _json_response(data)


async def handle_api_config(request):
    cfg = request.app["config"]
    return _json_response(cfg._data)


async def handle_notif_read(request):
    nid = request.match_info["id"]
    request.app["notification_store"].mark_read(nid)
    return web.Response(text="ok")


async def handle_notif_clear(request):
    request.app["notification_store"].clear_all()
    return web.Response(text="ok")


async def handle_api_actions(request):
    actions = request.app.get("actions")
    if not actions:
        return _json_response([])
    return _json_response(actions.to_dashboard_json())


async def handle_api_actions_execute(request):
    action_id = request.match_info["action_id"]
    actions = request.app.get("actions")
    if not actions:
        return web.Response(status=404, text="no action registry")
    action = actions.get(action_id)
    if not action:
        return web.Response(status=404, text="unknown action")
    await actions.execute(action_id)
    return web.Response(text="ok")


async def handle_api_mascots(request):
    mascots = sorted(
        p.stem for p in MASCOTS_DIR.glob("*.json")
    ) if MASCOTS_DIR.exists() else []
    return _json_response(mascots)


async def handle_api_config_post(request):
    try:
        data = await request.json()
    except json.JSONDecodeError:
        return web.Response(status=400, text="invalid json")

    cfg = request.app["config"]
    orchestrator = request.app.get("orchestrator")

    mascot_changed = (
        "selected_mascot" in data
        and data["selected_mascot"] != cfg.selected_mascot
    )

    for key, value in data.items():
        if isinstance(value, dict) and key in cfg._data and isinstance(cfg._data[key], dict):
            cfg._data[key].update(value)
        else:
            cfg._data[key] = value
    cfg.save()

    if mascot_changed and orchestrator:
        from backend.state_machine import StateMachine
        mascot_config = orchestrator._load_mascot_config()
        orchestrator.state_machine = StateMachine(mascot_config)
        orchestrator._wire_state_machine()
        orchestrator.state_machine.start()

    return web.Response(text="ok")


async def handle_dashboard(request):
    index = DASHBOARD_DIR / "index.html"
    if index.exists():
        return web.FileResponse(index)
    return web.Response(status=404, text="dashboard not found")


async def handle_websocket(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    request.app["websockets"].add(ws)

    try:
        async for msg in ws:
            pass
    finally:
        request.app["websockets"].discard(ws)

    return ws


async def _broadcast(app, data):
    payload = json.dumps(data)
    dead = []
    for ws in set(app["websockets"]):
        try:
            await ws.send_str(payload)
        except Exception:
            dead.append(ws)
    for ws in dead:
        app["websockets"].discard(ws)
