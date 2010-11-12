include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += sql xml

TEMPLATE = lib
CONFIG += plugin thread debug
TARGET = mythweather
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

QMAKE_COPY_DIR = sh ../../cpsvndir
win32:QMAKE_COPY_DIR = sh ../../cpsimple

INCLUDEPATH += $${PREFIX}/include/mythtv
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythui

datafiles.path = $${PREFIX}/share/mythtv/mythweather/
datafiles.files = weather-screens.xml

INSTALLS += datafiles

# Input
HEADERS += weather.h weatherSource.h sourceManager.h weatherScreen.h dbcheck.h
HEADERS += weatherSetup.h weatherUtils.h
SOURCES += main.cpp weather.cpp weatherSource.cpp sourceManager.cpp weatherScreen.cpp
SOURCES += dbcheck.cpp weatherSetup.cpp weatherUtils.cpp

include ( ../../libs-targetfix.pro )
