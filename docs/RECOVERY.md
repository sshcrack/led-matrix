# Recovery mode

When the panel cannot run normally, it shows why on the panel itself and
offers a repair page in the browser, so no SSH is needed.

## When it starts

`led-matrix.service` runs the launcher (`scripts/launcher.sh`), which starts
the installed package (`/usr/bin/led-matrix`, never touched by updates) with
`--recovery=<reason>` instead of the normal app when:

| Reason | Detected by the launcher | Panel |
|---|---|---|
| `storage` | `/var/lib/led-matrix` is missing, on the RAM overlay (the data partition did not mount) or not writable (ext4 remounted read-only after errors) | amber drive, `DATA` |
| `crash` | the service started more than 6 times since boot (`/run/led-matrix/starts`) without its API answering once; versions on trial are switched back first ([updates](transactional-updates.md)) | red X, `CRASH` |

The panel shows the reason as an icon and one word, plus the address of the
recovery page (`<ip>:<port>`, split over lines on narrow panels). It shows
`WIFI` instead of the address when the Pi has no network, and a progress bar
while a repair runs. This is an error screen, so the scenes' no-text rule does
not apply. Recovery mode loads no config, plugins or scenes. The panel layout
comes from `/etc/led-matrix/fallback.env` (written by the package, layout
only) when the real environment file on the data partition is gone.

## The recovery page

`http://<ip>:8080/` serves the page. The API needs the same token as normally
(`LED_MATRIX_API_TOKEN`; the page asks for it).

| Button | What happens |
|---|---|
| Repair storage | `e2fsck -fy` on the data partition, mount it again, start the panel |
| Reset storage | copy `config.json`, `config.json.last-good`, the environment file and uploads (up to 100 MiB) to RAM, format the data partition, put them back, start the panel. Without a readable environment file, the layout from `fallback.env` is used and Spotify credentials and the API token must be entered again |
| Install latest version | download the latest release (SHA-256 checked) and install it through the normal updater (needs working storage) |
| Upload config backup | replace `config.json` (needs working storage) |
| Try again | forget the crash count and restart the service normally |
| Download logs | journal of this boot for the led-matrix units, the update log, mounts |

Repair and reset only exist on the [panel card](PI_IMAGE.md) (`LABEL=LEDDATA` in
`/etc/fstab`); they never touch the system or boot partition. Without the label
(destroyed filesystem), reset uses partition 3 of the disk the system runs from.

## How it works

- The recovery daemon (`src_matrix/recovery/`) drops privileges like the
  normal app. It runs in `/`, so it holds nothing open on the data partition.
- Repair, reset and retry need root. The daemon writes the action name to
  `/run/led-matrix-recovery/request` (created by the launcher for the daemon's
  user). `led-matrix-recovery.path` then starts `led-matrix-recovery.service`,
  which runs `/usr/lib/led-matrix/recovery_action.sh`. That script accepts
  only those three names and reports progress in `state.json` next to the
  request.
- Install reuses `UpdateManager`: the package is downloaded to the data
  partition and handed to `led-matrix-update.path` when the daemon exits.

## Tests

- `tests/recovery_smoke.cpp`: reasons, the screen layout and the helper
  protocol.
- `tests/recovery_action_test.py`: the real helper against a simulated card,
  covering repair, failed repair, reset with and without readable data, big
  uploads, the missing label, refusing the system partition and non-card
  systems, and rejecting unknown or linked requests.
- `tests/update_transaction_test.py`: the launcher's recovery decisions
  (missing storage, RAM overlay, crash loop, healthy start clears the count).

To try it in the emulator:
`led-matrix --recovery=storage --led-emulator …` with
`LED_MATRIX_RECOVERY_DIR=<dir>`.
