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
# Set SKIP_PREVIEWS=1 to skip preview sync (useful when emulator is not built)
: "${SKIP_PREVIEWS:=0}"

# This script is just used to update the rpi
# Expected workflow:
#   1. Build emulator previews:
#      cmake --build emulator_build --target generate_scene_previews_incremental
#   2. Run this script to cross-compile and deploy (includes preview sync)

COMPILER_DIR=$(ls -td $CROSS_COMPILE_ROOT/tools/*/ | head -1)
echo "Using compilers in directory $COMPILER_DIR"

CROSS_COMPILER_DIR="$COMPILER_DIR/bin" cmake --preset cross-compile
cmake --build build --target install -j $(nproc)

# Sync host-generated previews into the cross-compile install tree before rsync.
# Previews are generated on the host using the emulator build and then packaged
# into the RPi deploy directory.  Pass SKIP_PREVIEWS=1 to opt out.
if [ "$SKIP_PREVIEWS" != "1" ] && [ -d "emulator_build/previews" ]; then
    echo "Syncing scene preview GIFs from emulator_build/previews/ ..."
    mkdir -p build/install/previews
    rsync -a --include="*.gif" --exclude="*" emulator_build/previews/ build/install/previews/
else
    echo "Skipping preview sync (SKIP_PREVIEWS=$SKIP_PREVIEWS or emulator_build/previews not found)"
fi

rsync -avz --delete $SCRIPT_DIR/../build/install/ $SSH_HOST:/home/pi/ledmat/run/

ssh $SSH_HOST sudo service ledmat restart
