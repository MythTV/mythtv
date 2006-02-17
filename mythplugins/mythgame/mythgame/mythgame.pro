include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgame
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = game_settings.xml
uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = game-ui.xml

INSTALLS += uifiles installfiles

# Input
HEADERS += gamehandler.h rominfo.h unzip.h gamesettings.h gametree.h
HEADERS += rom_metadata.h romedit.h

SOURCES += main.cpp gamehandler.cpp rominfo.cpp gametree.cpp unzip.c
SOURCES += gamesettings.cpp dbcheck.cpp rom_metadata.cpp romedit.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
