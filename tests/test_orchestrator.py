import json
import pytest
from pathlib import Path

from backend.config import Config
from backend.__main__ import Orchestrator


@pytest.fixture
def mascot_config():
    """Minimal mascot config for testing."""
    return {
        "version": "2.0",
        "name": "TestMascot",
        "initialNode": "node-idle",
        "autoPlay": True,
        "nodes": [
            {"id": "node-idle", "name": "Idle"},
            {"id": "node-working", "name": "Working"},
        ],
        "edges": [
            {
                "id": "edge-idle-loop",
                "isLoop": True,
                "source": "node-idle",
                "target": "node-idle",
                "videos": {"webm": "/tmp/fake-idle.webm"},
                "conditions": [],
            },
            {
                "id": "edge-to-working",
                "source": "node-idle",
                "target": "node-working",
                "videos": {"webm": "/tmp/fake-transition.webm"},
                "conditions": [{"input": "claudeCode::isWorking", "op": "==", "value": True}],
            },
        ],
        "inputs": [],
    }


@pytest.fixture
def test_config(tmp_path, mascot_config):
    """Create a config and mascot file in a temp directory."""
    config_path = tmp_path / "config.json"
    config_path.write_text(json.dumps({
        "selected_mascot": "test-mascot",
        "server": {"hook_port": 0, "dashboard_port": 0, "permission_timeout": 5},
    }))

    mascot_dir = tmp_path / "mascots"
    mascot_dir.mkdir()
    (mascot_dir / "test-mascot.json").write_text(json.dumps(mascot_config))

    return Config(path=str(config_path))


@pytest.fixture
def orchestrator(test_config, tmp_path, mascot_config, monkeypatch):
    """Create an Orchestrator without spawning the overlay."""
    mascot_path = tmp_path / "mascots" / "test-mascot.json"

    # Create fake video files so VideoCache.resolve() finds them on disk
    for name in ("fake-idle.webm", "fake-transition.webm"):
        (tmp_path / name).write_bytes(b"\x00")

    # Update mascot config to use paths that exist on disk
    mascot_config = json.loads(mascot_path.read_text())
    for edge in mascot_config["edges"]:
        videos = edge.get("videos", {})
        for fmt in list(videos):
            filename = videos[fmt].rsplit("/", 1)[-1]
            videos[fmt] = str(tmp_path / filename)
    mascot_path.write_text(json.dumps(mascot_config))

    def mock_load(self):
        return json.loads(mascot_path.read_text())

    monkeypatch.setattr(Orchestrator, "_load_mascot_config", mock_load)

    orch = Orchestrator(test_config)
    return orch


@pytest.fixture
async def aiohttp_app(orchestrator):
    return orchestrator.app


async def test_session_start_updates_store(aiohttp_client, aiohttp_app):
    client = await aiohttp_client(aiohttp_app)

    resp = await client.post("/hook", json={
        "hook_event_name": "SessionStart",
        "session_id": "sess-001",
        "cwd": "/home/user/my-project",
        "model": "opus",
    })
    assert resp.status == 200

    session = client.app["session_store"].get("sess-001")
    assert session is not None
    assert session.cwd == "/home/user/my-project"
    assert session.phase == "working"


async def test_notification_event_updates_store(aiohttp_client, aiohttp_app):
    client = await aiohttp_client(aiohttp_app)

    resp = await client.post("/hook", json={
        "hook_event_name": "Notification",
        "session_id": "sess-002",
        "title": "Task Done",
        "message": "Build completed successfully",
        "notification_type": "info",
    })
    assert resp.status == 200

    notifications = client.app["notification_store"].all()
    assert len(notifications) == 1
    assert notifications[0].title == "Task Done"
    assert notifications[0].message == "Build completed successfully"


async def test_event_processor_wired(aiohttp_client, aiohttp_app):
    """Verify the event processor is present and processes events."""
    client = await aiohttp_client(aiohttp_app)

    assert client.app.get("event_processor") is not None

    resp = await client.post("/hook", json={
        "hook_event_name": "SessionStart",
        "session_id": "sess-003",
        "cwd": "/tmp/test",
    })
    assert resp.status == 200

    events = client.app["event_store"].recent()
    assert len(events) >= 1
    assert events[-1].session_id == "sess-003"


async def test_state_machine_wired(aiohttp_client, aiohttp_app):
    """State machine should be set on the app."""
    client = await aiohttp_client(aiohttp_app)
    assert client.app["state_machine"] is not None


async def test_health_endpoint(aiohttp_client, aiohttp_app):
    client = await aiohttp_client(aiohttp_app)
    resp = await client.get("/health")
    assert resp.status == 200
