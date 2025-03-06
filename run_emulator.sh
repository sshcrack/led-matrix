#!/bin/bash

cmake --build build --target install
./build/install/main --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64 --led-emulator --led-emulator-scale=4
