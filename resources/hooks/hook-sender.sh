#!/usr/bin/env bash
# Masko Desktop hook sender — forwards Claude Code events to the local backend
CONFIG_PATH="${HOME}/.config/masko-desktop/config.json"
PORT=$(python3 -c "import json,sys; print(json.load(open('$CONFIG_PATH')).get('server',{}).get('hook_port',49152))" 2>/dev/null || echo 49152)
BASE="http://localhost:${PORT}"

if ! curl -sf --max-time 2 "${BASE}/health" > /dev/null 2>&1; then
    exit 0
fi

EVENT_NAME="${CLAUDE_HOOK_EVENT_NAME:-}"
EVENT_JSON=$(cat)

TERMINAL_PID=""
SHELL_PID="$$"
PPID_WALK=$$
for _ in 1 2 3 4 5 6 7 8 9 10; do
    PPID_WALK=$(ps -o ppid= -p "$PPID_WALK" 2>/dev/null | tr -d ' ')
    [ -z "$PPID_WALK" ] && break
    PROC_NAME=$(ps -o comm= -p "$PPID_WALK" 2>/dev/null)
    case "$PROC_NAME" in
        *code*|*cursor*|*iterm*|*Terminal*|*ghostty*|*kitty*|*wezterm*|*alacritty*|*warp*|*zed*|*gnome-terminal*|*konsole*|*xterm*|*tilix*|*foot*)
            TERMINAL_PID="$PPID_WALK"
            break
            ;;
    esac
done

ENRICHED=$(echo "$EVENT_JSON" | python3 -c "
import json,sys
d=json.load(sys.stdin)
tp='${TERMINAL_PID}'
sp='${SHELL_PID}'
if tp: d['terminal_pid']=int(tp)
if sp: d['shell_pid']=int(sp)
print(json.dumps(d))
" 2>/dev/null || echo "$EVENT_JSON")

if [ "$EVENT_NAME" = "PermissionRequest" ]; then
    curl -sf --max-time 120 -X POST "${BASE}/hook" \
        -H "Content-Type: application/json" \
        -d "$ENRICHED"
else
    curl -sf --max-time 5 -X POST "${BASE}/hook" \
        -H "Content-Type: application/json" \
        -d "$ENRICHED" &
fi
