include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythflix
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = netflix-ui.xml
installmenus.path = $${PREFIX}/share/mythtv/
installmenus.files = netflix_menu.xml
installfiles.path = $${PREFIX}/share/mythtv/mythflix
installfiles.files = netflix-rss.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installfiles installimages installmenus uifiles

# Input
HEADERS += mythflixqueue.h mythflix.h mythflixconfig.h newsengine.h
SOURCES += main.cpp mythflixqueue.cpp mythflix.cpp mythflixconfig.cpp newsengine.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
