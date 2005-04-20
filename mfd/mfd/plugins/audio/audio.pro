include (../../../settings.pro)
include (../../../options.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-audio

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          audio.h	  audiolistener.h   rtspout.h   livemedia.h
SOURCES += main.cpp audio.cpp audiolistener.cpp rtspout.cpp livemedia.cpp

HEADERS += daapinput.h   ../daapclient/daaprequest.h   ../daapclient/server_types.h 
SOURCES += daapinput.cpp ../daapclient/daaprequest.cpp

HEADERS += maopinstance.h
SOURCES += maopinstance.cpp

LIBS += -logg -lvorbisfile -lvorbis -mfdlib
LIBS += -lmad -lid3tag -lcdaudio -lFLAC -lcdda_interface -lcdda_paranoia

!isEmpty(MFD_RTSP_SUPPORT) {
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
}
