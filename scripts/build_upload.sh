#!/bin/bash
set -e

# This script is just used to update the rpi

COMPILER_DIR=$(ls -td $CROSS_COMPILE_ROOT/tools/*/ | head -1)
echo "Using compilers in directory $COMPILER_DIR"

CROSS_COMPILER_DIR="$COMPILER_DIR/bin" cmake --preset cross-compile
cmake --build build --target install -j $(nproc)

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
rsync -avz --delete $SCRIPT_DIR/../build/install/ ledmat:/home/pi/ledmat/run/

ssh ledmat sudo service ledmat restart
