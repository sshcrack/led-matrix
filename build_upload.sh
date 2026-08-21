#!/bin/bash
set -e

cmake --preset cross-compile
cmake --build --preset cross-compile --target package

deb=$(ls -t build/led-matrix-*.deb 2>/dev/null | head -1 || true)
if [ -z "$deb" ]; then
    echo "ERROR: No .deb package found in build/"
    exit 1
fi
echo "Installing $deb on ledmat..."
rsync -avz "$deb" ledmat:/tmp/
ssh ledmat sudo dpkg -i "/tmp/$(basename "$deb")"
