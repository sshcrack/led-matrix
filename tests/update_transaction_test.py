"""Exercise the real updater and launcher with temporary paths and fake OS/package commands.

usage: update_transaction_test.py <update_service.sh> <launcher.sh>

The fake `systemctl start` runs the real launcher, which execs the fake
binary of the version it picks; that binary records itself as running.
"""
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

LAUNCHER = Path(sys.argv.pop(2)).resolve()
SCRIPT = Path(sys.argv.pop(1)).resolve()

MOCK = r'''#!/usr/bin/env python3
import json, os, signal, subprocess, sys
from pathlib import Path
root = Path(os.environ['FIXTURE_ROOT']); scenario = os.environ.get('SCENARIO', 'success')
args = sys.argv[1:]; command = Path(sys.argv[0]).name
with (root / 'commands.log').open('a') as log: log.write(command + ' ' + ' '.join(args) + '\n')
def load(): return json.loads((root / 'state.json').read_text())
def save(state): (root / 'state.json').write_text(json.dumps(state))
def package(path): return json.loads(Path(path).read_text())
def interrupt(): os.kill(int(os.environ['UPDATER_PID']), signal.SIGTERM)
FAKE_BINARY = """#!/bin/bash
# Fake led-matrix {version}: record that it runs (the launcher execs this).
python3 - "$@" <<'EOF'
import json, os, sys
from pathlib import Path
root = Path(os.environ['FIXTURE_ROOT'])
state = json.loads((root / 'state.json').read_text())
state['running'] = '{version}'; state['args'] = sys.argv[1:]
(root / 'state.json').write_text(json.dumps(state))
config = root / 'data/config.json'
if '{version}' != '1.0.0' and not config.read_text().endswith(' {version}'):
    config.write_text('config migrated by {version}')  # a format the old version cannot read
EOF
"""
if command == 'dpkg-deb':
    if args[0] == '-x':
        metadata = package(args[1]); target = Path(args[2])
        (target / 'usr/bin').mkdir(parents=True)
        exe = target / 'usr/bin/led-matrix'
        exe.write_text(FAKE_BINARY.replace('{version}', metadata['Version'])); exe.chmod(0o755)
        if scenario == 'interrupted-unpack': interrupt()
    else:
        print(package(args[1])[args[2]])
elif command == 'dpkg-query': print('1.0.0')
elif command == 'dpkg': print('arm64')
elif command == 'findmnt': print('overlay' if scenario == 'no-data-partition' else 'ext4')
elif command == 'df':
    avail = 1000 if scenario == 'no-space' else 10_000_000
    print('Filesystem 1024-blocks Used Available Capacity Mounted on')
    print(f'/dev/fake 20000000 0 {avail} 0% /')
elif command == 'systemctl':
    state = load(); action = args[0]
    if action == 'is-enabled': print('enabled')
    elif action == 'is-active':
        crashed = scenario == 'crash' and state['running'] != '1.0.0'
        sys.exit(0 if state['active'] and not crashed else 3)
    elif action == 'show':
        if scenario == 'restarting' and state['running'] != '1.0.0': state['pid'] += 1; save(state)
        print(state['pid'] if state['active'] else 0)
    elif action == 'stop': state['active'] = False; state['running'] = None; save(state)
    elif action == 'start':
        if state['active']: sys.exit(0)
        result = subprocess.run(['bash', os.environ['LAUNCHER'], 'run', '--led-test'], env=os.environ)
        state = load(); state['active'] = result.returncode == 0; state['pid'] += 100; save(state)
        sys.exit(0 if state['active'] else 1)
elif command == 'curl':
    state = load(); url = next(a for a in args if a.startswith('http'))
    if url.endswith('/api/update/status'):
        version = state['running']
        if scenario == 'wrong-version' and version != '1.0.0': version = 'wrong'
        if scenario == 'interrupted' and version != '1.0.0': interrupt()
        print(json.dumps({'current_version': version}))
    elif url.endswith('/web/') and scenario == 'bad-web' and state['running'] != '1.0.0': sys.exit(22)
'''


class Fixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='matrix-update-')
        self.root = Path(self.temporary.name)
        for name in ('bin', 'data', 'app'):
            (self.root / name).mkdir()
        mock = self.root / 'bin/mock'
        mock.write_text(MOCK)
        mock.chmod(0o755)
        for name in ('systemctl', 'dpkg', 'dpkg-query', 'dpkg-deb', 'curl', 'findmnt', 'df'):
            (mock.parent / name).symlink_to(mock)
        (self.root / 'state.json').write_text(json.dumps({'running': None, 'active': False, 'pid': 100}))
        (self.root / 'data/config.json').write_text('original config')
        factory = self.root / 'factory'
        factory.write_text(MOCK.split('FAKE_BINARY = """')[1].split('"""')[0].replace('{version}', '1.0.0'))
        factory.chmod(0o755)
        self.env = {**os.environ, 'PATH': f"{self.root / 'bin'}:{os.environ['PATH']}",
                    'FIXTURE_ROOT': str(self.root), 'LAUNCHER': str(LAUNCHER),
                    'APP_DIR': str(self.root / 'app'), 'DATA_DIR': str(self.root / 'data'),
                    'FACTORY': str(factory), 'CONFIRM_ATTEMPTS': '0'}
        self.env.pop('LED_MATRIX_API_TOKEN', None)

    def tearDown(self):
        self.temporary.cleanup()

    def state(self):
        return json.loads((self.root / 'state.json').read_text())

    def start(self, **env):
        """systemctl start, as led-matrix.service would."""
        subprocess.run([str(self.root / 'bin/systemctl'), 'start'], env={**self.env, **env}, check=True, timeout=10)

    def stop(self):
        subprocess.run([str(self.root / 'bin/systemctl'), 'stop'], env=self.env, check=True, timeout=10)

    def app(self, name):
        return self.root / 'app' / name

    def link(self, name):
        return os.readlink(self.app(name)) if self.app(name).is_symlink() else None


