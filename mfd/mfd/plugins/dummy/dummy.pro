include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-dummy

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target



HEADERS +=          dummy.h   
SOURCES += main.cpp dummy.cpp 


