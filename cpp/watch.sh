while inotifywait --exclude build -r -e modify,create,delete,move .; do
    rsync . -av --exclude "build" --delete -e "ssh -i ~/.ssh/ledmat" pi@192.168.178.65:/home/pi/ledmat
done
