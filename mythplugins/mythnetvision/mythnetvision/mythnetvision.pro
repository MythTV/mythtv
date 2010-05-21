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

installbbclibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/bbciplayer
installbbclibs.files = scripts/nv_python_libs/bbciplayer/*.py
installbliptvlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/bliptv
installbliptvlibs.files = scripts/nv_python_libs/bliptv/*.py
installcommonlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/common
installcommonlibs.files = scripts/nv_python_libs/common/*.py
installconfiglibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/configs
installconfigHTMLlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/configs/HTML
installconfigHTMLlibs.files = scripts/nv_python_libs/configs/HTML/*.html
installconfigXMLlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/configs/XML
installconfigXMLlibs.files = scripts/nv_python_libs/configs/XML/*.xml
installconfigXMLdeflibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/configs/XML/defaultUserPrefs
installconfigXMLdeflibs.files = scripts/nv_python_libs/configs/XML/defaultUserPrefs/*.xml
installconfigXSLTlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/configs/XSLT
installconfigXSLTlibs.files = scripts/nv_python_libs/configs/XSLT/*.xsl
installdailymotionlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/dailymotion
installdailymotionlibs.files = scripts/nv_python_libs/dailymotion/*.py
installhululibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/hulu
installhululibs.files = scripts/nv_python_libs/hulu/*.py
installmnvsearchlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/mnvsearch
installmnvsearchlibs.files = scripts/nv_python_libs/mnvsearch/*.py
installmtvlibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/mtv
installmtvlibs.files = scripts/nv_python_libs/mtv/*.py
installrev3libs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/rev3
installrev3libs.files = scripts/nv_python_libs/rev3/*.py
installthewblibs.path = $${PREFIX}/share/mythtv/mythnetvision/scripts/nv_python_libs/thewb
installthewblibs.files = scripts/nv_python_libs/thewb/*.py
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
INSTALLS += installbbclibs installbliptvlibs installcommonlibs installconfiglibs installconfigHTMLlibs
INSTALLS += installconfigXMLlibs installconfigXMLdeflibs installconfigXSLTlibs installdailymotionlibs
INSTALLS += installhululibs installmnvsearchlibs installmtvlibs installrev3libs installthewblibs
INSTALLS += installtmdblibs installvimeolibs installvimeooauth installyoutubelibs
INSTALLS += installdiricons installtopicicons installfilmicons installmusicicons

# Input
HEADERS += dbcheck.h netgrabbermanager.h mythrssmanager.h netutils.h rsseditor.h
HEADERS += netsearch.h rssparse.h imagedownloadmanager.h downloadmanager.h treeeditor.h nettree.h
HEADERS += searcheditor.h

SOURCES += dbcheck.cpp netgrabbermanager.cpp mythrssmanager.cpp rssparse.cpp
SOURCES += netutils.cpp rsseditor.cpp netsearch.cpp searcheditor.cpp
SOURCES += downloadmanager.cpp imagedownloadmanager.cpp treeeditor.cpp nettree.cpp main.cpp

include ( ../../libs-targetfix.pro )
