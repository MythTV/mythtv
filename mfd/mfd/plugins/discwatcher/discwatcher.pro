include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib
INCLUDEPATH *= /usr/include/cdda

TEMPLATE = lib
CONFIG += plugin thread

TARGET = mfdplugin-discwatcher

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          discwatcher.h   devicewatcher.h
SOURCES += main.cpp discwatcher.cpp devicewatcher.cpp


