#!/bin/bash
set -e

cmake --preset cross-compile
cmake --build --preset cross-compile --target package

deb=$(ls -t build/*.deb | head -1)
echo "Installing $deb on ledmat..."
rsync -avz "$deb" ledmat:/tmp/
ssh ledmat sudo dpkg -i "/tmp/$(basename "$deb")"
ssh ledmat sudo systemctl restart led-matrix.service
