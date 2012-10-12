include ( ../../settings.pro)
include ( ../../version.pro)
include ( ../programs-libs.pro)

QT += network xml sql

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)
QMAKE_CFLAGS += -w

# llvm/xcode doesn't compile if -O3 is enabled (or any other -O for that matter)
macx: QMAKE_CFLAGS -= -O3 -O2 -O1 -Os

# Input
SOURCES += main.cpp transcode.cpp mpeg2fix.cpp helper.c
SOURCES += audioreencodebuffer.cpp cutter.cpp videodecodebuffer.cpp
SOURCES += commandlineparser.cpp
SOURCES += replex/element.c replex/mpg_common.c replex/multiplex.c \
           replex/pes.c     replex/ringbuffer.c replex/ts.c
HEADERS += mpeg2fix.h transcodedefs.h commandlineparser.h
HEADERS += audioreencodebuffer.h cutter.h videodecodebuffer.h
HEADERS += replex/element.h replex/mpg_common.h replex/multiplex.h \
           replex/pes.h     replex/ringbuffer.h replex/ts.h

INCLUDEPATH += replex
INCLUDEPATH += ../../libs/libavcodec
INCLUDEPATH += ../../libs/libavformat
INCLUDEPATH += ../../libs/libavutil

!contains( CONFIG_LIBMPEG2EXTERNAL, yes) {
        DEPENDPATH  += ../../libs/libmythmpeg2
        INCLUDEPATH += ../../libs/libmythmpeg2
        LIBS += -L../../libs/libmythmpeg2 -lmythmpeg2-$$LIBVERSION
        POST_TARGETDEPS += ../../libs/libmythmpeg2/libmythmpeg2-$${MYTH_LIB_EXT}
}
