#!/bin/bash

# ga.sh p-checkbox
#/home/peter/proj/github.com/MythTV/mythtv/mythtv/html/backend/src/app/config/settings/capture-cards/dvb/dvb.component.html

# awk -v "outdir=$TEMPDIR" -v "diskser=$diskser" -v "desc=$desc" -v "cover=$cover" \
#  -v modeldir=$TEMPDIR/index -v playlist_datetime=$playlist_datetime '
set -e

names=$1

if [[ "$names" == "" ]] ; then
    names=$(find . -name "*.html")
fi

for name in $names ; do
    echo File: $name
    if ! grep p-checkbox $name > /dev/null ; then
        echo No checkboxes
        continue
    fi

    outfile=${name}.fix
    # outfile=/dev/null

# infile=/home/peter/proj/github.com/MythTV/mythtv-webapp/mythtv/html/backend/src/app/config/settings/capture-cards/dvb/dvb.component.html
# outfile=/home/peter/proj/github.com/MythTV/mythtv-webapp/mythtv/html/backend/src/app/config/settings/capture-cards/dvb/dvb.component.html.fix
# src/app/config/settings/general/jobqueue-backend/jobqueue-backend.component.html
    awk -v outfile=${outfile} '

BEGIN {
}

/^ *<!--/ {
    print $0 > outfile
    next
}

/<p-checkbox +inputId=/ {
    id = $2
    sub("inputId","for", id)
    lookforlabel = 1
    print $0 > outfile
    next
}

/<p-checkbox/ {
    print "ERROR - checkbox without inputId" $0
    exit 99
}

/ label=/ {
    if (lookforlabel) {
        label = $0
        sub("^.*{{","",label)
        sub(" *[|] *translate *}}.*","",label)
        empty = $0 ''
        sub("label=.*}} *\"","", empty)
        lookforlabel=0
        print empty > outfile
        next
    }
}

/<\/p-checkbox>/ {
    print $0 > outfile
    space = $0
    sub("[^ ]* *$","",space)
    if (id != "" && label != "") {
        print space "<label " id " class=\"label block\">{{" label " |" > outfile
        print space "    translate }}</label>" > outfile
        print id " " label
    }
    lookforlabel = 0
    next
}

{
    print $0 > outfile
}

' $name
mv -f $outfile $name
done
