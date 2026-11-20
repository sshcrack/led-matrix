"""Exercise the real panel hardware helper (scripts/apply_hardware.sh).

usage: apply_hardware_test.py <apply_hardware.sh>

systemctl, curl and debconf-set-selections are fakes on PATH. The fake daemon
"answers" GET /hardware with the managed --led-* flags of the env file as it was
when it was last restarted, unless the scenario says it does not start.
"""
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(sys.argv.pop(1)).resolve()

MANAGED = ('rows', 'cols', 'chain', 'parallel', 'slowdown-gpio', 'gpio-mapping', 'pixel-mapper', 'multiplexing',
           'row-addr-type', 'panel-type', 'pwm-bits', 'pwm-lsb-nanoseconds', 'pwm-dither-bits', 'rgb-sequence')

FAKE = r'''#!/usr/bin/env python3
import json, os, re, sys
from pathlib import Path
root = Path(os.environ['FIXTURE_ROOT']); command = Path(sys.argv[0]).name; args = sys.argv[1:]
with (root / 'commands.log').open('a') as log: log.write(command + ' ' + ' '.join(args) + '\n')
managed = %r
if command == 'systemctl':
    if args[:1] == ['restart']:
        text = (root / 'etc' / 'led-matrix.env').read_text() if (root / 'etc' / 'led-matrix.env').exists() else ''
        m = re.search(r'^MATRIX_OPTS="(.*)"$', text, re.M)
        words = (m.group(1) if m else '').split()
        flags = ' '.join(w for w in words if w.split('=')[0][len('--led-'):] in managed)
        (root / 'running').write_text(flags)
elif command == 'curl':
    scenario = os.environ.get('SCENARIO', '')
    running = (root / 'running').read_text() if (root / 'running').exists() else ''
    if scenario == 'no-start' and 'rows=32' in running: sys.exit(7)
    print(json.dumps({'flags': running}))
elif command == 'debconf-set-selections':
    (root / 'debconf.txt').write_text(sys.stdin.read())
''' % (MANAGED,)

ORIGINAL_ENV = '''# Runtime options for the led-matrix service.
MATRIX_OPTS="--led-rows=64 --led-cols=64 --led-chain=2 --led-parallel=2 --led-drop-priv-user=pi --led-drop-priv-group=pi --led-slowdown-gpio=3 --led-pixel-mapper Rotate:270 --led-no-hardware-pulse"
SPOTIFY_CLIENT_ID="abc"
'''

EXPECTED_FLAGS = ('--led-rows=32 --led-cols=64 --led-chain=2 --led-parallel=2 --led-slowdown-gpio=2 --led-gpio-mapping=regular '
                  '--led-pixel-mapper=Rotate:90 --led-multiplexing=0 --led-row-addr-type=0 --led-pwm-bits=11 '
                  '--led-pwm-lsb-nanoseconds=130 --led-pwm-dither-bits=0 --led-rgb-sequence=RGB')


class ApplyHardwareTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / 'bin').mkdir()
        for name in ('systemctl', 'curl', 'debconf-set-selections'):
            path = self.root / 'bin' / name
            path.write_text(FAKE)
            path.chmod(0o755)
        (self.root / 'etc').mkdir()
        self.env_file = self.root / 'etc' / 'led-matrix.env'
        self.env_file.write_text(ORIGINAL_ENV)
        self.env_file.chmod(0o600)
        os.symlink(self.env_file, self.root / 'default-led-matrix')
        self.dir = self.root / 'hardware'
        self.dir.mkdir()

    def tearDown(self):
        self.tmp.cleanup()

    def run_helper(self, options, scenario='', raw=None):
        request = self.dir / 'request.json'
        request.write_text(raw if raw is not None else json.dumps({'options': options, 'port': '8080'}))
        env = dict(os.environ, PATH=f"{self.root / 'bin'}:{os.environ['PATH']}", FIXTURE_ROOT=str(self.root),
                   SCENARIO=scenario, DIR=str(self.dir), ENV_FILE=str(self.root / 'default-led-matrix'),
                   DAEMON_USER=str(os.getuid()), HEALTH_TIMEOUT='2', HEALTH_INTERVAL='0.1')
        result = subprocess.run(['bash', str(SCRIPT)], env=env, capture_output=True, text=True, timeout=60)
        state = json.loads((self.dir / 'state.json').read_text()) if (self.dir / 'state.json').exists() else None
        return result, state

    def matrix_opts(self):
        for line in self.env_file.read_text().splitlines():
            if line.startswith('MATRIX_OPTS='):
                return line[len('MATRIX_OPTS="'):-1]
        return None

    def commands(self):
        log = self.root / 'commands.log'
        return log.read_text().splitlines() if log.exists() else []

    def test_applies_and_keeps_other_flags_and_lines(self):
        result, state = self.run_helper({'rows': 32, 'cols': 64, 'chain': 2, 'parallel': 2, 'slowdown': 2, 'pixel_mapper': 'Rotate:90'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(state['state'], 'applied')
        self.assertEqual(state['flags'], EXPECTED_FLAGS)
        opts = self.matrix_opts()
        self.assertTrue(opts.startswith(EXPECTED_FLAGS + ' '))
        # Unmanaged flags stay; the old "--led-pixel-mapper Rotate:270" pair is gone.
        self.assertIn('--led-drop-priv-user=pi --led-drop-priv-group=pi --led-no-hardware-pulse', opts)
        self.assertNotIn('Rotate:270', opts)
        self.assertNotIn('--led-slowdown-gpio=3', opts)
        self.assertIn('SPOTIFY_CLIENT_ID="abc"', self.env_file.read_text())
        self.assertEqual(stat.S_IMODE(self.env_file.stat().st_mode), 0o600)
        self.assertTrue((self.root / 'default-led-matrix').is_symlink(), 'the link must stay a link')
        self.assertFalse((self.dir / 'request.json').exists())
        self.assertIn('systemctl restart led-matrix.service', self.commands())
        debconf = (self.root / 'debconf.txt').read_text()
        self.assertIn('led-matrix/led-rows string 32', debconf)
        self.assertIn('led-matrix/led-slowdown-gpio string 2', debconf)
        self.assertIn('led-matrix/extra-opts string --led-gpio-mapping=regular --led-pixel-mapper=Rotate:90', debconf)
        self.assertNotIn('drop-priv', debconf)

    def test_rejects_invalid_requests_without_touching_anything(self):
        for options, raw in [({'rows': 7}, None), ({'rows': '64'}, None), ({'colour': 1}, None),
                             ({'pixel_mapper': 'Rotate:45'}, None), ({'pixel_mapper': 'Rotate:90;$(reboot)'}, None),
                             ({'gpio_mapping': 'regular --led-daemon=1'}, None), ({'pwm_bits': 1.5}, None),
                             (None, 'not json'), (None, json.dumps({'options': 'rows=64'}))]:
            with self.subTest(options=options, raw=raw):
                result, state = self.run_helper(options, raw=raw)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(state['state'], 'failed')
                self.assertEqual(self.env_file.read_text(), ORIGINAL_ENV)
                self.assertNotIn('systemctl restart led-matrix.service', self.commands())
                self.assertFalse((self.dir / 'request.json').exists())

    def test_restores_previous_settings_when_the_daemon_does_not_come_back(self):
        result, state = self.run_helper({'rows': 32, 'chain': 2, 'parallel': 2}, scenario='no-start')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(state['state'], 'failed')
        self.assertIn('restored', state['message'])
        self.assertEqual(self.env_file.read_text(), ORIGINAL_ENV)
        self.assertEqual(self.commands().count('systemctl restart led-matrix.service'), 2)

    def test_adds_missing_drop_privilege_flags(self):
        self.env_file.write_text('MATRIX_OPTS="--led-rows=64"\n')
        result, state = self.run_helper({'rows': 64, 'cols': 64})
        self.assertEqual(result.returncode, 0, result.stderr)
        uid = str(os.getuid())
        self.assertTrue(self.matrix_opts().endswith(f'--led-drop-priv-user={uid} --led-drop-priv-group={uid}'))

    def test_ignores_a_linked_request(self):
        target = self.root / 'secret'
        target.write_text('{"options": {}}')
        os.symlink(target, self.dir / 'request.json')
        env = dict(os.environ, PATH=f"{self.root / 'bin'}:{os.environ['PATH']}", FIXTURE_ROOT=str(self.root),
                   DIR=str(self.dir), ENV_FILE=str(self.env_file))
        subprocess.run(['bash', str(SCRIPT)], env=env, capture_output=True, text=True, timeout=30)
        self.assertEqual(self.env_file.read_text(), ORIGINAL_ENV)
        self.assertEqual(self.commands(), [])


if __name__ == '__main__':
    unittest.main()
