include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += sql

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythcontrols
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythui

# Input
HEADERS += action.h        actionid.h     mythcontrols.h
HEADERS += keybindings.h   keygrabber.h

SOURCES += action.cpp      actionset.cpp  mythcontrols.cpp
SOURCES += keybindings.cpp keygrabber.cpp
SOURCES += main.cpp

include ( ../../libs-targetfix.pro )
