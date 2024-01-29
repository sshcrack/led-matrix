while inotifywait --exclude target -r -e modify,create,delete,move .; do
    rsync . -av --exclude "target" --exclude "pixel-mover" --exclude "*.o" --delete -e "ssh -i ~/.ssh/ledmat" pi@192.168.178.65:/home/pi/ledmat
done
