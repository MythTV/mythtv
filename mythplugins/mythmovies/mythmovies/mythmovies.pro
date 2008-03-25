include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmovies
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = movie_settings.xml
uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = movies-ui.xml
wideuifiles.path = $${PREFIX}/share/mythtv/themes/default-wide
wideuifiles.files = theme-wide/movies-ui.xml

INSTALLS += uifiles wideuifiles installfiles

# Input
HEADERS += moviesui.h helperobjects.h moviessettings.h

SOURCES += main.cpp moviesui.cpp moviessettings.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

mingw:DEFINES += USING_MINGW
#The following line was inserted by qt3to4
QT += xml sql opengl qt3support
