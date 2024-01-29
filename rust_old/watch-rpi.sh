while inotifywait -r -e modify,create,delete,move .; do
    echo "make"
    make
    echo "Running"
    ./run.sh
done
