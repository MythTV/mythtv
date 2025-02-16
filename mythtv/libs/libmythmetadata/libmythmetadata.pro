include ( ../../settings.pro )

QT += network xml sql widgets

TEMPLATE = lib
TARGET = mythmetadata-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += META_API

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += version.cpp

# Input

HEADERS += cleanup.h  dbaccess.h  dirscan.h  globals.h  parentalcontrols.h
HEADERS += videoscan.h  videoutils.h  videometadata.h  videometadatalistmanager.h
HEADERS += quicksp.h metadatacommon.h metadatadownload.h metadataimagedownload.h
HEADERS += bluraymetadata.h mythmetaexp.h metadatafactory.h mythuimetadataresults.h
HEADERS += mythuiimageresults.h
HEADERS += musicmetadata.h musicutils.h metaio.h metaiotaglib.h
HEADERS += metaioflacvorbis.h metaioavfcomment.h metaiomp4.h
HEADERS += metaiowavpack.h metaioid3.h metaiooggvorbis.h
HEADERS += imagetypes.h imagemetadata.h imagethumbs.h imagescanner.h imagemanager.h
HEADERS += musicfilescanner.h metadatagrabber.h lyricsdata.h

SOURCES += cleanup.cpp  dbaccess.cpp  dirscan.cpp  globals.cpp
SOURCES += parentalcontrols.cpp  videoscan.cpp  videoutils.cpp
SOURCES += videometadata.cpp  videometadatalistmanager.cpp
SOURCES += metadatacommon.cpp metadatadownload.cpp metadataimagedownload.cpp
SOURCES += bluraymetadata.cpp metadatafactory.cpp mythuimetadataresults.cpp
SOURCES += mythuiimageresults.cpp
SOURCES += musicmetadata.cpp musicutils.cpp metaio.cpp metaiotaglib.cpp
SOURCES += metaioflacvorbis.cpp metaioavfcomment.cpp metaiomp4.cpp
SOURCES += metaiowavpack.cpp metaioid3.cpp metaiooggvorbis.cpp
SOURCES += imagemetadata.cpp imagethumbs.cpp imagescanner.cpp imagemanager.cpp
SOURCES += musicfilescanner.cpp metadatagrabber.cpp lyricsdata.cpp

INCLUDEPATH += .. ../../external/FFmpeg

# for TagLib
INCLUDEPATH += $${CONFIG_TAGLIB_INCLUDES}

LIBS += -L../libmythbase           -lmythbase-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../external/FFmpeg/libavutil -lmythavutil
LIBS += -L../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../external/FFmpeg/libavformat -lmythavformat
LIBS += -L../libmyth              -lmyth-$${LIBVERSION}
LIBS += -L../libmythtv              -lmythtv-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libswscale -lmythswscale

!using_system_libexiv2 {
    darwin {
        QMAKE_CXXFLAGS = "-I../../external/libexiv2/include" $${QMAKE_CXXFLAGS}
    } else {
        INCLUDEPATH += ../../external/libexiv2/include
    }
    DEPENDPATH += ../../external/libexiv2
    LIBS += -L../../external/libexiv2 -lmythexiv2-0.28
}

!using_system_libbluray {
    INCLUDEPATH += ../../external/libmythbluray/src
    DEPENDPATH += ../../external/libmythbluray
    LIBS += -L../../external/libmythbluray     -lmythbluray-$${LIBVERSION}
} else {
    DEFINES += HAVE_LIBBLURAY
}

using_system_libbluray:android {
    LIBS += -lbluray -lxml2
}

# for TagLib
LIBS += $${CONFIG_TAGLIB_LIBS}

win32-msvc*:LIBS += -ltag

using_mheg:LIBS += -L../libmythfreemheg        -lmythfreemheg-$${LIBVERSION}

mingw:LIBS += -lws2_32
mingw:LIBS += libbluray

win32-msvc* {

    LIBS += -lws2_32
    LIBS += -ltag
    INCLUDEPATH += $$SRC_PATH_BARE/../platform/win32/msvc/external/taglib/include/taglib
}

inc.path = $${PREFIX}/include/mythtv/libmythmetadata/

inc.files = cleanup.h  dbaccess.h  dirscan.h  globals.h  parentalcontrols.h
inc.files += videoscan.h  videoutils.h  videometadata.h  videometadatalistmanager.h
inc.files += quicksp.h metadatacommon.h metadatadownload.h metadataimagedownload.h
inc.files += bluraymetadata.h mythmetaexp.h metadatafactory.h mythuimetadataresults.h
inc.files += mythuiimageresults.h metadataimagehelper.h
inc.files += musicmetadata.h musicutils.h
inc.files += metaio.h metaiotaglib.h
inc.files += metaioflacvorbis.h metaioavfcomment.h metaiomp4.h
inc.files += metaiowavpack.h metaioid3.h metaiooggvorbis.h
inc.files += imagetypes.h imagemetadata.h imagemanager.h
inc.files += musicfilescanner.h metadatagrabber.h lyricsdata.h

INSTALLS += inc

macx {
    using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

POSTINC =

contains(INCLUDEPATH, /usr/include) {
  POSTINC += /usr/include
  INCLUDEPATH -= /usr/include
}
contains(INCLUDEPATH, /usr/local/include) {
  POSTINC += /usr/local/include
  INCLUDEPATH -= /usr/local/include
}
contains(INCLUDEPATH, /usr/X11R6/include) {
  POSTINC += /usr/X11R6/include
  INCLUDEPATH -= /usr/X11R6/include
}

INCLUDEPATH += $$POSTINC

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
using_system_libexiv2 {
    LIBS += -lexiv2
}

test_clean.commands = -cd test/ && $(MAKE) -f Makefile clean
clean.depends = test_clean
QMAKE_EXTRA_TARGETS += test_clean clean
test_distclean.commands = -cd test/ && $(MAKE) -f Makefile distclean
distclean.depends = test_distclean
QMAKE_EXTRA_TARGETS += test_distclean distclean
