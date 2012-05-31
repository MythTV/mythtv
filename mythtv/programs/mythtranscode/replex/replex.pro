include ( ../../../settings.pro)

TEMPLATE = app
TARGET = mythreplex
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += avi.c element.c mpg_common.c multiplex.c ringbuffer.c
SOURCES += ts.c replex.c pes.c

HEADERS += avi.h element.h mpg_common.h multiplex.h ringbuffer.h
HEADERS += ts.h replex.h pes.h

INCLUDEPATH += replex
INCLUDEPATH += ../../../external/FFmpeg
INCLUDEPATH += ../../../libs/libmythbase

QMAKE_CFLAGS += -w

LIBS += -L../../../external/FFmpeg/libavutil
LIBS += -L../../../external/FFmpeg/libavcodec
LIBS += -L../../../external/FFmpeg/libavformat
LIBS += -L../../../libs/libmythbase
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lmythbase-$$LIBVERSION
LIBS += $$EXTRA_LIBS

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}

# llvm/xcode doesn't compile if -O3 is enabled (or any other -O for that matter)
macx: QMAKE_CFLAGS -= -O3 -O2 -O1 -Os

POST_TARGETDEPS += ../../../external/FFmpeg/libavutil/$$avLibName(avutil)
POST_TARGETDEPS += ../../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
POST_TARGETDEPS += ../../../external/FFmpeg/libavformat/$$avLibName(avformat)

DEPENDPATH += ../../../external/FFmpeg
DEPENDPATH += ../../../libs/libmythbase

CONFIG  -= qt
