include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgame

installscripts.path = $${PREFIX}/share/mythtv/metadata/Game
installscripts.files = scripts/*.py
installgiantbomb.path = $${PREFIX}/share/mythtv/metadata/Game/giantbomb
installgiantbomb.files = scripts/giantbomb/*.py
installgiantbombxsl.path = $${PREFIX}/share/mythtv/metadata/Game/giantbomb/XSLT
installgiantbombxsl.files = scripts/giantbomb/XSLT/*.xsl

target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target installscripts installgiantbomb installgiantbombxsl

# Input
HEADERS += gamehandler.h rominfo.h unzip.h gamesettings.h gameui.h
HEADERS += rom_metadata.h romedit.h gamedetails.h

SOURCES += main.cpp gamehandler.cpp rominfo.cpp gameui.cpp unzip.c
SOURCES += gamesettings.cpp dbcheck.cpp rom_metadata.cpp romedit.cpp
SOURCES += gamedetails.cpp

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

LIBS += -lz

#The following line was inserted by qt3to4
QT += xml sql opengl 

include ( ../../libs-targetfix.pro )
