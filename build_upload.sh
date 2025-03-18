#!/bin/bash
set -e

COMPILER_DIR=$(ls -td $CROSS_COMPILE_ROOT/tools/*/ | head -1)
echo "Using compilers in directory $COMPILER_DIR"

CROSS_COMPILER_DIR="$COMPILER_DIR/bin" cmake --preset cross-compile
cmake --build build --target install -j $(nproc)

./sync.sh
ssh ledmat sudo service ledmat restart
