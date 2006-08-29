include ( ../../../config.mak )
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
INCLUDEPATH += ../../../libs/libavcodec ../../../libs/libavformat
INCLUDEPATH += ../../../libs/libavutil  ../../../libs/libmythmpeg2

LIBS += -lmythavcodec-$$LIBVERSION -lmythavformat-$$LIBVERSION -lmythavutil-$$LIBVERSION
LIBS += $$EXTRA_LIBS
LIBS += -L../../../libs/libavutil -L../../../libs/libavcodec -L../../../libs/libavformat

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}

TARGETDEPS += ../../../libs/libavformat/libmythavformat-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../../libs/libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../../libs/libavutil/libmythavutil-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}

DEPENDPATH += ../../../libs/libavutil ../../../libs/libavformat ../../../libs/libsavcodec
