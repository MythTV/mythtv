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
uifiles_wide.path = $${PREFIX}/share/mythtv/themes/default-wide
uifiles_wide.files = theme-wide/zoneminder-ui.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += menufiles uifiles uifiles_wide installimages #installfiles 

# Input
HEADERS += zmconsole.h zmplayer.h zmevents.h zmliveplayer.h zmdefines.h 
HEADERS += zmsettings.h zmclient.h

SOURCES += main.cpp zmconsole.cpp zmplayer.cpp zmevents.cpp zmliveplayer.cpp 
SOURCES += zmsettings.cpp zmclient.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
#The following line was inserted by qt3to4
QT += opengl sql xml qt3support
