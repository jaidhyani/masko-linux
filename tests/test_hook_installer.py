import json
import os
from pathlib import Path
from backend.hook_installer import HookInstaller

def test_install_creates_script(tmp_path):
    installer = HookInstaller(
        settings_path=tmp_path / ".claude" / "settings.json",
        hook_dir=tmp_path / ".masko-desktop" / "hooks",
        config_path=tmp_path / "config.json",
    )
    (tmp_path / "config.json").write_text(json.dumps({"server": {"hook_port": 49152}}))
    installer.install()
    script = tmp_path / ".masko-desktop" / "hooks" / "hook-sender.sh"
    assert script.exists()
    assert os.access(script, os.X_OK)

def test_install_registers_hooks(tmp_path):
    installer = HookInstaller(
        settings_path=tmp_path / ".claude" / "settings.json",
        hook_dir=tmp_path / ".masko-desktop" / "hooks",
        config_path=tmp_path / "config.json",
    )
    (tmp_path / "config.json").write_text(json.dumps({"server": {"hook_port": 49152}}))
    installer.install()
    settings = json.loads((tmp_path / ".claude" / "settings.json").read_text())
    hooks = settings["hooks"]
    assert "SessionStart" in hooks
    assert "PermissionRequest" in hooks
    assert len(hooks) == 17

def test_is_registered(tmp_path):
    installer = HookInstaller(
        settings_path=tmp_path / ".claude" / "settings.json",
        hook_dir=tmp_path / ".masko-desktop" / "hooks",
        config_path=tmp_path / "config.json",
    )
    (tmp_path / "config.json").write_text(json.dumps({"server": {"hook_port": 49152}}))
    assert not installer.is_registered()
    installer.install()
    assert installer.is_registered()

def test_uninstall(tmp_path):
    installer = HookInstaller(
        settings_path=tmp_path / ".claude" / "settings.json",
        hook_dir=tmp_path / ".masko-desktop" / "hooks",
        config_path=tmp_path / "config.json",
    )
    (tmp_path / "config.json").write_text(json.dumps({"server": {"hook_port": 49152}}))
    installer.install()
    installer.uninstall()
    assert not installer.is_registered()

def test_install_preserves_existing_hooks(tmp_path):
    settings_path = tmp_path / ".claude" / "settings.json"
    settings_path.parent.mkdir(parents=True)
    settings_path.write_text(json.dumps({
        "hooks": {"SessionStart": [{"matcher": "", "hooks": [{"type": "command", "command": "other-hook.sh"}]}]}
    }))
    installer = HookInstaller(
        settings_path=settings_path,
        hook_dir=tmp_path / ".masko-desktop" / "hooks",
        config_path=tmp_path / "config.json",
    )
    (tmp_path / "config.json").write_text(json.dumps({"server": {"hook_port": 49152}}))
    installer.install()
    settings = json.loads(settings_path.read_text())
    session_start_hooks = settings["hooks"]["SessionStart"]
    assert len(session_start_hooks) == 2
