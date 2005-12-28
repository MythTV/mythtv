include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavcodec-$$LIBVERSION
CONFIG += thread dll warn_off
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.18.2

INCLUDEPATH = ../ ../../ 

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE

# Debug mode on x86 must compile without -fPIC and with -O, 
# otherwise gcc runs out of registers.
debug:contains(TARGET_ARCH_X86, yes) {
    QMAKE_CFLAGS_SHLIB = 
}

QMAKE_CFLAGS_DEBUG += -O

# Input
SOURCES += utils.c mem.c allcodecs.c mpegvideo.c h263.c jrevdct.c
SOURCES += jfdctfst.c mpegaudio.c ac3enc.c mjpeg.c audresample.c dsputil.c
SOURCES += motion_est.c imgconvert.c imgresample.c msmpeg4.c mpeg12.c
SOURCES += h263dec.c svq1.c rv10.c mpegaudiodec.c pcm.c simple_idct.c
SOURCES += ratecontrol.c adpcm.c eval.c jfdctint.c dv.c error_resilience.c
SOURCES += wmadec.c fft.c mdct.c mace.c huffyuv.c opts.c cyuv.c
SOURCES += golomb.c h264.c raw.c indeo3.c asv1.c vp3.c 4xm.c cabac.c
SOURCES += ra144.c ra288.c vcr1.c cljr.c roqvideo.c dpcm.c tscc.c
SOURCES += interplayvideo.c xan.c rpza.c cinepak.c msrle.c msvideo1.c
SOURCES += vqavideo.c idcinvideo.c adx.c rational.c faandct.c snow.c sonic.c
SOURCES += 8bps.c parser.c smc.c flicvideo.c truemotion1.c vmdav.c lcl.c
SOURCES += qtrle.c g726.c flac.c vp3dsp.c integer.c h261.c resample2.c
SOURCES += h264idct.c png.c pnm.c qdrw.c qpeg.c rangecoder.c ulti.c xl.c
SOURCES += bitstream.c vc9.c libpostproc/postprocess.c #ffv1.c 

inc.path = $${PREFIX}/include/mythtv/ffmpeg/
inc.files = avcodec.h rational.h common.h

INSTALLS += inc

LIBS += $$LOCAL_LIBDIR_X11

contains( CONFIG_AC3, yes ) {
    SOURCES += a52dec.c
    !contains( CONFIG_A52BIN, yes ) {
        SOURCES += liba52/bit_allocate.c liba52/a52_bitstream.c liba52/downmix.c
        SOURCES += liba52/imdct.c liba52/parse.c liba52/crc.c liba52/resample.c
    }
}

using_xvmc {
    SOURCES += xvmcvideo.c
    DEFINES += HAVE_XVMC
}

using_xvmc_vld {
    SOURCES += xvmcvldvideo.c
    DEFINES += HAVE_XVMC_VLD
}

contains( AMR_NB, yes) {
    SOURCES += amr.c
}

contains( CONFIG_MP3LAME, yes ) {
    SOURCES += mp3lameaudio.c
    LIBS += -lmp3lame
}

contains( CONFIG_FAAD, yes) {
    SOURCES += faad.c
    LIBS += -lfaad
}
    
contains( CONFIG_VORBIS, yes ) {
     SOURCES += oggvorbis.c
}

contains( CONFIC_DTS, yes ) {
    SOURCES += dts.c
}

contains( CONFIG_FAAC, yes ) {
    SOURCES += faac.c
}

contains( TARGET_GPROF, yes ) {
    QMAKE_CFLAGS_RELEASE += -p
    QMAKE_LFLAGS_RELEASE += -p
}

contains( TARGET_MMX, yes ) {
    SOURCES += i386/fdct_mmx.c i386/cputest.c i386/dsputil_mmx.c
    SOURCES += i386/mpegvideo_mmx.c i386/idct_mmx.c i386/motion_est_mmx.c
    SOURCES += i386/simple_idct_mmx.c i386/fft_sse.c i386/vp3dsp_mmx.c
    SOURCES += i386/vp3dsp_sse2.c
#    contains( TARGET_BUILTIN_VECTOR, yes ) {
#        QMAKE_CFLAGS_RELEASE += -msse
#        QMAKE_CFLAGS_DEBUG += -msse
#    }
}

contains( TARGET_ARCH_ARMV4L, yes ) {
    SOURCES += armv4l/jrevdct_arm.S armv4l/dsputil_arm.c
}

contains( HAVE_MLIB, yes ) {
    SOURCES += mlib/dsputil_mlib.c
    QMAKE_CFLAGS_RELEASE += $$MLIB_INC
}

contains( TARGET_ARCH_ALPHA, yes ) {
    SOURCES += alpha/dsputil_alpha.c alpha/mpegvideo_alpha.c 
    SOURCES += alpha/motion_est_alpha.c alpha/dsputil_alpha_asm.S
    QMAKE_CFLAGS_RELEASE += -fforce-addr -freduce-all-givs
}

contains( TARGET_ARCH_POWERPC, yes ) {
    SOURCES += ppc/dsputil_ppc.c ppc/mpegvideo_ppc.c
}

contains( TARGET_ALTIVEC, yes ) {
    SOURCES += ppc/dsputil_altivec.c ppc/mpegvideo_altivec.c ppc/idct_altivec.c
    SOURCES += ppc/gmc_altivec.c ppc/fdct_altivec.c ppc/fft_altivec.c
    SOURCES += ppc/dsputil_h264_altivec.c
  macx {
    QMAKE_CFLAGS_RELEASE += -faltivec
    QMAKE_CFLAGS_DEBUG   += -faltivec
  }
  !macx {
    QMAKE_CFLAGS_RELEASE += -maltivec -mabi=altivec
  }
}

macx {
    LIBS               += -lz
    QMAKE_LFLAGS_SHLIB += -single_module
    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC3000000
}
