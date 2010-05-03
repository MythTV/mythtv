include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network sql xml

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythnetvision
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythui
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythdb

installfiles.path = $${PREFIX}/share/mythtv/mythnetvision

installscripts.path = $${PREFIX}/share/mythtv/mythnetvision/scripts
installscripts.files = scripts/*.py scripts/*.sh scripts/*.pl

installpylibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs
installpylibs.files = scripts/nv_python_libs/*.py

installbliptvlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/bliptv
installbliptvlibs.files = scripts/nv_python_libs/bliptv/*.py
installdailymotionlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/dailymotion
installdailymotionlibs.files = scripts/nv_python_libs/dailymotion/*.py
installmtvlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/mtv
installmtvlibs.files = scripts/nv_python_libs/mtv/*.py
installtmdblibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/tmdb
installtmdblibs.files = scripts/nv_python_libs/tmdb/*.py
installvimeolibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/vimeo
installvimeolibs.files = scripts/nv_python_libs/vimeo/*.py scripts/nv_python_libs/vimeo/*.pyc
installvimeooauth.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/vimeo/oauth
installvimeooauth.files = scripts/nv_python_libs/vimeo/oauth/*.py
installyoutubelibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/youtube
installyoutubelibs.files = scripts/nv_python_libs/youtube/*.py

installicons.path = $${PREFIX}/share/mythtv/mythnetvision/icons
installicons.files = icons/*.png icons/*.jpg
installdiricons.path = $${PREFIX}/share/mythtv/mythnetvision/icons/directories
installtopicicons.path = $${PREFIX}/share/mythtv/mythnetvision/icons/directories/topics/
installtopicicons.files = icons/directories/topics/*.png icons/directories/topics/*.jpg
installfilmicons.path = $${PREFIX}/share/mythtv/mythnetvision/icons/directories/film_genres/
installfilmicons.files = icons/directories/film_genres/*.png icons/directories/film_genres/*.jpg
installmusicicons.path = $${PREFIX}/share/mythtv/mythnetvision/icons/directories/music_genres/
installmusicicons.files = icons/directories/music_genres/*.png icons/directories/music_genres/*.jpg

INSTALLS += installfiles installscripts installicons installpylibs
INSTALLS += installbliptvlibs installdailymotionlibs
INSTALLS += installmtvlibs installtmdblibs installvimeolibs installvimeooauth installyoutubelibs
INSTALLS += installdiricons installtopicicons installfilmicons installmusicicons

# Input
HEADERS += dbcheck.h grabbermanager.h rssmanager.h treedbutil.h netutils.h rssdbutil.h rsseditor.h
HEADERS += search.h netsearch.h parse.h imagedownloadmanager.h downloadmanager.h treeeditor.h nettree.h

SOURCES += dbcheck.cpp grabbermanager.cpp rssmanager.cpp parse.cpp treedbutil.cpp netutils.cpp search.cpp
SOURCES += rssdbutil.cpp rsseditor.cpp netsearch.cpp
SOURCES += downloadmanager.cpp imagedownloadmanager.cpp treeeditor.cpp nettree.cpp main.cpp

include ( ../../libs-targetfix.pro )
