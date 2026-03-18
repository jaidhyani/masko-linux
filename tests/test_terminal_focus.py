from unittest.mock import patch, MagicMock
from backend.terminal_focus import focus_terminal


def test_focus_with_xdotool():
    with patch("backend.terminal_focus.shutil") as mock_shutil, \
         patch("backend.terminal_focus.subprocess") as mock_sub:
        mock_shutil.which.return_value = "/usr/bin/xdotool"
        mock_sub.run.return_value = MagicMock(stdout="12345\n", returncode=0)
        assert focus_terminal(1234) is True


def test_focus_no_tools():
    with patch("backend.terminal_focus.shutil") as mock_shutil:
        mock_shutil.which.return_value = None
        assert focus_terminal(1234) is False


def test_focus_no_pid():
    assert focus_terminal(0) is False
