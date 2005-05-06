include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavformat-$$LIBVERSION
CONFIG += thread dll warn_off
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.18.1 

INCLUDEPATH += ../ ../../ ../libavcodec ../libmythtv

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE

LIBS += $$LOCAL_LIBDIR_X11

# Input
HEADERS += asf.h avformat.h avi.h avio.h dv.h mpegts.h os_support.h qtpalette.h

SOURCES += 4xm.c allformats.c asf.c au.c avidec.c avienc.c avio.c aviobuf.c 
SOURCES += crc.c cutils.c dv.c ffm.c file.c flvdec.c flvenc.c idcin.c idroq.c
SOURCES += img.c ipmovie.c mov.c movenc.c mp3.c mpeg.c mpegts.c mpegtsenc.c 
SOURCES += mpjpeg.c nut.c os_support.c rm.c psxstr.c raw.c flic.c audio.c
SOURCES += segafilm.c swf.c utils.c wav.c wc3movie.c westwood.c yuv4mpeg.c
SOURCES += sierravmd.c asf-enc.c matroska.c img2.c electronicarts.c sol.c
SOURCES += nsvdec.c

# not using:  barpainet.* beosaudio.cpp, dv1394.*, dc1394.c, framehook.*
# not using:  gif.c gifdec.c grab.c http.c jpeg.c png.c pnm.c rtp.*
# not using:  rtpproto.c rtsp* sgi.c tcp.c udp.c yuv.c

contains( AMR_NB, yes ) {
    SOURCES += amr.c
}
contains( AMR_NB_FIXED, yes ) {
    SOURCES += amr.c
}
contains( AMR_WB, yes ) {
    SOURCES += amr.c
}

contains( CONFIG_VORBIS, yes ) {
    SOURCES += ogg.c
}

inc.path = $${PREFIX}/include/mythtv/ffmpeg/
inc.files = avformat.h avio.h

INSTALLS += inc

LIBS += -L../libavcodec -lmythavcodec-$$LIBVERSION

macx {
    LIBS               += -lz
    QMAKE_LFLAGS_SHLIB += -single_module
    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC4000000
    SOURCES            -= audio.c
#    SOURCES            += audio-darwin.c
}

