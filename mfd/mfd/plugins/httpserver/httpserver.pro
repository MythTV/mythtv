include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread

TARGET = mfdplugin-httpserver

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          httpserver.h
SOURCES += main.cpp httpserver.cpp


