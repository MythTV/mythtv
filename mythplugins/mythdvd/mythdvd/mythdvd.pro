include ( ../../mythconfig.mak )
include ( ../../settings.pro )

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

HEADERS += settings.h dbcheck.h dvdripbox.h dvdinfo.h titledialog.h

SOURCES += main.cpp settings.cpp dbcheck.cpp dvdripbox.cpp dvdinfo.cpp 
SOURCES += titledialog.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
