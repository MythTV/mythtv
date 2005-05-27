include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread

TARGET = mfdplugin-transcoder

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          transcoder.h
SOURCES += main.cpp transcoder.cpp

#	LIBS += -L./daaplib/ -ldaaplib -lid3tag -lz 

