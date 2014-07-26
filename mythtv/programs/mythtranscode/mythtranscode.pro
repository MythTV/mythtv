include ( ../../settings.pro)
include ( ../../version.pro)
include ( ../programs-libs.pro)

QT += network xml sql
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)
QMAKE_CFLAGS += -w

# llvm/xcode doesn't compile if -O3 is enabled (or any other -O for that matter)
macx: QMAKE_CFLAGS -= -O3 -O2 -O1 -Os

# Input
SOURCES += main.cpp transcode.cpp mpeg2fix.cpp
SOURCES += audioreencodebuffer.cpp cutter.cpp videodecodebuffer.cpp
SOURCES += commandlineparser.cpp
SOURCES += external/replex/element.c external/replex/mpg_common.c
SOURCES += external/replex/multiplex.c external/replex/pes.c
SOURCES += external/replex/ringbuffer.c external/replex/ts.c

HEADERS += mpeg2fix.h transcodedefs.h commandlineparser.h
HEADERS += audioreencodebuffer.h cutter.h videodecodebuffer.h
HEADERS += external/replex/element.h external/replex/mpg_common.h
HEADERS += external/replex/multiplex.h external/replex/pes.h
HEADERS += external/replex/ringbuffer.h external/replex/ts.h

DEPENDPATH += external/replex
DEPENDPATH += ../../libs/libavcodec
DEPENDPATH += ../../libs/libavformat
DEPENDPATH += ../../libs/libavutil
DEPENDPATH += ../../libs/libmythtv/recorders
DEPENDPATH += ../../external/minilzo

LIBS += -L../../external/minilzo -lmythminilzo-$$LIBVERSION
POST_TARGETDEPS += ../../external/minilzo/libmythminilzo-$${MYTH_LIB_EXT}

!contains( CONFIG_LIBMPEG2EXTERNAL, yes) {
        DEPENDPATH += ../../libs/libmythmpeg2
        LIBS += -L../../libs/libmythmpeg2 -lmythmpeg2-$$LIBVERSION
        POST_TARGETDEPS += ../../libs/libmythmpeg2/libmythmpeg2-$${MYTH_LIB_EXT}
}

INCLUDEPATH += $$DEPENDPATH
