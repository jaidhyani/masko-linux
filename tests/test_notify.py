from unittest.mock import patch, MagicMock
from backend.notify import send_notification


def test_sends_notification():
    with patch("backend.notify.shutil") as mock_shutil, \
         patch("backend.notify.subprocess") as mock_sub:
        mock_shutil.which.return_value = "/usr/bin/notify-send"
        send_notification("Title", "Body")
        mock_sub.run.assert_called_once()
        args = mock_sub.run.call_args[0][0]
        assert args[0] == "notify-send"
        assert "Title" in args
        assert "Body" in args


def test_no_notify_send():
    with patch("backend.notify.shutil") as mock_shutil:
        mock_shutil.which.return_value = None
        send_notification("Title")
