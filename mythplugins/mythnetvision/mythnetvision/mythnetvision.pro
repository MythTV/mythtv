include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network sql xml

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythnetvision
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

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
HEADERS += rsseditor.h searcheditor.h
HEADERS += netsearch.h imagedownloadmanager.h downloadmanager.h treeeditor.h nettree.h

SOURCES += rsseditor.cpp netsearch.cpp searcheditor.cpp
SOURCES += downloadmanager.cpp imagedownloadmanager.cpp treeeditor.cpp nettree.cpp main.cpp

include ( ../../libs-targetfix.pro )
