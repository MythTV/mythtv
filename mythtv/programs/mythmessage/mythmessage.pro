include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network sql

TEMPLATE = app
CONFIG += thread
TARGET = mythmessage
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += $$EXTRA_LIBS 
INCLUDEPATH += ../../libs/libmythbase

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += commandlineparser.h
SOURCES += main.cpp commandlineparser.cpp

mingw: LIBS += -lwinmm -lws2_32
