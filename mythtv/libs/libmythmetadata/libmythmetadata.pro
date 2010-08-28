include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythmetadata-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += version.cpp

# Input

HEADERS += cleanup.h  dbaccess.h  dirscan.h  globals.h  parentalcontrols.h
HEADERS += videoscan.h  videoutils.h  metadata.h  metadatalistmanager.h
HEADERS += quicksp.h

SOURCES += cleanup.cpp  dbaccess.cpp  dirscan.cpp  globals.cpp  
SOURCES += parentalcontrols.cpp  videoscan.cpp  videoutils.cpp
SOURCES += metadata.cpp  metadatalistmanager.cpp

INCLUDEPATH += ../libmythdb
INCLUDEPATH += ../.. ../ ./ ../libmythupnp ../libmythui
INCLUDEPATH += ../../external/FFmpeg ../libmyth
DEPENDPATH += ../libmythsamplerate ../libmythsoundtouch
DEPENDPATH += ../libmythfreesurround
DEPENDPATH += ../ ../libmythui ../libmythdb
DEPENDPATH += ../libmythupnp ../libmythtv ../libmyth

LIBS += -L../libmythsamplerate   -lmythsamplerate-$${LIBVERSION}
LIBS += -L../libmythsoundtouch   -lmythsoundtouch-$${LIBVERSION}
LIBS += -L../libmythdb           -lmythdb-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../libmythupnp         -lmythupnp-$${LIBVERSION}
LIBS += -L../libmythfreesurround -lmythfreesurround-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../external/FFmpeg/libavcore  -lmythavcodec
LIBS += -L../../external/FFmpeg/libavutil  -lmythavutil
LIBS += -L../libmyth              -lmyth-$${LIBVERSION}

mingw {

    LIBS += -lws2_32
}

inc.path = $${PREFIX}/include/mythtv/video/

inc.files = cleanup.h  dbaccess.h  dirscan.h  globals.h  parentalcontrols.h
inc.files += videoscan.h  videoutils.h  metadata.h  metadatalistmanager.h
inc.files += quicksp.h

INSTALLS += inc

macx {

    QMAKE_LFLAGS_SHLIB += -flat_namespace
}

QT += network xml sql

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
