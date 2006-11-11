#!/bin/sh -e

# Copy this file to /etc/udev/scripts and add the contents mythtv-udev.rules
# to your udev.rules in order to enable monitoring of hotpluggable disk
# drives, such as USB keychains and USB connected cameras.
# NOTE: You need udev version 0.71 or later for this functionality.

MYTH_FIFO=/tmp/mythtv_media

if [ -p $MYTH_FIFO ] && /bin/ps -C mythfrontend > /dev/null ; then
    echo $ACTION /sys$DEVPATH $DEVNAME > $MYTH_FIFO
fi
