include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network widgets

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythzoneminder
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += zmconsole.h zmplayer.h zmevents.h zmliveplayer.h zmdefines.h
HEADERS += zmsettings.h zmclient.h alarmnotifythread.h zmminiplayer.h

SOURCES += mythzoneminder.cpp zmconsole.cpp zmplayer.cpp zmevents.cpp zmliveplayer.cpp
SOURCES += zmsettings.cpp zmclient.cpp alarmnotifythread.cpp zmminiplayer.cpp

QT += sql xml

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

android {
    # to discriminate plugins in a flat directory structure
    TARGET = mythplugin$${TARGET}
}

include ( ../../libs-targetfix.pro )
