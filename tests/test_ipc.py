import asyncio
import json
import pytest
from backend.ipc import IPCServer

@pytest.mark.asyncio
async def test_send_and_receive(tmp_path):
    sock_path = str(tmp_path / "test.sock")
    received = []

    async def on_message(msg):
        received.append(msg)

    server = IPCServer(sock_path, on_message=on_message)
    await server.start()
    await asyncio.sleep(0.01)  # Allow server to start accepting connections

    reader, writer = await asyncio.open_unix_connection(sock_path)
    await asyncio.sleep(0.01)  # Allow _handle_client to run

    await server.send({"cmd": "show"})
    line = await asyncio.wait_for(reader.readline(), timeout=2)
    assert json.loads(line) == {"cmd": "show"}

    writer.write(json.dumps({"event": "ready"}).encode() + b"\n")
    await writer.drain()
    await asyncio.sleep(0.1)
    assert len(received) == 1
    assert received[0] == {"event": "ready"}

    writer.close()
    await server.stop()

@pytest.mark.asyncio
async def test_not_connected_send(tmp_path):
    sock_path = str(tmp_path / "test.sock")
    server = IPCServer(sock_path, on_message=lambda m: None)
    await server.start()
    await server.send({"cmd": "show"})  # should not raise
    await server.stop()
