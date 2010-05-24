include ( ../../settings.pro )
include ( ../programs-libs.pro )

QT += network

TEMPLATE = app
CONFIG += thread
TARGET = mythtvosd
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += $$EXTRA_LIBS 
INCLUDEPATH += ../../libs/libmythdb

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp

mingw: LIBS += -lpthread -lwinmm -lws2_32
