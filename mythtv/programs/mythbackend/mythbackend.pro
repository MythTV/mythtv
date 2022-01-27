include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql widgets
contains(QT_MAJOR_VERSION, 5): QT += script
mingw | win32-msvc* {
   # script debugger currently only enabled for WIN32 builds
   QT += scripttools
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
HEADERS += upnpcdstv.h upnpcdsmusic.h upnpcdsvideo.h mediaserver.h
HEADERS += internetContent.h main_helpers.h backendcontext.h
HEADERS += httpconfig.h mythsettings.h commandlineparser.h
HEADERS += recordingextender.h

HEADERS += serviceHosts/mythServiceHost.h    serviceHosts/guideServiceHost.h
HEADERS += serviceHosts/contentServiceHost.h serviceHosts/dvrServiceHost.h
HEADERS += serviceHosts/channelServiceHost.h serviceHosts/videoServiceHost.h
HEADERS += serviceHosts/captureServiceHost.h serviceHosts/imageServiceHost.h
HEADERS += serviceHosts/musicServiceHost.h

HEADERS += services/myth.h services/guide.h services/content.h services/dvr.h
HEADERS += services/serviceUtil.h services/channel.h services/video.h
HEADERS += services/capture.h services/image.h services/music.h


SOURCES += autoexpire.cpp encoderlink.cpp filetransfer.cpp httpstatus.cpp
SOURCES += main.cpp mainserver.cpp playbacksock.cpp scheduler.cpp server.cpp
SOURCES += backendhousekeeper.cpp
SOURCES += upnpcdstv.cpp upnpcdsmusic.cpp upnpcdsvideo.cpp mediaserver.cpp
SOURCES += internetContent.cpp main_helpers.cpp backendcontext.cpp
SOURCES += httpconfig.cpp mythsettings.cpp commandlineparser.cpp
SOURCES += recordingextender.cpp

SOURCES += services/myth.cpp services/guide.cpp services/content.cpp 
SOURCES += services/dvr.cpp services/channel.cpp services/video.cpp
SOURCES += services/serviceUtil.cpp services/capture.cpp
SOURCES += services/image.cpp services/music.cpp

HEADERS += servicesv2/v2myth.h servicesv2/v2connectionInfo.h servicesv2/v2wolInfo.h
HEADERS += servicesv2/v2databaseInfo.h servicesv2/v2versionInfo.h
HEADERS += servicesv2/v2storageGroupDir.h servicesv2/v2storageGroupDirList.h
HEADERS += servicesv2/v2timeZoneInfo.h
HEADERS += servicesv2/v2LabelValue.h servicesv2/v2logMessage.h servicesv2/v2logMessageList.h
HEADERS += servicesv2/v2frontend.h servicesv2/v2frontendList.h servicesv2/v2settingList.h
HEADERS += servicesv2/v2backendInfo.h servicesv2/v2buildInfo.h servicesv2/v2envInfo.h
HEADERS += servicesv2/v2logInfo.h

HEADERS += servicesv2/v2video.h
HEADERS += servicesv2/v2videoMetadataInfo.h servicesv2/v2videoMetadataInfoList.h
HEADERS += servicesv2/v2artworkInfo.h servicesv2/v2artworkInfoList.h
HEADERS += servicesv2/v2genre.h servicesv2/v2genreList.h
HEADERS += servicesv2/v2castMember.h servicesv2/v2castMemberList.h
HEADERS += servicesv2/v2videoLookupInfo.h servicesv2/v2videoLookupInfoList.h
HEADERS += servicesv2/v2videoStreamInfo.h servicesv2/v2videoStreamInfoList.h
HEADERS += servicesv2/v2blurayInfo.h
HEADERS += servicesv2/v2country.h servicesv2/v2countryList.h
HEADERS += servicesv2/v2language.h servicesv2/v2languageList.h
HEADERS += servicesv2/v2databaseStatus.h


HEADERS += servicesv2/v2dvr.h servicesv2/v2recording.h
HEADERS += servicesv2/v2programAndChannel.h servicesv2/v2programList.h
HEADERS += servicesv2/v2channelGroup.h servicesv2/v2channelGroupList.h
HEADERS += servicesv2/v2recRule.h
HEADERS += servicesv2/v2cutting.h servicesv2/v2cutList.h
HEADERS += servicesv2/v2markup.h servicesv2/v2markupList.h
HEADERS += servicesv2/v2encoder.h servicesv2/v2encoderList.h
HEADERS += servicesv2/v2input.h servicesv2/v2inputList.h
HEADERS += servicesv2/v2recRuleFilter.h servicesv2/v2recRuleFilterList.h
HEADERS += servicesv2/v2titleInfo.h servicesv2/v2titleInfoList.h
HEADERS += servicesv2/v2recRuleList.h
HEADERS += servicesv2/v2content.h
HEADERS += servicesv2/v2guide.h servicesv2/v2programGuide.h
HEADERS += servicesv2/v2channel.h
HEADERS += servicesv2/v2channelInfoList.h servicesv2/v2lineup.h
HEADERS += servicesv2/v2videoSource.h servicesv2/v2videoSourceList.h
HEADERS += servicesv2/v2videoMultiplex.h servicesv2/v2videoMultiplexList.h
HEADERS += servicesv2/v2status.h
HEADERS += servicesv2/preformat.h servicesv2/v2backendStatus.h
HEADERS += servicesv2/v2capture.h
HEADERS += servicesv2/v2captureCard.h servicesv2/v2captureCardList.h
HEADERS += servicesv2/v2music.h
HEADERS += servicesv2/v2musicMetadataInfo.h servicesv2/v2musicMetadataInfoList.h
HEADERS += servicesv2/v2config.h

SOURCES += servicesv2/v2myth.cpp
SOURCES += servicesv2/v2video.cpp
SOURCES += servicesv2/v2dvr.cpp
SOURCES += servicesv2/v2content.cpp
SOURCES += servicesv2/v2guide.cpp
SOURCES += servicesv2/v2channel.cpp
SOURCES += servicesv2/v2status.cpp
SOURCES += servicesv2/v2capture.cpp
SOURCES += servicesv2/v2music.cpp
SOURCES += servicesv2/v2serviceUtil.cpp
SOURCES += servicesv2/v2config.cpp

using_oss:DEFINES += USING_OSS

using_dvb:DEFINES += USING_DVB

using_valgrind:DEFINES += USING_VALGRIND

using_libdns_sd:DEFINES += USING_LIBDNS_SD

xml_conf.path = $${PREFIX}/share/mythtv/backend-config/
xml_conf.files = config_backend_general.xml config_backend_database.xml

INSTALLS += xml_conf
