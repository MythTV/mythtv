INCLUDEPATH += ../../libs/
INCLUDEPATH += ../../external/FFmpeg

  QMAKE_CXXFLAGS += -isystem ../../external/libmythdvdnav/dvdnav
  QMAKE_CXXFLAGS += -isystem ../../external/libmythdvdnav/dvdread

!using_system_libbluray:INCLUDEPATH += ../../external/libmythbluray/src

LIBS += -L../../libs/libmyth -L../../libs/libmythtv
LIBS += -L../../external/FFmpeg/libswresample
LIBS += -L../../external/FFmpeg/libavutil
LIBS += -L../../external/FFmpeg/libavcodec
LIBS += -L../../external/FFmpeg/libavformat
LIBS += -L../../external/FFmpeg/libswscale
LIBS += -L../../external/FFmpeg/libavfilter
LIBS += -L../../libs/libmythbase
LIBS += -L../../libs/libmythui
LIBS += -L../../libs/libmythupnp
LIBS += -L../../libs/libmythmetadata
LIBS += -L../../libs/libmythprotoserver

# Insist that /usr/local/lib come after all the libraries provided
# in the MythTV sources.
contains (QMAKE_LIBDIR_POST, /usr/local/lib) {
  QMAKE_LIBDIR_POST -= /usr/local/lib
  LIBS += -L/usr/local/lib
}

LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythswresample
LIBS += -lmythavutil
LIBS += -lmythavcodec
LIBS += -lmythavfilter
LIBS += -lmythtv-$$LIBVERSION
LIBS += -lmythupnp-$$LIBVERSION
LIBS += -lmythbase-$$LIBVERSION
LIBS += -lmythui-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION
LIBS += -lmythmetadata-$$LIBVERSION
LIBS += -lmythprotoserver-$$LIBVERSION

using_frontend: using_opengl: QT += opengl
using_mheg:LIBS += -L../../libs/libmythfreemheg -lmythfreemheg-$$LIBVERSION
using_hdhomerun:LIBS += -lhdhomerun
using_taglib: LIBS += $$CONFIG_TAGLIB_LIBS

!using_system_libexiv2 {
    LIBS += -L../../external/libexiv2 -lmythexiv2-0.28 -lexpat
    freebsd: LIBS += -lprocstat -liconv
    darwin: LIBS += -liconv -lz
}

win32 {
    CONFIG += console
}

!mingw {
    POST_TARGETDEPS += ../../libs/libmythui/libmythui-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmyth/libmyth-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmythtv/libmythtv-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../external/FFmpeg/libswresample/$$avLibName(swresample)
    POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
    POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
    POST_TARGETDEPS += ../../external/FFmpeg/libavformat/$$avLibName(avformat)
    POST_TARGETDEPS += ../../external/FFmpeg/libswscale/$$avLibName(swscale)
    POST_TARGETDEPS += ../../external/FFmpeg/libswresample/$$avLibName(swresample)
    POST_TARGETDEPS += ../../external/FFmpeg/libavfilter/$$avLibName(avfilter)
    POST_TARGETDEPS += ../../libs/libmythupnp/libmythupnp-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmythbase/libmythbase-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmythprotoserver/libmythprotoserver-$${MYTH_SHLIB_EXT}
}

DEPENDPATH += ../.. ../../external/FFmpeg

macx:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
macx:using_dvdv:LIBS += -lobjc

LIBS += $$EXTRA_LIBS $$LATE_LIBS
