include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += sql xml network widgets

TEMPLATE = lib
CONFIG += plugin thread 
TARGET = mythweather
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

datafiles.path = $${PREFIX}/share/mythtv/mythweather/
datafiles.files = weather-screens.xml

INSTALLS += datafiles

# Input
HEADERS += weather.h weatherSource.h sourceManager.h weatherScreen.h weatherdbcheck.h
HEADERS += weatherSetup.h weatherUtils.h
SOURCES += mythweather.cpp weather.cpp weatherSource.cpp sourceManager.cpp weatherScreen.cpp
SOURCES += weatherdbcheck.cpp weatherSetup.cpp weatherUtils.cpp

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

android {
    # to discriminate plugins in a flat directory structure
    TARGET = mythplugin$${TARGET}
}

include ( ../../libs-targetfix.pro )
