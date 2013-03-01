include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network sql xml
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythnews
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythui
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythbase

installfiles.path = $${PREFIX}/share/mythtv/mythnews
installfiles.files = news-sites.xml

INSTALLS += installfiles

# Input
HEADERS += mythnews.h     mythnewsconfig.h   mythnewseditor.h
HEADERS += newssite.h     newsarticle.h
HEADERS += newsdbutil.h   dbcheck.h
SOURCES += mythnews.cpp   mythnewsconfig.cpp mythnewseditor.cpp
SOURCES += newssite.cpp   newsarticle.cpp
SOURCES += newsdbutil.cpp dbcheck.cpp
SOURCES += main.cpp

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../../libs-targetfix.pro )
