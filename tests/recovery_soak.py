#!/usr/bin/env python3
"""Daemon + desktop scene worker recovery soak (issue #143).

Starts the emulator daemon headless and the desktop scene worker as separate
processes, then repeatedly breaks the link between them while a demand-driven
/live_frame viewer keeps pulling frames:

  idle_viewers      connected viewers cause no capture work; one `next` costs
                    exactly one frame; duplicates and concurrent viewers coalesce
  scene_lab_stale   a replaced Scene Lab session is rejected with 409
  worker_handoff    scene switches with the worker attached start desktop
                    sessions and deliver fresh desktop frames
  worker_kill       SIGKILL the worker at staggered moments around a scene
                    switch, then restart it (the daemon must fall back to local
                    rendering immediately and hand off again once it is back)
  worker_stall      SIGSTOP/SIGCONT the worker while scenes switch (delivery
                    interrupted without the socket closing)
  desktop_flap      the desktop's control socket drops and reconnects, which
                    parks the worker without killing it
  hard_cut          /skip (a transition-less switch) never leaves a desktop
                    session running for a scene that is no longer shown
  stale_frames      forged UDP frames for superseded/unknown sessions, replayed
                    sequences, wrong sizes and malformed packets are rejected
  viewer_churn      viewers that vanish mid-request, send junk, or connect and
                    drop while scenes switch
  final_idle        after everything the daemon performs zero capture work

Each fault must be followed by usable frames (a well-formed LMF1 frame with the
expected size and some lit pixels) within a bound, and /diagnostics counters
must show that no frame is captured without outstanding demand.

The driver is dependency-free (stdlib only, its own minimal WebSocket client).
Both binaries come from different build trees (ENABLE_EMULATOR and
ENABLE_DESKTOP are mutually exclusive), so it takes both paths and exits with
status 77 (ctest SKIP_RETURN_CODE) when one is missing. Every child runs in its
own process group and is killed on exit, on signals and on the global deadline.

  tests/recovery_soak.py --daemon-install emulator_build/install \
      --worker-build desktop_build [--profile quick|full] [--time-scale 3]
"""
import argparse
import atexit
import base64
import ctypes
import fcntl
import json
import os
import random
import re
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

SKIP = 77
PANEL = 128  # --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64
DAEMON_FLAGS = ["--led-chain", "2", "--led-parallel", "2", "--led-rows", "64",
                "--led-cols", "64", "--led-emulator", "--led-emulator-headless"]
SANITIZER_REPORT = re.compile(
    r"(ERROR: (Address|Leak|Thread)Sanitizer|runtime error:|WARNING: ThreadSanitizer|"
    r"SUMMARY: (Address|Thread|UndefinedBehavior)Sanitizer|terminate called|"
    r"Assertion .* failed|_GLIBCXX_ASSERTIONS)")


class SoakFailure(Exception):
    pass


def log(message):
    print(f"[soak {time.strftime('%H:%M:%S')}] {message}", flush=True)


# --------------------------------------------------------------------------
# Process management: every child gets its own process group and dies with us.
# --------------------------------------------------------------------------
CHILDREN = []
_cleaned = False


def _set_pdeathsig():
    try:  # kill the child if the driver itself is SIGKILLed (ctest timeout)
        ctypes.CDLL(None).prctl(1, signal.SIGKILL)
    except Exception:
        pass


