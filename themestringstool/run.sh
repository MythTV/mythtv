#!/bin/bash
TS=`pwd`/themestrings
MYTHTHEMES=`ls ../myththemes/ --file-type |grep "/$"`
XMLPLUGINS="browser-ui.xml dvd-ui.xml gallery-ui.xml game-ui.xml \
         music-ui.xml mytharchive-ui.xml mythburn-ui.xml netvision-ui.xml \
         news-ui.xml video-ui.xml zoneminder-ui.xml weather-ui.xml"

# mythtv: Exclude mythplugins directory and myththemes files related to plugins
pushd ..
    chmod a-x mythplugins
    for I in ${MYTHTHEMES}; do
        for J in ${XMLPLUGINS}; do
            [ -e myththemes/${I}${J} ] && chmod a-r myththemes/${I}${J}
        done
    done
popd > /dev/null

echo "-------------------------------------------------------------------------"
echo "\"Can't open [plugin-specific xml-file]\" error messages are expected below"
echo "-------------------------------------------------------------------------"
pushd ../mythtv/themes
    $TS ../.. . > /dev/null
popd > /dev/null

echo "-------------------------------------------------------------------------"
echo "...until this point"
echo "-------------------------------------------------------------------------"

pushd ..
    chmod a+x mythplugins
    for I in ${MYTHTHEMES}; do
        for J in ${XMLPLUGINS}; do
            [ -e myththemes/${I}${J} ] && chmod a+r myththemes/${I}${J}
        done
    done
popd > /dev/null


# updateplugin plugindir [xml file] [xml file]
function updateplugin {
    pushd ../mythplugins/$1
    mkdir temp_themestrings > /dev/null 2>&1
    COUNT=0
    [ -n "$2" ] && for i in `echo ../../myththemes/*/${2}`; do
        cp $i temp_themestrings/${COUNT}-${2}
        COUNT=$((COUNT+1))
    done

    [ -n "$3" ] && for i in `echo ../../myththemes/*/${3}`; do
        cp $i temp_themestrings/${COUNT}-${3}
        COUNT=$((COUNT+1))
    done

    $TS . `pwd`/i18n > /dev/null
    
    rm -rf temp_themestrings
    
    popd > /dev/null
}

# exclude mythweather us_nws strings
chmod a-r ../mythplugins/mythweather/mythweather/scripts/us_nws/maps.xml

updateplugin mytharchive mytharchive-ui.xml mythburn-ui.xml
updateplugin mythbrowser browser-ui.xml
updateplugin mythgallery gallery-ui.xml
updateplugin mythgame game-ui.xml
updateplugin mythmusic music-ui.xml
updateplugin mythnetvision netvision-ui.xml
updateplugin mythnews news-ui.xml
updateplugin mythvideo dvd-ui.xml video-ui.xml
updateplugin mythweather weather-ui.xml
updateplugin mythzoneminder zoneminder-ui.xml  #zoneminder-ui.xml doesn't exist at the moment, but it's included for future use

chmod a+r ../mythplugins/mythweather/mythweather/scripts/us_nws/maps.xml

pushd .. > /dev/null
    svn st
    emacs `svn st | grep "^M" | sed -e "s/^M//"` &
popd > /dev/null
