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
INCLUDEPATH += ../../../libs

QMAKE_CFLAGS += -w

LIBS += -L../../../external/FFmpeg/libavutil
LIBS += -L../../../external/FFmpeg/libavcodec
LIBS += -L../../../external/FFmpeg/libavformat
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += $$EXTRA_LIBS

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}

POST_TARGETDEPS += ../../../external/FFmpeg/libavutil/$$avLibName(avutil)
POST_TARGETDEPS += ../../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
POST_TARGETDEPS += ../../../external/FFmpeg/libavformat/$$avLibName(avformat)

DEPENDPATH += ../../../external/FFmpeg

CONFIG  -= qt
