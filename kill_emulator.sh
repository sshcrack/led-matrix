#!/bin/bash

# Find the PID of the emulator process
PID=$(pgrep -f "emulator_build/install/main")

if [ -z "$PID" ]; then
  echo "Emulator process not found."
  exit 1
fi

# Send SIGINT
kill -SIGINT "$PID"
echo "Sent SIGINT to process $PID."
