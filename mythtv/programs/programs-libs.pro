INCLUDEPATH += ../.. ../../libs/ ../../libs/libmyth ../../libs/libmyth/audio
INCLUDEPATH +=  ../../libs/libmythtv ../../external/FFmpeg
INCLUDEPATH += ../../libs/libmythupnp ../../libs/libmythui ../../libs/libmythmetadata
INCLUDEPATH += ../../libs/libmythlivemedia ../../libs/libmythbase ../../libs/libmythhdhomerun
INCLUDEPATH += ../../libs/libmythdvdnav ../../libs/libmythbluray ../../libs/libmythsamplerate
INCLUDEPATH += ../../libs/libmythtv/mpeg
INCLUDEPATH += ../../libs/libmythtv/vbitext
INCLUDEPATH += ../../libs/libmythservicecontracts
INCLUDEPATH += ../../libs/libmythprotoserver

LIBS += -L../../libs/libmyth -L../../libs/libmythtv
LIBS += -L../../external/FFmpeg/libavutil
LIBS += -L../../external/FFmpeg/libavcodec
LIBS += -L../../external/FFmpeg/libavformat
LIBS += -L../../external/FFmpeg/libswscale
LIBS += -L../../libs/libmythbase
LIBS += -L../../libs/libmythui
LIBS += -L../../libs/libmythupnp
LIBS += -L../../libs/libmythmetadata
LIBS += -L../../libs/libmythservicecontracts
LIBS += -L../../libs/libmythprotoserver

LIBS += -lmythtv-$$LIBVERSION
LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lmythupnp-$$LIBVERSION
LIBS += -lmythbase-$$LIBVERSION
LIBS += -lmythui-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION
LIBS += -lmythmetadata-$$LIBVERSION
LIBS += -lmythservicecontracts-$$LIBVERSION
LIBS += -lmythprotoserver-$$LIBVERSION

using_live:LIBS += -L../../libs/libmythlivemedia -lmythlivemedia-$$LIBVERSION
using_mheg:LIBS += -L../../libs/libmythfreemheg -lmythfreemheg-$$LIBVERSION
using_hdhomerun:LIBS += -L../../libs/libmythhdhomerun -lmythhdhomerun-$$LIBVERSION

mingw {
    CONFIG += console
}

POST_TARGETDEPS += ../../libs/libmythui/libmythui-$${MYTH_SHLIB_EXT}
POST_TARGETDEPS += ../../libs/libmyth/libmyth-$${MYTH_SHLIB_EXT}
POST_TARGETDEPS += ../../libs/libmythtv/libmythtv-$${MYTH_SHLIB_EXT}
POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
POST_TARGETDEPS += ../../external/FFmpeg/libavformat/$$avLibName(avformat)
POST_TARGETDEPS += ../../external/FFmpeg/libswscale/$$avLibName(swscale)
POST_TARGETDEPS += ../../libs/libmythupnp/libmythupnp-$${MYTH_SHLIB_EXT}
POST_TARGETDEPS += ../../libs/libmythbase/libmythbase-$${MYTH_SHLIB_EXT}
POST_TARGETDEPS += ../../libs/libmythservicecontracts/libmythservicecontracts-$${MYTH_SHLIB_EXT}
POST_TARGETDEPS += ../../libs/libmythprotoserver/libmythprotoserver-$${MYTH_SHLIB_EXT}
using_live: POST_TARGETDEPS += ../../libs/libmythlivemedia/libmythlivemedia-$${MYTH_SHLIB_EXT}
using_hdhomerun: POST_TARGETDEPS += ../../libs/libmythhdhomerun/libmythhdhomerun-$${MYTH_SHLIB_EXT}

DEPENDPATH += ../.. ../../libs ../../libs/libmyth
DEPENDPATH += ../../libs/libmythtv
DEPENDPATH += ../../libs/libmythtv/mpeg ../../libs/libmythtv/vbitext
DEPENDPATH += ../../external/FFmpeg
DEPENDPATH += ../../libs/libmythupnp ../../libs/libmythui
DEPENDPATH += ../../libs/libmythlivemedia ../../libmythbase ../../libmythhdhomerun
DEPENDPATH +=../../libs/libmythservicecontracts ../../libs/libmythprotoserver

using_opengl:CONFIG += opengl

macx:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
macx:using_dvdv:LIBS += -lobjc

LIBS += $$EXTRA_LIBS $$LATE_LIBS
