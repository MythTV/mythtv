include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

include (config.pro)

!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgallery
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv

# Input
HEADERS += iconview.h          singleview.h
HEADERS += imageview.h
HEADERS += galleryfilter.h galleryfilterdlg.h
HEADERS += gallerysettings.h   dbcheck.h
HEADERS += galleryutil.h
HEADERS += thumbgenerator.h    thumbview.h
SOURCES += iconview.cpp        singleview.cpp
SOURCES += imageview.cpp
SOURCES += galleryfilter.cpp galleryfilterdlg.cpp
SOURCES += gallerysettings.cpp dbcheck.cpp
SOURCES += galleryutil.cpp
SOURCES += thumbgenerator.cpp  thumbview.cpp
SOURCES += main.cpp

opengl {
    SOURCES *= glsingleview.cpp gltexture.cpp
    HEADERS *= glsingleview.h   gltexture.h
    DEFINES += USING_OPENGL
}

QT += network opengl sql xml
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../../libs-targetfix.pro )
