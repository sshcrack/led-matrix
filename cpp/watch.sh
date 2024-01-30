#!/bin/bash

#while inotifywait --exclude build -r -e modify,create,delete,move .; do
while true; do
    echo "Waiting until enter press"
    read -s -n 1 key
    echo "Updating..."
    rsync . -av --exclude "rpi-rgb-led-matrix" --exclude "CMakeFiles" --exclude "cmake-build-debug" --exclude "cmake-build-pi-release" --exclude "cmake-build-debug-wsl" --exclude "build" --delete -e "ssh -i ~/.ssh/ledmat" pi@192.168.178.65:/home/pi/ledmat
done
