#!/usr/bin/env bash
set -euo pipefail

INSTALL_DIR="${HOME}/.local/share/masko-desktop"
BIN_DIR="${HOME}/.local/bin"

echo "=== Masko Desktop Linux Installer ==="
echo "Install to: $INSTALL_DIR"
echo ""

# --- Dependency check ---
MISSING=0
for cmd in python3 gcc pkg-config curl; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "ERROR: $cmd not found."
        MISSING=1
    fi
done

for lib in x11 xext xcomposite xfixes cairo pango pangocairo libavformat libavcodec libavutil libswscale libcjson; do
    if ! pkg-config --exists "$lib" 2>/dev/null; then
        echo "ERROR: $lib-dev not found."
        MISSING=1
    fi
done

if [ "$MISSING" -eq 1 ]; then
    echo ""
    echo "Install missing dependencies:"
    echo "  sudo apt-get install gcc pkg-config curl xdotool wmctrl \\"
    echo "    libx11-dev libxext-dev libxcomposite-dev libxfixes-dev \\"
    echo "    libcairo2-dev libpango1.0-dev libcjson-dev \\"
    echo "    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev"
    exit 1
fi

# --- Determine source directory ---
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"

# --- Install files ---
echo "Copying files..."
mkdir -p "$INSTALL_DIR"
for dir in backend overlay dashboard resources; do
    rm -rf "$INSTALL_DIR/$dir"
    cp -r "$SRC_DIR/$dir" "$INSTALL_DIR/"
done
cp "$SRC_DIR/requirements.txt" "$INSTALL_DIR/"
cp "$SRC_DIR/Makefile" "$INSTALL_DIR/"

# Clean build artifacts from the copy
rm -f "$INSTALL_DIR/overlay/"*.o "$INSTALL_DIR/overlay/masko-overlay"
find "$INSTALL_DIR" -name __pycache__ -type d -exec rm -rf {} + 2>/dev/null || true

# --- Build C overlay ---
echo "Building C overlay..."
make -C "$INSTALL_DIR/overlay" clean 2>/dev/null || true
make -C "$INSTALL_DIR/overlay"

# --- Python venv ---
echo "Setting up Python environment..."
python3 -m venv "$INSTALL_DIR/.venv"
"$INSTALL_DIR/.venv/bin/pip" install -q -r "$INSTALL_DIR/requirements.txt"

# --- Install Claude Code hooks ---
echo "Installing Claude Code hooks..."
PYTHONPATH="$INSTALL_DIR" "$INSTALL_DIR/.venv/bin/python" -c "
from backend.hook_installer import HookInstaller
HookInstaller().install()
print('  Hooks registered in ~/.claude/settings.json')
"

# --- Launcher script in PATH ---
mkdir -p "$BIN_DIR"
cat > "$BIN_DIR/masko-desktop" << LAUNCHER
#!/usr/bin/env bash
cd "$INSTALL_DIR"
exec "$INSTALL_DIR/.venv/bin/python" -m backend "\$@"
LAUNCHER
chmod +x "$BIN_DIR/masko-desktop"

# --- Systemd user service ---
echo "Installing systemd service..."
mkdir -p "${HOME}/.config/systemd/user/"
cat > "${HOME}/.config/systemd/user/masko-desktop.service" << SERVICE
[Unit]
Description=Masko Desktop — Claude Code companion
After=graphical-session.target

[Service]
Type=simple
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/.venv/bin/python -m backend
Restart=on-failure
RestartSec=5
Environment=DISPLAY=:0

[Install]
WantedBy=default.target
SERVICE
systemctl --user daemon-reload
systemctl --user enable masko-desktop.service

# --- Install icon ---
echo "Installing icon..."
for size in 48 64 128 256; do
    ICON_DIR="${HOME}/.local/share/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$ICON_DIR"
    cp "$INSTALL_DIR/resources/masko-icon-${size}.png" "$ICON_DIR/masko-desktop.png"
done
gtk-update-icon-cache "${HOME}/.local/share/icons/hicolor/" 2>/dev/null || true

# --- Desktop application entry ---
echo "Installing desktop entry..."
mkdir -p "${HOME}/.local/share/applications/"
cat > "${HOME}/.local/share/applications/masko-desktop.desktop" << DESKTOP
[Desktop Entry]
Type=Application
Name=Masko Desktop
GenericName=Claude Code Companion
Comment=Animated mascot companion for Claude Code
Exec=${BIN_DIR}/masko-desktop
Icon=masko-desktop
Terminal=false
Categories=Development;Utility;
Keywords=masko;claude;code;mascot;companion;
StartupNotify=false
DESKTOP
update-desktop-database "${HOME}/.local/share/applications/" 2>/dev/null || true

echo ""
echo "=== Installation complete ==="
echo ""
echo "  Installed to:  $INSTALL_DIR"
echo "  Launcher:      $BIN_DIR/masko-desktop"
echo ""
echo "  Start now:          masko-desktop"
echo "  Or via systemd:     systemctl --user start masko-desktop"
echo "  Or search 'masko' in your app launcher"
echo "  Dashboard:          http://localhost:49153"
echo ""
echo "  To uninstall:     $INSTALL_DIR/uninstall.sh"

# --- Install uninstall script ---
cat > "$INSTALL_DIR/uninstall.sh" << 'UNINSTALL'
#!/usr/bin/env bash
set -euo pipefail

INSTALL_DIR="${HOME}/.local/share/masko-desktop"
BIN_DIR="${HOME}/.local/bin"

echo "=== Uninstalling Masko Desktop ==="

# Stop and disable service
systemctl --user disable --now masko-desktop.service 2>/dev/null || true
rm -f "${HOME}/.config/systemd/user/masko-desktop.service"
systemctl --user daemon-reload 2>/dev/null || true

# Remove desktop entry
rm -f "${HOME}/.local/share/applications/masko-desktop.desktop"
update-desktop-database "${HOME}/.local/share/applications/" 2>/dev/null || true

# Remove icons
for size in 48 64 128 256; do
    rm -f "${HOME}/.local/share/icons/hicolor/${size}x${size}/apps/masko-desktop.png"
done
gtk-update-icon-cache "${HOME}/.local/share/icons/hicolor/" 2>/dev/null || true

# Remove launcher
rm -f "$BIN_DIR/masko-desktop"

# Remove Claude Code hooks
if command -v python3 &>/dev/null && [ -d "$INSTALL_DIR" ]; then
    PYTHONPATH="$INSTALL_DIR" "$INSTALL_DIR/.venv/bin/python" -c "
from backend.hook_installer import HookInstaller
HookInstaller().uninstall()
print('  Hooks removed from ~/.claude/settings.json')
" 2>/dev/null || true
fi

# Remove install directory (do this last since uninstall.sh lives here)
rm -rf "$INSTALL_DIR"

echo ""
echo "Masko Desktop uninstalled."
echo "Video cache at ~/.cache/masko-desktop/ was preserved."
echo "To remove it: rm -rf ~/.cache/masko-desktop"
UNINSTALL
chmod +x "$INSTALL_DIR/uninstall.sh"
