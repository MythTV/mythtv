include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QMAKE_COPY_DIR = sh ../../cpsvndir
win32:QMAKE_COPY_DIR = sh ../../cpsimple

INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libavformat
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libswscale
DEPENDPATH *= $${INCLUDEPATH}

LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lmythmetadata-$$LIBVERSION
LIBS += -lmythtv-$$LIBVERSION

TEMPLATE = lib opengl
CONFIG += plugin thread
TARGET = mytharchive
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

HEADERS += archivesettings.h logviewer.h fileselector.h
HEADERS += recordingselector.h videoselector.h dbcheck.h
HEADERS += archiveutil.h selectdestination.h
HEADERS += mythburn.h themeselector.h editmetadata.h thumbfinder.h
HEADERS += exportnative.h importnative.h

SOURCES += main.cpp archivesettings.cpp logviewer.cpp
SOURCES += fileselector.cpp recordingselector.cpp videoselector.cpp
SOURCES += dbcheck.cpp archiveutil.cpp selectdestination.cpp
SOURCES += mythburn.cpp themeselector.cpp editmetadata.cpp thumbfinder.cpp
SOURCES += exportnative.cpp importnative.cpp

burnuifiles.path = $${SYSROOT}$${PREFIX}/share/mythtv/themes/default
burnuifiles.files = mythburn-ui.xml

scriptfiles.path = $${SYSROOT}$${PREFIX}/share/mythtv/mytharchive/scripts
scriptfiles.files = ../mythburn/scripts/*

introfiles.path = $${SYSROOT}$${PREFIX}/share/mythtv/mytharchive/intro
introfiles.files = ../mythburn/intro/*.mpg

musicfiles.path = $${SYSROOT}$${PREFIX}/share/mythtv/mytharchive/music
musicfiles.files = ../mythburn/music/*.ac3

imagefiles.path = $${SYSROOT}$${PREFIX}/share/mythtv/mytharchive/images
imagefiles.files = ../mythburn/images/*.png

themefiles.path = $${SYSROOT}$${PREFIX}/share/mythtv/mytharchive/themes
themefiles.files = ../mythburn/themes/*

profilefiles.path = $${SYSROOT}$${PREFIX}/share/mythtv/mytharchive/encoder_profiles
profilefiles.files = ../mythburn/encoder_profiles/*.xml

nativeuifiles.path = $${SYSROOT}$${PREFIX}/share/mythtv/themes/default
nativeuifiles.files = mythnative-ui.xml

INSTALLS +=  scriptfiles introfiles themefiles imagefiles musicfiles
INSTALLS +=  profilefiles burnuifiles
INSTALLS +=  nativeuifiles

#The following line was inserted by qt3to4
QT += xml sql opengl network

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../../libs-targetfix.pro )
