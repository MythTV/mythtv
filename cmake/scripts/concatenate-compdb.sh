#!/bin/sh

OUTPUT=$1
MYTHTV=$2
MYTHPLUGINS=$3

head -n -1 ${MYTHTV}/compile_commands.json       > ${OUTPUT}/compile_commands.json
tail -n -1 ${MYTHPLUGINS}/compile_commands.json >> ${OUTPUT}/compile_commands.json
