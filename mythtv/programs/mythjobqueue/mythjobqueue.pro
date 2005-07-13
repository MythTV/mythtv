INCLUDEPATH += ../../libs/ ../../libs/libmyth

LIBS += -L../../libs/libavcodec -L../../libs/libavformat
LIBS += -L../../libs/libmyth -L../../libs/libmythtv

include ( ../../config.mak )
include (../../settings.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += -lmythavcodec-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythtv-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS
isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}
TARGETDEPS += ../../libs/libmyth/libmyth-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libmythtv/libmythtv-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libavformat/libmythavformat-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}

DEPENDPATH += ../../libs/libavcodec ../../libs/libavformat
DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp
