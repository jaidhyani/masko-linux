#!/usr/bin/env bash
# E2E Smoke Test for Masko Desktop Linux
# Runs the full stack on Xvfb and validates behavior with screenshots
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

DISPLAY_NUM=:42
SCREENSHOT_DIR="$PROJECT_DIR/tests/e2e-screenshots"
REPORT_FILE="$PROJECT_DIR/tests/e2e-report.md"
BACKEND_PORT=49152
BACKEND_PID=""
XVFB_PID=""
PASS_COUNT=0
FAIL_COUNT=0
STEP_FILE="/tmp/masko-e2e-step-counter"
echo "0" > "$STEP_FILE"

mkdir -p "$SCREENSHOT_DIR"

# Kill leftover processes from previous runs
pkill -f "Xvfb $DISPLAY_NUM" 2>/dev/null || true
# Kill any backend listening on our port
fuser -k "$BACKEND_PORT/tcp" 2>/dev/null || true
sleep 1

cleanup() {
    [ -n "$BACKEND_PID" ] && kill "$BACKEND_PID" 2>/dev/null || true
    [ -n "$XVFB_PID" ] && kill "$XVFB_PID" 2>/dev/null || true
    sleep 1
    kill $(jobs -p) 2>/dev/null || true
}
trap cleanup EXIT

screenshot() {
    local name="$1"
    local step=$(cat "$STEP_FILE")
    step=$((step + 1))
    echo "$step" > "$STEP_FILE"
    local filename="$(printf '%02d' $step)-${name}.png"
    DISPLAY=$DISPLAY_NUM import -window root "$SCREENSHOT_DIR/$filename" 2>/dev/null || true
    echo "$filename"
}

pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "  ✓ PASS: $1"
}

fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo "  ✗ FAIL: $1"
}

check() {
    local description="$1"
    local condition="$2"
    if eval "$condition"; then
        pass "$description"
    else
        fail "$description"
    fi
}

append_report() {
    echo "$1" >> "$REPORT_FILE"
}

append_screenshot() {
    local filename="$1"
    local caption="$2"
    append_report ""
    append_report "![${caption}](e2e-screenshots/${filename})"
    append_report "*${caption}*"
    append_report ""
}

# Initialize report
cat > "$REPORT_FILE" << EOF
# Masko Desktop Linux — E2E Smoke Test Report

**Generated:** $(date '+%Y-%m-%d %H:%M:%S')

This report validates the full masko-linux stack: Python backend + C overlay, running on an Xvfb virtual display.

---

EOF

echo "=== Masko Desktop Linux — E2E Smoke Test ==="
echo ""

# ─── Step 1: Preflight checks ───
echo "Step 1: Preflight checks..."

check "Overlay binary exists" "[ -x '$PROJECT_DIR/overlay/masko-overlay' ]"
check "Python venv exists" "[ -x '$PROJECT_DIR/.venv/bin/python' ]"
check "Mascot config exists" "[ -f '$PROJECT_DIR/resources/mascots/masko.json' ]"

append_report "## 1. Preflight Checks"
append_report ""
append_report "- Overlay binary: \`overlay/masko-overlay\` $([ -x "$PROJECT_DIR/overlay/masko-overlay" ] && echo '✅' || echo '❌')"
append_report "- Python venv: \`.venv/bin/python\` $([ -x "$PROJECT_DIR/.venv/bin/python" ] && echo '✅' || echo '❌')"
append_report "- Mascot config: \`resources/mascots/masko.json\` $([ -f "$PROJECT_DIR/resources/mascots/masko.json" ] && echo '✅' || echo '❌')"
append_report ""

# ─── Step 2: Start Xvfb ───
echo "Step 2: Starting Xvfb..."
Xvfb $DISPLAY_NUM -screen 0 1024x768x24+32 -ac 2>/dev/null &
XVFB_PID=$!
sleep 1
export DISPLAY=$DISPLAY_NUM

