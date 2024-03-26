#!/bin/bash

set -e
echo Building
make -C cmake-build-pi-release
echo Running
sudo "PATH=$PATH:/home/pi/.nvm/versions/node/v21.6.1/bin" ./run.sh
