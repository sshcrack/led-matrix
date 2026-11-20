#!/bin/bash
# The daemon drops privileges to pi:pi (MATRIX_OPTS). Raspberry Pi Imager's user
# may have another name; then pi is a locked account that only owns the panel's
# data. The image's data files were created as uid 1000, which Imager's user may
# now have, so the data is handed to pi either way.
set -euo pipefail
if ! id pi >/dev/null 2>&1; then
    uid=()
    getent passwd 1000 >/dev/null || uid=(--uid 1000)
    useradd "${uid[@]}" --user-group --no-create-home --home-dir /var/lib/led-matrix --shell /usr/sbin/nologin pi
    passwd --lock pi >/dev/null
    echo "Created the daemon user pi"
fi
chown -R pi:pi /var/lib/led-matrix