check "Xvfb is running" "kill -0 $XVFB_PID 2>/dev/null"

append_report "## 2. Xvfb Display"
append_report ""
append_report "- Display: \`$DISPLAY_NUM\` (1024x768x24)"
append_report "- PID: \`$XVFB_PID\`"
append_report ""

SS=$(screenshot "xvfb-blank")
append_screenshot "$SS" "Step 2: Blank Xvfb display ready"

# ─── Step 3: Start Backend ───
echo "Step 3: Starting backend..."

E2E_CONFIG="$PROJECT_DIR/tests/e2e-config.json"
cat > "$E2E_CONFIG" << EOF
{
    "selected_mascot": "masko",
    "overlay": {"x": 400, "y": 250, "width": 200, "height": 200},
    "server": {"hook_port": $BACKEND_PORT, "dashboard_port": 49153, "permission_timeout": 30},
    "hotkeys": {"modifier": "Super", "toggle": "m", "approve": "Return", "deny": "Escape"},
    "notifications": {"system_notify": false}
}
EOF

cd "$PROJECT_DIR"
.venv/bin/python -m backend --config "$E2E_CONFIG" 2>/tmp/masko-e2e-backend.log &
BACKEND_PID=$!
sleep 3

check "Backend process is running" "kill -0 $BACKEND_PID 2>/dev/null"

HEALTH_STATUS=$(curl -sf -o /dev/null -w "%{http_code}" "http://localhost:$BACKEND_PORT/health" 2>/dev/null || echo "000")
check "Health endpoint returns 200" "[ '$HEALTH_STATUS' = '200' ]"

append_report "## 3. Backend Started"
append_report ""
append_report "- PID: \`$BACKEND_PID\`"
append_report "- Port: \`$BACKEND_PORT\`"
append_report "- Health check: HTTP \`$HEALTH_STATUS\`"
append_report ""

SS=$(screenshot "backend-started")
append_screenshot "$SS" "Step 3: Backend started, overlay window should appear"

# ─── Step 4: Verify overlay window ───
echo "Step 4: Checking overlay window..."
sleep 2

WID=$(DISPLAY=$DISPLAY_NUM xdotool search --name "masko-overlay" 2>/dev/null | head -1 || echo "")
check "Overlay window exists" "[ -n '$WID' ]"

if [ -n "$WID" ]; then
    OVERRIDE=$(DISPLAY=$DISPLAY_NUM xwininfo -id "$WID" 2>/dev/null | grep -i "override" | grep -i "yes" || echo "")
    check "override_redirect is set" "[ -n '$OVERRIDE' ]"

    append_report "## 4. Overlay Window"
    append_report ""
    append_report "- Window ID: \`$WID\`"
    append_report "- override_redirect: $([ -n "$OVERRIDE" ] && echo 'Yes ✅' || echo 'No ❌')"
    append_report ""
else
    append_report "## 4. Overlay Window"
    append_report ""
    append_report "- **No overlay window found.** The overlay may not have started or uses a different window name."
    append_report ""
fi

SS=$(screenshot "overlay-window")
append_screenshot "$SS" "Step 4: Overlay window visible"

# ─── Step 5: Send SessionStart event ───
echo "Step 5: Sending SessionStart event..."
HOOK_RESP=$(curl -sf -w "\n%{http_code}" -X POST "http://localhost:$BACKEND_PORT/hook" \
    -H "Content-Type: application/json" \
    -d '{"hook_event_name":"SessionStart","session_id":"e2e-test-session","cwd":"/home/user/my-project","model":"opus"}' \
    2>/dev/null || echo -e "\n000")
HOOK_STATUS=$(echo "$HOOK_RESP" | tail -1)
check "SessionStart accepted (HTTP 200)" "[ '$HOOK_STATUS' = '200' ]"

sleep 2

append_report "## 5. SessionStart Event"
append_report ""
append_report "- Session: \`e2e-test-session\`"
append_report "- HTTP status: \`$HOOK_STATUS\`"
append_report "- Expected: Mascot transitions to Working state"
append_report ""

