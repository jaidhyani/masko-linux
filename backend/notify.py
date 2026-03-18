import subprocess
import shutil


def send_notification(title: str, body: str = ""):
    if shutil.which("notify-send"):
        cmd = ["notify-send", title]
        if body:
            cmd.append(body)
        subprocess.run(cmd, timeout=5)
