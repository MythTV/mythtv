include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql script
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

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
HEADERS += playbacksock.h scheduler.h server.h backendhousekeeper.h
HEADERS += backendutil.h
HEADERS += upnpcdstv.h upnpcdsmusic.h upnpcdsvideo.h mediaserver.h
HEADERS += internetContent.h main_helpers.h backendcontext.h
HEADERS += httpconfig.h mythsettings.h commandlineparser.h

HEADERS += serviceHosts/mythServiceHost.h    serviceHosts/guideServiceHost.h
HEADERS += serviceHosts/contentServiceHost.h serviceHosts/dvrServiceHost.h
HEADERS += serviceHosts/channelServiceHost.h serviceHosts/videoServiceHost.h
HEADERS += serviceHosts/captureServiceHost.h

HEADERS += services/myth.h services/guide.h services/content.h services/dvr.h
HEADERS += services/serviceUtil.h services/channel.h services/video.h
HEADERS += services/capture.h

SOURCES += autoexpire.cpp encoderlink.cpp filetransfer.cpp httpstatus.cpp
SOURCES += main.cpp mainserver.cpp playbacksock.cpp scheduler.cpp server.cpp
SOURCES += backendhousekeeper.cpp backendutil.cpp
SOURCES += upnpcdstv.cpp upnpcdsmusic.cpp upnpcdsvideo.cpp mediaserver.cpp
SOURCES += internetContent.cpp main_helpers.cpp backendcontext.cpp
SOURCES += httpconfig.cpp mythsettings.cpp commandlineparser.cpp

SOURCES += services/myth.cpp services/guide.cpp services/content.cpp 
SOURCES += services/dvr.cpp services/channel.cpp services/video.cpp
SOURCES += services/serviceUtil.cpp services/capture.cpp

using_oss:DEFINES += USING_OSS

using_dvb:DEFINES += USING_DVB

using_valgrind:DEFINES += USING_VALGRIND

using_libdns_sd:DEFINES += USING_LIBDNS_SD

xml_conf.path = $${PREFIX}/share/mythtv/backend-config/
xml_conf.files = config_backend_general.xml config_backend_database.xml

INSTALLS += xml_conf
