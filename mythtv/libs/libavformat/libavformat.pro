include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavformat-$$LIBVERSION
CONFIG += thread dll warn_off
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.15.0 

include ( ../../config.mak )

!exists( ../../config.mak ) {
    error(Please run the configure script first)
}

INCLUDEPATH += ../../ ../libavcodec ../libmythtv

QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -DHAVE_AV_CONFIG_H -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
QMAKE_CFLAGS_DEBUG = -g -DHAVE_AV_CONFIG_H -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE

# Input
HEADERS += asf.h avformat.h avi.h avio.h dv.h mpegts.h os_support.h qtpalette.h

SOURCES += 4xm.c allformats.c asf.c au.c avidec.c avienc.c avio.c aviobuf.c 
SOURCES += crc.c cutils.c dv.c ffm.c file.c flvdec.c flvenc.c idcin.c idroq.c
SOURCES += img.c ipmovie.c mov.c movenc.c mp3.c mpeg.c mpegts.c mpegtsenc.c 
SOURCES += mpjpeg.c nut.c os_support.c rm.c psxstr.c raw.c flic.c audio.c
SOURCES += segafilm.c swf.c utils.c wav.c wc3movie.c westwood.c yuv4mpeg.c
SOURCES += sierravmd.c asf-enc.c matroska.c

# not using:  barpainet.* beosaudio.cpp, dv1394.*, framehook.*
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