SS=$(screenshot "session-start")
append_screenshot "$SS" "Step 5: After SessionStart — mascot in Working state"

# ─── Step 6: Send PermissionRequest event ───
echo "Step 6: Sending PermissionRequest event..."

curl -sf --max-time 15 -X POST "http://localhost:$BACKEND_PORT/hook" \
    -H "Content-Type: application/json" \
    -d '{"hook_event_name":"PermissionRequest","session_id":"e2e-test-session","tool_name":"Bash","tool_input":{"command":"rm -rf /tmp/test"},"tool_use_id":"e2e-perm-1"}' \
    > /tmp/masko-e2e-perm-response.txt 2>/dev/null &
PERM_CURL_PID=$!

sleep 2

append_report "## 6. Permission Request"
append_report ""
append_report "- Tool: \`Bash\`"
append_report "- Command: \`rm -rf /tmp/test\`"
append_report "- The hook call blocks until the user responds (approve/deny) or timeout."
append_report ""

SS=$(screenshot "permission-prompt")
append_screenshot "$SS" "Step 6: Permission prompt should be visible"

# ─── Step 7: Click Approve button ───
echo "Step 7: Clicking Approve button..."

# Click the approve button directly on the overlay window using window-local
# coordinates. override_redirect windows may not receive root-level clicks.
# Button layout: approve is at window-local ~(55, 155), center of left button.
OVERLAY_WID=$(DISPLAY=$DISPLAY_NUM xdotool search --name "masko-overlay" 2>/dev/null | head -1 || echo "")
if [ -n "$OVERLAY_WID" ]; then
    DISPLAY=$DISPLAY_NUM xdotool mousemove --window "$OVERLAY_WID" 55 155 click --window "$OVERLAY_WID" 1 2>/dev/null || true
fi
sleep 2

# Wait up to 10 more seconds for curl to complete
for i in $(seq 1 10); do
    if ! kill -0 "$PERM_CURL_PID" 2>/dev/null; then
        break
    fi
    sleep 1
done
kill "$PERM_CURL_PID" 2>/dev/null || true
wait "$PERM_CURL_PID" 2>/dev/null || true
PERM_RESPONSE=$(cat /tmp/masko-e2e-perm-response.txt 2>/dev/null || echo "")

HAS_ACTION=$(echo "$PERM_RESPONSE" | grep -o '"action"' || echo "")
check "Permission response received" "[ -n '$HAS_ACTION' ]"

append_report "## 7. Permission Response (Approve Click)"
append_report ""
append_report "- Clicked approve button on overlay window"
append_report "- Response: \`$PERM_RESPONSE\`"
append_report ""

SS=$(screenshot "after-approve")
append_screenshot "$SS" "Step 7: After clicking Approve — prompt dismissed"

# ─── Step 8: Send Notification event ───
echo "Step 8: Sending Notification event..."
NOTIF_RESP=$(curl -sf -w "\n%{http_code}" -X POST "http://localhost:$BACKEND_PORT/hook" \
    -H "Content-Type: application/json" \
    -d '{"hook_event_name":"Notification","session_id":"e2e-test-session","title":"Build Complete","message":"All tests passed!"}' \
    2>/dev/null || echo -e "\n000")
NOTIF_STATUS=$(echo "$NOTIF_RESP" | tail -1)
check "Notification accepted (HTTP 200)" "[ '$NOTIF_STATUS' = '200' ]"

sleep 1

append_report "## 8. Notification Event"
append_report ""
append_report "- Title: Build Complete"
append_report "- HTTP status: \`$NOTIF_STATUS\`"
append_report ""

SS=$(screenshot "notification")
append_screenshot "$SS" "Step 8: Notification sent"

