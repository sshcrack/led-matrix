#!/bin/bash
set -e

# SKIP_WEB_BUILD is sticky in the CMake cache (it persists across
# `cmake --preset` runs), so a dev-speedup configure would silently produce a
# .deb with no web UI. The matrix would then answer /web/ with
# 403 {"error":"Invalid path"}. Always force it OFF for deploy packages.
#
# The cross-compile presets don't cap ninja jobs, so it would default to
# nproc+2 and run ~16 parallel g++/ld invocations. These translation units are
# template-heavy and a full `package` has been observed to exhaust RAM and
# trigger systemd-oomd / kernel OOM kills of unrelated apps.
JOBS="${JOBS:-4}"

cmake --preset cross-compile -DSKIP_WEB_BUILD=OFF
cmake --build --preset cross-compile --target package --parallel "$JOBS"

deb=$(ls -t build/led-matrix-*.deb 2>/dev/null | head -1 || true)
if [ -z "$deb" ]; then
    echo "ERROR: No .deb package found in build/"
    exit 1
fi
echo "Installing $deb on ledmat..."
rsync -avz "$deb" ledmat:/tmp/
# Use the candidate updater, not the previously installed script: this also
# bootstraps health checks for devices upgrading from an older updater.
rsync -avz scripts/update_service.sh ledmat:/tmp/led-matrix-update-service.sh
ssh ledmat sudo -n install -d -m 700 /var/cache/led-matrix/deploy
ssh ledmat sudo -n install -m 700 /tmp/led-matrix-update-service.sh /var/cache/led-matrix/deploy/update_service.sh
ssh ledmat sudo -n systemd-run --wait --collect --unit=led-matrix-deploy \
    /var/cache/led-matrix/deploy/update_service.sh "/tmp/$(basename "$deb")"
ssh ledmat rm -f /tmp/led-matrix-update-service.sh
