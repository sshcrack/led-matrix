#!/usr/bin/env python3
"""Simulated power cuts on a panel card (docs/POWER_CUT_TEST.md).

Each cycle starts an activity, reboots the Pi instantly with sysrq 'b' (no sync, no
unmount: what was not on the card is lost, as in a power cut), waits for the API and
checks what survived. The SD card itself keeps power, so this does not test the
card's own behaviour on power loss.

    tests/powercut_soak.py <host> <results.jsonl> [counts]

<host> is an SSH host with passwordless sudo whose daemon answers on port 8080.
counts is "<n>" (n cycles of every state) or "idle=4,update=6,...", states:
    idle      playback only
    settings  POST /brightness_schedule in a loop (fade_minutes counts up)
    uploads   POST a ~240 KB shader to /api/custom-assets in a loop
    update    queue an app update, cut 2-75 s later (copy, unpack, switch)
    switch    queue an app update, cut 0-6 s after "Switching" (version on trial)
    boot      cut, then cut again 0-8 s after SSH answers
Updates alternate between two packages in $POWERCUT_DEB_DIR on the Pi (default
/var/lib/led-matrix/update/powercut/led-matrix-<version>_arm64.deb, versions from
$POWERCUT_VERSIONS, default "1.22.4 1.22.5"). The updater deletes a package after
a successful update; copy it again before the next run.

Leaves behind: the test shader powercut_test.frag and a changed fade_minutes.
Stops when the API does not come back within 5 minutes.
"""
import json
import os
import random
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
import uuid

HOST = API = ''
DEB_DIR = os.environ.get('POWERCUT_DEB_DIR', '/var/lib/led-matrix/update/powercut')
VERSIONS = os.environ.get('POWERCUT_VERSIONS', '1.22.4 1.22.5').split()
BACK_TIMEOUT = 300
SHADER = 'powercut_test.frag'
STATES = ('idle', 'settings', 'uploads', 'update', 'switch', 'boot')


