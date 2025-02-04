#!/bin/bash

#while inotifywait --exclude build -r -e modify,create,delete,move .; do
while true; do
    echo "Waiting until enter press"
    read -s -n 1 key
    echo "Updating..."
    ./sync.sh
done
