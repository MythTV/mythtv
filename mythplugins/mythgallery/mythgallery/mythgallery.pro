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

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = gallery-ui.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installimages uifiles

# Input
HEADERS += iconview.h          singleview.h
HEADERS += imageview.h
HEADERS += gallerysettings.h   dbcheck.h 
HEADERS += qtiffio.h           galleryutil.h
HEADERS += constants.h
HEADERS += thumbgenerator.h    thumbview.h
SOURCES += iconview.cpp        singleview.cpp
SOURCES += imageview.cpp
SOURCES += gallerysettings.cpp dbcheck.cpp
SOURCES += qtiffio.cpp         galleryutil.cpp
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
