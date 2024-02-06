#!/bin/bash

#while inotifywait --exclude build -r -e modify,create,delete,move .; do
while true; do
    echo "Waiting until enter press"
    read -s -n 1 key
    echo "Updating..."
    rsync . -av --exclude "images" --exclude "rpi-rgb-led-matrix" --exclude "CMakeFiles" --exclude "cmake-build*" --exclude "cmake-build-pi-release" --exclude "cmake-build-debug-wsl" --exclude "build" --delete -e "ssh -i ~/.ssh/ledmat" pi@10.6.0.23:/home/pi/ledmat
done
