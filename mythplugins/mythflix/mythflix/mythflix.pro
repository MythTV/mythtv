include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythflix
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = netflix-ui.xml
installmenus.path = $${PREFIX}/share/mythtv/
installmenus.files = netflix_menu.xml
installfiles.path = $${PREFIX}/share/mythtv/mythflix
installfiles.files = netflix-rss.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png
installscripts.path = $${PREFIX}/share/mythtv/mythflix/scripts
installscripts.files = scripts/*.pl

INSTALLS += installfiles installimages installmenus installscripts uifiles

# Input
HEADERS += mythflixqueue.h mythflix.h mythflixconfig.h newsengine.h
SOURCES += main.cpp mythflixqueue.cpp mythflix.cpp mythflixconfig.cpp newsengine.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
