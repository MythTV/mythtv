#!/bin/bash
#
# This script uses the themestringstool to automate the generation of
# themestrings.
#
# The themes are downloaded from the internet and the strings are extracted
# from specific themes or from all of the available themes.
#
# Strings related to the frontend or the backend goes into the "mythtv"
# translation. Strings related to a plugin goes into the respective plugin.
#
# It should be sufficient to run this script once without any arguments, to
# update all themestrings for mythfrontend and the plugins.
#

set -e

TS=$(pwd)/themestrings
DOWNLOAD_DIR="temp_download"
MYTHTHEMES_CORE=$(ls ../mythtv/themes/ --file-type |grep "/$")
MYTHTHEMES_DL="http://themes.mythtv.org/themes/repository/trunk/themes.zip"
REAL_XMLPLUGINS="browser-ui.xml gallery-ui.xml game-ui.xml music-ui.xml \
    mytharchive-ui.xml mythburn-ui.xml netvision-ui.xml \
    news-ui.xml zoneminder-ui.xml weather-ui.xml \
    mythnative-ui.xml musicsettings-ui.xml steppes-music.xml music-base.xml \
    base_archive.xml image-ui.xml base_music.xml stream-ui.xml \
    base_netvision.xml"

BOGUS_XMLPLUGINS="dvd-ui.xml gallery2-ui.xml" # dvd-ui.xml (DVD ripper) needs
    # to be in this list to be excluded from mythtv. It isn't used anymore,
    # but some themes still contains it.
    # gallery2-ui.xml is a bogus file in the blue-abstract theme.

XMLPLUGINS="${REAL_XMLPLUGINS} ${BOGUS_XMLPLUGINS}"

if [ ! -e ${TS} ]; then
    echo ""
    echo "ERROR: The executable ${TS} doesn't exist, have you compiled it yet?"
    echo ""
    exit 1
fi

if [ $(id -u) -eq 0 ]; then
    echo ""
    echo "ERROR: You need to run this script as a regular user, you\
          cannot run it with root/sudo."
    echo ""
    exit 1
fi

