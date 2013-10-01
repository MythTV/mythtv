include ( ../../settings.pro )

QT += network xml sql
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

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
HEADERS += imagemetadata.h imageutils.h imagescan.h imagescanthread.h
HEADERS += imagethumbgenthread.h

SOURCES += cleanup.cpp  dbaccess.cpp  dirscan.cpp  globals.cpp
SOURCES += parentalcontrols.cpp  videoscan.cpp  videoutils.cpp
SOURCES += videometadata.cpp  videometadatalistmanager.cpp
SOURCES += metadatacommon.cpp metadatadownload.cpp metadataimagedownload.cpp
SOURCES += bluraymetadata.cpp metadatafactory.cpp mythuimetadataresults.cpp
SOURCES += mythuiimageresults.cpp
SOURCES += musicmetadata.cpp musicutils.cpp metaio.cpp metaiotaglib.cpp
SOURCES += metaioflacvorbis.cpp metaioavfcomment.cpp metaiomp4.cpp
SOURCES += metaiowavpack.cpp metaioid3.cpp metaiooggvorbis.cpp
SOURCES += imagemetadata.cpp imageutils.cpp imagescan.cpp imagescanthread.cpp
SOURCES += imagethumbgenthread.cpp

INCLUDEPATH += ../libmythbase ../libmythtv
INCLUDEPATH += ../.. ../ ./ ../libmythupnp ../libmythui
INCLUDEPATH += ../../external/FFmpeg ../libmyth  ../../external/libmythbluray

# for TagLib
INCLUDEPATH += $${CONFIG_TAGLIB_INCLUDES}

win32-msvc*:INCLUDEPATH += ../../platform/win32/msvc/external/taglib/taglib/Headers

DEPENDPATH += ../../external/libsamplerate ../libmythsoundtouch
DEPENDPATH += ../libmythfreesurround
DEPENDPATH += ../ ../libmythui ../libmythbase
DEPENDPATH += ../libmythupnp ../libmythtv ../libmyth
DEPENDPATH += ../../external/libmythbluray

LIBS += -L../../external/libsamplerate   -lmythsamplerate-$${LIBVERSION}
LIBS += -L../libmythsoundtouch   -lmythsoundtouch-$${LIBVERSION}
LIBS += -L../libmythbase           -lmythbase-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../libmythupnp         -lmythupnp-$${LIBVERSION}
LIBS += -L../libmythservicecontracts -lmythservicecontracts-$${LIBVERSION}
LIBS += -L../libmythfreesurround -lmythfreesurround-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../external/FFmpeg/libavutil  -lmythavutil
LIBS += -L../../external/FFmpeg/libavformat -lmythavformat
LIBS += -L../libmyth              -lmyth-$${LIBVERSION}
LIBS += -L../libmythtv              -lmythtv-$${LIBVERSION}
LIBS += -L../../external/libmythbluray     -lmythbluray-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libswscale -lmythswscale

# for TagLib
LIBS += $${CONFIG_TAGLIB_LIBS}

win32-msvc*:LIBS += -ltag

using_mheg:LIBS += -L../libmythfreemheg        -lmythfreemheg-$${LIBVERSION}
using_live:LIBS += -L../libmythlivemedia        -lmythlivemedia-$${LIBVERSION}

mingw:LIBS += -lws2_32

win32-msvc* {

    LIBS += -lws2_32
    LIBS += -ltag
    INCLUDEPATH += $$SRC_PATH_BARE/platform/win32/msvc/external/taglib/include/taglib
}

inc.path = $${PREFIX}/include/mythtv/metadata/

inc.files = cleanup.h  dbaccess.h  dirscan.h  globals.h  parentalcontrols.h
inc.files += videoscan.h  videoutils.h  videometadata.h  videometadatalistmanager.h
inc.files += quicksp.h metadatacommon.h metadatadownload.h metadataimagedownload.h
inc.files += bluraymetadata.h mythmetaexp.h metadatafactory.h mythuimetadataresults.h
inc.files += mythuiimageresults.h metadataimagehelper.h
inc.files += musicmetadata.h musicutils.h
inc.files += metaio.h metaiotaglib.h
inc.files += metaioflacvorbis.h metaioavfcomment.h metaiomp4.h
inc.files += metaiowavpack.h metaioid3.h metaiooggvorbis.h
inc.files += imagemetadata.h imageutils.h imagescan.h imagescanthread.h
inc.files += imagethumbgenthread.h

INSTALLS += inc

macx {
    using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS -lexiv2
