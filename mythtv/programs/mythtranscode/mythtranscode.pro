include ( ../../settings.pro)
include ( ../../version.pro)
include ( ../programs-libs.pro)

QT += network xml sql widgets

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)
QMAKE_CFLAGS += -w

# llvm/xcode doesn't compile if -O3 is enabled (or any other -O for that matter)
macx: QMAKE_CFLAGS -= -O3 -O2 -O1 -Os

# Input
SOURCES += mythtranscode.cpp transcode.cpp mpeg2fix.cpp
SOURCES += audioreencodebuffer.cpp cutter.cpp videodecodebuffer.cpp
SOURCES += mythtranscode_commandlineparser.cpp
SOURCES += external/replex/element.cpp external/replex/mpg_common.cpp
SOURCES += external/replex/multiplex.cpp external/replex/pes.cpp
SOURCES += external/replex/ringbuffer.cpp external/replex/ts.cpp
SOURCES += mythtranscodeplayer.cpp

HEADERS += mpeg2fix.h transcodedefs.h mythtranscode_commandlineparser.h
HEADERS += audioreencodebuffer.h cutter.h videodecodebuffer.h
HEADERS += external/replex/element.h external/replex/mpg_common.h
HEADERS += external/replex/multiplex.h external/replex/pes.h
HEADERS += external/replex/ringbuffer.h external/replex/ts.h
HEADERS += mythtranscodeplayer.h

DEPENDPATH += external/replex

!contains( CONFIG_LIBMPEG2EXTERNAL, yes) {
        LIBS += -L../../libs/libmythmpeg2 -lmythmpeg2-$$LIBVERSION
        POST_TARGETDEPS += ../../libs/libmythmpeg2/libmythmpeg2-$${MYTH_LIB_EXT}
}

INCLUDEPATH += $$DEPENDPATH
