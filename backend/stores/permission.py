import asyncio
from dataclasses import dataclass
from typing import Any, Optional


@dataclass
class PendingPermission:
    tool_use_id: str
    tool_name: str
    tool_input: dict[str, Any]
    session_id: str
    response_future: asyncio.Future


class PendingPermissionStore:
    def __init__(self):
        self._pending: dict[str, PendingPermission] = {}

    def add(self, tool_use_id: str, permission: PendingPermission):
        self._pending[tool_use_id] = permission

    def get(self, tool_use_id: str) -> Optional[PendingPermission]:
        return self._pending.get(tool_use_id)

    def resolve(self, tool_use_id: str, action: str):
        perm = self._pending.pop(tool_use_id, None)
        if perm and not perm.response_future.done():
            perm.response_future.set_result(action)

    def deny_all(self):
        for perm in list(self._pending.values()):
            if not perm.response_future.done():
                perm.response_future.set_result("deny")
        self._pending.clear()

    def all_pending(self) -> list[PendingPermission]:
        return list(self._pending.values())
