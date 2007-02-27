include ( ../../mythconfig.mak )
include ( ../../settings.pro )
 
TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythzoneminder
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

menufiles.path = $${PREFIX}/share/mythtv
menufiles.files = zonemindermenu.xml
uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = zoneminder-ui.xml
#installfiles.path = $${PREFIX}/share/mythtv/mythnews
#installfiles.files = news-sites.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += menufiles uifiles installimages #installfiles 

# Input
HEADERS += zmconsole.h zmplayer.h zmevents.h zmliveplayer.h zmdefines.h 
HEADERS += zmsettings.h zmclient.h

SOURCES += main.cpp zmconsole.cpp zmplayer.cpp zmevents.cpp zmliveplayer.cpp 
SOURCES += zmsettings.cpp zmclient.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
