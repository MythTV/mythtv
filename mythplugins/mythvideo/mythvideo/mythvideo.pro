include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythvideo

target.path = $${LIBDIR}/mythtv/plugins

installscripts.path = $${PREFIX}/share/mythtv/mythvideo/scripts
installscripts.files = scripts/*.pl scripts/*.py scripts/README scripts/jamu-example.conf scripts/jamu.README
installscriptsjamumods.path = $${PREFIX}/share/mythtv/mythvideo/scripts/ttvdb                                           
installscriptsjamumods.files = scripts/Television/ttvdb/*.py scripts/Television/ttvdb/*.conf
installscriptmodules.path = $${PREFIX}/share/mythtv/mythvideo/scripts/Movie/MythTV
installscriptmodules.files = scripts/MythTV/MythVideoCommon.pm

INSTALLS += installscripts installscriptsjamumods installscriptmodules target

# Input

HEADERS += metadata.h videofilter.h dbcheck.h
HEADERS += globalsettings.h fileassoc.h editmetadata.h
HEADERS += videodlg.h videopopups.h videoscan.h
HEADERS += videolist.h dbaccess.h quicksp.h metadatalistmanager.h
HEADERS += cleanup.h globals.h dirscan.h videoutils.h
HEADERS += parentalcontrols.h playercommand.h playersettings.h
HEADERS += metadatasettings.h

#MythDVD
HEADERS += dvdripbox.h dvdinfo.h titledialog.h


SOURCES += main.cpp metadata.cpp
SOURCES += videofilter.cpp dbcheck.cpp cleanup.cpp
SOURCES += globalsettings.cpp fileassoc.cpp editmetadata.cpp
SOURCES += videodlg.cpp videopopups.cpp videoscan.cpp
SOURCES += videolist.cpp dbaccess.cpp metadatalistmanager.cpp
SOURCES += globals.cpp dirscan.cpp videoutils.cpp
SOURCES += parentalcontrols.cpp playercommand.cpp
SOURCES += playersettings.cpp metadatasettings.cpp

#MythDVD
SOURCES += dvdripbox.cpp dvdinfo.cpp titledialog.cpp

mingw:DEFINES += USING_MINGW

QT += sql xml network

include ( ../../libs-targetfix.pro )
