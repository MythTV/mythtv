#!/bin/sh

# First, install nuvexport itself
install -Dv -o root -g root -m 0755 nuvexport /usr/local/bin/nuvexport
install -Dv -o root -g root -m 0755 nuvinfo   /usr/local/bin/nuvinfo

# Next, create the nuvexport shared directory
mkdir -pvm 0755 /usr/local/share/nuvexport

# Finally, install the other files
install -v -o root -g root -m 0644 *pm /usr/local/share/nuvexport
