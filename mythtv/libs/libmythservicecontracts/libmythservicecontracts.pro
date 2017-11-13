include ( ../../settings.pro )

QT += script

TEMPLATE = lib
TARGET = mythservicecontracts-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += SERVICE_API

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += version.cpp

INCLUDEPATH += ../libmythbase
DEPENDPATH += ../libmythbase
LIBS += -L../libmythbase -lmythbase-$${LIBVERSION}

# Input

HEADERS += serviceexp.h service.h datacontracthelper.h

HEADERS += services/mythServices.h    services/guideServices.h
HEADERS += services/contentServices.h services/dvrServices.h
HEADERS += services/channelServices.h services/videoServices.h
HEADERS += services/captureServices.h services/musicServices.h
HEADERS += services/frontendServices.h
HEADERS += services/imageServices.h
HEADERS += services/rttiServices.h

HEADERS += datacontracts/enum.h                  datacontracts/enumItem.h
HEADERS += datacontracts/connectionInfo.h        datacontracts/databaseInfo.h
HEADERS += datacontracts/programAndChannel.h     datacontracts/programGuide.h
HEADERS += datacontracts/recording.h             datacontracts/settingList.h
HEADERS += datacontracts/wolInfo.h               datacontracts/programList.h
HEADERS += datacontracts/encoder.h               datacontracts/encoderList.h
HEADERS += datacontracts/storageGroupDir.h       datacontracts/storageGroupDirList.h
HEADERS += datacontracts/channelInfoList.h       datacontracts/videoSource.h
HEADERS += datacontracts/videoSourceList.h       datacontracts/videoMultiplex.h
HEADERS += datacontracts/videoMultiplexList.h    datacontracts/videoMetadataInfo.h
HEADERS += datacontracts/videoMetadataInfoList.h datacontracts/blurayInfo.h
HEADERS += datacontracts/timeZoneInfo.h          datacontracts/videoLookupInfo.h
HEADERS += datacontracts/videoLookupInfoList.h   datacontracts/versionInfo.h
HEADERS += datacontracts/lineup.h                datacontracts/captureCard.h
HEADERS += datacontracts/captureCardList.h       datacontracts/recRule.h
HEADERS += datacontracts/recRuleList.h           datacontracts/artworkInfo.h
HEADERS += datacontracts/artworkInfoList.h       datacontracts/frontendStatus.h
HEADERS += datacontracts/frontendActionList.h
HEADERS += datacontracts/liveStreamInfo.h        datacontracts/liveStreamInfoList.h
HEADERS += datacontracts/titleInfo.h             datacontracts/titleInfoList.h
HEADERS += datacontracts/labelValue.h
HEADERS += datacontracts/logMessage.h            datacontracts/logMessageList.h
HEADERS += datacontracts/imageMetadataInfoList.h datacontracts/imageMetadataInfo.h
HEADERS += datacontracts/imageSyncInfo.h         datacontracts/channelGroup.h
HEADERS += datacontracts/channelGroupList.h      datacontracts/input.h
HEADERS += datacontracts/inputList.h
HEADERS += datacontracts/recRuleFilter.h         datacontracts/recRuleFilterList.h
HEADERS += datacontracts/castMember.h            datacontracts/castMemberList.h
HEADERS += datacontracts/frontend.h              datacontracts/frontendList.h
HEADERS += datacontracts/cutting.h               datacontracts/cutList.h
HEADERS += datacontracts/backendInfo.h           datacontracts/envInfo.h
HEADERS += datacontracts/buildInfo.h             datacontracts/logInfo.h
HEADERS += datacontracts/genre.h                 datacontracts/genreList.h
HEADERS += datacontracts/musicMetadataInfo.h     datacontracts/musicMetadataInfoList.h

HEADERS += enums/recStatus.h

SOURCES += service.cpp
SOURCES += enums/recStatus.cpp

INCLUDEPATH += ./enums
INCLUDEPATH += ./datacontracts
INCLUDEPATH += ./services

# Dirty hack to prevent cross dependencies.
# needed only for enums in programtypes.h, recordingtypes.h
INCLUDEPATH += ../libmyth
HEADERS += ../libmyth/programtypes.h ../libmyth/recordingtypes.h
SOURCES += ../libmyth/programtypes.cpp ../libmyth/recordingtypes.cpp

