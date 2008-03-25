include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread debug
TARGET = mythweather
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = weather-ui.xml
wideuifiles.path = $${PREFIX}/share/mythtv/themes/default-wide
wideuifiles.files = theme-wide/weather-ui.xml
datafiles.path = $${PREFIX}/share/mythtv/mythweather/
datafiles.files = weather-screens.xml
installfiles.path = $${PREFIX}/share/mythtv/
installfiles.files = weather_settings.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png
installwideimages.path = $${PREFIX}/share/mythtv/themes/default-wide
installwideimages.files = theme-wide/images/*.png
installscripts.path = $${PREFIX}/share/mythtv/mythweather/scripts
installscripts.files = scripts/*
INSTALLS += installfiles datafiles installimages installwideimages uifiles wideuifiles installscripts

# Input

HEADERS += weather.h weatherSource.h sourceManager.h defs.h weatherScreen.h dbcheck.h
HEADERS += weatherSetup.h
SOURCES += main.cpp weather.cpp weatherSource.cpp sourceManager.cpp weatherScreen.cpp
SOURCES += dbcheck.cpp weatherSetup.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

mingw:DEFINES += USING_MINGW
#The following line was inserted by qt3to4
QT += opengl sql xml qt3support
