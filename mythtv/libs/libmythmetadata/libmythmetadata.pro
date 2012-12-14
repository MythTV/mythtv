include ( ../../settings.pro )

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

SOURCES += cleanup.cpp  dbaccess.cpp  dirscan.cpp  globals.cpp
SOURCES += parentalcontrols.cpp  videoscan.cpp  videoutils.cpp
SOURCES += videometadata.cpp  videometadatalistmanager.cpp
SOURCES += metadatacommon.cpp metadatadownload.cpp metadataimagedownload.cpp
SOURCES += bluraymetadata.cpp metadatafactory.cpp mythuimetadataresults.cpp
SOURCES += mythuiimageresults.cpp

INCLUDEPATH += ../libmythbase ../libmythtv
INCLUDEPATH += ../.. ../ ./ ../libmythupnp ../libmythui
INCLUDEPATH += ../../external/FFmpeg ../libmyth ../libmythbluray
DEPENDPATH += ../libmythsamplerate ../libmythsoundtouch
DEPENDPATH += ../libmythfreesurround
DEPENDPATH += ../ ../libmythui ../libmythbase
DEPENDPATH += ../libmythupnp ../libmythtv ../libmyth
DEPENDPATH += ../libmythbluray

LIBS += -L../libmythsamplerate   -lmythsamplerate-$${LIBVERSION}
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
LIBS += -L../libmythbluray        -lmythbluray-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libswscale -lmythswscale
using_mheg:LIBS += -L../libmythfreemheg        -lmythfreemheg-$${LIBVERSION}
using_live:LIBS += -L../libmythlivemedia        -lmythlivemedia-$${LIBVERSION}

mingw {

    LIBS += -lws2_32
}

inc.path = $${PREFIX}/include/mythtv/metadata/

inc.files = cleanup.h  dbaccess.h  dirscan.h  globals.h  parentalcontrols.h
inc.files += videoscan.h  videoutils.h  videometadata.h  videometadatalistmanager.h
inc.files += quicksp.h metadatacommon.h metadatadownload.h metadataimagedownload.h
inc.files += bluraymetadata.h mythmetaexp.h metadatafactory.h mythuimetadataresults.h
inc.files += mythuiimageresults.h metadataimagehelper.h

INSTALLS += inc

macx {
    using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
}

QT += network xml sql

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
