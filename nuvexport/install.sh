#!/bin/sh

# First, install nuvexport itself
    install -Dv -o root -g root -m 0755 nuvexport /usr/local/bin/nuvexport
    install -Dv -o root -g root -m 0755 nuvinfo   /usr/local/bin/nuvinfo
    install -Dv -o root -g root -m 0755 mpeg2cut  /usr/local/bin/mpeg2cut

# Next, create the nuvexport shared directory
    mkdir -pvm 0755 /usr/local/share/nuvexport

# Finally, install the other files
    for dir in 'export' 'mythtv' 'nuv_export'; do
        mkdir -pvm 0755 /usr/local/share/nuvexport/"$dir"
        install -v -o root -g root -m 0755 "$dir"/*pm /usr/local/share/nuvexport/"$dir"
    done
