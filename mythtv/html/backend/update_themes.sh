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

# These themes are banned because they cause inconsistent row height in tables
# These are coincidentally also the themes that come with fonts.
# lara-dark-blue.css
# lara-dark-indigo.css
# lara-dark-purple.css
# lara-dark-teal.css
# lara-light-blue.css
# lara-light-indigo.css
# lara-light-purple.css
# lara-light-teal.css
# md-dark-deeppurple.css
# md-dark-indigo.css
# md-light-deeppurple.css
# md-light-indigo.css
# mdc-dark-deeppurple.css
# mdc-dark-indigo.css
# mdc-light-deeppurple.css
# mdc-light-indigo.css
# tailwind-light.css
