include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( config.pro )

!exists( config.pro ) {
    error(Missing config.pro: please run the configure script)
}

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythdvd
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = dvdmenu.xml dvd_settings.xml
uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = dvd-ui.xml images/*.png

INSTALLS += installfiles uifiles

LIBS += 

HEADERS += config.h settings.h dbcheck.h

SOURCES += main.cpp settings.cpp dbcheck.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
