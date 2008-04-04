include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgame
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += gamehandler.h rominfo.h unzip.h gamesettings.h gametree.h
HEADERS += rom_metadata.h romedit.h

SOURCES += main.cpp gamehandler.cpp rominfo.cpp gametree.cpp unzip.c
SOURCES += gamesettings.cpp dbcheck.cpp rom_metadata.cpp romedit.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

#The following line was inserted by qt3to4
QT += xml sql opengl qt3support 
