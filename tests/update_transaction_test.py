"""Exercise the real updater with temporary paths and fake OS/package commands."""
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(sys.argv.pop(1)).resolve()
MOCK = r'''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
root=Path(os.environ['FIXTURE_ROOT']); scenario=os.environ.get('SCENARIO','success')
state=json.loads((root/'state.json').read_text()); args=sys.argv[1:]; command=Path(sys.argv[0]).name
with (root/'commands.log').open('a') as log: log.write(command+' '+' '.join(args)+'\n')
def save(): (root/'state.json').write_text(json.dumps(state))
def package(path): return json.loads(Path(path).read_text())
if command=='dpkg-deb': print(package(args[1])[args[2]])
elif command=='dpkg-query': print(state['version'])
elif command=='dpkg':
 if '--print-architecture' in args: print('arm64')
 else:
  metadata=package(args[-1]); state['version']=metadata['Version']; state['active']=False
  (root/'data/config.json').write_text('modified config'); (root/'environment').write_text('modified environment'); save()
  if scenario=='interrupted' and state['version']=='1.1.0':
   import signal; os.kill(os.getppid(),signal.SIGTERM)
  if scenario=='install-failure' and state['version']=='1.1.0': sys.exit(1)
  if scenario=='rollback-failure' and state['version']=='1.0.0': sys.exit(1)
elif command=='systemctl':
 action=args[0]
 if action=='is-enabled': print('enabled')
 elif action=='is-active': sys.exit(0 if state['active'] else 3)
 elif action=='show':
  if scenario=='restarting' and state['version']=='1.1.0': state['pid']+=1; save()
  print(state['pid'] if state['active'] else 0)
 elif action=='stop': state['active']=False; save()
 elif action=='mask': state['masked']=True; save()
 elif action=='unmask': state['masked']=False; save()
 elif action=='start':
  state['active']=not state['masked']; state['pid']=100 if state['version']=='1.0.0' else 200; save()
  sys.exit(0 if state['active'] else 1)
elif command=='curl':
 url=next(a for a in args if a.startswith('http'))
 if 'github.com' in url:
  if scenario=='no-rollback': sys.exit(22)
  Path(args[args.index('-o')+1]).write_text(json.dumps({'Package':'led-matrix','Version':'1.0.0','Architecture':'arm64'}))
 elif url.endswith('/api/update/status'):
  version='wrong' if scenario in ('wrong-version','rollback-failure') and state['version']=='1.1.0' else state['version']
  print(json.dumps({'current_version':version}))
 elif url.endswith('/web/') and scenario=='bad-web' and state['version']=='1.1.0': sys.exit(22)
'''


class TransactionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='matrix-update-')
        self.root = Path(self.temporary.name)
        for name in ('bin', 'data', 'cache', 'backup'):
            (self.root / name).mkdir()
        mock = self.root / 'bin/mock'
        mock.write_text(MOCK)
        mock.chmod(0o755)
        for name in ('systemctl', 'dpkg', 'dpkg-query', 'dpkg-deb', 'curl'):
            (mock.parent / name).symlink_to(mock)
        (self.root / 'state.json').write_text(json.dumps({'version': '1.0.0', 'active': True, 'masked': False, 'pid': 100}))
        (self.root / 'data/config.json').write_text('original config')
        (self.root / 'environment').write_text('original environment')
        self.candidate = self.root / 'candidate.deb'
        self.candidate.write_text(json.dumps({'Package': 'led-matrix', 'Version': '1.1.0', 'Architecture': 'arm64'}))
        self.wrapper = self.root / 'run.sh'
        self.wrapper.write_text('source "$1"\nshift\n' + '\n'.join(
            f'{name}="{self.root / path}"' for name, path in
            [('DATA_DIR', 'data'), ('CACHE_DIR', 'cache'), ('BACKUP_DIR', 'backup'), ('ENV_FILE', 'environment'), ('LOG_FILE', 'log')]
        ) + '\nHEALTH_ATTEMPTS=5\nHEALTH_INTERVAL=0\nmain "$@"\n')

    def tearDown(self):
        self.temporary.cleanup()

    def run_update(self, scenario='success', *args):
        return subprocess.run(['bash', str(self.wrapper), str(SCRIPT), *(args or [str(self.candidate)])],
                              env={**os.environ, 'PATH': str(self.root / 'bin') + ':' + os.environ['PATH'],
                                   'FIXTURE_ROOT': str(self.root), 'SCENARIO': scenario},
                              capture_output=True, text=True, timeout=10)

    def assert_preserved(self):
        self.assertEqual((self.root / 'data/config.json').read_text(), 'original config')
        self.assertEqual((self.root / 'environment').read_text(), 'original environment')
        self.assertFalse(json.loads((self.root / 'state.json').read_text())['masked'])

    def test_commit_only_after_health(self):
        result = self.run_update()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.root / 'data/.update_success').exists())
        self.assertFalse((self.root / 'data/.update_error').exists())
        self.assertEqual(json.loads((self.root / 'cache/current.deb').read_text())['Version'], '1.1.0')
        self.assertEqual(json.loads((self.root / 'cache/previous.deb').read_text())['Version'], '1.0.0')
        self.assertGreaterEqual((self.root / 'commands.log').read_text().count('/api/update/status'), 3)
        self.assert_preserved()

    def test_unhealthy_candidate_rolls_back(self):
        for scenario in ('wrong-version', 'bad-web', 'restarting', 'install-failure', 'interrupted'):
            with self.subTest(scenario=scenario):
                result = self.run_update(scenario)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(json.loads((self.root / 'state.json').read_text())['version'], '1.0.0')
                self.assertIn('rolled back to 1.0.0', (self.root / 'data/.update_error').read_text())
                self.assertFalse((self.root / 'data/.update_success').exists())
                self.assert_preserved()

    def test_missing_rollback_package_does_not_install(self):
        result = self.run_update('no-rollback')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('dpkg --force', (self.root / 'commands.log').read_text())
        self.assertEqual(json.loads((self.root / 'state.json').read_text())['version'], '1.0.0')
        self.assert_preserved()

    def test_invalid_candidate_is_rejected(self):
        metadata=json.loads(self.candidate.read_text()); metadata['Package']='another-package'
        self.candidate.write_text(json.dumps(metadata))
        result = self.run_update()
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('dpkg --force', (self.root / 'commands.log').read_text())
        self.assert_preserved()

    def test_rollback_failure_is_reported(self):
        result=self.run_update('rollback-failure')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('rollback package installation failed', (self.root/'data/.update_error').read_text())
        self.assert_preserved()

    def test_parallel_invocation_cannot_change_transaction(self):
        lock=(self.root/'cache/update.lock').open('w')
        fcntl.flock(lock, fcntl.LOCK_EX|fcntl.LOCK_NB)
        (self.root/'data/.update_success').write_text('existing transaction')
        result=self.run_update()
        self.assertNotEqual(result.returncode,0)
        self.assertEqual((self.root/'data/.update_success').read_text(),'existing transaction')
        self.assertFalse((self.root/'commands.log').exists())
        lock.close()


if __name__ == '__main__':
    unittest.main()
