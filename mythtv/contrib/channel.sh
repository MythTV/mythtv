#!/bin/sh

REMOTE_NAME=gi-motorola-dct2000

for digit in $(echo $1 | sed -e 's/./& /g'); do 
    irsend SEND_ONCE $REMOTE_NAME $digit
    sleep 1
done
