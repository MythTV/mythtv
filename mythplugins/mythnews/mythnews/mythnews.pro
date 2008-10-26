include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network sql xml
 
TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythnews
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythui
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythdb

installfiles.path = $${PREFIX}/share/mythtv/mythnews
installfiles.files = news-sites.xml

INSTALLS += installfiles

# Input
HEADERS += mythnews.h mythnewsconfig.h mythnewseditor.h newsengine.h
HEADERS += newsdbutil.h dbcheck.h
SOURCES += main.cpp mythnews.cpp mythnewsconfig.cpp mythnewseditor.cpp
SOURCES += newsengine.cpp newsdbutil.cpp dbcheck.cpp

include ( ../../libs-targetfix.pro )
