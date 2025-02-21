#!/bin/bash
set -e

# Sync executable
bn=$(basename $1)
rpiaddr="192.168.43.240"

if [ "$2" == "all" ]; then
    rsync -az $1/build/compositeur $1/build/decodeur $1/build/convertisseur $1/build/filtreur $1/build/redimensionneur "pi@$rpiaddr:/home/pi/projects/laboratoire3/"
else
    rsync -az $1/build/$2 "pi@$rpiaddr:/home/pi/projects/laboratoire3/"
    # Execute GDB
    ssh "pi@$rpiaddr" "nohup gdbserver :$3 /home/pi/projects/laboratoire3/$2 -r 0 -w 427 -h 240 /mem2 /mem3> /home/pi/capture-stdout-$2 2> /home/pi/capture-stderr-$2 < /dev/null &"
    sleep 1
fi
