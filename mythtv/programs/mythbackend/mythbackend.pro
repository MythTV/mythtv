include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql

TEMPLATE = app
CONFIG += thread
TARGET = mythbackend
target.path = $${PREFIX}/bin
INSTALLS = target

setting.path = $${PREFIX}/share/mythtv/
setting.files += devicemaster.xml deviceslave.xml MXML_scpd.xml

INSTALLS += setting

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += autoexpire.h encoderlink.h filetransfer.h httpstatus.h mainserver.h
HEADERS += playbacksock.h scheduler.h server.h housekeeper.h backendutil.h
HEADERS += upnpcdstv.h upnpcdsmusic.h upnpcdsvideo.h mediaserver.h
HEADERS += internetContent.h main_helpers.h backendcontext.h

HEADERS += serviceHosts/mythServiceHost.h    serviceHosts/guideServiceHost.h
HEADERS += serviceHosts/contentServiceHost.h serviceHosts/dvrServiceHost.h

HEADERS += services/myth.h services/guide.h services/content.h services/dvr.h
HEADERS += services/serviceUtil.h

SOURCES += autoexpire.cpp encoderlink.cpp filetransfer.cpp httpstatus.cpp
SOURCES += main.cpp mainserver.cpp playbacksock.cpp scheduler.cpp server.cpp
SOURCES += housekeeper.cpp backendutil.cpp
SOURCES += upnpcdstv.cpp upnpcdsmusic.cpp upnpcdsvideo.cpp mediaserver.cpp
SOURCES += internetContent.cpp main_helpers.cpp backendcontext.cpp

SOURCES += services/myth.cpp services/guide.cpp services/content.cpp 
SOURCES += services/dvr.cpp
SOURCES += services/serviceUtil.cpp

using_oss:DEFINES += USING_OSS

using_dvb:DEFINES += USING_DVB

using_valgrind:DEFINES += USING_VALGRIND

