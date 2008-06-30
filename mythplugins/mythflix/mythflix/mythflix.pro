include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythflix
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv

installfiles.path = $${PREFIX}/share/mythtv/mythflix
installfiles.files = netflix-rss.xml
installscripts.path = $${PREFIX}/share/mythtv/mythflix/scripts
installscripts.files = scripts/*.pl

INSTALLS += installfiles installscripts

# Input
HEADERS += mythflixqueue.h mythflix.h mythflixconfig.h
HEADERS += newsengine.h dbcheck.h flixutil.h
SOURCES += main.cpp mythflixqueue.cpp mythflix.cpp mythflixconfig.cpp
SOURCES += newsengine.cpp dbcheck.cpp flixutil.cpp


#The following line was inserted by qt3to4
QT += network xml sql opengl qt3support

include ( ../../libs-targetfix.pro )
