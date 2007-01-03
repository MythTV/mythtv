include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythvideo
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = video-ui.xml
uifiles.files += dvd-ui.xml

uifiles_wide.path = $${PREFIX}/share/mythtv/themes/default-wide
uifiles_wide.files = theme-wide/video-ui.xml
uifiles_wide.files += theme-wide/dvd-ui.xml

installfiles.path = $${PREFIX}/share/mythtv/
installfiles.files = videomenu.xml video_settings.xml
installfiles.files += dvdmenu.xml dvd_settings.xml

installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

installscripts.path = $${PREFIX}/share/mythtv/mythvideo/scripts
installscripts.files = scripts/*

INSTALLS += installfiles uifiles uifiles_wide installimages installscripts

# Input

HEADERS += metadata.h videomanager.h videobrowser.h videofilter.h dbcheck.h
HEADERS += globalsettings.h videotree.h fileassoc.h editmetadata.h
HEADERS += videogallery.h videoselected.h videodlg.h videoscan.h
HEADERS += videolist.h dbaccess.h quicksp.h metadatalistmanager.h
HEADERS += cleanup.h globals.h dirscan.h videoutils.h imagecache.h

#MythDVD
HEADERS += dvdripbox.h dvdinfo.h titledialog.h


SOURCES += main.cpp metadata.cpp videomanager.cpp videobrowser.cpp
SOURCES += videofilter.cpp dbcheck.cpp cleanup.cpp 
SOURCES += globalsettings.cpp videotree.cpp fileassoc.cpp editmetadata.cpp
SOURCES += videogallery.cpp videoselected.cpp videodlg.cpp videoscan.cpp
SOURCES += videolist.cpp dbaccess.cpp metadatalistmanager.cpp
SOURCES += globals.cpp dirscan.cpp videoutils.cpp imagecache.cpp

#MythDVD
SOURCES += dvdripbox.cpp dvdinfo.cpp titledialog.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
    # need C99 flag for isnan() to work
    DEFINES += _GLIBCPP_USE_C99
}
