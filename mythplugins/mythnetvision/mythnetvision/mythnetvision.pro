include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network sql xml widgets

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythnetvision
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

LIBS += -lmythmetadata-$$LIBVERSION

installfiles.path = $${PREFIX}/share/mythtv/mythnetvision

installicons.path = $${PREFIX}/share/mythtv/mythnetvision/icons
installicons.files = icons/*.png icons/*.jpg
installdiricons.path = $${PREFIX}/share/mythtv/mythnetvision/icons/directories
installtopicicons.path = $${PREFIX}/share/mythtv/mythnetvision/icons/directories/topics/
installtopicicons.files = icons/directories/topics/*.png icons/directories/topics/*.jpg
installfilmicons.path = $${PREFIX}/share/mythtv/mythnetvision/icons/directories/film_genres/
installfilmicons.files = icons/directories/film_genres/*.png icons/directories/film_genres/*.jpg
installmusicicons.path = $${PREFIX}/share/mythtv/mythnetvision/icons/directories/music_genres/
installmusicicons.files = icons/directories/music_genres/*.png icons/directories/music_genres/*.jpg

INSTALLS += installfiles installicons
INSTALLS += installdiricons installtopicicons installfilmicons installmusicicons

# Input
HEADERS += rsseditor.h searcheditor.h netcommon.h
HEADERS += neteditorbase.h netsearch.h treeeditor.h nettree.h netbase.h

SOURCES += rsseditor.cpp netsearch.cpp searcheditor.cpp netcommon.cpp
SOURCES += neteditorbase.cpp treeeditor.cpp nettree.cpp mythnetvision.cpp netbase.cpp

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

android {
    # to discriminate plugins in a flat directory structure
    TARGET = mythplugin$${TARGET}
}

include ( ../../libs-targetfix.pro )
