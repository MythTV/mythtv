include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += sql xml qt3support

TEMPLATE = lib
CONFIG += plugin thread debug
TARGET = mythweather
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv

datafiles.path = $${PREFIX}/share/mythtv/mythweather/
datafiles.files = weather-screens.xml
installscripts.path = $${PREFIX}/share/mythtv/mythweather/scripts
installscripts.files = scripts/*
INSTALLS += datafiles installscripts

# Input

HEADERS += weather.h weatherSource.h sourceManager.h weatherScreen.h dbcheck.h
HEADERS += weatherSetup.h weatherUtils.h
SOURCES += main.cpp weather.cpp weatherSource.cpp sourceManager.cpp weatherScreen.cpp
SOURCES += dbcheck.cpp weatherSetup.cpp weatherUtils.cpp

include ( ../../libs-targetfix.pro )
