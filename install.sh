#!/usr/bin/env bash
set -euo pipefail
echo "=== Masko Desktop Linux Installer ==="

# Check dependencies
for cmd in python3 gcc pkg-config curl xdotool; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "ERROR: $cmd not found. Please install it."
        exit 1
    fi
done

for lib in x11 xext xcomposite xfixes cairo pango pangocairo libavformat libavcodec libavutil libswscale libcjson; do
    if ! pkg-config --exists "$lib" 2>/dev/null; then
        echo "ERROR: $lib development headers not found."
        echo "Run: sudo apt-get install lib${lib}-dev"
        exit 1
    fi
done

echo "Building overlay..."
make -C overlay

echo "Setting up Python environment..."
python3 -m venv .venv
.venv/bin/pip install -q -r requirements.txt

echo "Installing hooks..."
.venv/bin/python -c "
from backend.hook_installer import HookInstaller
installer = HookInstaller()
installer.install()
print('Hooks installed.')
"

echo "Installing systemd service..."
mkdir -p ~/.config/systemd/user/
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
sed "s|MASKO_DIR|$SCRIPT_DIR|g" masko.service > ~/.config/systemd/user/masko-desktop.service
systemctl --user daemon-reload
systemctl --user enable masko-desktop.service

echo ""
echo "Installation complete!"
echo "Start with: systemctl --user start masko-desktop"
echo "Dashboard:  http://localhost:49153"
