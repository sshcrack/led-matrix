#!/bin/bash

if [  -f ".env" ]; then
    echo "Environment file found at '.env'. Sourcing..."
    set -a && source .env && set +a
else
    echo "Environment file '.env' not found. You can create one if you want"
fi


cmake --build build --target install
./emulator_build/install/main --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64 --led-emulator --led-emulator-scale=4