def ssh(cmd, timeout=60):
    r = subprocess.run(['ssh', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=4', HOST, cmd],
                       capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stdout


def http(method, path, body=None, headers=None, timeout=5):
    req = urllib.request.Request(API + path, data=body, method=method, headers=headers or {})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.status, resp.read()


def get_json(path, timeout=5):
    return json.loads(http('GET', path, timeout=timeout)[1])


def cut():
    # The connection dies with the kernel; don't wait for it.
    subprocess.Popen(['ssh', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=4', HOST,
                      "sudo -n sh -c 'echo b > /proc/sysrq-trigger'"],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return time.monotonic()


def wait_down(t0, limit=20):
    while time.monotonic() - t0 < limit:
        try:
            http('GET', '/status', timeout=1)
            time.sleep(0.3)
        except Exception:
            return True
    return False


def wait_back(t0):
    """('normal'|'recovery'|None, seconds from the cut until the API answered)."""
    while time.monotonic() - t0 < BACK_TIMEOUT:
        try:
            if 'operation_mode' in get_json('/status', timeout=2):
                return 'normal', time.monotonic() - t0
        except Exception:
            pass
        try:
            if get_json('/api/recovery/status', timeout=2).get('reason'):
                return 'recovery', time.monotonic() - t0
        except Exception:
            pass
        time.sleep(1)
    return None, time.monotonic() - t0


AFTER = r'''
echo "args=$(ps -o args= -C led-matrix | head -1 | grep -o -- '--recovery=[a-z]*')"
echo "current=$(readlink /var/lib/led-matrix-app/current)"
echo "previous=$(readlink /var/lib/led-matrix-app/previous)"
echo "trial=$(cat /var/lib/led-matrix-app/trial 2>/dev/null)"
echo "corrupt=$(ls /var/lib/led-matrix/ | grep -c corrupt)"
echo "mounted=$(findmnt -n -o SOURCE /var/lib/led-matrix)"
echo "failed=$(systemctl --failed --no-legend --plain | awk '{print $1}' | tr '\n' ' ')"
echo "fs=$(sudo -n dmesg | grep -E 'mmcblk0p3' | grep -v -E 'mounted filesystem' | sed 's/^\[[^]]*\] //' | tr '\n' '|' | cut -c1-300)"
echo "app=$(journalctl -b -u led-matrix -o cat | grep -i -E 'load_recovery|last-good|corrupt|recovery mode|switching back|rolled back|rollback' | tr '\n' '|' | cut -c1-300)"
echo "versions=$(ls -A /var/lib/led-matrix-app/versions 2>/dev/null | tr '\n' ' ')"
echo "data_free_mib=$(df -Pm /var/lib/led-matrix | awk 'NR==2 {print $4}')"
'''


def collect():
    for _ in range(10):
        try:
            rc, out = ssh(AFTER, timeout=30)
            if rc == 0:
                return dict(line.split('=', 1) for line in out.splitlines() if '=' in line)
        except subprocess.TimeoutExpired:
            pass
        time.sleep(3)
    return {'error': 'ssh failed'}


def repair_if_recovery(kind):
    if kind != 'recovery':
        return None
    http('POST', '/api/recovery/action?name=repair')
    t0 = time.monotonic()
    while time.monotonic() - t0 < 180:
        time.sleep(3)
        try:
            if 'operation_mode' in get_json('/status', timeout=2):
                return 'repaired'
        except Exception:
            pass
    return 'repair did not bring playback back'


# --- activities -----------------------------------------------------------------------

class Settings:
    """Saves fade_minutes counting up; after the cut it must be the last acknowledged or last sent value."""

    def __init__(self):
        self.base = {k: v for k, v in get_json('/brightness_schedule').items() if k != 'current_brightness'}
        self.sent = self.acked = None
        self.stop = threading.Event()

    def run(self):
        n = random.randint(1, 50)
        while not self.stop.is_set():
            n = n % 120 + 1
            self.sent = n
            try:
                status, _ = http('POST', '/brightness_schedule', json.dumps({**self.base, 'fade_minutes': n}).encode(),
                                 {'Content-Type': 'application/json'}, timeout=3)
                if status == 200:
                    self.acked = n
            except Exception:
                return
            time.sleep(0.05)

    def check(self):
        value = get_json('/brightness_schedule')['fade_minutes']
        ok = self.acked is None or value in (self.acked, self.sent)
        return ok, f'fade_minutes={value} acked={self.acked} sent={self.sent}'


def shader_body(n):
    return (f'// powercut {n}\n' + f'// {n:08d} ' * 20000 +
            '\nvoid mainImage(out vec4 c, in vec2 p) { c = vec4(0); }\n').encode()


class Uploads:
    """Uploads numbered versions of one file; after the cut it must be complete and acknowledged or last sent."""

    def __init__(self):
        self.sent = self.acked = None
        self.stop = threading.Event()

    def run(self):
        n = random.randint(1, 1000)
        while not self.stop.is_set():
            n += 1
            self.sent = n
            boundary = uuid.uuid4().hex
            body = (f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="{SHADER}"\r\n'
                    'Content-Type: application/octet-stream\r\n\r\n').encode() + shader_body(n) + \
                f'\r\n--{boundary}--\r\n'.encode()
            try:
                status, _ = http('POST', '/api/custom-assets/shader', body,
                                 {'Content-Type': f'multipart/form-data; boundary={boundary}'}, timeout=10)
                if status in (200, 201):
                    self.acked = n
            except Exception:
                return

    def check(self):
        try:
            _, data = http('GET', f'/api/custom-assets/shader/{SHADER}/download', timeout=10)
        except urllib.error.HTTPError as e:
            return self.acked is None, f'download HTTP {e.code} (acked={self.acked})'
        first = data.split(b'\n', 1)[0].decode(errors='replace')
        try:
            n = int(first.split()[-1])
        except ValueError:
            return False, f'unreadable file, first line {first!r}, {len(data)} bytes'
        complete = data == shader_body(n)
        ok = complete and (self.acked is None or n in (self.acked, self.sent))
        return ok, f'file version {n} complete={complete} acked={self.acked} sent={self.sent}'


def queue_update():
    _, current = ssh('readlink /var/lib/led-matrix-app/current')
    current = current.strip()
    target = next((v for v in VERSIONS if f'/{v}-' not in current), VERSIONS[0])
    # Unpack every time: drop the target's folder unless it is what runs now.
    ssh(f'cd /var/lib/led-matrix-app/versions 2>/dev/null && for d in {target}-*; do '
        f'[ -e "$d" ] && [ "versions/$d" != "$(readlink ../current)" ] && sudo -n rm -rf "$d"; done; '
        f'cd /var/lib/led-matrix/update && printf "%s\\n8080\\n" {DEB_DIR}/led-matrix-{target}_arm64.deb > .request.tmp '
        f'&& mv .request.tmp request')
    return current, target


# --- cycles ---------------------------------------------------------------------------

def cycle(state, out):
    rec = {'state': state, 'start': time.strftime('%Y-%m-%d %H:%M:%S')}
    activity = None
    if state == 'idle':
        time.sleep(random.uniform(3, 30))
    elif state in ('settings', 'uploads'):
        activity = Settings() if state == 'settings' else Uploads()
        threading.Thread(target=activity.run, daemon=True).start()
        time.sleep(random.uniform(1, 10))
    elif state == 'update':
        rec['from'], rec['to'] = queue_update()
        time.sleep(random.uniform(2, 75))
    elif state == 'switch':
        rec['from'], rec['to'] = queue_update()
        _, line = ssh("timeout 200 sh -c 'journalctl -f -n0 -o cat -u led-matrix-update | grep -m1 Switching'",
                      timeout=220)
        rec['saw'] = line.strip()
        if not rec['saw']:
            rec['verdict'] = 'SKIPPED: the update never switched (package missing?)'
            out.write(json.dumps(rec) + '\n'); out.flush()
            return rec
        time.sleep(random.uniform(0, 6))
    t0 = cut()
    if activity:
        activity.stop.set()
    rec['went_down'] = wait_down(t0)
    if state == 'boot':
        # A second cut while the Pi is still booting (mounts, journal replay, service start).
        while time.monotonic() - t0 < 120:
            try:
                if ssh('true', timeout=6)[0] == 0:
                    break
            except subprocess.TimeoutExpired:
                pass
            time.sleep(0.5)
        time.sleep(random.uniform(0, 8))
        rec['second_cut_after'] = round(time.monotonic() - t0, 1)
        t0 = cut()
        wait_down(t0)
    kind, secs = wait_back(t0)
    rec['result'], rec['seconds_to_api'] = kind, round(secs, 1)
    if kind is None:
        rec['verdict'] = 'NOT BACK'
        out.write(json.dumps(rec) + '\n'); out.flush()
        return rec
    if state == 'switch':
        time.sleep(20)  # the launcher confirms a version on trial once it answers healthy
    rec.update(collect())
    if activity and kind == 'normal':
        rec['data_ok'], rec['data'] = activity.check()
    rec['repair'] = repair_if_recovery(kind)
    ok = kind == 'normal' and rec.get('data_ok', True) or rec['repair'] == 'repaired'
    rec['verdict'] = 'ok' if ok else 'PROBLEM'
    out.write(json.dumps(rec) + '\n'); out.flush()
    return rec


def main():
    global HOST, API
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    HOST, path = sys.argv[1], sys.argv[2]
    API = f'http://{HOST}:8080'
    spec = sys.argv[3] if len(sys.argv) > 3 else '10'
    if '=' in spec:
        counts = {k: int(v) for k, v in (part.split('=') for part in spec.split(','))}
    else:
        counts = {s: int(spec) for s in STATES}
    unknown = set(counts) - set(STATES)
    if unknown:
        print(f'unknown states: {", ".join(sorted(unknown))}')
        return 2
    states = [s for s, n in counts.items() for _ in range(n)]
    random.shuffle(states)
    with open(path, 'a') as out:
        for i, state in enumerate(states, 1):
            rec = cycle(state, out)
            print(f"{i}/{len(states)} {state}: {rec['verdict']} {rec.get('result')} {rec.get('seconds_to_api')}s"
                  f" {rec.get('data', '')} {rec.get('args', '')}".rstrip(), flush=True)
            if rec['verdict'] == 'NOT BACK':
                print('STOPPED: the panel did not come back', flush=True)
                return 1
            time.sleep(random.uniform(2, 6))
    print('DONE', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
