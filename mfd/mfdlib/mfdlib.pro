include (../settings.pro)

TEMPLATE = lib
CONFIG += thread dll
TARGET = mfdlib

target.path = $${PREFIX}/lib
INSTALLS += target

HEADERS += mfd_events.h   mfd_plugin.h
SOURCES += mfd_events.cpp mfd_plugin.cpp


