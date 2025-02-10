#!/bin/bash

$HOME/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake --build $HOME/Documents/cpp/led-matrix/cpp/cmake-build-rpi-cross-compile --target install -j 14
./sync.sh