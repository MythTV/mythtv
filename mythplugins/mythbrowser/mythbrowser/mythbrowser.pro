include (../../mythconfig.mak )
include (../../settings.pro )
include (../../programs-libs.pro )

LIBS += -lmythtv-$$LIBVERSION

QT += network xml sql opengl widgets webkitwidgets

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
SOURCES += libmythbrowser.cpp mythbrowser.cpp mythflashplayer.cpp webpage.cpp
SOURCES += bookmarkmanager.cpp bookmarkeditor.cpp browserdbutil.cpp

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

android {
    # to discriminate plugins in a flat directory structure
    TARGET = mythplugin$${TARGET}
}

include ( ../../libs-targetfix.pro )