class UpdaterTests(Fixture):
    def setUp(self):
        super().setUp()
        self.start()
        self.wrapper = self.root / 'run.sh'
        self.wrapper.write_text('source "$1"\nshift\n' + '\n'.join(
            f'{name}="{value}"' for name, value in
            [('APP_DIR', self.root / 'app'), ('DATA_DIR', self.root / 'data'), ('LOG_FILE', self.root / 'log'),
             ('LAUNCHER', LAUNCHER)]
        ) + '\nexport UPDATER_PID=$$\nHEALTH_ATTEMPTS=5\nHEALTH_INTERVAL=0\nmain "$@"\n')

    def deb(self, version='1.1.0', package='led-matrix', build=''):
        path = self.root / f'led-matrix-{version}{build}.deb'
        path.write_text(json.dumps({'Package': package, 'Version': version, 'Architecture': 'arm64',
                                    'Installed-Size': 1000, 'Build': build}))
        return path

    def run_update(self, scenario='success', deb=None):
        return subprocess.run(['bash', str(self.wrapper), str(SCRIPT), str(deb or self.deb())],
                              env={**self.env, 'SCENARIO': scenario}, capture_output=True, text=True, timeout=20)

    def flag(self, name):
        path = self.root / 'data' / name
        return path.read_text() if path.exists() else None

    def assert_back_on_factory(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.state()['running'], '1.0.0')
        self.assertTrue(self.state()['active'])
        self.assertIsNone(self.link('current'))
        self.assertFalse(self.app('trial').exists())
        self.assertIsNone(self.flag('.update_success'))
        self.assertEqual((self.root / 'data/config.json').read_text(), 'original config')

    def test_commit_only_after_health(self):
        result = self.run_update()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.state()['running'], '1.1.0')
        self.assertEqual(self.state()['args'], ['--led-test'])
        self.assertRegex(self.link('current'), r'^versions/1\.1\.0-[0-9a-f]{12}$')
        self.assertIsNone(self.link('previous'))  # the factory install is the fallback
        self.assertFalse(self.app('trial').exists())
        self.assertFalse(self.app('trial-config.json').exists())
        self.assertIsNotNone(self.flag('.update_success'))
        self.assertIsNone(self.flag('.update_error'))
        self.assertGreaterEqual((self.root / 'commands.log').read_text().count('/api/update/status'), 3)
        self.assertEqual(sorted(p.name for p in (self.root / 'app/versions').iterdir()), [self.link('current')[9:]])

    def test_next_update_keeps_one_previous_version(self):
        self.assertEqual(self.run_update(deb=self.deb('1.1.0')).returncode, 0)
        first = self.link('current')
        self.assertEqual(self.run_update(deb=self.deb('1.2.0')).returncode, 0)
        second = self.link('current')
        self.assertEqual(self.link('previous'), first)
        self.assertEqual(self.run_update(deb=self.deb('1.3.0')).returncode, 0)
        self.assertEqual(self.link('previous'), second)
        self.assertEqual(sorted(p.name for p in (self.root / 'app/versions').iterdir()),
                         sorted([second[9:], self.link('current')[9:]]))

    def test_unhealthy_candidate_rolls_back(self):
        for scenario in ('wrong-version', 'bad-web', 'restarting', 'crash', 'interrupted'):
            with self.subTest(scenario=scenario):
                result = self.run_update(scenario)
                self.assert_back_on_factory(result)
                self.assertIn('rolled back to 1.0.0', self.flag('.update_error'))

    def test_failed_second_update_returns_to_first(self):
        self.assertEqual(self.run_update(deb=self.deb('1.1.0')).returncode, 0)
        first = self.link('current')
        (self.root / 'data/config.json').write_text('config of 1.1.0')
        result = self.run_update('wrong-version', deb=self.deb('1.2.0'))
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.link('current'), first)
        self.assertEqual(self.state()['running'], '1.1.0')
        self.assertEqual((self.root / 'data/config.json').read_text(), 'config of 1.1.0')
        self.assertIn('rolled back to 1.1.0', self.flag('.update_error'))

    def test_rejected_before_switch(self):
        cases = [
            ('no-space', {}, 'Not enough space'),
            ('no-data-partition', {}, 'not on persistent storage'),
            ('success', {'package': 'another-package'}, 'Wrong package name'),
            ('interrupted-unpack', {}, 'Update failed'),
        ]
        for scenario, package, message in cases:
            with self.subTest(scenario=scenario):
                result = self.run_update(scenario, self.deb(build=scenario, **package))
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, self.flag('.update_error'))
                self.assertIsNone(self.link('current'))
                self.assertEqual(self.state()['running'], '1.0.0')
                self.assertEqual(list((self.root / 'app/versions').iterdir()), [])
                self.assertFalse(self.app('candidate.deb').exists())

    def test_same_build_again_is_a_no_op(self):
        deb = self.deb()
        self.assertEqual(self.run_update(deb=deb).returncode, 0)
        current = self.link('current')
        self.stop()  # the daemon exits after queueing an update
        result = self.run_update(deb=deb)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.link('current'), current)
        self.assertEqual(self.state()['running'], '1.1.0')
        self.assertIsNotNone(self.flag('.update_success'))

    def test_rebuilt_same_version_is_installed(self):
        self.assertEqual(self.run_update(deb=self.deb(build='a')).returncode, 0)
        first = self.link('current')
        self.assertEqual(self.run_update(deb=self.deb(build='b')).returncode, 0)
        self.assertNotEqual(self.link('current'), first)
        self.assertEqual(self.link('previous'), first)

    def test_parallel_invocation_cannot_change_transaction(self):
        lock = self.app('update.lock').open('w')
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        (self.root / 'data/.update_success').write_text('existing transaction')
        result = self.run_update()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.flag('.update_success'), 'existing transaction')
        self.assertIsNone(self.link('current'))
        lock.close()


