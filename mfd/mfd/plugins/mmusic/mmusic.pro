include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread

TARGET = mfdplugin-mmusic

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          mmusic.h         
SOURCES += main.cpp mmusic.cpp
