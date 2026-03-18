import json
import pytest
import asyncio
from backend.server import create_app


@pytest.fixture
async def aiohttp_app():
    return create_app()


async def test_health(aiohttp_client, aiohttp_app):
    client = await aiohttp_client(aiohttp_app)
    resp = await client.get("/health")
    assert resp.status == 200


async def test_hook_session_start(aiohttp_client, aiohttp_app):
    client = await aiohttp_client(aiohttp_app)
    resp = await client.post("/hook", json={
        "hook_event_name": "SessionStart",
        "session_id": "test-session",
        "cwd": "/tmp/project",
        "model": "opus",
    })
    assert resp.status == 200
    session = client.app["session_store"].get("test-session")
    assert session is not None
    assert session.cwd == "/tmp/project"


async def test_hook_invalid_json(aiohttp_client, aiohttp_app):
    client = await aiohttp_client(aiohttp_app)
    resp = await client.post("/hook", data=b"not json", headers={"Content-Type": "application/json"})
    assert resp.status == 400


async def test_input_endpoint(aiohttp_client, aiohttp_app):
    client = await aiohttp_client(aiohttp_app)
    resp = await client.post("/input", json={"name": "claudeCode::isWorking", "value": True})
    assert resp.status == 200
