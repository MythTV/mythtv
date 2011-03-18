include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network

TEMPLATE = app
CONFIG += thread
TARGET = mythmessage
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += $$EXTRA_LIBS 
INCLUDEPATH += ../../libs/libmythbase

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp

mingw: LIBS += -lwinmm -lws2_32
