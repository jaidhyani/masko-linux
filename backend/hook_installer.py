import json
import os
import stat
from pathlib import Path

HOOK_EVENTS = [
    "PreToolUse", "PostToolUse", "PostToolUseFailure", "Stop",
    "Notification", "SessionStart", "SessionEnd", "TaskCompleted",
    "PermissionRequest", "UserPromptSubmit", "SubagentStart",
    "SubagentStop", "PreCompact", "ConfigChange", "TeammateIdle",
    "WorktreeCreate", "WorktreeRemove",
]

HOOK_SCRIPT_TEMPLATE = Path(__file__).parent.parent / "resources" / "hooks" / "hook-sender.sh"


class HookInstaller:
    def __init__(self, settings_path=None, hook_dir=None, config_path=None):
        self._settings_path = Path(settings_path) if settings_path else Path.home() / ".claude" / "settings.json"
        self._hook_dir = Path(hook_dir) if hook_dir else Path.home() / ".masko-desktop" / "hooks"
        self._config_path = Path(config_path) if config_path else Path.home() / ".config" / "masko-desktop" / "config.json"
        self._hook_command = str(self._hook_dir / "hook-sender.sh")

    def is_registered(self) -> bool:
        settings = self._read_settings()
        hooks = settings.get("hooks", {})
        for event in HOOK_EVENTS:
            entries = hooks.get(event, [])
            for entry in entries:
                for h in entry.get("hooks", []):
                    if h.get("command") == self._hook_command:
                        return True
        return False

    def install(self):
        self._ensure_script()
        settings = self._read_settings()
        hooks = settings.setdefault("hooks", {})

        hook_entry = {"matcher": "", "hooks": [{"type": "command", "command": self._hook_command}]}

        for event in HOOK_EVENTS:
            entries = hooks.setdefault(event, [])
            already = any(
                h.get("command") == self._hook_command
                for entry in entries
                for h in entry.get("hooks", [])
            )
            if not already:
                entries.append(hook_entry)

        self._write_settings(settings)

    def uninstall(self):
        settings = self._read_settings()
        hooks = settings.get("hooks", {})
        for event in HOOK_EVENTS:
            entries = hooks.get(event, [])
            hooks[event] = [
                entry for entry in entries
                if not any(h.get("command") == self._hook_command for h in entry.get("hooks", []))
            ]
            if not hooks[event]:
                del hooks[event]
        self._write_settings(settings)

    def _ensure_script(self):
        self._hook_dir.mkdir(parents=True, exist_ok=True)
        dest = self._hook_dir / "hook-sender.sh"
        if HOOK_SCRIPT_TEMPLATE.exists():
            dest.write_text(HOOK_SCRIPT_TEMPLATE.read_text())
        else:
            dest.write_text("#!/usr/bin/env bash\n# placeholder\n")
        dest.chmod(dest.stat().st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    def _read_settings(self) -> dict:
        if self._settings_path.exists():
            return json.loads(self._settings_path.read_text())
        return {}

    def _write_settings(self, settings: dict):
        self._settings_path.parent.mkdir(parents=True, exist_ok=True)
        self._settings_path.write_text(json.dumps(settings, indent=2))
