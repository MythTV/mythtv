include (../settings.pro)

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}

include (../options.pro)

TEMPLATE = lib
CONFIG += thread dll
TARGET = mfdclient

target.path = $${PREFIX}/lib
INSTALLS = target

HEADERS += mfdinterface.h   discoverythread.h   mfdinstance.h \   
           ./mdnsd/mdnsd.h ./mdnsd/1035.h discovered.h \
           events.h   serviceclient.h   audioclient.h   metadataclient.h \
           ../mfdlib/httpoutrequest.h \
           ../mfdlib/httpinresponse.h \
           ../mfdlib/httpheader.h \
           ../mfdlib/httpgetvar.h \
           mdcaprequest.h mdcapresponse.h metadatacollection.h ../mfdlib/metadata.h \
           playlist.h playlistentry.h mfdcontent.h playlistchecker.h speakertracker.h \
           visualbase.h transcoderclient.h jobtracker.h

SOURCES += mfdinterface.cpp discoverythread.cpp mfdinstance.cpp \
           ./mdnsd/mdnsd.c ./mdnsd/1035.c discovered.cpp \
           events.cpp serviceclient.cpp audioclient.cpp metadataclient.cpp \
           ../mfdlib/httpoutrequest.cpp \
           ../mfdlib/httpinresponse.cpp \
           ../mfdlib/httpheader.cpp \
           ../mfdlib/httpgetvar.cpp \
           mdcaprequest.cpp mdcapresponse.cpp metadatacollection.cpp ../mfdlib/metadata.cpp \
           playlist.cpp playlistentry.cpp mfdcontent.cpp playlistchecker.cpp speakertracker.cpp \
           visualbase.cpp transcoderclient.cpp jobtracker.cpp

inc.path = $${PREFIX}/include/mfdclient/
inc.files  = mfdinterface.h mfdcontent.h ../mfdlib/metadata.h playlist.h playlistentry.h \
			 speakertracker.h visualbase.h jobtracker.h

LIBS += -L../mdcaplib -lmdcap -L../mtcplib -lmtcp -Wl,--export-dynamic

INSTALLS += inc

!isEmpty(MFD_RTSP_SUPPORT) {
HEADERS += rtspclient.h rtspin.h 
SOURCES +=  rtspclient.cpp rtspin.cpp 
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