class LauncherTests(Fixture):
    def install(self, version, link=None, complete=True):
        target = self.root / 'app/versions' / f'{version}-abc'
        (target / 'usr/bin').mkdir(parents=True)
        exe = target / 'usr/bin/led-matrix'
        exe.write_text((self.root / 'factory').read_text().replace('1.0.0', version))
        exe.chmod(0o755)
        if complete:
            (target / '.version').write_text(version + '\n')
        if link:
            os.symlink(f'versions/{version}-abc', self.app(link))
        return f'versions/{version}-abc'

    def test_fallback_order(self):
        self.start()
        self.assertEqual(self.state()['running'], '1.0.0')
        self.stop()
        self.install('1.1.0', 'previous')
        self.start()
        self.assertEqual(self.state()['running'], '1.1.0')
        self.stop()
        self.install('1.2.0', 'current', complete=False)
        self.start()
        self.assertEqual(self.state()['running'], '1.1.0')
        self.stop()
        os.unlink(self.app('current'))
        self.install('1.3.0', 'current')
        self.start()
        self.assertEqual(self.state()['running'], '1.3.0')

    def test_version_failing_to_start_is_switched_back(self):
        old = self.install('1.1.0', 'previous')
        new = self.install('1.2.0', 'current')
        self.app('trial').write_text(f'{new} 0\n')
        self.app('trial-config.json').write_text('config before 1.2.0')
        for starts in (1, 2, 3):
            self.start()
            self.assertEqual(self.state()['running'], '1.2.0')
            self.assertEqual(self.app('trial').read_text().split(), [new, str(starts)])
            self.stop()
        self.start()  # e.g. three crashes or power cuts in a row
        self.assertEqual(self.state()['running'], '1.1.0')
        self.assertEqual(self.link('current'), old)
        self.assertFalse(self.app('trial').exists())
        self.assertIn('did not start healthy 3 times; rolled back to 1.1.0',
                      (self.root / 'data/.update_error').read_text())
        self.assertEqual((self.root / 'data/config.json').read_text(), 'config before 1.2.0')

    def test_stale_trial_is_dropped(self):
        # Power cut after writing the trial, before switching `current`.
        current = self.install('1.1.0', 'current')
        self.app('trial').write_text('versions/1.2.0-abc 2\n')
        self.start()
        self.assertEqual(self.state()['running'], '1.1.0')
        self.assertEqual(self.link('current'), current)
        self.assertFalse(self.app('trial').exists())

    def test_healthy_version_on_trial_is_kept(self):
        new = self.install('1.2.0', 'current')
        self.app('trial').write_text(f'{new} 0\n')
        self.start(CONFIRM_ATTEMPTS='20', CONFIRM_INTERVAL='0.05')
        deadline = time.monotonic() + 10
        while self.app('trial').exists() and time.monotonic() < deadline:
            time.sleep(0.05)
        self.assertFalse(self.app('trial').exists())
        self.assertEqual(self.link('current'), new)

    def test_running_update_decides_itself(self):
        new = self.install('1.2.0', 'current')
        self.app('trial').write_text(f'{new} 0\n')
        lock = self.app('update.lock').open('w')
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        self.start(CONFIRM_ATTEMPTS='5', CONFIRM_INTERVAL='0.05')
        time.sleep(1.5)
        self.assertTrue(self.app('trial').exists())
        lock.close()

    def test_rollback_without_trial_does_nothing(self):
        current = self.install('1.2.0', 'current')
        self.install('1.1.0', 'previous')
        subprocess.run(['bash', str(LAUNCHER), 'rollback', 'reason'], env=self.env, check=True, timeout=10)
        self.assertEqual(self.link('current'), current)
        self.assertFalse((self.root / 'data/.update_error').exists())


if __name__ == '__main__':
    unittest.main()
