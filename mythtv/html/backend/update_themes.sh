#!/bin/bash

# Run this after upgrading PrimeNG to a new version.

scriptname=`readlink -e "$0"`
scriptpath=`dirname "$scriptname"`
cd "$scriptpath"

for file in ../assets/themes/*.css ; do
    name=$(basename $file)
    bname=${name%.css}
    cp -v node_modules/primeng/resources/themes/$bname/theme.css $file
    if [[ -d node_modules/primeng/resources/themes/$bname/fonts ]] ; then
        mkdir -p ../assets/themes/fonts/
        cp -v node_modules/primeng/resources/themes/$bname/fonts/* ../assets/themes/fonts/
    fi
done
