#!/bin/bash
TS=`pwd`/themestrings

pushd ..
    chmod a-x mythplugins
popd > /dev/null

pushd ../mythtv/themes
    $TS ../.. . > /dev/null
popd > /dev/null

pushd ..
    chmod a+x mythplugins
popd > /dev/null

for I in `ls ../mythplugins --file-type | grep "/$" | grep -v cleanup | grep -v mythweb` ; do
    pushd ../mythplugins/$I
    $TS . `pwd`/i18n > /dev/null
    popd > /dev/null
done

pushd .. > /dev/null
    svn st
    emacs `svn st | grep "^M" | sed -e "s/^M//"` &
popd > /dev/null
