INCLUDEPATH += ../.. ../../libs/ ../../libs/libmyth ../../libs/libmythtv
INCLUDEPATH += ../../libs/libavutil ../../libs/libavformat ../../libs/libavcodec
INCLUDEPATH += ../../libs/libmythupnp ../../libs/libmythui
INCLUDEPATH += ../../libs/libmythlivemedia

LIBS += -L../../libs/libmyth -L../../libs/libmythtv
LIBS += -L../../libs/libavutil -L../../libs/libavcodec -L../../libs/libavformat
LIBS += -L../../libs/libmythfreemheg
LIBS += -L../../libs/libmythui
LIBS += -L../../libs/libmythupnp
LIBS += -L../../libs/libmythlivemedia

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION -lmythavcodec-$$LIBVERSION 
LIBS += -lmythfreemheg-$$LIBVERSION
LIBS += -lmythupnp-$$LIBVERSION 
LIBS += -lmythlivemedia-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION $$EXTRA_LIBS

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}
TARGETDEPS += ../../libs/libmythui/libmythui-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libmyth/libmyth-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libmythtv/libmythtv-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libavutil/libmythavutil-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libavformat/libmythavformat-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libmythupnp/libmythupnp-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libmythlivemedia/libmythlivemedia-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}

DEPENDPATH += ../.. ../../libs ../../libs/libmyth ../../libs/libmythtv
DEPENDPATH += ../../libs/libavutil ../../libs/libavformat ../../libs/libsavcodec
DEPENDPATH += ../../libs/libmythupnp ../../libs/libmythui
DEPENDPATH += ../../libs/libmythlivemedia

CONFIG += opengl

macx:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
