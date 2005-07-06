include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgame
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = game_settings.xml
uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = game-ui.xml

INSTALLS += uifiles installfiles

# Input
HEADERS += gamehandler.h rominfo.h unzip.h gamesettings.h gametree.h

SOURCES += main.cpp gamehandler.cpp rominfo.cpp gametree.cpp unzip.c
SOURCES += gamesettings.cpp dbcheck.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
