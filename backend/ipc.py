import asyncio
import json
import os
from typing import Callable, Optional


class IPCServer:
    def __init__(self, sock_path: str, on_message: Callable):
        self._sock_path = sock_path
        self._on_message = on_message
        self._server = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._reader_task: Optional[asyncio.Task] = None

    async def start(self):
        if os.path.exists(self._sock_path):
            os.unlink(self._sock_path)
        self._server = await asyncio.start_unix_server(self._handle_client, path=self._sock_path)

    async def stop(self):
        if self._reader_task:
            self._reader_task.cancel()
        if self._writer:
            self._writer.close()
        if self._server:
            self._server.close()
            await self._server.wait_closed()
        if os.path.exists(self._sock_path):
            os.unlink(self._sock_path)

    async def send(self, msg: dict):
        if not self._writer:
            return
        try:
            data = json.dumps(msg).encode() + b"\n"
            self._writer.write(data)
            await self._writer.drain()
        except (ConnectionError, OSError):
            self._writer = None

    @property
    def connected(self) -> bool:
        return self._writer is not None

    async def _handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        self._writer = writer
        self._reader_task = asyncio.current_task()
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break
                try:
                    msg = json.loads(line)
                    await self._on_message(msg)
                except json.JSONDecodeError:
                    pass
        except asyncio.CancelledError:
            pass
        finally:
            self._writer = None
