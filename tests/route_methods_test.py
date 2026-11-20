"""Routes that change state take POST; GET only reads (#140).

usage: route_methods_test.py --install <emulator install prefix>

Starts the installed emulator daemon headless on a free port with an API token
and checks that
  - the old GET forms of the migrated routes answer 405 (Allow: POST) and change nothing,
  - the POST forms work with query, JSON and form parameters (the desktop app sends a form),
  - the request guard still refuses requests without the token and cross-site browser requests.
The daemon's config.json lives at a path compiled into the binary, so it is
backed up and restored byte for byte. Exits 77 (skipped) without an install tree.
"""
import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

TOKEN = 'route-methods-test-token'
FLAGS = ['--led-chain', '2', '--led-parallel', '2', '--led-rows', '64', '--led-cols', '64',
         '--led-emulator', '--led-emulator-headless']
MIGRATED_GETS = ['/set_active?id=x', '/set_enabled?enabled=false', '/toggle', '/skip',
                 '/operation_mode?mode=manual', '/video/search?q=rain']


class Client:
    def __init__(self, port):
        self.base = f'http://127.0.0.1:{port}'

    def call(self, method, path, body=None, form=None, token=TOKEN, headers=None):
        data, content_type = None, None
        if body is not None:
            data, content_type = json.dumps(body).encode(), 'application/json'
        elif form is not None:
            data, content_type = urllib.parse.urlencode(form).encode(), 'application/x-www-form-urlencoded'
        req = urllib.request.Request(self.base + path, data=data, method=method)
        if content_type:
            req.add_header('Content-Type', content_type)
        if token:
            req.add_header('Authorization', 'Bearer ' + token)
        for key, value in (headers or {}).items():
            req.add_header(key, value)
        try:
            with urllib.request.urlopen(req, timeout=10) as response:
                status, payload, allow = response.status, response.read(), response.headers.get('Allow')
        except urllib.error.HTTPError as error:
            status, payload, allow = error.code, error.read(), error.headers.get('Allow')
        try:
            parsed = json.loads(payload)
        except ValueError:
            parsed = payload.decode('utf-8', 'replace')
        return status, parsed, allow

    def status(self):
        code, body, _ = self.call('GET', '/status')
        assert code == 200, (code, body)
        return body


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--install', required=True)
    args = parser.parse_args()
    install = os.path.abspath(args.install)
    binary = os.path.join(install, 'bin', 'led-matrix')
    if not os.access(binary, os.X_OK):
        print(f'skipped: {binary} is not installed (cmake --build --preset emulator --target install)')
        return 77

    data_dir = os.path.join(install, 'data')
    os.makedirs(data_dir, exist_ok=True)
    config = os.path.join(data_dir, 'config.json')
    work = tempfile.mkdtemp(prefix='route-methods-')
    backup = os.path.join(work, 'config.json')
    had_config = os.path.exists(config)
    if had_config:
        shutil.copy2(config, backup)
        os.unlink(config)  # start from the defaults

    port = free_port()
    env = {k: v for k, v in os.environ.items() if k not in ('LED_MATRIX_ALLOWED_ORIGINS', 'SPOTIFY_CLIENT_ID', 'SPOTIFY_CLIENT_SECRET')}
    env.update(PORT=str(port), LED_MATRIX_API_TOKEN=TOKEN, TMPDIR=work)
    log = open(os.path.join(work, 'daemon.log'), 'wb')
    api = Client(port)
    failures = []
    daemon = None

    def check(condition, message):
        if not condition:
            failures.append(message)
            print('FAIL:', message)

    # Everything after the backup runs under the finally that restores it.
    try:
        daemon = subprocess.Popen([binary] + FLAGS, env=env, cwd=work, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 60
        while True:
            try:
                api.status()
                break
            except (OSError, AssertionError):
                if daemon.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('the daemon did not start, see ' + log.name)
                time.sleep(0.5)

        # A second preset to switch to.
        scenes = api.call('GET', '/list_scenes')[1]
        scene = next(s['name'] for s in scenes if not s['name'].startswith('shader'))
        code, created, _ = api.call('POST', '/add_preset', {'display_name': 'Route test', 'transition_duration': 0,
                                                           'transition_name': 'blend',
                                                           'scenes': [{'type': scene, 'uuid': '', 'arguments': {}}]})
        check(code == 200, f'add_preset: {code} {created}')
        other = created['id']

        before = api.status()
        for path in MIGRATED_GETS:
            code, body, allow = api.call('GET', path)
            check(code == 405 and allow == 'POST', f'GET {path} answered {code} (Allow: {allow}), expected 405 with Allow: POST')
        check(api.status() == before, f'the old GET forms changed the state: {before} -> {api.status()}')

        code, body, _ = api.call('GET', '/operation_mode')
        check(code == 200 and body == {'operation_mode': before['operation_mode']}, f'GET /operation_mode reads: {code} {body}')

        # POST forms: JSON, query and form parameters.
        check(api.call('POST', '/set_enabled', {'enabled': False})[0] == 200 and api.status()['turned_off'], 'POST /set_enabled JSON false')
        check(api.call('POST', '/set_enabled', form={'enabled': 'true'})[0] == 200 and not api.status()['turned_off'], 'POST /set_enabled form true (desktop app)')
        check(api.call('POST', '/set_enabled?enabled=false')[0] == 200 and api.status()['turned_off'], 'POST /set_enabled query false')
        check(api.call('POST', '/toggle')[0] == 200 and not api.status()['turned_off'], 'POST /toggle')
        check(api.call('POST', '/set_enabled', {'enabled': 'maybe'})[0] == 400, 'POST /set_enabled rejects other values')
        check(api.call('POST', '/operation_mode', {'mode': 'automatic'})[0] == 200 and api.status()['operation_mode'] == 'automatic', 'POST /operation_mode JSON')
        check(api.call('POST', '/operation_mode?mode=bogus')[0] == 400, 'POST /operation_mode rejects other modes')
        code, body, _ = api.call('POST', '/set_active', {'id': other})
        now = api.status()
        check(code == 200 and now['current'] == other and now['operation_mode'] == 'manual', f'POST /set_active JSON: {code} {body} {now}')
        check(api.call('POST', '/set_active', {'id': 'missing'})[0] == 400, 'POST /set_active rejects unknown ids')
        check(api.call('POST', '/skip')[0] == 200, 'POST /skip')

        # The request guard still applies to the POST routes.
        check(api.call('POST', '/toggle', token=None)[0] == 401, 'POST without the token is refused')
        state = api.status()
        code = api.call('POST', '/toggle', headers={'Origin': 'http://evil.example'})[0]
        check(code == 403 and api.status() == state, f'cross-site POST answered {code}, expected 403 and no change')
    except Exception as error:
        failures.append(repr(error))
        print('FAIL:', repr(error))
    finally:
        if daemon is not None:
            daemon.terminate()
            try:
                daemon.wait(timeout=20)
            except subprocess.TimeoutExpired:
                daemon.kill()
                daemon.wait()
        log.close()
        if had_config:
            shutil.copy2(backup, config)
        elif os.path.exists(config):
            os.unlink(config)
        for name in os.listdir(data_dir):
            if name.startswith('config.json.') and not had_config:
                os.unlink(os.path.join(data_dir, name))
        if not failures:
            shutil.rmtree(work, ignore_errors=True)

    if failures:
        print(f'{len(failures)} check(s) failed; daemon log in {work}')
        return 1
    print('ok')
    return 0


if __name__ == '__main__':
    sys.exit(main())
