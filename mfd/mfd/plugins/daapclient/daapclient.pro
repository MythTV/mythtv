include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread

QMAKE_CXXFLAGS_RELEASE  += -Wno-multichar 
QMAKE_CXXFLAGS_DEBUG    += -Wno-multichar

TARGET = mfdplugin-daapclient

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target



HEADERS +=          daapclient.h   daapinstance.h   daaprequest.h
SOURCES += main.cpp daapclient.cpp daapinstance.cpp daaprequest.cpp

HEADERS += daapresponse.h   database.h   server_types.h
SOURCES += daapresponse.cpp database.cpp

LIBS += -L../daapserver/daaplib/ -ldaaplib