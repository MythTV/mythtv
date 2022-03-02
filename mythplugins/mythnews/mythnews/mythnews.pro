include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network sql xml widgets

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythnews
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

installfiles.path = $${PREFIX}/share/mythtv/mythnews
installfiles.files = news-sites.xml

INSTALLS += installfiles

# Input
HEADERS += mythnews.h     mythnewsconfig.h   mythnewseditor.h
HEADERS += newssite.h     newsarticle.h
HEADERS += newsdbutil.h   newsdbcheck.h
SOURCES += mythnews.cpp   mythnewsconfig.cpp mythnewseditor.cpp
SOURCES += newssite.cpp   newsarticle.cpp
SOURCES += newsdbutil.cpp newsdbcheck.cpp
SOURCES += libmythnews.cpp

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

android {
    # to discriminate plugins in a flat directory structure
    TARGET = mythplugin$${TARGET}
}

include ( ../../libs-targetfix.pro )
