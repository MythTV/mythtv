include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythvideo
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = video-ui.xml
installfiles.path = $${PREFIX}/share/mythtv/
installfiles.files = videomenu.xml video_settings.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png
installscripts.path = $${PREFIX}/share/mythtv/mythvideo/scripts
installscripts.files = scripts/*

INSTALLS += installfiles uifiles installimages installscripts

# Input

HEADERS += metadata.h videomanager.h videobrowser.h videofilter.h dbcheck.h
HEADERS += globalsettings.h videotree.h fileassoc.h editmetadata.h
HEADERS += videogallery.h videoselected.h videodlg.h videoscan.h
HEADERS += videolist.h dbaccess.h quicksp.h metadatalistmanager.h
HEADERS += cleanup.h globals.h dirscan.h videoutils.h

SOURCES += main.cpp metadata.cpp videomanager.cpp videobrowser.cpp
SOURCES += videofilter.cpp dbcheck.cpp
SOURCES += globalsettings.cpp videotree.cpp fileassoc.cpp editmetadata.cpp
SOURCES += videogallery.cpp videoselected.cpp videodlg.cpp videoscan.cpp
SOURCES += videolist.cpp dbaccess.cpp metadatalistmanager.cpp
SOURCES += cleanup.cpp globals.cpp dirscan.cpp videoutils.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
    # need C99 flag for isnan() to work
    DEFINES += _GLIBCPP_USE_C99
}
