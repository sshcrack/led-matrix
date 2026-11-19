# Transactional updates

Automatic updates, the Updates page and `build_upload.sh` run
`scripts/update_service.sh` in a separate systemd unit, so restarting
`led-matrix.service` cannot terminate it. It does not run `dpkg`: each update
is unpacked as its own version below `/var/lib/led-matrix-app` (on the
[panel card](PI_IMAGE.md), the data partition) and the package installed with
`dpkg` stays untouched as the factory copy.

| Path in `/var/lib/led-matrix-app` (root) | Purpose |
|---|---|
| `versions/<version>-<hash>/` | a complete install tree (`usr/bin/led-matrix`, libraries, plugins, web UI) plus `.version` |
| `current` → `versions/…` | the version that runs |
| `previous` → `versions/…` | what `current` replaced; missing means the factory copy |
| `trial` | `<version dir> <starts>` while a new version has not yet been confirmed healthy |
| `trial-config.json` | `config.json` from before the switch, restored on a switch back |

`led-matrix.service` starts `/usr/lib/led-matrix/launcher run $MATRIX_OPTS`
(`scripts/launcher.sh`), which execs `current`, else `previous`, else
`/usr/bin/led-matrix`. The binary finds its libraries, plugins and
`share/led-matrix` files relative to itself (`get_share_dir()`), so a version
folder works anywhere.

## One update

1. Lock out concurrent updates; check the package name, version and
   architecture; refuse when `/var/lib/led-matrix-app` is on the RAM overlay
   (data partition missing).
2. Unpack into `versions/.partial-*`, sync, rename to `versions/<version>-<hash>`.
   Older versions are pruned first (only `current` and `previous` stay), and the
   space is checked against the package's `Installed-Size`. The same build
   already running is a no-op; a rebuild of the same version gets a new folder.
3. Stop the service, snapshot `config.json`, write `trial`, then switch
   `previous` and `current` with `rename(2)`.
4. Start the service. Success requires three consecutive checks with the same
   service PID: the unit is active, `/api/update/status` reports the candidate
   version and `/web/` answers (with `LED_MATRIX_API_TOKEN` when set; the unit
   reads `/etc/default/led-matrix`). The health port is the daemon's `PORT`,
   passed in the request file.
5. Commit: remove `trial`, prune, write `.update_success`.

A failed health check, a failed start or a caught SIGINT/SIGTERM after the
switch runs `launcher rollback <reason>`: `current` goes back to `previous` (or
is removed, which means the factory copy), `config.json` is restored, and
`.update_error` says `<reason>; rolled back to <version>`. Failures before the
switch only restart the old version and write `.update_error`.

## Power cuts and crash loops

Every step before the switch leaves the old version in place; the switch is one
rename. If the updater dies after it (power cut, OOM), the launcher finishes the
job on the next starts: it counts starts of the version on trial in `trial`
and, at the fourth start (`MAX_TRIAL_STARTS=3`; `RestartSec=3` keeps a crash
loop within systemd's start limit), switches back the same way. While a version
is on trial and no updater holds the lock, the launcher also confirms it: three
healthy checks in a row remove `trial`. A `trial` that does not match `current`
(cut between writing it and the switch) is dropped.

Installing a package with `dpkg`/`apt` means "run this package": `postinst`
removes the updated versions (not on `dpkg-reconfigure`). `apt purge` also
removes `/var/lib/led-matrix-app`.

## Tests and recovery

`tests/update_transaction_test.py` (ctest `update_transaction_test`) runs the
real updater and launcher with temporary paths and fake package/service/HTTP
commands; its fake `systemctl start` runs the real launcher, which execs fake
version binaries. It covers commit, rollback for each health failure, crash
loop and interrupted transactions, the fallback order, trial confirmation and
pruning:

```bash
python3 tests/update_transaction_test.py scripts/update_service.sh scripts/launcher.sh
```

On manual recovery, inspect `journalctl -u led-matrix-update` (or
`led-matrix-deploy`), `journalctl -u led-matrix` (`[launcher]` lines) and
`/var/log/led-matrix-update.log`. To go back to the factory copy, remove
`/var/lib/led-matrix-app/current` and restart the service.
