INCLUDEPATH += ../.. ../../libs/ ../../libs/libmyth ../../libs/libmythtv
INCLUDEPATH += ../../libs/libavutil ../../libs/libavformat ../../libs/libavcodec
INCLUDEPATH += ../../libs/libmythupnp ../../libs/libmythui
INCLUDEPATH += ../../libs/libmythlivemedia ../../libs/libmythdb ../../libmythhdhomerun
INCLUDEPATH += ../../libs/libmythdvdnav ../../libs/libmythbluray ../../libs/libmythsamplerate

LIBS += -L../../libs/libmyth -L../../libs/libmythtv
LIBS += -L../../libs/libavutil -L../../libs/libavcodec -L../../libs/libavformat
LIBS += -L../../libs/libswscale
LIBS += -L../../libs/libmythdb
LIBS += -L../../libs/libmythui
LIBS += -L../../libs/libmythupnp

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION -lmythavcodec-$$LIBVERSION
LIBS += -lmythswscale-$$LIBVERSION
LIBS += -lmythupnp-$$LIBVERSION
LIBS += -lmythdb-$$LIBVERSION
LIBS += -lmythui-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION

using_live:LIBS += -L../../libs/libmythlivemedia -lmythlivemedia-$$LIBVERSION
using_mheg:LIBS += -L../../libs/libmythfreemheg -lmythfreemheg-$$LIBVERSION
using_hdhomerun:LIBS += -L../../libs/libmythhdhomerun -lmythhdhomerun-$$LIBVERSION

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
TARGETDEPS += ../../libs/libswscale/libmythswscale-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libmythupnp/libmythupnp-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libmythdb/libmythdb-$${MYTH_SHLIB_EXT}
using_live: TARGETDEPS += ../../libs/libmythlivemedia/libmythlivemedia-$${MYTH_SHLIB_EXT}
using_hdhomerun: TARGETDEPS += ../../libs/libmythhdhomerun/libmythhdhomerun-$${MYTH_SHLIB_EXT}

DEPENDPATH += ../.. ../../libs ../../libs/libmyth ../../libs/libmythtv
DEPENDPATH += ../../libs/libavutil ../../libs/libavformat ../../libs/libavcodec
DEPENDPATH += ../../libs/libswscale
DEPENDPATH += ../../libs/libmythupnp ../../libs/libmythui
DEPENDPATH += ../../libs/libmythlivemedia ../../libmythdb ../../libmythhdhomerun

using_opengl:CONFIG += opengl

macx:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
macx:using_dvdv:LIBS += -lobjc

LIBS += $$EXTRA_LIBS $$LATE_LIBS
