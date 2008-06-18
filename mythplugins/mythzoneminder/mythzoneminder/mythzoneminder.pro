include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythzoneminder
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += zmconsole.h zmplayer.h zmevents.h zmliveplayer.h zmdefines.h 
HEADERS += zmsettings.h zmclient.h

SOURCES += main.cpp zmconsole.cpp zmplayer.cpp zmevents.cpp zmliveplayer.cpp 
SOURCES += zmsettings.cpp zmclient.cpp


QT += opengl sql xml qt3support

include ( ../../libs-targetfix.pro )
