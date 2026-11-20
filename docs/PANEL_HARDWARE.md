# Panel hardware settings

The **Panel hardware** card (web UI → Integrations) changes the `--led-*` options the
daemon starts with: panel size, chain, parallel chains, rotation, wiring
(`--led-gpio-mapping`), GPIO slowdown and, under *Advanced*, pixel mapper,
multiplexing, row address type, driver chip, colour order and PWM settings.

## Test pattern

*Show test pattern* runs the `panel_test` scene (plugin `PanelSetup`) through Scene Lab
for as long as the switch is on:

| Look for | Wrong means |
|---|---|
| red, green, yellow, blue corners clockwise from the top-left; white arrow pointing up | rotation or mirroring |
| white frame around the whole picture | rows/cols/chain/parallel |
| straight magenta diagonal, grey grid every 32 px | multiplexing or row address type |
| cyan line sweeping left to right across all panels | order of the chained panels |
| smooth grey ramp at the bottom | colour depth / colour order |

## How a change is applied

The daemon runs as `pi` and cannot write `/etc/default/led-matrix`, so:

1. `POST /hardware {"options": {...}}` validates the options
   (`src_matrix/hardware/panel_options.cpp`: fixed ranges and lists, then the library's
   `Options::Validate`) and writes `/var/lib/led-matrix/hardware/request.json`.
   The options it already runs with only confirm them (no restart).
2. `led-matrix-hardware.path` starts `led-matrix-hardware.service`, which runs
   `/usr/lib/led-matrix/apply_hardware.sh` as root. It validates the request again,
   replaces the managed flags in `MATRIX_OPTS` (every other flag, such as
   `--led-drop-priv-*` or `--led-no-hardware-pulse`, stays), updates the debconf answers
   so `dpkg-reconfigure` keeps them, and restarts the service.
3. It waits up to 90 s for `GET /hardware` to report the new flags. Otherwise it restores
   the previous file and restarts again. The result is
   `/var/lib/led-matrix/hardware/state.json`, which `GET /hardware` returns as `result`.

App updates switch binaries only (no `dpkg`), so a Pi whose installed package predates
this has no helper: `GET /hardware` then reports `supported: false` with a reason, and
installing the current `.deb` once (`sudo apt install ./led-matrix-<version>_arm64.deb`)
adds it.

The panel image creates `hardware/setup-needed`; the web UI asks to check the layout
until a `POST /hardware` removes it. The emulator takes its options from the command
line, so it answers `409` to changes.

Tests: `panel_options_smoke` (validation, flag order, round trip through the library's
flag parser) and `apply_hardware` (the helper with fake `systemctl`/`curl`).
