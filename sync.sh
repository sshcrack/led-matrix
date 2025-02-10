#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

rsync -avz --delete $SCRIPT_DIR/cmake-build-rpi-cross-compile/install/ ledmat:/home/pi/ledmat/run/