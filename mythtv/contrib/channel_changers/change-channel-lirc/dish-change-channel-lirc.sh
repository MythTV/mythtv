#!/bin/sh

# This script is provided without any warranty of fitness to whoever
# wishes to use it. It has been used by me for many months with the
# VIP211 receivers. -- Daniel Kristjansson (March 20th, 2008)

# If you have a separate IR reciever you will want to give it "/dev/lircd"
# and use something like "/dev/lircd1" for the transmitter.
# If the tranmitter is on a remote LIRC server, use the ip/hostname:port
# with -a to address it.

DEVICE="-d /dev/lircd"
#DEVICE="-d /dev/lircd1"
#DEVICE="-a hostname:port"

# Use dish1, dish2 .. dish16 depending on which id you are using
# Every dish receiver can be assigned a different remote code, so
# that you can have multiple receivers in one room, and controlled
# by the same LIRC transmitter.

REMOTE_NAME=dish1

# This is the command passed in from MythTV.

cmd="$1"

# First, leave sleep mode..
irsend $DEVICE SEND_ONCE $REMOTE_NAME select
sleep 0.3

case $cmd in
    [0-9]*)
    # make sure we leave any encrypted channel..

    # Note: this IR send slows down MythTV LiveTV, if you want faster
    # channel changing in LiveTV remove it. However if someone ever
    # changes to a channel you don't get, either with this script or
    # manually, you need this "up" to leave that channel.

    irsend $DEVICE SEND_ONCE $REMOTE_NAME up
    sleep 0.3

    for digit in $(echo $1 | sed -e 's/./& /g'); do 
        irsend $DEVICE SEND_ONCE $REMOTE_NAME $digit
        # If things work OK with sleep 1, try this for faster channel changes:
        sleep 0.15
    done

    # Send a select so that this channel change takes immediately.
    irsend $DEVICE SEND_ONCE $REMOTE_NAME SELECT

    ;;

    *)
        irsend $DEVICE SEND_ONCE $REMOTE_NAME $cmd
        ;;
esac
