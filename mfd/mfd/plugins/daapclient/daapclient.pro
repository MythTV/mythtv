include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-daapclient

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target



HEADERS +=          daapclient.h   
SOURCES += main.cpp daapclient.cpp 


