include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lmythmetadata-$$LIBVERSION
LIBS += -lmythtv-$$LIBVERSION
# libmythtv dependencies
using_mheg: LIBS += -lmythfreemheg-$$LIBVERSION
using_hdhomerun: LIBS += -lhdhomerun

TEMPLATE = lib opengl
CONFIG += plugin thread
TARGET = mytharchive
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

HEADERS += archivesettings.h logviewer.h fileselector.h
HEADERS += recordingselector.h videoselector.h archivedbcheck.h
HEADERS += archiveutil.h selectdestination.h
HEADERS += mythburn.h themeselector.h editmetadata.h thumbfinder.h
HEADERS += exportnative.h importnative.h

SOURCES += mytharchive.cpp archivesettings.cpp logviewer.cpp
SOURCES += fileselector.cpp recordingselector.cpp videoselector.cpp
SOURCES += archivedbcheck.cpp archiveutil.cpp selectdestination.cpp
SOURCES += mythburn.cpp themeselector.cpp editmetadata.cpp thumbfinder.cpp
SOURCES += exportnative.cpp importnative.cpp

burnuifiles.path = $${PREFIX}/share/mythtv/themes/default
burnuifiles.files = mythburn-ui.xml

scriptfiles.path = $${PREFIX}/share/mythtv/mytharchive/scripts
scriptfiles.files = ../mythburn/scripts/*

introfiles.path = $${PREFIX}/share/mythtv/mytharchive/intro
introfiles.files = ../mythburn/intro/*.mpg

musicfiles.path = $${PREFIX}/share/mythtv/mytharchive/music
musicfiles.files = ../mythburn/music/*.ac3

imagefiles.path = $${PREFIX}/share/mythtv/mytharchive/images
imagefiles.files = ../mythburn/images/*.png

themefiles.path = $${PREFIX}/share/mythtv/mytharchive/themes
themefiles.files = ../mythburn/themes/*

profilefiles.path = $${PREFIX}/share/mythtv/mytharchive/encoder_profiles
profilefiles.files = ../mythburn/encoder_profiles/*.xml

nativeuifiles.path = $${PREFIX}/share/mythtv/themes/default
nativeuifiles.files = mythnative-ui.xml

INSTALLS +=  scriptfiles introfiles themefiles imagefiles musicfiles
INSTALLS +=  profilefiles burnuifiles
INSTALLS +=  nativeuifiles

QT += xml sql opengl network widgets

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

android {
    # to discriminate plugins in a flat directory structure
    TARGET = mythplugin$${TARGET}
}

include ( ../../libs-targetfix.pro )
