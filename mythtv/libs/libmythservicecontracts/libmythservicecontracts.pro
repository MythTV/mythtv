include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythservicecontracts-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += SERVICE_API

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += version.cpp

# Input

HEADERS += serviceexp.h service.h datacontracthelper.h

HEADERS += services/mythServices.h    services/guideServices.h
HEADERS += services/contentServices.h services/dvrServices.h
HEADERS += services/channelServices.h services/videoServices.h

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
HEADERS += datacontracts/lineup.h

SOURCES += service.cpp

INCLUDEPATH += ./datacontracts
INCLUDEPATH += ./services

# needed only for enums
INCLUDEPATH += ../libmyth

LIBS += $$EXTRA_LIBS

inc.path = $${PREFIX}/include/mythtv/libmythservicecontracts/
inc.files = serviceexp.h service.h datacontracthelper.h

incServices.path = $${PREFIX}/include/mythtv/libmythservicecontracts/services/
incServices.files  = services/mythServices.h    services/guideServices.h
incServices.files += services/contentServices.h services/dvrServices.h
incServices.files += services/channelServices.h services/videoServices.h

incDatacontracts.path = $${PREFIX}/include/mythtv/libmythservicecontracts/datacontracts/
incDatacontracts.files  = datacontracts/connectionInfo.h      datacontracts/databaseInfo.h
incDatacontracts.files += datacontracts/programAndChannel.h   datacontracts/programGuide.h
incDatacontracts.files += datacontracts/recording.h           datacontracts/settingList.h
incDatacontracts.files += datacontracts/wolInfo.h             datacontracts/channelInfoList.h
incDatacontracts.files += datacontracts/videoSource.h         datacontracts/videoSourceList.h
incDatacontracts.files += datacontracts/videoMultiplex.h      datacontracts/videoMultiplexList.h
incDatacontracts.files += datacontracts/videoMetadataInfo.h   datacontracts/videoMetadataInfoList.h
incDatacontracts.files += datacontracts/blurayInfo.h          datacontracts/videoLookupInfo.h
incDatacontracts.files += datacontracts/timeZoneInfo.h        datacontracts/videoLookupInfoList.h
incDatacontracts.files += datacontracts/versionInfo.h         datacontracts/lineup.h

INSTALLS += inc incServices incDatacontracts

macx {
    QMAKE_LFLAGS_SHLIB += -flat_namespace
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$LATE_LIBS