INCLUDEPATH += $$DEPENDPATH

inc.path = $${PREFIX}/include/mythtv/libmythservicecontracts/
inc.files = serviceexp.h service.h datacontracthelper.h

incServices.path = $${PREFIX}/include/mythtv/libmythservicecontracts/services/
incServices.files  = services/mythServices.h    services/guideServices.h
incServices.files += services/contentServices.h services/dvrServices.h
incServices.files += services/channelServices.h services/videoServices.h
incServices.files += services/captureServices.h services/musicServices.h
incServices.files += services/frontendServices.h
incServices.files += services/imageServices.h
incServices.files += services/rttiServices.h

incEnums.path = $${PREFIX}/include/mythtv/libmythservicecontracts/enums/
incEnums.files = enums/recStatus.h

incDatacontracts.path = $${PREFIX}/include/mythtv/libmythservicecontracts/datacontracts/
incDatacontracts.files  = datacontracts/connectionInfo.h      datacontracts/databaseInfo.h
incDatacontracts.files += datacontracts/programAndChannel.h   datacontracts/programGuide.h
incDatacontracts.files += datacontracts/recording.h           datacontracts/settingList.h
incDatacontracts.files += datacontracts/wolInfo.h             datacontracts/channelInfoList.h
incDatacontracts.files += datacontracts/videoSource.h         datacontracts/videoSourceList.h
incDatacontracts.files += datacontracts/videoMultiplex.h      datacontracts/videoMultiplexList.h
incDatacontracts.files += datacontracts/videoMetadataInfo.h   datacontracts/videoMetadataInfoList.h
incDatacontracts.files += datacontracts/musicMetadataInfo.h   datacontracts/musicMetadataInfoList.h
incDatacontracts.files += datacontracts/blurayInfo.h          datacontracts/videoLookupInfo.h
incDatacontracts.files += datacontracts/timeZoneInfo.h        datacontracts/videoLookupInfoList.h
incDatacontracts.files += datacontracts/versionInfo.h         datacontracts/lineup.h
incDatacontracts.files += datacontracts/captureCard.h         datacontracts/captureCardList.h
incDatacontracts.files += datacontracts/recRule.h             datacontracts/recRuleList.h
incDatacontracts.files += datacontracts/artworkInfo.h         datacontracts/artworkInfoList.h
incDatacontracts.files += datacontracts/frontendStatus.h      datacontracts/frontendActionList.h
incDatacontracts.files += datacontracts/liveStreamInfo.h      datacontracts/liveStreamInfoList.h
incDatacontracts.files += datacontracts/titleInfo.h           datacontracts/titleInfoList.h
incDatacontracts.files += datacontracts/labelValue.h
incDatacontracts.files += datacontracts/logMessage.h          datacontracts/logMessageList.h
incDatacontracts.files += datacontracts/imageMetadataInfoList.h datacontracts/imageMetadataInfo.h
incDatacontracts.files += datacontracts/imageSyncInfo.h       datacontracts/channelGroup.h
incDatacontracts.files += datacontracts/channelGroupList.h    datacontracts/input.h
incDatacontracts.files += datacontracts/inputList.h
incDatacontracts.files += datacontracts/recRuleFilter.h       datacontracts/recRuleFilterList.h
incDatacontracts.files += datacontracts/castMember.h          datacontracts/castMemberList.h
incDatacontracts.files += datacontracts/enum.h                datacontracts/enumItem.h
incDatacontracts.files += datacontracts/cutting.h             datacontracts/cutList.h
incDatacontracts.files += datacontracts/backendInfo.h         datacontracts/envInfo.h
incDatacontracts.files += datacontracts/buildInfo.h           datacontracts/logInfo.h

INSTALLS += inc incServices incDatacontracts incEnums

macx {
    QMAKE_LFLAGS_SHLIB += -flat_namespace
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

android {
    DEPENDPATH += ../libmythbase
    INCLUDEPATH += ../libmythbase
    HEADERS += ../libmyth/programtypes.h
    SOURCES += ../libmyth/programtypes.cpp
    LIBS += -L../libmythbase -lmythbase-$${LIBVERSION}
}
