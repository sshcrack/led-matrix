#!/bin/bash
set -e

cmake --preset cross-compile
cmake --build --preset cross-compile --target install
rsync -avz --delete build/install/ ledmat:/home/pi/ledmat/run/

ssh ledmat sudo service ledmat restart