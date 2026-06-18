#!/bin/bash
set -e

cmake --preset cross-compile
cmake --build --preset cross-compile --target install

rsync -avz --rsync-path="sudo rsync" build/install/ ledmat:/usr/
ssh ledmat sudo systemctl restart led-matrix.service
