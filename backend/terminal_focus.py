import subprocess
import shutil


def focus_terminal(pid: int) -> bool:
    """Focus the terminal window containing the given PID."""
    if not pid:
        return False

    if shutil.which("xdotool"):
        result = subprocess.run(
            ["xdotool", "search", "--pid", str(pid)],
            capture_output=True, text=True, timeout=5
        )
        wids = result.stdout.strip().split("\n")
        if wids and wids[0]:
            subprocess.run(["xdotool", "windowactivate", "--sync", wids[0]], timeout=5)
            return True

    if shutil.which("wmctrl"):
        subprocess.run(["wmctrl", "-ia", str(pid)], timeout=5)
        return True

    return False
