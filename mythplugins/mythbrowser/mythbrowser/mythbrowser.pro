include (../../mythconfig.mak )
include (../../settings.pro )
include (../../programs-libs.pro )

QT += network xml sql opengl webkit

TEMPLATE = lib
CONFIG += thread opengl plugin warn_on
TARGET = mythbrowser
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installimages

# Input
HEADERS += mythbrowser.h mythflashplayer.h webpage.h 
HEADERS += bookmarkmanager.h bookmarkeditor.h browserdbutil.h
SOURCES += main.cpp mythbrowser.cpp mythflashplayer.cpp webpage.cpp
SOURCES += bookmarkmanager.cpp bookmarkeditor.cpp browserdbutil.cpp

include ( ../../libs-targetfix.pro )
