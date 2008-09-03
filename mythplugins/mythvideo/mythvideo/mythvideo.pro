include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )
 
TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythvideo

target.path = $${LIBDIR}/mythtv/plugins

installscripts.path = $${PREFIX}/share/mythtv/mythvideo/scripts
installscripts.files = scripts/*.pl scripts/*.py scripts/README

installscriptmodules.path = $${PREFIX}/share/mythtv/mythvideo/scripts/MythTV
installscriptmodules.files = scripts/MythTV/MythVideoCommon.pm
 
INSTALLS += installscripts installscriptmodules target

# Input

HEADERS += metadata.h videomanager.h videobrowser.h videofilter.h dbcheck.h
HEADERS += globalsettings.h videotree.h fileassoc.h editmetadata.h
HEADERS += videogallery.h videoselected.h videodlg.h videoscan.h
HEADERS += videolist.h dbaccess.h quicksp.h metadatalistmanager.h
HEADERS += cleanup.h globals.h dirscan.h videoutils.h imagecache.h
HEADERS += parentalcontrols.h videoui-common.h

#MythDVD
HEADERS += dvdripbox.h dvdinfo.h titledialog.h


SOURCES += main.cpp metadata.cpp videomanager.cpp videobrowser.cpp
SOURCES += videofilter.cpp dbcheck.cpp cleanup.cpp 
SOURCES += globalsettings.cpp videotree.cpp fileassoc.cpp editmetadata.cpp
SOURCES += videogallery.cpp videoselected.cpp videodlg.cpp videoscan.cpp
SOURCES += videolist.cpp dbaccess.cpp metadatalistmanager.cpp
SOURCES += globals.cpp dirscan.cpp videoutils.cpp imagecache.cpp
SOURCES += parentalcontrols.cpp videoui-common.cpp

#MythDVD
SOURCES += dvdripbox.cpp dvdinfo.cpp titledialog.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
    # need C99 flag for isnan() to work
    DEFINES += _GLIBCPP_USE_C99
}

mingw:DEFINES += USING_MINGW

#The following line was inserted by qt3to4
QT += opengl sql xml qt3support
