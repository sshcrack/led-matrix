"""Exercise the real recovery helper against a simulated panel card.

usage: recovery_action_test.py <recovery_action.sh>

The data partition is a directory (`device/`); fake mount/umount/mountpoint
turn the mount points into links to it, e2fsck/mkfs.ext4 act on it.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(sys.argv.pop(1)).resolve()

MOCK = r'''#!/usr/bin/env python3
import json, os, shutil, sys
from pathlib import Path
root = Path(os.environ['FIXTURE_ROOT']); scenario = os.environ.get('SCENARIO', '')
args = sys.argv[1:]; command = Path(sys.argv[0]).name
with (root / 'commands.log').open('a') as log: log.write(command + ' ' + ' '.join(args) + '\n')
device = root / 'device'
targets = {str(root / 'srv'): device, str(root / 'data'): device / 'state', str(root / 'app'): device / 'app'}
def link(path, target):
    path = Path(path)
    if path.is_symlink(): path.unlink()
    elif path.exists(): path.rmdir()
    path.symlink_to(target)
def unlink(path):
    path = Path(path)
    if path.is_symlink(): path.unlink(); path.mkdir()
if command == 'blkid':
    if scenario != 'no-label': print('/dev/fake3')
    else: sys.exit(2)
elif command == 'lsblk':
    if '-no' in args: print('fake')
    else: print('/dev/fake 0\n/dev/fake1 1\n/dev/fake2 2\n/dev/fake3 3')
elif command == 'findmnt':
    target = args[-1]
    print({'/media/root-ro': '/dev/fake2', '/boot/firmware': '/dev/fake1'}.get(target, '/dev/fake2'))
elif command == 'mountpoint': sys.exit(0 if Path(args[-1]).is_symlink() else 1)
elif command == 'umount':
    if scenario == 'busy' and args[-1] == str(root / 'data'): sys.exit(32)
    unlink(args[-1])
elif command == 'mount':
    if '-o' in args:  # mount -o ro <device> <dir>: the old filesystem for the rescue copy
        if scenario == 'unreadable': sys.exit(32)
        link(args[-1], device)
    else:
        target = args[-1]
        if scenario == 'unmountable': sys.exit(32)
        if target != str(root / 'srv') and not device.exists(): sys.exit(32)
        targets[target].mkdir(parents=True, exist_ok=True)
        link(target, targets[target])
        if scenario == 'readonly' and target == str(root / 'data'): (device / 'state').chmod(0o555)
elif command == 'e2fsck': sys.exit(int(os.environ.get('E2FSCK_RC', '1')))
elif command == 'mkfs.ext4':
    if device.exists(): (device / 'state').exists() and (device / 'state').chmod(0o755); shutil.rmtree(device)
    device.mkdir(); (root / 'label').write_text(args[args.index('-L') + 1])
elif command == 'systemctl': pass
'''


class RecoveryActionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='matrix-recovery-')
        self.root = Path(self.temporary.name)
        for name in ('bin', 'run', 'srv', 'data', 'app', 'counter'):
            (self.root / name).mkdir()
        mock = self.root / 'bin/mock'
        mock.write_text(MOCK)
        mock.chmod(0o755)
        for name in ('blkid', 'lsblk', 'findmnt', 'mountpoint', 'umount', 'mount', 'e2fsck', 'mkfs.ext4', 'systemctl'):
            (mock.parent / name).symlink_to(mock)
        self.fstab = self.root / 'fstab'
        self.fstab.write_text('LABEL=LEDDATA  /srv/led-matrix  ext4  defaults,nofail  0  2\n')
        (self.root / 'fallback.env').write_text('MATRIX_OPTS="--led-rows=64"\n')
        (self.root / 'counter/starts').write_text('7\n')
        # The card's data partition with settings, uploads and the environment file.
        device = self.root / 'device'
        (device / 'state/data').mkdir(parents=True)
        (device / 'etc').mkdir()
        (device / 'app').mkdir()
        (device / 'state/config.json').write_text('{"presets":{}}')
        (device / 'state/config.json.last-good').write_text('{"last":"good"}')
        (device / 'state/data/upload.png').write_bytes(b'png')
        (device / 'etc/led-matrix.env').write_text('MATRIX_OPTS="--led-rows=32"\nLED_MATRIX_API_TOKEN="secret"\n')
        for mount, target in (('srv', device), ('data', device / 'state'), ('app', device / 'app')):
            (self.root / mount).rmdir()
            (self.root / mount).symlink_to(target)
        self.wrapper = self.root / 'run.sh'
        self.wrapper.write_text('source "$1"\nis_block_device() { [[ $1 == /dev/fake* ]]; }\nmain\n')

    def tearDown(self):
        state = self.root / 'device/state'
        if state.exists():
            state.chmod(0o755)
        self.temporary.cleanup()

    def run_action(self, action, scenario='', **env):
        request = self.root / 'run/request'
        request.write_text(action + '\n')
        return subprocess.run(['bash', str(self.wrapper), str(SCRIPT)], capture_output=True, text=True, timeout=20, env={
            **os.environ, 'PATH': f"{self.root / 'bin'}:{os.environ['PATH']}", 'FIXTURE_ROOT': str(self.root),
            'SCENARIO': scenario, 'RUN_DIR': str(self.root / 'run'), 'WORK_DIR': str(self.root / 'work'),
            'COUNTER': str(self.root / 'counter/starts'), 'DATA_MOUNT': str(self.root / 'srv'),
            'DATA_DIR': str(self.root / 'data'), 'APP_DIR': str(self.root / 'app'), 'FSTAB': str(self.fstab),
            'FALLBACK_ENV': str(self.root / 'fallback.env'), 'DAEMON_USER': os.environ.get('USER', 'root'), **env})

    def state(self):
        return json.loads((self.root / 'run/state.json').read_text())

    def commands(self):
        log = self.root / 'commands.log'
        return log.read_text() if log.exists() else ''

    def assert_restarted(self, restarted=True):
        self.assertEqual('systemctl restart --no-block led-matrix.service' in self.commands(), restarted)
        self.assertEqual((self.root / 'counter/starts').exists(), not restarted)

    def assert_mounted(self):
        for mount in ('srv', 'data', 'app'):
            self.assertTrue((self.root / mount).is_symlink(), mount)

    def test_repair(self):
        result = self.run_action('repair', E2FSCK_RC='1')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.state()['state'], 'done')
        self.assertIn('e2fsck -fy /dev/fake3', self.commands())
        self.assert_mounted()
        self.assert_restarted()
        self.assertFalse((self.root / 'run/request').exists())

    def test_repair_that_cannot_fix_suggests_reset(self):
        result = self.run_action('repair', E2FSCK_RC='4')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.state()['state'], 'failed')
        self.assertIn('try Reset storage', self.state()['message'])
        self.assert_restarted(False)

    def check_repair_failure(self, scenario, message):
        result = self.run_action('repair', scenario)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.state()['state'], 'failed')
        self.assertIn(message, self.state()['message'])
        self.assert_restarted(False)

    def test_repair_with_busy_mount(self):
        self.check_repair_failure('busy', 'in use')

    def test_repair_of_unmountable_partition(self):
        self.check_repair_failure('unmountable', 'cannot be mounted')

    def test_repair_that_stays_read_only(self):
        self.check_repair_failure('readonly', 'read-only')

    def test_reset_keeps_settings_uploads_and_environment(self):
        result = self.run_action('reset')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('mkfs.ext4 -F -q -L LEDDATA /dev/fake3', self.commands())
        self.assertEqual((self.root / 'label').read_text(), 'LEDDATA')
        device = self.root / 'device'
        self.assertEqual((device / 'state/config.json').read_text(), '{"presets":{}}')
        self.assertEqual((device / 'state/config.json.last-good').read_text(), '{"last":"good"}')
        self.assertEqual((device / 'state/data/upload.png').read_bytes(), b'png')
        self.assertIn('secret', (device / 'etc/led-matrix.env').read_text())
        self.assertEqual((device / 'etc/led-matrix.env').stat().st_mode & 0o777, 0o600)
        self.assertTrue((device / 'app').is_dir())
        self.assertIn('4 settings items kept', self.state()['message'])
        self.assertFalse((self.root / 'work').exists())
        self.assert_mounted()
        self.assert_restarted()

    def test_reset_of_unreadable_partition_uses_the_fallback_layout(self):
        result = self.run_action('reset', 'unreadable')
        self.assertEqual(result.returncode, 0, result.stderr)
        device = self.root / 'device'
        self.assertFalse((device / 'state/config.json').exists())
        self.assertEqual((device / 'etc/led-matrix.env').read_text(), 'MATRIX_OPTS="--led-rows=64"\n')
        self.assertIn('0 settings items kept', self.state()['message'])
        self.assert_restarted()

    def test_reset_without_label_finds_partition_three(self):
        result = self.run_action('reset', 'no-label')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('mkfs.ext4 -F -q -L LEDDATA /dev/fake3', self.commands())

    def test_big_uploads_stay_behind(self):
        result = self.run_action('reset', RESCUE_UPLOADS_MIB='0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.root / 'device/state/data/upload.png').exists())
        self.assertTrue((self.root / 'device/state/config.json').exists())

    def test_only_on_the_panel_card(self):
        self.fstab.write_text('PARTUUID=1234-02  /  ext4  defaults  0  1\n')
        for action in ('repair', 'reset'):
            with self.subTest(action=action):
                result = self.run_action(action)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("panel card", self.state()['message'])
                self.assertNotIn('mkfs', self.commands())
                self.assertNotIn('e2fsck', self.commands())

    def test_never_formats_the_system(self):
        # A system partition reported as the data partition is refused.
        wrapper = self.root / 'run.sh'
        wrapper.write_text('source "$1"\nis_block_device() { [[ $1 == /dev/fake* ]]; }\n'
                           'blkid() { echo /dev/fake2; }\nmain\n')
        result = self.run_action('reset')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('No data partition found', self.state()['message'])
        self.assertNotIn('mkfs', self.commands())

    def test_retry(self):
        result = self.run_action('retry')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assert_restarted()

    def test_rejects_unknown_and_linked_requests(self):
        result = self.run_action('rm -rf /')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.commands(), '')
        (self.root / 'run/request').unlink(missing_ok=True)
        target = self.root / 'elsewhere'
        target.write_text('reset\n')
        (self.root / 'run/request').symlink_to(target)
        result = subprocess.run(['bash', str(self.wrapper), str(SCRIPT)], capture_output=True, text=True, timeout=20,
                                env={**os.environ, 'PATH': f"{self.root / 'bin'}:{os.environ['PATH']}",
                                     'FIXTURE_ROOT': str(self.root), 'RUN_DIR': str(self.root / 'run')})
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.commands(), '')


if __name__ == '__main__':
    unittest.main()
