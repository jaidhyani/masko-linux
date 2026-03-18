import asyncio
import json
import logging
import os
import shutil
import signal
import subprocess
import sys
from pathlib import Path

from aiohttp import web

from backend.config import Config
from backend.event_processor import EventProcessor
from backend.ipc import IPCServer
from backend.models import ClaudeEvent, HookEventType
from backend.server import create_dashboard_app
from backend.state_machine import ConditionValue, StateMachine
from backend.stores.event import EventStore
from backend.stores.notification import NotificationStore
from backend.stores.permission import PendingPermission, PendingPermissionStore
from backend.stores.session import SessionStore
from backend.stores.session_finished import SessionFinishedStore
from backend.stores.session_switcher import SessionSwitcherStore
from backend.terminal_focus import focus_terminal
from backend.video_cache import VideoCache

logger = logging.getLogger("masko")

SOCK_PATH = "/tmp/masko-overlay.sock"
OVERLAY_BIN = Path(__file__).parent.parent / "overlay" / "masko-overlay"
MAX_RESPAWN_RETRIES = 5
RESPAWN_DELAY = 1.0


def create_orchestrated_app(config, state_machine, event_processor, ipc, orchestrator):
    """Build the aiohttp app with all stores and the permission-aware hook handler."""
    app = web.Application()
    app["config"] = config
    app["event_store"] = event_processor.event_store
    app["session_store"] = event_processor.session_store
    app["notification_store"] = event_processor.notification_store
    app["permission_store"] = PendingPermissionStore()
    app["state_machine"] = state_machine
    app["event_processor"] = event_processor
    app["ipc"] = ipc

    async def handle_health(request):
        return web.Response(text="ok")

    async def handle_hook(request):
        try:
            data = await request.json()
        except json.JSONDecodeError:
            return web.Response(status=400, text="invalid json")

        event = ClaudeEvent.from_dict(data)
        event_processor.process(event)

        if event.event_type == HookEventType.PERMISSION_REQUEST:
            return await orchestrator.handle_permission_request(request.app, event)

        if event.event_type == HookEventType.SESSION_END:
            await orchestrator.handle_session_end(event)

        return web.Response(text="ok")

    async def handle_input(request):
        try:
            data = await request.json()
        except json.JSONDecodeError:
            return web.Response(status=400, text="invalid json")
        sm = request.app.get("state_machine")
        if sm:
            name = data.get("name", "")
            value = data.get("value")
            sm.set_input(name, ConditionValue(value))
        return web.Response(text="ok")

    app.router.add_get("/health", handle_health)
    app.router.add_post("/hook", handle_hook)
    app.router.add_post("/input", handle_input)
    return app


class Orchestrator:
    def __init__(self, config: Config):
        self.config = config
        self.video_cache = VideoCache()
        self.session_switcher = SessionSwitcherStore()
        self.session_finished = SessionFinishedStore()

        mascot_config = self._load_mascot_config()
        self.state_machine = StateMachine(mascot_config)

        event_store = EventStore()
        session_store = SessionStore()
        notification_store = NotificationStore()

        self.event_processor = EventProcessor(
            event_store=event_store,
            session_store=session_store,
            notification_store=notification_store,
            state_machine=self.state_machine,
        )

        self.ipc = IPCServer(SOCK_PATH, self._handle_ipc_message)

        self.app = create_orchestrated_app(
            config, self.state_machine, self.event_processor, self.ipc, self,
        )

        self.overlay_ready = False
        self.overlay_process = None
        self._respawn_count = 0
        self._shutdown = False

        self._wire_state_machine()

    def _load_mascot_config(self) -> dict:
        mascot_name = self.config.selected_mascot
        mascot_path = Path(__file__).parent.parent / "resources" / "mascots" / f"{mascot_name}.json"
        if not mascot_path.exists():
            logger.error("Mascot config not found: %s", mascot_path)
            sys.exit(1)
        return json.loads(mascot_path.read_text())

    def _wire_state_machine(self):
        def on_transition(edge, video_url):
            logger.info("State transition → %s, video: %s", edge.get("target", "?"), video_url)
            resolved = self.video_cache.resolve(video_url)
            logger.info("Resolved video: %s", resolved)
            asyncio.ensure_future(self.ipc.send({
                "cmd": "play", "video": resolved, "loop": False,
            }))

        def on_loop_start(video_url):
            logger.info("Loop start, video: %s", video_url)
            resolved = self.video_cache.resolve(video_url)
            logger.info("Resolved loop video: %s", resolved)
            asyncio.ensure_future(self.ipc.send({
                "cmd": "play", "video": resolved, "loop": True,
            }))

        self.state_machine.on_transition = on_transition
        self.state_machine.on_loop_start = on_loop_start

    async def handle_permission_request(self, app, event):
        loop = asyncio.get_event_loop()
        future = loop.create_future()

        permission = PendingPermission(
            tool_use_id=event.tool_use_id or event.id,
            tool_name=event.tool_name or "",
            tool_input=event.tool_input or {},
            session_id=event.session_id or "",
            response_future=future,
        )
        app["permission_store"].add(permission.tool_use_id, permission)

        ipc = app.get("ipc")
        if ipc:
            await ipc.send({
                "cmd": "permission",
                "tool": permission.tool_name,
                "input": json.dumps(permission.tool_input) if isinstance(permission.tool_input, dict) else str(permission.tool_input),
                "id": permission.tool_use_id,
            })

        try:
            action = await asyncio.wait_for(future, timeout=self.config.permission_timeout)
        except asyncio.TimeoutError:
            app["permission_store"].resolve(permission.tool_use_id, "deny")
            action = "deny"

        return web.json_response({"action": action})

    async def _handle_ipc_message(self, msg):
        msg_type = msg.get("type") or msg.get("event")

        if msg_type == "ready":
            self.overlay_ready = True
            self._respawn_count = 0
            logger.info("Overlay ready")
            self.state_machine.start()

        elif msg_type == "permission_response":
            tool_use_id = msg.get("id") or msg.get("tool_use_id")
            action = msg.get("action", "deny")
            if tool_use_id:
                self.app["permission_store"].resolve(tool_use_id, action)

        elif msg_type == "hotkey":
            await self._handle_hotkey(msg.get("key"))

        elif msg_type == "error":
            logger.error("Overlay error: %s", msg.get("message", "unknown"))

        elif msg_type == "menu":
            await self._handle_menu_action(msg.get("action", ""))

        elif msg_type == "position":
            x, y = msg.get("x", 100), msg.get("y", 100)
            self.config._data["overlay"]["x"] = x
            self.config._data["overlay"]["y"] = y
            self.config.save()

        elif msg_type == "video_ended":
            self.state_machine.handle_video_ended()

        elif msg_type == "loop_completed":
            self.state_machine.handle_loop_completed()

    async def handle_session_end(self, event):
        session = self.event_processor.session_store.get(event.session_id)
        if not session:
            return
        toast = self.session_finished.make_toast_data(session)
        await self.ipc.send({"cmd": "toast", **toast})

    async def _handle_menu_action(self, action):
        dashboard_url = f"http://localhost:{self.config.dashboard_port}"

        if action == "open_dashboard":
            xdg = shutil.which("xdg-open")
            if xdg:
                subprocess.Popen([xdg, dashboard_url],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        elif action == "open_settings":
            xdg = shutil.which("xdg-open")
            if xdg:
                subprocess.Popen([xdg, f"{dashboard_url}#settings"],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        elif action == "reset_position":
            default_x, default_y = 100, 100
            self.config._data["overlay"]["x"] = default_x
            self.config._data["overlay"]["y"] = default_y
            self.config.save()
            await self.ipc.send({"cmd": "move", "x": default_x, "y": default_y})

    async def _handle_hotkey(self, key):
        if key == "toggle":
            pass
        elif key == "session_switcher":
            await self._toggle_session_switcher()
        elif key == "session_next":
            self.session_switcher.nav("next")
            await self._send_session_switcher_nav()
        elif key == "session_prev":
            self.session_switcher.nav("prev")
            await self._send_session_switcher_nav()
        elif key == "session_confirm":
            await self._confirm_session_switch()

    async def _toggle_session_switcher(self):
        if self.session_switcher.is_open:
            self.session_switcher.close()
            await self.ipc.send({"cmd": "session_switcher", "action": "close"})
            return

        sessions = self.event_processor.session_store.active_sessions()
        if not sessions:
            return
        self.session_switcher.open([
            {"id": s.id, "cwd": s.cwd or "", "model": s.model or ""}
            for s in sessions
        ])
        await self.ipc.send({
            "cmd": "session_switcher",
            "action": "open",
            "sessions": self.session_switcher.sessions,
            "selected": self.session_switcher.selected,
        })

    async def _send_session_switcher_nav(self):
        if not self.session_switcher.is_open:
            return
        await self.ipc.send({
            "cmd": "session_switcher",
            "action": "nav",
            "selected": self.session_switcher.selected,
        })

    async def _confirm_session_switch(self):
        if not self.session_switcher.is_open or not self.session_switcher.sessions:
            return
        selected = self.session_switcher.sessions[self.session_switcher.selected]
        self.session_switcher.close()
        await self.ipc.send({"cmd": "session_switcher", "action": "close"})

        active = self.event_processor.session_store.active_sessions()
        target = next((s for s in active if s.id == selected["id"]), None)
        if target and target.terminal_pid:
            focus_terminal(target.terminal_pid)

    async def _spawn_overlay(self):
        if not OVERLAY_BIN.exists():
            logger.warning("Overlay binary not found: %s", OVERLAY_BIN)
            return

        self.overlay_process = await asyncio.create_subprocess_exec(
            str(OVERLAY_BIN), "--sock", SOCK_PATH,
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.PIPE,
            start_new_session=True,
        )
        logger.info("Overlay spawned (pid=%d)", self.overlay_process.pid)

    async def _monitor_overlay(self):
        """Watch overlay process and respawn on unexpected exit."""
        while not self._shutdown:
            if self.overlay_process is None:
                await asyncio.sleep(1)
                continue

            await self.overlay_process.wait()
            if self._shutdown:
                break

            logger.warning("Overlay exited (code=%s)", self.overlay_process.returncode)
            self.overlay_ready = False
            self.overlay_process = None

            self.app["permission_store"].deny_all()

            if self._respawn_count >= MAX_RESPAWN_RETRIES:
                logger.error("Overlay respawn limit reached (%d)", MAX_RESPAWN_RETRIES)
                break

            self._respawn_count += 1
            logger.info("Respawning overlay (attempt %d/%d)", self._respawn_count, MAX_RESPAWN_RETRIES)
            await asyncio.sleep(RESPAWN_DELAY)
            await self._spawn_overlay()

    async def start(self):
        await self.ipc.start()
        await self._spawn_overlay()

        self.state_machine.start()

        monitor_task = asyncio.create_task(self._monitor_overlay())
        self.app["_monitor_task"] = monitor_task

    async def shutdown(self):
        self._shutdown = True

        monitor = self.app.get("_monitor_task")
        if monitor and not monitor.done():
            monitor.cancel()

        if self.overlay_process and self.overlay_process.returncode is None:
            pid = self.overlay_process.pid
            try:
                os.killpg(pid, signal.SIGTERM)
            except OSError:
                self.overlay_process.terminate()
            try:
                await asyncio.wait_for(self.overlay_process.wait(), timeout=2)
            except asyncio.TimeoutError:
                logger.warning("Overlay did not exit after SIGTERM, sending SIGKILL")
                try:
                    os.killpg(pid, signal.SIGKILL)
                except OSError:
                    self.overlay_process.kill()
                await self.overlay_process.wait()

        self.app["permission_store"].deny_all()
        await self.ipc.stop()


def parse_args():
    import argparse
    parser = argparse.ArgumentParser(description="Masko Desktop backend")
    parser.add_argument("--config", help="Path to config JSON file")
    return parser.parse_args()


async def main():
    args = parse_args()

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
        stream=sys.stderr,
    )

    config = Config(path=args.config)
    orchestrator = Orchestrator(config)

    loop = asyncio.get_event_loop()
    shutdown_event = asyncio.Event()

    def signal_handler():
        shutdown_event.set()

    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, signal_handler)

    try:
        await orchestrator.start()
    except Exception:
        logger.exception("Failed to start orchestrator")
        sys.exit(1)

    runner = web.AppRunner(orchestrator.app)
    await runner.setup()
    try:
        site = web.TCPSite(runner, "127.0.0.1", config.hook_port)
        await site.start()
    except OSError as e:
        if "Address already in use" in str(e) or e.errno == 98:
            logger.error("Port %d already in use. Is another instance running?", config.hook_port)
            await orchestrator.shutdown()
            await runner.cleanup()
            sys.exit(1)
        raise

    logger.info("Masko hook server listening on 127.0.0.1:%d", config.hook_port)

    dashboard_app = create_dashboard_app(orchestrator.app)
    dashboard_runner = web.AppRunner(dashboard_app)
    await dashboard_runner.setup()
    try:
        dashboard_site = web.TCPSite(dashboard_runner, "127.0.0.1", config.dashboard_port)
        await dashboard_site.start()
    except OSError as e:
        if "Address already in use" in str(e) or e.errno == 98:
            logger.warning("Dashboard port %d already in use, skipping dashboard", config.dashboard_port)
            dashboard_runner = None
        else:
            raise

    if dashboard_runner:
        logger.info("Masko dashboard listening on 127.0.0.1:%d", config.dashboard_port)

    await shutdown_event.wait()

    logger.info("Shutting down...")
    await orchestrator.shutdown()
    if dashboard_runner:
        await dashboard_runner.cleanup()
    await runner.cleanup()


if __name__ == "__main__":
    asyncio.run(main())
