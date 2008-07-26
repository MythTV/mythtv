include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )
 
TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythnews
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv

installfiles.path = $${PREFIX}/share/mythtv/mythnews
installfiles.files = news-sites.xml

INSTALLS += installfiles

# Input
HEADERS += mythnews.h mythnewsconfig.h mythnewseditor.h newsengine.h
HEADERS += newsdbutil.h dbcheck.h
SOURCES += main.cpp mythnews.cpp mythnewsconfig.cpp mythnewseditor.cpp
SOURCES += newsengine.cpp newsdbutil.cpp dbcheck.cpp

#The following line was inserted by qt3to4
QT += network opengl sql xml qt3support

include ( ../../libs-targetfix.pro )
