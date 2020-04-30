#!/bin/bash

while [[ ! -e ".git" ]] ; do
    cd ..
    if [[ `pwd` == "/" ]] ; then
        echo "fail."
        exit
    fi
done

CHECKSUM=`find . -name \*.cpp -o -name \*.c -o -name \*.h | md5sum | cut -b -32`

if [[ ! -e "compile_commands.json" ]] ; then
    echo $CHECKSUM > "source-checksum"
    echo "new"
    exit
fi

if [[ ! -e "source-checksum" ]] ; then
    echo $CHECKSUM > "source-checksum"
    echo "new2"
    exit
fi

line=$(head -n 1 "source-checksum")
if [[ $line != $CHECKSUM ]]; then
    echo $CHECKSUM > "source-checksum"
    echo "changed"
else
    echo "ok"
fi

