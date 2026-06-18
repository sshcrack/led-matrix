#!/bin/bash
set -e

cmake --preset cross-compile
cmake --build --preset cross-compile --target install

rsync -avz --delete --rsync-path="sudo rsync" build/install/ ledmat:/
ssh ledmat sudo systemctl restart led-matrix.service
