include (../../../settings.pro)

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-dummy

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          dummy.h   ../../mfdplugin.h   ../../events.h
SOURCES += main.cpp dummy.cpp ../../mfdplugin.cpp ../../events.cpp


