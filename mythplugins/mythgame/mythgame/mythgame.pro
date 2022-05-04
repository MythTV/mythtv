include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgame

LIBS += -lmythmetadata-$$LIBVERSION

installscripts.path = $${PREFIX}/share/mythtv/metadata/Game
installscripts.files = scripts/*.py scripts/*.pl
installgiantbomb.path = $${PREFIX}/share/mythtv/metadata/Game/giantbomb
installgiantbomb.files = scripts/giantbomb/*.py
installgiantbombxsl.path = $${PREFIX}/share/mythtv/metadata/Game/giantbomb/XSLT
installgiantbombxsl.files = scripts/giantbomb/XSLT/*.xsl

target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target installscripts installgiantbomb installgiantbombxsl

# Input
HEADERS += gamehandler.h rominfo.h gamesettings.h gamedbcheck.h gameui.h
HEADERS += rom_metadata.h romedit.h gamedetails.h gamescan.h

SOURCES += mythgame.cpp gamehandler.cpp rominfo.cpp gameui.cpp
SOURCES += gamesettings.cpp gamedbcheck.cpp rom_metadata.cpp romedit.cpp
SOURCES += gamedetails.cpp gamescan.cpp

DEFINES += MPLUGIN_API NOUNCRYPT

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

QT += xml sql opengl network widgets

android {
    # to discriminate plugins in a flat directory structure
    TARGET = mythplugin$${TARGET}
}

include ( ../../libs-targetfix.pro )
