#!/bin/zsh

if [ "$1" != "create" ]; then
  # run normally in not an `hdiutil create` command
  hdiutil "$@"
  exit 0
fi

# On macOS 13, a race condition may present where hdiutil is unable to
# write to the filesystem during an `hdiutil create` command.  Attempt
# to work around this by attempting to build up to 15 times.
COUNTER=0
until /usr/bin/hdiutil "$@"; do
  if [ $i -eq 15 ]; then
    exit 1;
  fi
  let COUNTER+=1
  sleep 2
done
