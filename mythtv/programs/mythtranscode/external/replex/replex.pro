include ( ../../../../settings.pro)

TEMPLATE = app
TARGET = mythreplex
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += avi.cpp element.cpp mpg_common.cpp multiplex.cpp ringbuffer.cpp
SOURCES += ts.cpp replex.cpp pes.cpp

HEADERS += avi.h element.h mpg_common.h multiplex.h ringbuffer.h
HEADERS += ts.h replex.h pes.h

INCLUDEPATH += replex
INCLUDEPATH += ../../../../external/FFmpeg
INCLUDEPATH += ../../../../libs

QMAKE_CFLAGS += -w

LIBS += -L../../../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../../../external/FFmpeg/libavutil -lmythavutil
LIBS += -L../../../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../../../external/FFmpeg/libavformat -lmythavformat
LIBS += -L../../../../libs/libmythbase -lmythbase-$$LIBVERSION
LIBS += $$EXTRA_LIBS

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}

# llvm/xcode doesn't compile if -O3 is enabled (or any other -O for that matter)
macx: QMAKE_CFLAGS -= -O3 -O2 -O1 -Os

POST_TARGETDEPS += ../../../../external/FFmpeg/libswresample/$$avLibName(swresample)
POST_TARGETDEPS += ../../../../external/FFmpeg/libavutil/$$avLibName(avutil)
POST_TARGETDEPS += ../../../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
POST_TARGETDEPS += ../../../../external/FFmpeg/libavformat/$$avLibName(avformat)

DEPENDPATH += ../../../.. ../../../../external/FFmpeg

# NB the myth libraries need link link against Qt libs so CONFIG needs qt
