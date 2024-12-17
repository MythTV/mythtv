#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

#
# Enumerate all headers
#

set(HEADERS serviceexp.h service.h datacontracthelper.h)

set(DATACONTRACTS_HEADERS
    datacontracts/artworkInfo.h
    datacontracts/artworkInfoList.h
    datacontracts/backendInfo.h
    datacontracts/blurayInfo.h
    datacontracts/buildInfo.h
    datacontracts/captureCard.h
    datacontracts/captureCardList.h
    datacontracts/castMember.h
    datacontracts/castMemberList.h
    datacontracts/channelGroup.h
    datacontracts/channelGroupList.h
    datacontracts/channelInfoList.h
    datacontracts/connectionInfo.h
    datacontracts/cutList.h
    datacontracts/cutting.h
    datacontracts/databaseInfo.h
    datacontracts/encoder.h
    datacontracts/encoderList.h
    datacontracts/enum.h
    datacontracts/enumItem.h
    datacontracts/envInfo.h
    datacontracts/frontend.h
    datacontracts/frontendActionList.h
    datacontracts/frontendList.h
    datacontracts/frontendStatus.h
    datacontracts/genre.h
    datacontracts/genreList.h
    datacontracts/imageMetadataInfo.h
    datacontracts/imageMetadataInfoList.h
    datacontracts/imageSyncInfo.h
    datacontracts/input.h
    datacontracts/inputList.h
    datacontracts/labelValue.h
    datacontracts/lineup.h
    datacontracts/liveStreamInfo.h
    datacontracts/liveStreamInfoList.h
    datacontracts/logInfo.h
    datacontracts/logMessage.h
    datacontracts/logMessageList.h
    datacontracts/markup.h
    datacontracts/markupList.h
    datacontracts/musicMetadataInfo.h
    datacontracts/musicMetadataInfoList.h
    datacontracts/programAndChannel.h
    datacontracts/programGuide.h
    datacontracts/programList.h
    datacontracts/recRule.h
    datacontracts/recRuleFilter.h
    datacontracts/recRuleFilterList.h
    datacontracts/recRuleList.h
    datacontracts/recording.h
    datacontracts/settingList.h
    datacontracts/storageGroupDir.h
    datacontracts/storageGroupDirList.h
    datacontracts/timeZoneInfo.h
    datacontracts/titleInfo.h
    datacontracts/titleInfoList.h
    datacontracts/versionInfo.h
    datacontracts/videoLookupInfo.h
    datacontracts/videoLookupInfoList.h
    datacontracts/videoMetadataInfo.h
    datacontracts/videoMetadataInfoList.h
    datacontracts/videoMultiplex.h
    datacontracts/videoMultiplexList.h
    datacontracts/videoSource.h
    datacontracts/videoSourceList.h
    datacontracts/videoStreamInfo.h
    datacontracts/videoStreamInfoList.h
    datacontracts/wolInfo.h)

set(SERVICES_HEADERS
    services/captureServices.h
    services/channelServices.h
    services/contentServices.h
    services/dvrServices.h
    services/frontendServices.h
    services/guideServices.h
    services/imageServices.h
    services/musicServices.h
    services/mythServices.h
    services/rttiServices.h
    services/videoServices.h)

#
# Declare the library
#
add_library(mythservicecontracts ${DATACONTRACTS_HEADERS} ${HEADERS}
                                 ${SERVICES_HEADERS} service.cpp)

#
# All remaining target information
#

target_compile_definitions(mythservicecontracts PRIVATE SERVICE_API)

target_include_directories(
  mythservicecontracts
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(
  mythservicecontracts
  PUBLIC Qt${QT_VERSION_MAJOR}::Core
         $<$<BOOL:${USING_QTSCRIPT}>:Qt${QT_VERSION_MAJOR}::Script> mythbase)

#
# Installation section
#

install(TARGETS mythservicecontracts
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

#
# Copy three files in the libxxx directory, and then copy the entire contracts
# and services directories into the libxxx directory.
#
install(FILES ${HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythservicecontracts)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/datacontracts
                  ${CMAKE_CURRENT_SOURCE_DIR}/services
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythservicecontracts)
