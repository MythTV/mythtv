INCLUDEPATH += ../../libs/ ../../libs/libmyth ../../libs/libmythtv  ../..
INCLUDEPATH += ../../libs/libavutil ../../libs/libavformat ../../libs/libavcodec

LIBS += -L../../libs/libmyth -L../../libs/libmythtv
LIBS += -L../../libs/libavutil -L../../libs/libavcodec -L../../libs/libavformat
LIBS += -L../../libs/libfreemheg
LIBS += -L../../libs/libmythui

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION -lmythavcodec-$$LIBVERSION 
LIBS += -lfreemheg-$$LIBVERSION
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

DEPENDPATH += ../../libs/libmyth ../../libs/libmythtv ../../libs/libsavcodec
DEPENDPATH += ../../libs/libavutil ../../libs/libavformat ../../libs/libmythui

CONFIG += opengl

macx:using_firewire:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
