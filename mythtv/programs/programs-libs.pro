INCLUDEPATH += ../.. ../../libs/ ../../libs/libmyth ../../libs/libmythtv
INCLUDEPATH += ../../external/FFmpeg
INCLUDEPATH += ../../libs/libmythupnp ../../libs/libmythui ../../libs/libmythmetadata
INCLUDEPATH += ../../libs/libmythlivemedia ../../libs/libmythdb ../../libmythhdhomerun
INCLUDEPATH += ../../libs/libmythdvdnav ../../libs/libmythbluray ../../libs/libmythsamplerate

LIBS += -L../../libs/libmyth -L../../libs/libmythtv
LIBS += -L../../external/FFmpeg/libavutil
LIBS += -L../../external/FFmpeg/libavcodec
LIBS += -L../../external/FFmpeg/libavcore
LIBS += -L../../external/FFmpeg/libavformat
LIBS += -L../../external/FFmpeg/libswscale
LIBS += -L../../libs/libmythdb
LIBS += -L../../libs/libmythui
LIBS += -L../../libs/libmythupnp
LIBS += -L../../libs/libmythmetdata

LIBS += -lmythtv-$$LIBVERSION
LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavcore
LIBS += -lmythavutil
LIBS += -lmythupnp-$$LIBVERSION
LIBS += -lmythdb-$$LIBVERSION
LIBS += -lmythui-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION
LIBS += -lmythmetadata-$$LIBVERSION

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
TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
TARGETDEPS += ../../external/FFmpeg/libavcore/$$avLibName(avcore)
TARGETDEPS += ../../external/FFmpeg/libavformat/$$avLibName(avformat)
TARGETDEPS += ../../external/FFmpeg/libswscale/$$avLibName(swscale)
TARGETDEPS += ../../libs/libmythupnp/libmythupnp-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libmythdb/libmythdb-$${MYTH_SHLIB_EXT}
using_live: TARGETDEPS += ../../libs/libmythlivemedia/libmythlivemedia-$${MYTH_SHLIB_EXT}
using_hdhomerun: TARGETDEPS += ../../libs/libmythhdhomerun/libmythhdhomerun-$${MYTH_SHLIB_EXT}

DEPENDPATH += ../.. ../../libs ../../libs/libmyth ../../libs/libmythtv
DEPENDPATH += ../../external/FFmpeg
DEPENDPATH += ../../libs/libmythupnp ../../libs/libmythui
DEPENDPATH += ../../libs/libmythlivemedia ../../libmythdb ../../libmythhdhomerun

using_opengl:CONFIG += opengl

macx:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
macx:using_dvdv:LIBS += -lobjc

LIBS += $$EXTRA_LIBS $$LATE_LIBS
