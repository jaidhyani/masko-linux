import json
from pathlib import Path
import copy

DEFAULTS = {
    "selected_mascot": "masko",
    "overlay": {"x": 100, "y": 100, "width": 150, "height": 150},
    "hotkeys": {"modifier": "Super", "toggle": "m", "approve": "Return", "deny": "Escape"},
    "server": {"hook_port": 49152, "dashboard_port": 49153, "permission_timeout": 120},
    "notifications": {"system_notify": True},
}

class Config:
    def __init__(self, path=None):
        self._path = Path(path) if path else Path.home() / ".config" / "masko-desktop" / "config.json"
        self._data = _deep_merge(copy.deepcopy(DEFAULTS), self._load())

    def _load(self):
        if self._path.exists():
            return json.loads(self._path.read_text())
        return {}

    def save(self):
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._path.write_text(json.dumps(self._data, indent=2))

    @property
    def hook_port(self):
        return self._data["server"]["hook_port"]

    @property
    def dashboard_port(self):
        return self._data["server"]["dashboard_port"]

    @property
    def permission_timeout(self):
        return self._data["server"]["permission_timeout"]

    @property
    def selected_mascot(self):
        return self._data["selected_mascot"]

    @selected_mascot.setter
    def selected_mascot(self, value):
        self._data["selected_mascot"] = value

    @property
    def hotkeys(self):
        return self._data["hotkeys"]

    @property
    def overlay_position(self):
        return self._data["overlay"]

def _deep_merge(base, override):
    for k, v in override.items():
        if k in base and isinstance(base[k], dict) and isinstance(v, dict):
            _deep_merge(base[k], v)
        else:
            base[k] = v
    return base
