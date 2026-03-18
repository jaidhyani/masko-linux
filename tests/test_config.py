import json
import os
import pytest
from backend.config import Config

def test_default_config(tmp_path):
    cfg = Config(path=tmp_path / "config.json")
    assert cfg.hook_port == 49152
    assert cfg.dashboard_port == 49153
    assert cfg.selected_mascot == "masko"
    assert cfg.hotkeys["modifier"] == "Super"
    assert cfg.permission_timeout == 120

def test_load_existing_config(tmp_path):
    p = tmp_path / "config.json"
    p.write_text(json.dumps({"selected_mascot": "clippy", "server": {"hook_port": 9999}}))
    cfg = Config(path=p)
    assert cfg.selected_mascot == "clippy"
    assert cfg.hook_port == 9999
    assert cfg.dashboard_port == 49153

def test_save_config(tmp_path):
    p = tmp_path / "config.json"
    cfg = Config(path=p)
    cfg.selected_mascot = "otto"
    cfg.save()
    loaded = json.loads(p.read_text())
    assert loaded["selected_mascot"] == "otto"
