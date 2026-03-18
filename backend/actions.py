"""Declarative action registry. Define actions once, expose everywhere."""

from dataclasses import dataclass, field
from typing import Callable, Optional


@dataclass
class Action:
    id: str
    label: str
    surfaces: list[str] = field(default_factory=lambda: ["context_menu", "dashboard"])
    category: str = "navigation"
    separator_after: bool = False
    hotkey: Optional[str] = None
    handler: Optional[Callable] = None


class ActionRegistry:
    def __init__(self):
        self._actions: dict[str, Action] = {}
        self._register_defaults()

    def _register_defaults(self):
        self.register(Action(
            id="open_dashboard", label="Dashboard",
            surfaces=["context_menu"], category="navigation",
            separator_after=True,
        ))
        self.register(Action(
            id="change_mascot", label="Change Mascot...",
            surfaces=["context_menu", "dashboard"], category="mascot",
        ))
        self.register(Action(
            id="reset_position", label="Reset Position",
            surfaces=["context_menu"], category="overlay",
            separator_after=True,
        ))
        self.register(Action(
            id="toggle_visibility", label="Hide Mascot",
            surfaces=["context_menu", "dashboard", "hotkey"], category="overlay",
            hotkey="Super+M",
        ))
        self.register(Action(
            id="quit", label="Quit",
            surfaces=["context_menu"], category="system",
        ))

    def register(self, action: Action):
        self._actions[action.id] = action

    def get(self, action_id: str) -> Optional[Action]:
        return self._actions.get(action_id)

    async def execute(self, action_id: str):
        action = self._actions.get(action_id)
        if action and action.handler:
            await action.handler()

    def for_surface(self, surface: str) -> list[Action]:
        return [a for a in self._actions.values() if surface in a.surfaces]

    def to_menu_json(self) -> list[dict]:
        """JSON for the C overlay context menu."""
        return [
            {"id": a.id, "label": a.label, "separator_after": a.separator_after}
            for a in self.for_surface("context_menu")
        ]

    def to_dashboard_json(self) -> list[dict]:
        """JSON for the dashboard settings page."""
        return [
            {"id": a.id, "label": a.label, "category": a.category, "hotkey": a.hotkey}
            for a in self.for_surface("dashboard")
        ]
