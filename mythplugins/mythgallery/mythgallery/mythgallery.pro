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
HEADERS += gallerysettings.h   dbcheck.h
HEADERS += galleryutil.h
HEADERS += thumbgenerator.h    thumbview.h
SOURCES += iconview.cpp        singleview.cpp
SOURCES += imageview.cpp
SOURCES += gallerysettings.cpp dbcheck.cpp
SOURCES += galleryutil.cpp
SOURCES += thumbgenerator.cpp  thumbview.cpp
SOURCES += main.cpp

opengl {
    SOURCES *= glsingleview.cpp gltexture.cpp
    HEADERS *= glsingleview.h   gltexture.h
    DEFINES += USING_OPENGL
}


#The following line was inserted by qt3to4
QT += network opengl sql xml

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../../libs-targetfix.pro )
