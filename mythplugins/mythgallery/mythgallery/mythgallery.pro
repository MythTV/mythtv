include ( ../../settings.pro )

include (config.pro)

!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgallery
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target
LIBS += -ltiff

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = gallery-ui.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installimages uifiles

# Input
HEADERS += iconview.h singleview.h gallerysettings.h dbcheck.h 
HEADERS += thumbgenerator.h qtiffio.h galleryutil.h constants.h
SOURCES += iconview.cpp main.cpp singleview.cpp gallerysettings.cpp dbcheck.cpp
SOURCES += thumbgenerator.cpp qtiffio.cpp galleryutil.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
