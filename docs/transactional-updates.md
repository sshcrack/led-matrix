# Transactional updates

Automatic updates and `build_upload.sh` run the updater in a separate systemd
unit. Restarting `led-matrix.service` therefore cannot terminate its installer.

Before changing packages, the updater validates package identity/architecture,
locks out concurrent updates, and retains the installed version's DEB. It uses
`/var/cache/led-matrix/current.deb` when available; the first transaction fetches
the matching published release. If a rollback package cannot be obtained, the
candidate is not installed and the old service is restarted.

The transaction snapshots `/etc/default/led-matrix` and
`/var/lib/led-matrix/config.json` under `/var/backups/led-matrix/current/`.
These backups and the package cache are root-only. User data is not deleted.
A temporary runtime service mask prevents maintainer scripts from starting a
partially configured daemon. Configuration is restored before starting either
version, including display-off state and hardware/Spotify settings.

Success requires three consecutive checks with the same service PID: the unit
must be active, `/api/update/status` must report the candidate version, and
`/web/` must return success. No live frame capture is requested. The default
health port is 8080; the daemon forwards its `PORT` to the updater through
`LED_MATRIX_UPDATE_PORT`.

Installation failure, health failure, or a caught SIGINT/SIGTERM triggers installation of
`previous.deb`, restoration of the configuration snapshot, and checks of the old
version. Only a validated candidate produces `.update_success`. A failed update
produces `.update_error`, explicitly distinguishing a successful rollback from a
rollback requiring manual recovery. The Updates API consumes these completion
flags after the daemon has restarted.

The cache retains at most the current and previous package plus a staged
candidate during installation. The snapshot is replaced on the next transaction.
On manual recovery, inspect `journalctl -u led-matrix-update` (or
`led-matrix-deploy`) and `/var/log/led-matrix-update.log`.

Tests source the real script with temporary paths and fake system/package/HTTP
commands, so they exercise health checks and rollback without changing the host.
Run `python3 tests/update_transaction_test.py scripts/update_service.sh`.
