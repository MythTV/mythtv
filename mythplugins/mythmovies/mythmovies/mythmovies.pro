include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += xml sql

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmovies
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += moviesui.h helperobjects.h moviessettings.h

SOURCES += main.cpp moviesui.cpp moviessettings.cpp

include ( ../../libs-targetfix.pro )
