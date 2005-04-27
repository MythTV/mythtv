include (../../../settings.pro)
include (../../../options.pro)

INCLUDEPATH += ../../../mfdlib

INCLUDEPATH += $${LIVEMEDIAPREFIX}/liveMedia
INCLUDEPATH += $${LIVEMEDIAPREFIX}/groupsock
INCLUDEPATH += $${LIVEMEDIAPREFIX}/UsageEnvironment
INCLUDEPATH += $${LIVEMEDIAPREFIX}/BasicUsageEnvironment
INCLUDEPATH += $${LIVEMEDIAPREFIX}/liveMedia/include
INCLUDEPATH += $${LIVEMEDIAPREFIX}/groupsock/include
INCLUDEPATH += $${LIVEMEDIAPREFIX}/UsageEnvironment/include
INCLUDEPATH += $${LIVEMEDIAPREFIX}/BasicUsageEnvironment/include
LIBS += -L$${LIVEMEDIAPREFIX}/liveMedia
LIBS += -L$${LIVEMEDIAPREFIX}/groupsock
LIBS += -L$${LIVEMEDIAPREFIX}/UsageEnvironment
LIBS += -L$${LIVEMEDIAPREFIX}/BasicUsageEnvironment
LIBS += -lliveMedia
LIBS += -lgroupsock
LIBS += -lBasicUsageEnvironment
LIBS += -lUsageEnvironment

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-speakers

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          speakers.h	 rtspin.h
SOURCES += main.cpp speakers.cpp rtspin.cpp



