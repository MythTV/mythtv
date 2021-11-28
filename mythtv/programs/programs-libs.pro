#pthreads directory has config.h, need path to be after library paths
win32-msvc*:INCLUDEPATH -= $$SRC_PATH_BARE/../platform/win32/msvc/external/pthreads.2

INCLUDEPATH += ../.. ../../libs/ ../../libs/libmyth ../../libs/libmyth/audio
INCLUDEPATH +=  ../../libs/libmythtv ../.. ../../external/FFmpeg
INCLUDEPATH += ../../libs/libmythupnp ../../libs/libmythui ../../libs/libmythmetadata
INCLUDEPATH += ../../libs/libmythbase

!win32-msvc* {
  QMAKE_CXXFLAGS += -isystem ../../external/libmythdvdnav/dvdnav
  QMAKE_CXXFLAGS += -isystem ../../external/libmythdvdnav/dvdread
}

win32-msvc* {
  INCLUDEPATH += ../../external/libmythdvdnav/dvdnav
  INCLUDEPATH += ../../external/libmythdvdnav/dvdread
}

!using_libbluray_external:INCLUDEPATH += ../../external/libmythbluray/src
INCLUDEPATH += ../../libs/libmythtv/mpeg
INCLUDEPATH += ../../libs/libmythtv/vbitext
INCLUDEPATH += ../../libs/libmythservicecontracts
INCLUDEPATH += ../../libs/libmythprotoserver

win32-msvc*:INCLUDEPATH += $$SRC_PATH_BARE/../platform/win32/msvc/external/pthreads.2

LIBS += -L../../libs/libmyth -L../../libs/libmythtv
LIBS += -L../../external/FFmpeg/libswresample
LIBS += -L../../external/FFmpeg/libavutil
LIBS += -L../../external/FFmpeg/libavcodec
LIBS += -L../../external/FFmpeg/libavformat
LIBS += -L../../external/FFmpeg/libswscale
LIBS += -L../../external/FFmpeg/libpostproc
LIBS += -L../../external/FFmpeg/libavfilter
LIBS += -L../../libs/libmythbase
LIBS += -L../../libs/libmythui
LIBS += -L../../libs/libmythupnp
LIBS += -L../../libs/libmythmetadata
LIBS += -L../../libs/libmythservicecontracts
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
LIBS += -lmythpostproc
LIBS += -lmythavfilter
LIBS += -lmythtv-$$LIBVERSION
LIBS += -lmythupnp-$$LIBVERSION
LIBS += -lmythbase-$$LIBVERSION
LIBS += -lmythui-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION
LIBS += -lmythmetadata-$$LIBVERSION
LIBS += -lmythservicecontracts-$$LIBVERSION
LIBS += -lmythprotoserver-$$LIBVERSION

using_frontend: using_opengl: QT += opengl
using_mheg:LIBS += -L../../libs/libmythfreemheg -lmythfreemheg-$$LIBVERSION
using_hdhomerun:LIBS += -lhdhomerun
using_taglib: LIBS += $$CONFIG_TAGLIB_LIBS

!using_libexiv2_external {
    LIBS += -L../../external/libexiv2 -lmythexiv2-0.28 -lexpat
    freebsd: LIBS += -lprocstat -liconv
    darwin: LIBS += -liconv -lz
}

win32 {
    CONFIG += console
}

!mingw || win32-msvc* {
    POST_TARGETDEPS += ../../libs/libmythui/libmythui-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmyth/libmyth-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmythtv/libmythtv-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../external/FFmpeg/libswresample/$$avLibName(swresample)
    POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
    POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
    POST_TARGETDEPS += ../../external/FFmpeg/libavformat/$$avLibName(avformat)
    POST_TARGETDEPS += ../../external/FFmpeg/libswscale/$$avLibName(swscale)
    POST_TARGETDEPS += ../../external/FFmpeg/libswresample/$$avLibName(swresample)
    POST_TARGETDEPS += ../../external/FFmpeg/libpostproc/$$avLibName(postproc)
    POST_TARGETDEPS += ../../external/FFmpeg/libavfilter/$$avLibName(avfilter)
    POST_TARGETDEPS += ../../libs/libmythupnp/libmythupnp-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmythbase/libmythbase-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmythservicecontracts/libmythservicecontracts-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../libs/libmythprotoserver/libmythprotoserver-$${MYTH_SHLIB_EXT}
}

DEPENDPATH += ../.. ../../libs ../../libs/libmyth ../../libs/libmyth/audio
DEPENDPATH += ../../libs/libmythtv
DEPENDPATH += ../../libs/libmythtv/mpeg ../../libs/libmythtv/vbitext
DEPENDPATH += ../.. ../../external/FFmpeg
DEPENDPATH += ../../libs/libmythupnp ../../libs/libmythui
DEPENDPATH += ../../libmythbase
DEPENDPATH +=../../libs/libmythservicecontracts ../../libs/libmythprotoserver

using_mingw:DEFINES += USING_MINGW

macx:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
macx:using_dvdv:LIBS += -lobjc

LIBS += $$EXTRA_LIBS $$LATE_LIBS