# ─── Step 9: Send SessionEnd event ───
echo "Step 9: Sending SessionEnd event..."
END_RESP=$(curl -sf -w "\n%{http_code}" -X POST "http://localhost:$BACKEND_PORT/hook" \
    -H "Content-Type: application/json" \
    -d '{"hook_event_name":"SessionEnd","session_id":"e2e-test-session","reason":"completed"}' \
    2>/dev/null || echo -e "\n000")
END_STATUS=$(echo "$END_RESP" | tail -1)
check "SessionEnd accepted (HTTP 200)" "[ '$END_STATUS' = '200' ]"

sleep 2

append_report "## 9. SessionEnd Event"
append_report ""
append_report "- HTTP status: \`$END_STATUS\`"
append_report "- Expected: Mascot returns to Idle state"
append_report ""

SS=$(screenshot "session-end")
append_screenshot "$SS" "Step 9: After SessionEnd — mascot Idle"

# ─── Step 10: Rapid events stress test ───
echo "Step 10: Sending rapid events..."
RAPID_PIDS=""
for i in $(seq 1 5); do
    curl -sf --max-time 5 -X POST "http://localhost:$BACKEND_PORT/hook" \
        -H "Content-Type: application/json" \
        -d "{\"hook_event_name\":\"PreToolUse\",\"session_id\":\"e2e-rapid-$i\",\"tool_name\":\"Read\"}" \
        > /dev/null 2>&1 &
    RAPID_PIDS="$RAPID_PIDS $!"
done
for pid in $RAPID_PIDS; do
    wait "$pid" 2>/dev/null || true
done
sleep 1

check "Backend still healthy after rapid events" \
    "curl -sf -o /dev/null http://localhost:$BACKEND_PORT/health"

append_report "## 10. Stress Test — Rapid Events"
append_report ""
append_report "Sent 5 concurrent \`PreToolUse\` events. Backend still responsive."
append_report ""

SS=$(screenshot "rapid-events")
append_screenshot "$SS" "Step 10: After rapid events — system still responsive"

# ─── Step 11: Overlay still alive ───
echo "Step 11: Final health checks..."

check "Backend still running" "kill -0 $BACKEND_PID 2>/dev/null"

FINAL_WID=$(DISPLAY=$DISPLAY_NUM xdotool search --name "masko-overlay" 2>/dev/null | head -1 || echo "")
check "Overlay still alive" "[ -n '$FINAL_WID' ]"

append_report "## 11. Final Health Checks"
append_report ""
append_report "- Backend alive: $(kill -0 $BACKEND_PID 2>/dev/null && echo 'Yes ✅' || echo 'No ❌')"
append_report "- Overlay alive: $([ -n "$FINAL_WID" ] && echo 'Yes ✅' || echo 'No ❌')"
append_report ""

SS=$(screenshot "final-state")
append_screenshot "$SS" "Step 11: Final state of the display"

# ─── Summary ───
echo ""
echo "=== Results ==="
echo "  Passed: $PASS_COUNT"
echo "  Failed: $FAIL_COUNT"
echo "  Total:  $((PASS_COUNT + FAIL_COUNT))"
echo ""
echo "Screenshots: $SCREENSHOT_DIR/"
echo "Report:      $REPORT_FILE"

append_report "---"
append_report ""
append_report "## Summary"
append_report ""
append_report "| Metric | Value |"
append_report "|--------|-------|"
append_report "| Tests Passed | $PASS_COUNT |"
append_report "| Tests Failed | $FAIL_COUNT |"
append_report "| Total Checks | $((PASS_COUNT + FAIL_COUNT)) |"
append_report "| Screenshots | $(ls "$SCREENSHOT_DIR"/*.png 2>/dev/null | wc -l) |"
append_report ""
if [ $FAIL_COUNT -eq 0 ]; then
    append_report "**All checks passed.** ✅"
else
    append_report "**$FAIL_COUNT check(s) failed.** ❌"
fi
append_report ""
append_report "---"
append_report "*Generated by tests/test_e2e_smoke.sh*"

exit $( [ $FAIL_COUNT -eq 0 ] && echo 0 || echo 1 )
