#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

set(HEADERS_TO_INSTALL
    cleanup.h
    bluraymetadata.h
    dbaccess.h
    dirscan.h
    globals.h
    imagemanager.h
    imagemetadata.h
    imagetypes.h
    lyricsdata.h
    metadatacommon.h
    metadatadownload.h
    metadatafactory.h
    metadatagrabber.h
    metadataimagedownload.h
    metaio.h
    metaioavfcomment.h
    metaioflacvorbis.h
    metaioid3.h
    metaiomp4.h
    metaiooggvorbis.h
    metaiotaglib.h
    metaiowavpack.h
    musicfilescanner.h
    musicmetadata.h
    musicutils.h
    mythmetaexp.h
    mythuiimageresults.h
    mythuimetadataresults.h
    parentalcontrols.h
    quicksp.h
    videometadata.h
    videometadatalistmanager.h
    videoscan.h
    videoutils.h)

add_library(
  mythmetadata
  ${HEADERS_TO_INSTALL}
  bluraymetadata.cpp
  cleanup.cpp
  dbaccess.cpp
  dirscan.cpp
  globals.cpp
  imagemanager.cpp
  imagemetadata.cpp
  imagescanner.cpp
  imagethumbs.cpp
  lyricsdata.cpp
  metadatacommon.cpp
  metadatadownload.cpp
  metadatafactory.cpp
  metadatagrabber.cpp
  metadataimagedownload.cpp
  metaio.cpp
  metaioavfcomment.cpp
  metaioflacvorbis.cpp
  metaioid3.cpp
  metaiomp4.cpp
  metaiooggvorbis.cpp
  metaiotaglib.cpp
  metaiowavpack.cpp
  musicfilescanner.cpp
  musicmetadata.cpp
  musicutils.cpp
  mythuiimageresults.cpp
  mythuimetadataresults.cpp
  parentalcontrols.cpp
  videometadata.cpp
  videometadatalistmanager.cpp
  videoscan.cpp
  videoutils.cpp)

target_compile_definitions(mythmetadata PRIVATE META_API)

target_include_directories(
  mythmetadata
  PRIVATE .
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(
  mythmetadata
  PUBLIC Qt${QT_VERSION_MAJOR}::Core
         Qt${QT_VERSION_MAJOR}::Network
         Qt${QT_VERSION_MAJOR}::Sql
         Qt${QT_VERSION_MAJOR}::Xml
         Qt${QT_VERSION_MAJOR}::Widgets
         PkgConfig::EXIV2
         PkgConfig::TAGLIB
         mythbluray
         PkgConfig::LIBAVCODEC
         PkgConfig::LIBAVFORMAT
         PkgConfig::LIBAVUTIL
         PkgConfig::LIBSWRESAMPLE
         PkgConfig::LIBSWSCALE
         mythbase
         mythui
         mythtv
         myth)

install(TARGETS mythmetadata LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${HEADERS_TO_INSTALL}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythmetadata)
