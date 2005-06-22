#!/bin/sh

REMOTE_NAME=gi-motorola-dct2000
cmd="$1"

case $cmd in
    [0-9]*)
    for digit in $(echo $1 | sed -e 's/./& /g'); do 
        irsend SEND_ONCE $REMOTE_NAME $digit
        sleep 1
        # If things work OK with sleep 1, try this for faster channel changes:
        # sleep 0.3
    done
    ;;

    *)
        irsend SEND_ONCE $REMOTE_NAME $cmd
        ;;
esac
