#!/bin/bash

set -e
echo Building
make -C cmake-build-pi-release
echo Running
sudo ./run.sh
