include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmovies
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += moviesui.h helperobjects.h moviessettings.h

SOURCES += main.cpp moviesui.cpp moviessettings.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

mingw:DEFINES += USING_MINGW
#The following line was inserted by qt3to4
QT += xml sql opengl qt3support
