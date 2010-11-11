#!/bin/bash
#
# This script uses the themestringstool to automate the themestrings generation,
# by redirecting themestrings from the plugins into the themestrings.h files of
# the plugins, instead of simply putting all of them into the themestrings.h
# file in mythfrontend.
#
# The script also downloads the latest third party themes from mythtv.org and
# extracts their descriptions for translation.
#
# It should be sufficient to run this script once without any arguments, to
# update all themestrings for mythfrontend and the plugins.
#

set -e

TS=`pwd`/themestrings
MYTHTHEMES=`ls ../myththemes/ --file-type |grep "/$"`
MYTHTHEMES_CORE=`ls ../mythtv/themes/ --file-type |grep "/$"`
MYTHTHEMES_DL="http://themes.mythtv.org/themes/repository/trunk/themes.zip"
XMLPLUGINS="browser-ui.xml gallery-ui.xml game-ui.xml music-ui.xml \
         mytharchive-ui.xml mythburn-ui.xml netvision-ui.xml \
         news-ui.xml video-ui.xml zoneminder-ui.xml weather-ui.xml"

if [ ! -e ${TS} ]; then
    echo ""
    echo "ERROR: The executable ${TS} doesn't exist, have you compiled it yet?"
    echo ""
    exit 1
fi

if [ `id -u` -eq 0 ]; then
    echo ""
    echo "ERROR: You need to run this script as a regular user, you\
          cannot run it with root/sudo."
    echo ""
    exit 1
fi

# Exclude mythplugins directory and myththemes files related to plugins as
# we don't want these strings added to mythfrontend.
pushd ..
    chmod a-x mythplugins
    for I in ${MYTHTHEMES}; do
        for J in ${XMLPLUGINS}; do
            [ -e myththemes/${I}${J} ] && chmod a-r myththemes/${I}${J}
        done
    done
popd > /dev/null

# Also exclude the plugin-related theme files in the mythtv core themes.
pushd ../mythtv/themes
    for I in ${MYTHTHEMES_CORE}; do
        for J in ${XMLPLUGINS}; do
            [ -e ${I}${J} ] && chmod a-r ${I}${J}
        done
    done
popd > /dev/null

# Download third party themes to extract their description
mkdir temp_download
wget ${MYTHTHEMES_DL} -O temp_download/themes.zip
unzip temp_download/themes.zip -d temp_download/

echo "-------------------------------------------------------------------------"
echo "\"Can't open [plugin-specific xml-file]\" error messages are expected below"
echo "-------------------------------------------------------------------------"
pushd ../mythtv/themes
    $TS ../.. . > /dev/null
popd > /dev/null

echo "-------------------------------------------------------------------------"
echo "...until this point"
echo "-------------------------------------------------------------------------"
rm -rf temp_download

# Restore file permissions
pushd ..
    chmod a+x mythplugins
    for I in ${MYTHTHEMES}; do
        for J in ${XMLPLUGINS}; do
            [ -e myththemes/${I}${J} ] && chmod a+r myththemes/${I}${J}
        done
    done
popd > /dev/null

pushd ../mythtv/themes
    for I in ${MYTHTHEMES_CORE}; do
        for J in ${XMLPLUGINS}; do
            [ -e ${I}${J} ] && chmod a+r ${I}${J}
        done
    done
popd > /dev/null

# updateplugin plugindir [xml file] [xml file]
function updateplugin {
    pushd ../mythplugins/$1
    mkdir temp_themestrings > /dev/null 2>&1
    COUNT=0
    [ -n "$2" ] && for i in `ls ../../myththemes/*/${2} ../../mythtv/themes/*/${2} 2>/dev/null`; do
        cp $i temp_themestrings/${COUNT}-${2}
        COUNT=$((COUNT+1))
    done

    [ -n "$3" ] && for i in `ls ../../myththemes/*/${3} ../../mythtv/themes/*/${3} 2>/dev/null`; do
        cp $i temp_themestrings/${COUNT}-${3}
        COUNT=$((COUNT+1))
    done

    $TS . `pwd`/i18n > /dev/null
    
    rm -rf temp_themestrings
    
    popd > /dev/null
}

updateplugin mytharchive mytharchive-ui.xml mythburn-ui.xml
updateplugin mythbrowser browser-ui.xml
updateplugin mythgallery gallery-ui.xml
updateplugin mythgame game-ui.xml
updateplugin mythmusic music-ui.xml
updateplugin mythnetvision netvision-ui.xml
updateplugin mythnews news-ui.xml
updateplugin mythvideo video-ui.xml
updateplugin mythweather weather-ui.xml
updateplugin mythzoneminder zoneminder-ui.xml

pushd .. > /dev/null
    svn st
#    emacs `svn st | grep "^M" | sed -e "s/^M//"` &
popd > /dev/null
