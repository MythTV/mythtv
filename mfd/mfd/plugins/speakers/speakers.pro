include (../../../settings.pro)
include (../../../options.pro)

INCLUDEPATH += ../../../mfdlib

INCLUDEPATH += $${LIVEMEDIAPREFIX}/liveMedia
INCLUDEPATH += $${LIVEMEDIAPREFIX}/groupsock
INCLUDEPATH += $${LIVEMEDIAPREFIX}/UsageEnvironment
INCLUDEPATH += $${LIVEMEDIAPREFIX}/BasicUsageEnvironment

LIBS += -lliveMedia
LIBS += -lgroupsock
LIBS += -lBasicUsageEnvironment
LIBS += -lUsageEnvironment

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-speakers

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          speakers.h	 rtspin.h	livemedia.h
SOURCES += main.cpp speakers.cpp rtspin.cpp livemedia.cpp



