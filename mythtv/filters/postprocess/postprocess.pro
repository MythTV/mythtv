include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG -= moc qt
CONFIG += plugin thread
target.path = $${PREFIX}/lib/mythtv/filters
INSTALLS = target

INCLUDEPATH += ../../libs/libmythtv ../../libs/libavcodec
DEPENDPATH += ../../libs/libmythtv ../../libs/libavcodec

LIBS += -L../../libs/libavcodec
LIBS += -lmythavcodec-$${LIBVERSION}

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
TARGETDEPS += ../../libs/libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}

QMAKE_CFLAGS_RELEASE += -Wno-missing-prototypes
QMAKE_CFLAGS_DEBUG += -Wno-missing-prototypes

SOURCES += filter_postprocess.c