class Child:
    def __init__(self, name, argv, env, cwd, log_path):
        self.name = name
        self.log_path = log_path
        self.forced = False  # had to be SIGKILLed after ignoring SIGTERM
        self.log_file = open(log_path, "wb")
        self.proc = subprocess.Popen(argv, env=env, cwd=cwd, stdin=subprocess.DEVNULL,
                                     stdout=self.log_file, stderr=subprocess.STDOUT,
                                     start_new_session=True, preexec_fn=_set_pdeathsig)
        CHILDREN.append(self)

    @property
    def pid(self):
        return self.proc.pid

    def alive(self):
        return self.proc.poll() is None

    def signal(self, sig):
        if self.alive():
            os.killpg(self.proc.pid, sig)

    def stop(self, grace=5.0):
        """SIGTERM, then SIGKILL after `grace`; returns the exit status."""
        if self.alive():
            try:
                os.killpg(self.proc.pid, signal.SIGCONT)
                os.killpg(self.proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                self.proc.wait(grace)
            except subprocess.TimeoutExpired:
                self.forced = True
                self.kill()
        self._reap_group()
        return self.proc.returncode

    def kill(self):
        try:
            os.killpg(self.proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        try:
            self.proc.wait(5)
        except subprocess.TimeoutExpired:
            pass
        self._reap_group()

    def _reap_group(self):
        # Anything the child left in its group (helpers, ffmpeg, ...) goes too.
        try:
            os.killpg(self.proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        if not self.log_file.closed:
            self.log_file.close()

    def log_text(self, tail=None):
        try:
            with open(self.log_path, "rb") as handle:
                data = handle.read().decode("utf-8", "replace")
        except OSError:
            return ""
        return data if tail is None else data[-tail:]


def cleanup():
    global _cleaned
    if _cleaned:
        return
    _cleaned = True
    for child in reversed(CHILDREN):
        try:
            child.kill()
        except Exception:
            pass


atexit.register(cleanup)


def _die_on_signal(signum, _frame):
    raise SystemExit(128 + signum)


for _sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
    signal.signal(_sig, _die_on_signal)


def start_watchdog(seconds):
    """Hard stop: kill every child and exit even if the main thread is stuck."""
    def run():
        time.sleep(seconds)
        sys.stderr.write(f"[soak] global deadline of {seconds:.0f}s exceeded\n")
        cleanup()
        os._exit(4)
    threading.Thread(target=run, daemon=True, name="watchdog").start()


# --------------------------------------------------------------------------
# HTTP and WebSocket helpers
# --------------------------------------------------------------------------
class Api:
    def __init__(self, port):
        self.port = port
        self.base = f"http://127.0.0.1:{port}"

    def request(self, path, body=None, timeout=10, expect_ok=True):
        data = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(self.base + path, data=data,
                                     method="POST" if body is not None else "GET")
        if data is not None:
            req.add_header("Content-Type", "application/json")
        try:
            with urllib.request.urlopen(req, timeout=timeout) as response:
                payload = response.read()
                status = response.status
        except urllib.error.HTTPError as error:
            payload, status = error.read(), error.code
        text = payload.decode("utf-8", "replace")
        try:
            parsed = json.loads(text)
        except ValueError:
            parsed = text
        if expect_ok and not 200 <= status < 300:
            raise SoakFailure(f"{path} -> HTTP {status}: {text[:200]}")
        return status, parsed

    def get(self, path, **kw):
        return self.request(path, **kw)[1]

    def diag(self):
        return self.get("/diagnostics")


class WsClient:
    """Minimal RFC 6455 client: text/binary frames, ping/pong, close."""

    def __init__(self, port, path, timeout=5.0):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.buf = b""
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall((f"GET {path} HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n"
                           "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").encode())
        header = self._read_until(b"\r\n\r\n", time.monotonic() + timeout)
        if b" 101 " not in header.split(b"\r\n", 1)[0]:
            raise SoakFailure(f"WebSocket upgrade refused: {header[:120]!r}")

    def _read_until(self, marker, deadline):
        while marker not in self.buf:
            self._fill(deadline)
        head, _, self.buf = self.buf.partition(marker)
        return head + marker

    def _fill(self, deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("websocket read timed out")
        self.sock.settimeout(remaining)
        try:
            chunk = self.sock.recv(1 << 16)
        except socket.timeout:
            raise TimeoutError("websocket read timed out") from None
        if not chunk:
            raise ConnectionError("websocket closed by peer")
        self.buf += chunk

    def _take(self, count, deadline):
        while len(self.buf) < count:
            self._fill(deadline)
        data, self.buf = self.buf[:count], self.buf[count:]
        return data

    def send(self, opcode, payload=b""):
        mask = os.urandom(4)
        length = len(payload)
        head = bytes([0x80 | opcode])
        if length < 126:
            head += bytes([0x80 | length])
        elif length < 65536:
            head += bytes([0x80 | 126]) + struct.pack(">H", length)
        else:
            head += bytes([0x80 | 127]) + struct.pack(">Q", length)
        masked = bytes(b ^ mask[i & 3] for i, b in enumerate(payload))
        self.sock.sendall(head + mask + masked)

    def send_text(self, text):
        self.send(0x1, text.encode())

    def recv(self, timeout):
        """Next data frame as (opcode, payload); answers pings itself."""
        deadline = time.monotonic() + timeout
        while True:
            first, second = self._take(2, deadline)
            opcode, length = first & 0x0F, second & 0x7F
            if length == 126:
                length = struct.unpack(">H", self._take(2, deadline))[0]
            elif length == 127:
                length = struct.unpack(">Q", self._take(8, deadline))[0]
            payload = self._take(length, deadline)
            if opcode == 0x9:
                self.send(0xA, payload)
            elif opcode == 0x8:
                raise ConnectionError("websocket closed by peer")
            elif opcode in (0x1, 0x2):
                return opcode, payload

    def close(self, abrupt=False):
        try:
            if abrupt:  # RST instead of FIN: the peer sees a vanished viewer
                self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                                     struct.pack("ii", 1, 0))
            else:
                self.send(0x8, struct.pack(">H", 1000))
        except OSError:
            pass
        try:
            self.sock.close()
        except OSError:
            pass


def parse_frame(payload):
    """LMF1: magic, u16 width, u16 height, u32 sequence (LE), RGB bytes."""
    if len(payload) < 12 or payload[:4] != b"LMF1":
        raise SoakFailure(f"not an LMF1 frame ({len(payload)} bytes)")
    width, height, sequence = struct.unpack("<HHI", payload[4:12])
    rgb = payload[12:]
    if len(rgb) != width * height * 3:
        raise SoakFailure(f"LMF1 body is {len(rgb)} bytes for {width}x{height}")
    return width, height, sequence, rgb


def is_lit(rgb):
    """True when the frame has visible content (not a blank panel)."""
    return len(rgb) - rgb.count(0) >= 30


class FakeDesktop:
    """The desktop app's control socket. A scene worker only produces frames
    while the desktop it belongs to (same client id) owns the producer role."""

    def __init__(self, port, client_id):
        self.ws = WsClient(port, f"/desktopWebsocket?client_id={client_id}")
        self.messages = []
        self.closed = threading.Event()
        self.thread = threading.Thread(target=self._read, daemon=True, name="desktop")
        self.thread.start()

    def _read(self):
        try:
            while not self.closed.is_set():
                try:
                    _, payload = self.ws.recv(1.0)
                    self.messages.append(payload[:80])
                except TimeoutError:
                    continue
        except (ConnectionError, OSError):
            pass

    def close(self, abrupt=False):
        self.closed.set()
        self.ws.close(abrupt=abrupt)
        self.thread.join(5)


class ViewerPump:
    """A well-behaved browser: one `next`, wait for the frame, repeat."""

    def __init__(self, port, label="pump", timeout=6.0):
        self.port, self.label, self.timeout = port, label, timeout
        self.lock = threading.Lock()
        self.stop_event = threading.Event()
        self.frames = 0
        self.lit_frames = 0
        self.errors = []
        self.reconnects = 0
        self.last_frame = time.monotonic()
        self.last_lit_frame = time.monotonic()
        self.max_gap = 0.0
        self.last_sequence = 0
        self.thread = threading.Thread(target=self._run, daemon=True, name=label)

    def start(self):
        self.thread.start()
        return self

    def stop(self):
        self.stop_event.set()
        self.thread.join(15)

    def snapshot(self):
        with self.lock:
            return dict(frames=self.frames, lit_frames=self.lit_frames,
                        errors=list(self.errors), reconnects=self.reconnects,
                        max_gap=self.max_gap, last_frame=self.last_frame,
                        last_lit_frame=self.last_lit_frame)

    def reset_gap(self):
        with self.lock:
            self.max_gap = 0.0

    def _run(self):
        client = None
        while not self.stop_event.is_set():
            try:
                if client is None:
                    client = WsClient(self.port, "/live_frame")
                    self.last_sequence = 0
                client.send_text("next")
                opcode, payload = client.recv(self.timeout)
                if opcode != 0x2:
                    raise SoakFailure("live frame arrived as text")
                width, height, sequence, rgb = parse_frame(payload)
                if (width, height) != (PANEL, PANEL):
                    raise SoakFailure(f"live frame is {width}x{height}, expected {PANEL}x{PANEL}")
                if self.last_sequence and not 0 < (sequence - self.last_sequence) & 0xFFFFFFFF < 1 << 31:
                    raise SoakFailure(f"sequence went {self.last_sequence} -> {sequence}")
                self.last_sequence = sequence
                now = time.monotonic()
                lit = is_lit(rgb)
                with self.lock:
                    self.max_gap = max(self.max_gap, now - self.last_frame)
                    self.last_frame = now
                    self.frames += 1
                    if lit:
                        self.lit_frames += 1
                        self.last_lit_frame = now
                time.sleep(0.02)
            except (TimeoutError, ConnectionError, OSError, SoakFailure) as error:
                with self.lock:
                    self.errors.append(f"{type(error).__name__}: {error}")
                if client:
                    client.close(abrupt=True)
                client = None
                with self.lock:
                    self.reconnects += 1
                time.sleep(0.3)
        if client:
            client.close()


# --------------------------------------------------------------------------
# The soak
# --------------------------------------------------------------------------
def free_port():
    for _ in range(200):
        port = random.randint(20000, 40000)
        probes = []
        try:
            for kind in (socket.SOCK_STREAM, socket.SOCK_DGRAM):
                sock = socket.socket(socket.AF_INET, kind)
                probes.append(sock)
                sock.bind(("127.0.0.1", port))
            return port
        except OSError:
            continue
        finally:
            for sock in probes:
                sock.close()
    raise SoakFailure("no free TCP+UDP port found")


class Soak:
    def __init__(self, args, daemon_bin, install_dir, worker_bin, plugin_dir, workdir):
        self.args = args
        self.scale = args.time_scale
        self.daemon_bin, self.install_dir = daemon_bin, install_dir
        self.worker_bin, self.plugin_dir = worker_bin, plugin_dir
        self.workdir = workdir
        self.port = free_port()
        self.api = Api(self.port)
        self.daemon = None
        self.worker = None
        self.worker_starts = 0
        self.pump = None
        self.desktop = None
        self.timings = {}
        self.notes = []

    # ---- infrastructure ----
    def t(self, seconds):
        return seconds * self.scale

    def wait_for(self, predicate, timeout, what, interval=0.1):
        deadline = time.monotonic() + self.t(timeout)
        last = None
        while time.monotonic() < deadline:
            try:
                last = predicate()
                if last:
                    return last
            except (urllib.error.URLError, ConnectionError, OSError, KeyError) as error:
                last = error
            self.assert_alive()
            time.sleep(interval)
        raise SoakFailure(f"timed out after {self.t(timeout):.1f}s waiting for {what} (last: {last!r})")

    def assert_alive(self):
        if self.daemon and not self.daemon.alive():
            raise SoakFailure(f"daemon exited with status {self.daemon.proc.returncode}")

    def guard_config(self):
        """The daemon keeps config.json at a path compiled into the binary
        (<install>/data). Lock it against a second soak and restore it on exit,
        including after an earlier run that was killed before it could."""
        data_dir = os.path.join(self.install_dir, "data")
        os.makedirs(data_dir, exist_ok=True)
        self.config_path = os.path.join(data_dir, "config.json")
        self.backup_path = self.config_path + ".recovery-soak-backup"
        self.lock_file = open(os.path.join(data_dir, ".recovery-soak.lock"), "w")
        try:
            fcntl.flock(self.lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            raise SoakFailure(f"another recovery soak is using {self.install_dir}") from None
        if os.path.exists(self.backup_path):  # a previous run died mid-way
            os.replace(self.backup_path, self.config_path)
        self.had_config = os.path.exists(self.config_path)
        if self.had_config:
            shutil.copy2(self.config_path, self.backup_path)
        self.config_guarded = True

    def restore_config(self):
        if not getattr(self, "config_guarded", False):
            return
        self.config_guarded = False
        if self.had_config:
            os.replace(self.backup_path, self.config_path)
        else:
            for path in (self.config_path, self.backup_path):
                try:
                    os.unlink(path)
                except OSError:
                    pass
        self.lock_file.close()

    def child_env(self):
        env = dict(os.environ)
        for name in ("LED_MATRIX_API_TOKEN", "LED_MATRIX_ALLOWED_ORIGINS", "SPOTIFY_CLIENT_ID",
                     "SPOTIFY_CLIENT_SECRET"):
            env.pop(name, None)
        env["TMPDIR"] = os.path.join(self.workdir, "tmp")
        os.makedirs(env["TMPDIR"], exist_ok=True)
        return env

    def start_daemon(self):
        self.guard_config()
        binary = self.daemon_bin
        env = self.child_env()
        env["PORT"] = str(self.port)
        cwd = os.path.join(self.workdir, "daemon-cwd")
        os.makedirs(cwd, exist_ok=True)
        self.daemon = Child("daemon", [binary] + DAEMON_FLAGS, env, cwd,
                            os.path.join(self.workdir, "daemon.log"))
        self.wait_for(lambda: self.api.get("/status", timeout=2), 60, "daemon /status")
        self.wait_for(lambda: self.api.diag().get("registry") is not None, 30, "daemon diagnostics")

    def ensure_desktop(self):
        if self.desktop is None:
            self.desktop = FakeDesktop(self.port, "soak-worker")
            self.wait_for(lambda: self.api.diag()["desktop_connections"] >= 1, 10, "the desktop connection")

    def drop_desktop(self, abrupt=True):
        if self.desktop:
            self.desktop.close(abrupt=abrupt)
            self.desktop = None

    def start_worker(self):
        self.ensure_desktop()
        self.worker_starts += 1
        cwd = os.path.join(self.workdir, "worker-cwd")
        os.makedirs(cwd, exist_ok=True)
        self.worker = Child(
            "worker",
            [self.worker_bin, "--host", "127.0.0.1", "--port", str(self.port),
             "--client-id", "soak-worker", "--plugin-dir", self.plugin_dir,
             "--crash-dir", os.path.join(self.workdir, "worker-crashes")],
            self.child_env(), cwd, os.path.join(self.workdir, f"worker-{self.worker_starts}.log"))
        return self.worker

    def kill_worker(self):
        if self.worker:
            self.worker.kill()
            self.worker = None

    def remote(self):
        return self.api.diag()["remote_render"]

    def live(self):
        return self.api.diag()["live_frame"]

    def wait_worker_ready(self, timeout=30):
        def ready():
            if self.worker and not self.worker.alive():
                raise SoakFailure("worker exited early:\n" + self.worker.log_text(2000))
            remote = self.remote()
            return remote["worker_available"] and remote["worker_compatible_scene_count"] > 0
        self.wait_for(ready, timeout, "the daemon to accept the scene worker")

    def wait_worker_gone(self, timeout=10):
        self.wait_for(lambda: not self.remote()["worker_available"], timeout,
                      "the daemon to notice the worker is gone")

    # ---- viewer / frame checks ----
    def expect_frames(self, what, within=8.0, frames=3):
        """The pump must show `frames` more lit frames within the bound."""
        before = self.pump.snapshot()
        started = time.monotonic()
        self.wait_for(lambda: self.pump.snapshot()["lit_frames"] >= before["lit_frames"] + frames,
                      within, f"{frames} usable frames {what}")
        waited = time.monotonic() - started
        self.timings.setdefault("recovery_s", []).append(round(waited, 2))
        return waited

    def check_pump_healthy(self, what):
        state = self.pump.snapshot()
        if state["errors"]:
            raise SoakFailure(f"viewer errors {what}: {state['errors'][:3]}")
        limit = self.t(self.args.max_frame_gap)
        if state["max_gap"] > limit:
            raise SoakFailure(f"viewer saw a {state['max_gap']:.1f}s gap {what} (limit {limit:.1f}s)")

    def settle_idle_check(self, seconds, what):
        """No outstanding demand: presentation continues (local or desktop
        frames), nothing is captured."""
        time.sleep(self.t(0.6))  # let any request that was already queued be served
        before = self.api.diag()
        time.sleep(seconds)  # a measurement window, not a bound: no scaling
        after = self.api.diag()
        d_requests = after["live_frame"]["requests"] - before["live_frame"]["requests"]
        d_captures = after["live_frame"]["captures"] - before["live_frame"]["captures"]
        d_frames = after["renderer"]["presentation_frames"] - before["renderer"]["presentation_frames"]
        if d_requests or d_captures:
            raise SoakFailure(f"{what}: {d_captures} frames captured / {d_requests} requested "
                              f"with no viewer demand ({before['live_frame']} -> {after['live_frame']})")
        if d_frames <= 0:
            raise SoakFailure(f"{what}: the matrix stopped presenting frames while idle")
        return d_frames

    def new_viewer(self):
        return WsClient(self.port, "/live_frame")

    def one_frame(self, viewer, timeout=8.0):
        viewer.send_text("next")
        opcode, payload = viewer.recv(self.t(timeout))
        if opcode != 0x2:
            raise SoakFailure("live frame was text")
        return parse_frame(payload)

    # ---- presets ----
    def add_preset(self, name, scenes, duration_ms, transition_ms=700):
        body = {"display_name": name, "transition_duration": transition_ms,
                "transition_name": "blend",
                "scenes": [{"type": scene, "uuid": "", "arguments": {"duration": duration_ms}}
                           for scene in scenes]}
        return self.api.request(f"/add_preset?id={name}", body)[1]["id"]

    def activate(self, preset_id):
        self.api.get(f"/set_active?id={preset_id}")

    def skip(self):
        self.api.get("/skip")

    def active_scene(self):
        return self.api.diag()["renderer"]["active_scene"]

    # ---- scenarios ----
    def scenario_idle_viewers(self):
        self.activate(self.presets["rotate"])
        viewer = self.new_viewer()
        try:
            frames_idle = self.settle_idle_check(2.0, "idle connected viewer")
            width, height, first_seq, rgb = self.one_frame(viewer)
            if (width, height) != (PANEL, PANEL):
                raise SoakFailure(f"panel is {width}x{height}")
            before = self.live()
            for _ in range(3):  # duplicates while waiting are ignored
                viewer.send_text("next")
            opcode, payload = viewer.recv(self.t(8))
            _, _, second_seq, _ = parse_frame(payload)
            try:
                viewer.recv(self.t(0.8))
                raise SoakFailure("a duplicate `next` produced a second frame")
            except TimeoutError:
                pass
            if not first_seq < second_seq:
                raise SoakFailure("frame sequence did not advance")
            captured = self.live()["captures"] - before["captures"]
            if captured != 1:
                raise SoakFailure(f"three queued `next` messages captured {captured} frames, expected 1")

            viewers = [self.new_viewer() for _ in range(4)]
            before = self.live()
            for other in viewers:
                other.send_text("next")
            for other in viewers:
                parse_frame(other.recv(self.t(8))[1])
            captured = self.live()["captures"] - before["captures"]
            if not 1 <= captured <= len(viewers):
                raise SoakFailure(f"{len(viewers)} concurrent viewers caused {captured} captures")
            for other in viewers:
                other.close()
            self.settle_idle_check(1.5, "after concurrent viewers")
        finally:
            viewer.close()
        return f"{frames_idle} presented frames idle with 0 captures; 1 capture per `next`"

    def scenario_scene_lab_stale(self):
        first = self.api.request("/scene_lab/start", {"scene": "boids", "fps": 20})[1]
        first_id = first.get("session_id") or first.get("session", {}).get("session_id")
        if not first_id:
            raise SoakFailure(f"Scene Lab start returned no session id: {first}")
        second = self.api.request("/scene_lab/start", {"scene": "metablob", "fps": 20})[1]
        second_id = second.get("session_id") or second.get("session", {}).get("session_id")
        if not second_id or second_id == first_id:
            raise SoakFailure("a new Scene Lab start did not create a new session")
        for path, body in (("/scene_lab/update", {"fps": 15}),
                           ("/scene_lab/heartbeat", {}),
                           ("/scene_lab/stop", {})):
            status, _ = self.api.request(path, dict(body, session_id=first_id), expect_ok=False)
            if status != 409:
                raise SoakFailure(f"stale Scene Lab session on {path} got HTTP {status}, expected 409")
        self.api.request("/scene_lab/update", {"fps": 15, "session_id": second_id})
        self.api.request("/scene_lab/stop", {"session_id": second_id})
        status, _ = self.api.request("/scene_lab/stop", {"session_id": second_id}, expect_ok=False)
        if status == 500:
            raise SoakFailure("stopping a finished Scene Lab session crashed the route")
        self.activate(self.presets["rotate"])
        return "replaced/stopped Scene Lab sessions answer 409"

    def next_desktop_session(self, previous, what, timeout=20.0):
        """Wait for the daemon's next scene change to start a desktop session.

        Scene changes at the end of a scene's duration always run through a
        transition, and a transition into an offloadable scene is rendered by
        the worker whenever one is attached. (/skip is a hard cut instead and
        only offloads scenes the Pi measured as too expensive.)
        """
        def started():
            remote = self.remote()
            return remote["session"] if (remote["requested"] and remote["session"] != previous
                                         and remote["frame_fresh"]) else 0
        return self.wait_for(started, timeout, f"a new desktop session {what} (last {previous})",
                             interval=0.05)

    def desktop_delivering(self, what, timeout=25.0):
        self.wait_for(lambda: (lambda r: r["requested"] and r["frame_fresh"])(self.remote()),
                      timeout, f"fresh desktop frames {what}")

    def scenario_worker_handoff(self):
        self.start_worker()
        self.wait_worker_ready()
        sessions = []
        last = self.remote()["session"]
        for index in range(self.args.handoffs):
            last = self.next_desktop_session(last, f"for handoff {index + 1}")
            sessions.append(last)
            self.expect_frames(f"during handoff {index + 1}", 6)
        self.check_pump_healthy("during handoffs")
        counts = self.api.diag()["udp"]["frame_streams"].get("RenderOffload", {})
        return f"{len(sessions)} desktop sessions, RenderOffload frames {counts}"

    def scenario_worker_kill(self):
        delays = [0.0, 0.15, 0.5, 1.2, 2.5]
        last = self.remote()["session"]
        for cycle in range(self.args.kill_cycles):
            delay = delays[cycle % len(delays)]
            last = self.next_desktop_session(last, f"before kill {cycle + 1}")
            time.sleep(self.t(delay))
            self.kill_worker()
            self.pump.reset_gap()
            self.expect_frames(f"after SIGKILL of the worker {delay}s into a handoff (cycle {cycle + 1})", 8)
            self.wait_worker_gone(10)
            if self.remote()["frame_fresh"]:
                raise SoakFailure("daemon still reports fresh desktop frames after the worker died")
            self.skip()  # a hard cut with no worker must stay local
            self.expect_frames("while no worker is attached", 8)
            self.start_worker()
            self.wait_worker_ready()
            self.desktop_delivering(f"after restarting the worker (cycle {cycle + 1})")
            self.expect_frames("after desktop recovery", 6)
            self.check_pump_healthy(f"in kill cycle {cycle + 1}")
        return f"{self.args.kill_cycles} kill/restart cycles"

    def scenario_worker_stall(self):
        last = self.remote()["session"]
        for cycle in range(self.args.stall_cycles):
            last = self.next_desktop_session(last, f"before stall {cycle + 1}")
            time.sleep(self.t(0.2))
            self.worker.signal(signal.SIGSTOP)
            try:
                self.pump.reset_gap()
                self.expect_frames("while the worker is frozen", 10)
                self.wait_worker_gone(10)
                self.skip()
                self.expect_frames("after switching with a frozen worker", 8)
            finally:
                self.worker.signal(signal.SIGCONT)
            self.wait_worker_ready()
            self.desktop_delivering(f"after the worker resumed (cycle {cycle + 1})")
            self.expect_frames("after the worker resumed", 6)
            self.check_pump_healthy(f"in stall cycle {cycle + 1}")
        return f"{self.args.stall_cycles} stall/resume cycles"

    def scenario_desktop_flap(self):
        last = self.remote()["session"]
        for cycle in range(self.args.flaps):
            last = self.next_desktop_session(last, f"before desktop flap {cycle + 1}")
            time.sleep(self.t(0.4))
            self.drop_desktop(abrupt=cycle % 2 == 0)
            self.pump.reset_gap()
            self.expect_frames("after the desktop control socket dropped", 8)
            self.wait_worker_gone(12)
            self.expect_frames("while the desktop is away", 8)
            self.ensure_desktop()
            self.wait_worker_ready()
            self.desktop_delivering(f"after the desktop reconnected (cycle {cycle + 1})")
            self.expect_frames("after the desktop reconnected", 6)
            self.check_pump_healthy(f"in desktop flap {cycle + 1}")
        return f"{self.args.flaps} desktop reconnects"

    def scenario_hard_cut(self):
        """A desktop session must never outlive its scene, also across /skip.

        A natural handoff puts the heavy scene (metablob) on the worker; /skip
        then cuts straight to a cheap scene (starfield) that the Pi keeps. The
        worker must stop rendering the scene that left.
        """
        self.activate(self.presets["cut"])
        leaked = []
        for index in range(self.args.hard_cuts):
            def heavy_on_desktop():
                diag = self.api.diag()
                remote = diag["remote_render"]
                return (diag["renderer"]["active_scene"] == "metablob" and remote["scene"] == "metablob"
                        and remote["requested"] and remote["frame_fresh"])
            self.wait_for(heavy_on_desktop, 20, "metablob rendered by the worker", interval=0.05)
            self.skip()
            time.sleep(self.t(1.2))
            diag = self.api.diag()
            remote, active = diag["remote_render"], diag["renderer"]["active_scene"]
            if remote["requested"] and remote["scene"] != active:
                leaked.append(f"cut {index + 1}: showing '{active}', worker still renders "
                              f"'{remote['scene']}' (session {remote['session']}, "
                              f"fresh={remote['frame_fresh']}, placement "
                              f"{diag['render_placement'].get('placement')})")
        if leaked:
            raise SoakFailure("desktop session outlived its scene after /skip:\n  " + "\n  ".join(leaked))
        self.expect_frames("after hard cuts", 5)
        return f"{self.args.hard_cuts} /skip hard cuts left no orphaned desktop session"

    def udp_send(self, packets):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            for packet in packets:
                sock.sendto(packet, ("127.0.0.1", self.port))
        finally:
            sock.close()

    @staticmethod
    def frame_packets(session, sequence, width, height, color, chunk_count=None, index_shift=0):
        """RenderOffload frame datagrams: AD 01 <plugin 5> <u32 size> + header + RGB chunk."""
        payload = bytes(color) * (width * height)
        chunks = [payload[i:i + 1200] for i in range(0, len(payload), 1200)]
        packets = []
        for index, chunk in enumerate(chunks):
            header = struct.pack(">BIIHHHHI", 4, session, sequence, width, height,
                                 index + index_shift, chunk_count or len(chunks), index * 1200)
            body = header + chunk
            packets.append(b"\xad\x01\x05" + struct.pack(">I", len(body)) + body)
        return packets

    def scenario_stale_frames(self):
        # Slow rotation: a natural switch during the injection only makes the
        # forged ids staler, it can never make a forged frame valid.
        self.activate(self.presets["slow"])
        self.wait_worker_ready()
        session = self.next_desktop_session(self.remote()["session"], "for the stale-frame checks")
        magenta = (255, 0, 255)
        huge = 0x7FFFF000
        garbage_header = struct.pack(">BIIHHHHI", 4, session, huge, PANEL, PANEL, 0, 41, 0)
        packets = []
        packets += self.frame_packets(0xFFFF0000, huge, PANEL, PANEL, magenta)      # unknown session
        packets += self.frame_packets(session + 1000, huge, PANEL, PANEL, magenta)  # future session
        packets += self.frame_packets(session, 1, PANEL, PANEL, magenta)            # replayed sequence
        packets += self.frame_packets(session, huge, 64, 64, magenta)               # wrong size
        packets += self.frame_packets(session, huge + 1, PANEL, PANEL, magenta, chunk_count=3)
        packets += self.frame_packets(session, huge + 2, PANEL, PANEL, magenta, index_shift=60000)
        packets += [b"", b"\xad", b"\xad\x01\x05", b"\xad\x01\x05\xff\xff\xff\xff" + b"x" * 40,
                    b"\xad\x01\x05" + struct.pack(">I", 21) + garbage_header[:21],
                    b"\x00" * 1500, os.urandom(1400)]
        for _ in range(3):
            self.udp_send(packets)
        viewer = self.new_viewer()
        try:
            deadline = time.monotonic() + self.t(2.5)
            frames = 0
            while time.monotonic() < deadline:
                _, _, _, rgb = self.one_frame(viewer)
                frames += 1
                forged = sum(1 for i in range(0, len(rgb), 3) if rgb[i:i + 3] == bytes(magenta))
                if forged > len(rgb) // 12:
                    raise SoakFailure("a forged frame for a stale session reached the matrix")
            if frames == 0:
                raise SoakFailure("no live frames while forged traffic was processed")
        finally:
            viewer.close()
        remote = self.remote()
        if remote["last_sequence"] >= huge:
            raise SoakFailure(f"forged frames moved the desktop sequence: {remote}")
        self.desktop_delivering("after forged traffic")

        # The old session after a worker restart is stale as well.
        self.kill_worker()
        self.wait_worker_gone(10)
        self.start_worker()
        self.wait_worker_ready()
        new_session = self.next_desktop_session(session, "after the restart", timeout=30)
        self.udp_send(self.frame_packets(session, huge + 10, PANEL, PANEL, magenta))
        time.sleep(self.t(1.0))
        remote = self.remote()
        if remote["last_sequence"] >= huge:
            raise SoakFailure(f"a frame for the pre-restart session was accepted: {remote}")
        self.expect_frames("after replayed old-session frames", 5)
        self.check_pump_healthy("during stale-frame injection")
        return f"{len(packets) * 3} forged/malformed datagrams rejected (session {session} -> {new_session})"

    def scenario_viewer_churn(self):
        self.activate(self.presets["rotate"])
        self.wait_worker_ready()
        rng = random.Random(143)
        opened = 0
        for index in range(self.args.churn):
            viewer = self.new_viewer()
            opened += 1
            action = rng.choice(["drop_idle", "drop_waiting", "junk", "close_clean", "double_wait"])
            if action == "drop_waiting":
                viewer.send_text("next")
                viewer.close(abrupt=True)
            elif action == "junk":
                viewer.send_text("not-next")
                viewer.send(0x2, os.urandom(300))
                viewer.send(0x9, b"hi")
                viewer.send_text("next")
                viewer.close(abrupt=rng.random() < 0.5)
            elif action == "double_wait":
                viewer.send_text("next")
                viewer.send_text("next")
                try:
                    viewer.recv(self.t(6))
                except (TimeoutError, ConnectionError):
                    pass
                viewer.close()
            elif action == "close_clean":
                viewer.send_text("next")
                parse_frame(viewer.recv(self.t(8))[1])
                viewer.close()
            else:
                viewer.close(abrupt=True)
            if index % 7 == 3:
                self.skip()  # churn right through a scene (and desktop) handoff
            time.sleep(self.t(0.15))
        self.api.get("/status")
        self.expect_frames("after viewer churn", 6)
        self.check_pump_healthy("during viewer churn")
        return f"{opened} viewers opened and dropped"

    def scenario_final_idle(self):
        self.pump.stop()
        state = self.pump.snapshot()
        self.pump = None
        idle = self.settle_idle_check(3.0, "all viewers gone")
        self.kill_worker()
        self.wait_worker_gone(10)
        self.settle_idle_check(2.0, "all viewers and the worker gone")
        return f"viewer pulled {state['frames']} frames in total; {idle} presented frames while idle, 0 captures"

    # ---- run ----
    def run(self):
        results = []
        started = time.monotonic()
        log(f"workdir {self.workdir}, port {self.port}, time scale x{self.scale}")
        self.start_daemon()
        log("daemon ready")
        self.presets = {
            "rotate": self.add_preset("soak-rotate", ["boids", "metablob", "julia_set", "neontunnel"], 2500),
            "cut": self.add_preset("soak-cut", ["metablob", "starfield"], 2500),
            "slow": self.add_preset("soak-slow", ["boids", "metablob", "julia_set"], 9000),
        }
        self.activate(self.presets["rotate"])
        self.wait_for(lambda: self.active_scene(), 20, "an active scene")

        order = [("idle_viewers", self.scenario_idle_viewers),
                 ("scene_lab_stale", self.scenario_scene_lab_stale),
                 ("worker_handoff", self.scenario_worker_handoff),
                 ("worker_kill", self.scenario_worker_kill),
                 ("worker_stall", self.scenario_worker_stall),
                 ("desktop_flap", self.scenario_desktop_flap),
                 ("hard_cut", self.scenario_hard_cut),
                 ("stale_frames", self.scenario_stale_frames),
                 ("viewer_churn", self.scenario_viewer_churn),
                 ("final_idle", self.scenario_final_idle)]
        selected = [name for name, _ in order if not self.args.only or name in self.args.only]
        for name, function in order:
            if name not in selected:
                continue
            began = time.monotonic()
            # Scenarios after the first two keep a viewer pulling frames in the
            # background (the idle checks need an exact capture count instead).
            if name not in ("idle_viewers", "scene_lab_stale") and self.pump is None:
                self.pump = ViewerPump(self.port).start()
                self.wait_for(lambda: self.pump.snapshot()["lit_frames"] >= 2, 15, "the first viewer frames")
            if name in ("worker_kill", "worker_stall", "desktop_flap", "hard_cut", "stale_frames", "viewer_churn") and self.worker is None:
                self.start_worker()
                self.wait_worker_ready()
            log(f"-> {name}")
            try:
                detail = function()
            except Exception:
                results.append((name, False, time.monotonic() - began, ""))
                raise
            elapsed = time.monotonic() - began
            results.append((name, True, elapsed, detail))
            log(f"   ok {name} in {elapsed:.1f}s: {detail}")
        self.results = results
        self.total = time.monotonic() - started

    def finish(self):
        """Stop the daemon and fail on any sanitizer/assert report in any log."""
        problems = []
        if self.pump:
            self.pump.stop()
        self.kill_worker()
        self.drop_desktop(abrupt=False)
        if self.daemon:
            status = self.daemon.stop(grace=self.t(15))
            self.notes.append(f"daemon exit status on SIGTERM: {status}")
            if self.daemon.forced:
                problems.append(f"the daemon ignored SIGTERM for {self.t(15):.0f}s and had to be killed")
        for child in CHILDREN:
            text = child.log_text()
            for match in SANITIZER_REPORT.finditer(text):
                start = max(0, match.start() - 200)
                problems.append(f"{child.name} log ({os.path.basename(child.log_path)}): "
                                f"{text[start:match.start() + 800]}")
                break
        if problems:
            raise SoakFailure("sanitizer/assertion output found:\n" + "\n---\n".join(problems))


def resolve(args):
    install = os.path.abspath(args.daemon_install) if args.daemon_install else None
    daemon = os.path.join(install, "bin", "led-matrix") if install else None
    worker_dir = os.path.abspath(args.worker_build) if args.worker_build else None
    worker = os.path.join(worker_dir, "led-matrix-scene-worker") if worker_dir else None
    plugins = os.path.join(worker_dir, "scene_plugins") if worker_dir else None
    missing = []
    if not daemon or not os.access(daemon, os.X_OK):
        missing.append(f"emulator daemon ({daemon})")
    if not worker or not os.access(worker, os.X_OK):
        missing.append(f"desktop scene worker ({worker})")
    elif not os.path.isdir(plugins):
        missing.append(f"worker plugins ({plugins})")
    return daemon, install, worker, plugins, missing


PROFILES = {
    "quick": dict(handoffs=3, kill_cycles=3, stall_cycles=1, flaps=1, hard_cuts=3, churn=24),
    "full": dict(handoffs=8, kill_cycles=10, stall_cycles=4, flaps=4, hard_cuts=10, churn=120),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--daemon-install", help="install tree of the emulator build (contains bin/led-matrix)")
    parser.add_argument("--worker-build", help="desktop build dir (led-matrix-scene-worker, scene_plugins/)")
    parser.add_argument("--profile", choices=sorted(PROFILES), default="quick")
    parser.add_argument("--time-scale", type=float, default=1.0,
                        help="multiply every wait bound (use 3 for sanitizer builds)")
    parser.add_argument("--max-runtime", type=float, default=None,
                        help="hard deadline in seconds (default: 150 quick / 900 full, times the scale)")
    parser.add_argument("--max-frame-gap", type=float, default=5.0,
                        help="longest allowed pause between live frames (seconds, before scaling)")
    parser.add_argument("--only", action="append", help="run only this scenario (repeatable)")
    parser.add_argument("--keep", action="store_true", help="keep the work directory")
    for name in ("handoffs", "kill_cycles", "stall_cycles", "flaps", "hard_cuts", "churn"):
        parser.add_argument("--" + name.replace("_", "-"), dest=name, type=int)
    args = parser.parse_args()
    for key, value in PROFILES[args.profile].items():
        if getattr(args, key) is None:
            setattr(args, key, value)

    daemon, install, worker, plugins, missing = resolve(args)
    if missing:
        print("SKIP: missing " + ", ".join(missing))
        return SKIP
    if not sys.platform.startswith("linux"):
        print("SKIP: the soak needs Linux process groups and signals")
        return SKIP

    start_watchdog((args.max_runtime or (150 if args.profile == "quick" else 900)) * args.time_scale)
    workdir = tempfile.mkdtemp(prefix="recovery-soak-")
    soak = Soak(args, daemon, install, worker, plugins, workdir)
    status = 0
    try:
        try:
            soak.run()
            soak.finish()
        except BaseException as error:
            status = 1
            print(f"\nFAILED: {type(error).__name__}: {error}", file=sys.stderr)
            try:
                diag = soak.api.diag()
                print("last /diagnostics: " + json.dumps(
                    {k: diag.get(k) for k in ("live_frame", "remote_render", "desktop_connections")}),
                    file=sys.stderr)
            except Exception:
                pass
            for child in CHILDREN:
                print(f"--- tail of {os.path.basename(child.log_path)} ---\n{child.log_text(2500)}",
                      file=sys.stderr)
            if soak.pump:
                print("viewer: " + json.dumps(soak.pump.snapshot(), default=str), file=sys.stderr)
            if isinstance(error, (KeyboardInterrupt, SystemExit)):
                raise
    finally:
        cleanup()
        soak.restore_config()
        if args.keep or status:
            print(f"work directory kept: {workdir}")
        else:
            shutil.rmtree(workdir, ignore_errors=True)
    if status == 0:
        print("\nrecovery soak passed in %.1fs (profile %s, scale x%s)" % (soak.total, args.profile, args.time_scale))
        for name, ok, elapsed, detail in soak.results:
            print(f"  {name:<16} {elapsed:6.1f}s  {detail}")
        for note in soak.notes:
            print("  " + note)
        if "recovery_s" in soak.timings:
            samples = sorted(soak.timings["recovery_s"])
            print(f"  usable-frame recovery: median {samples[len(samples) // 2]}s, max {samples[-1]}s "
                  f"over {len(samples)} faults")
    return status


if __name__ == "__main__":
    sys.exit(main())