# Download overview of themes to extract their descriptions and URLs
[ -d ${DOWNLOAD_DIR}/themes ] && rm -rf temp_download/themes/*
[ -d ${DOWNLOAD_DIR}/trunk ] && rm -rf temp_download/trunk/*
[ ! -d ${DOWNLOAD_DIR}/themes ] && mkdir -p temp_download/themes
echo "Downloading list of themes.."
wget -q ${MYTHTHEMES_DL} -O ${DOWNLOAD_DIR}/themes.zip
echo "Extracting list of themes.."
unzip -q -o ${DOWNLOAD_DIR}/themes.zip -d temp_download/

# Download the themes
echo ""
echo "Downloading and extracting all available themes:"
echo ""
for i in $(ls ${DOWNLOAD_DIR}/trunk/); do
    THEME_URL=$(cat ${DOWNLOAD_DIR}/trunk/${i}/themeinfo.xml |grep "</*url>"|sed -e "s/ *<\/*url>//g")
    THEME_FILENAME=$(basename ${THEME_URL})
    [ ! -f ${DOWNLOAD_DIR}/${THEME_FILENAME} ] && echo "Downloading theme ${THEME_FILENAME}.." && wget -q ${THEME_URL} -P temp_download/
    echo "Extracting theme ${THEME_FILENAME}.."
    unzip -q -o ${DOWNLOAD_DIR}/${THEME_FILENAME} -d temp_download/themes/
done


#####################################################################
#                                                                   #
# Select the themes from which strings should be extracted          #
#                                                                   #
#####################################################################

# Select the themes that should be translatable (theme name = directory name after extraction)
TRANSLATABLE_THEMES="Arclight Childish Graphite Mythbuntu MythCenter MythCenter-wide Steppes Steppes-narrow Terra A-Forest blue-abstract-wide"
#TRANSLATABLE_THEMES=$(ls ${DOWNLOAD_DIR}/themes/ --file-type |grep "/$"|tr '/' ' ') #All themes

#Remove the extracted themes which shouldn't be translatable
echo ""
echo "Strings will be extracted from the following themes:"
echo ${TRANSLATABLE_THEMES} | tr ' ' '\n'
shopt -s extglob
rm -rf ${DOWNLOAD_DIR}/themes/!($(echo ${TRANSLATABLE_THEMES}|tr ' ' '|'))
shopt -u extglob


#####################################################################
#                                                                   #
# Generate a themestrings.h file for mythtv                         #
#                                                                   #
# Contains non-plugin related strings as well as theme descriptions #
#                                                                   #
#####################################################################

# Exclude mythplugins directory and myththemes files related to plugins as
# we don't want these strings added to mythfrontend
chmod a-x ../mythplugins
for I in ${TRANSLATABLE_THEMES}; do
    for J in ${XMLPLUGINS}; do
        [ -e ${DOWNLOAD_DIR}/themes/${I}/${J} ] && chmod a-r temp_download/themes/${I}/${J}
    done
done

# Also exclude the plugin-related theme files in the mythtv core themes
pushd ../mythtv/themes > /dev/null
    for I in ${MYTHTHEMES_CORE}; do
        for J in ${XMLPLUGINS}; do
            [ -e ${I}${J} ] && chmod a-r ${I}${J}
        done
    done
popd > /dev/null

# Generate themestrings.h file for mythtv
pushd ../mythtv/themes  > /dev/null
    echo ""
    echo "Generating themestrings.h file for mythtv.."
    ${TS} ../.. . &> /dev/null
    ls -l themestrings.h
    echo "Number of strings: $(( $(cat themestrings.h|wc -l)-2 ))"
popd > /dev/null

# Restore directory permissions for mythplugins
chmod a+x ../mythplugins
for I in ${TRANSLATABLE_THEMES}; do
    for J in ${XMLPLUGINS}; do
        [ -e ${DOWNLOAD_DIR}/themes/${I}/${J} ] && chmod a+r temp_download/themes/${I}/${J}
    done
done

# Restore file permissions for plugin-related theme files
pushd ../mythtv/themes  > /dev/null
    for I in ${MYTHTHEMES_CORE}; do
        for J in ${XMLPLUGINS}; do
            [ -e ${I}${J} ] && chmod a+r ${I}${J}
        done
    done
popd > /dev/null


#####################################################################
#                                                                   #
# Generate themestrings.h files for the plugins                     #
#                                                                   #
# Contains plugin-related strings from core and downloadable themes #
#                                                                   #
#####################################################################

# updateplugin plugindir [xml files]
function updateplugin {
    ARGS=($@)
    XML_FILES=${ARGS[@]:1}
    pushd ../mythplugins/${1}  > /dev/null
    [ ! -d temp_themestrings ] && mkdir temp_themestrings &> /dev/null

    for xmlfile in ${XML_FILES}; do
        COUNT=0
        [ -n "${xmlfile}" ] && for sourcefile in $(ls ../../themestringstool/${DOWNLOAD_DIR}/themes/*/${xmlfile} ../../mythtv/themes/*/${xmlfile} 2>/dev/null); do
            cp ${sourcefile} temp_themestrings/${COUNT}-${xmlfile}
            COUNT=$((COUNT+1))
        done
    done

    echo ""
    echo "Generating themestrings.h file for ${1}.."
    ${TS} . $(pwd)/i18n > /dev/null
    ls -l i18n/themestrings.h
    echo "Number of strings: $(( $(cat i18n/themestrings.h|wc -l)-2 ))"
    
    rm -rf temp_themestrings
    
    popd > /dev/null
}

updateplugin mytharchive mytharchive-ui.xml mythburn-ui.xml mythnative-ui.xml base_archive.xml
updateplugin mythbrowser browser-ui.xml
updateplugin mythgallery gallery-ui.xml image-ui.xml
updateplugin mythgame game-ui.xml
updateplugin mythmusic music-ui.xml musicsettings-ui.xml steppes-music.xml music-base.xml base_music.xml stream-ui.xml
updateplugin mythnetvision netvision-ui.xml base_netvision.xml
updateplugin mythnews news-ui.xml
updateplugin mythweather weather-ui.xml
updateplugin mythzoneminder zoneminder-ui.xml

