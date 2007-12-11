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
mingw {
    LIBS += -lpthread
    CONFIG += console
}

TARGETDEPS += ../../libs/libmythui/libmythui-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libmyth/libmyth-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libmythtv/libmythtv-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libavutil/libmythavutil-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libavcodec/libmythavcodec-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libavformat/libmythavformat-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libmythupnp/libmythupnp-$${MYTH_SHLIB_EXT}
using_live: TARGETDEPS += ../../libs/libmythlivemedia/libmythlivemedia-$${MYTH_SHLIB_EXT}

DEPENDPATH += ../.. ../../libs ../../libs/libmyth ../../libs/libmythtv
DEPENDPATH += ../../libs/libavutil ../../libs/libavformat ../../libs/libsavcodec
DEPENDPATH += ../../libs/libmythupnp ../../libs/libmythui
DEPENDPATH += ../../libs/libmythlivemedia

CONFIG += opengl

macx:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
macx:using_dvdv:LIBS += -lobjc
