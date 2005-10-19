include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythweather
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = weather-ui.xml
installfiles.path = $${PREFIX}/share/mythtv/mythweather
installfiles.files = weathertypes.dat accid.dat
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png
 
INSTALLS += installfiles installimages uifiles

# Input

HEADERS += weather.h
SOURCES += main.cpp weather.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
