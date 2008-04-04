include ( ../../mythconfig.mak )
include ( ../../settings.pro )

include (config.pro)

!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgallery
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target
LIBS += -ltiff
LIBS *= $$LOCAL_LIBDIR_X11

INCLUDEPATH += $${PREFIX}/include/mythtv

# Input
HEADERS += iconview.h          singleview.h
HEADERS += imageview.h
HEADERS += gallerysettings.h   dbcheck.h 
# disable qtiffio until it is ported to qt4
#HEADERS += qtiffio.h           galleryutil.h
HEADERS += galleryutil.h
HEADERS += constants.h
HEADERS += thumbgenerator.h    thumbview.h
SOURCES += iconview.cpp        singleview.cpp
SOURCES += imageview.cpp
SOURCES += gallerysettings.cpp dbcheck.cpp
# disable qtiffio until it is ported to qt4
#SOURCES += qtiffio.cpp         galleryutil.cpp
SOURCES += galleryutil.cpp
SOURCES += thumbgenerator.cpp  thumbview.cpp
SOURCES += main.cpp

opengl {
    SOURCES *= glsingleview.cpp gltexture.cpp
    HEADERS *= glsingleview.h   gltexture.h
    DEFINES += USING_OPENGL
}

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

mingw:DEFINES += USING_MINGW
#The following line was inserted by qt3to4
QT += network opengl sql xml qt3support 
