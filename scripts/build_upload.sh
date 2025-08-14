#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd $SCRIPT_DIR/..
if [  -f ".env" ]; then
    echo "Environment file found at '.env'. Sourcing..."
    set -a && source .env && set +a
else
    echo "Environment file '.env' not found. You can create one if you want"
fi

: "${SSH_HOST:=ledmat}"

# This script is just used to update the rpi

COMPILER_DIR=$(ls -td $CROSS_COMPILE_ROOT/tools/*/ | head -1)
echo "Using compilers in directory $COMPILER_DIR"

CROSS_COMPILER_DIR="$COMPILER_DIR/bin" cmake --preset cross-compile
cmake --build build --target install -j $(nproc)

rsync -avz --delete $SCRIPT_DIR/../build/install/ $SSH_HOST:/home/pi/ledmat/run/

ssh $SSH_HOST sudo service ledmat restart
