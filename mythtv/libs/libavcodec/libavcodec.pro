include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavcodec-$$LIBVERSION
CONFIG += thread dll warn_off
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.18.0

INCLUDEPATH = ../ ../../ 

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE

# Debug mode on x86 must compile without -fPIC and with -O, 
# otherwise gcc runs out of registers.
debug:contains(TARGET_ARCH_X86, yes) {
    QMAKE_CFLAGS_SHLIB = 
}

QMAKE_CFLAGS_DEBUG += -O

# Input
SOURCES += bitstream.c utils.c mem.c allcodecs.c mpegvideo.c jrevdct.c 
SOURCES += jfdctfst.c jfdctint.c mpegaudio.c ac3enc.c mjpeg.c audresample.c
SOURCES += resample2.c dsputil.c motion_est.c imgconvert.c imgresample.c
SOURCES += mpeg12.c mpegaudiodec.c pcm.c simple_idct.c ratecontrol.c adpcm.c
SOURCES += eval.c error_resilience.c fft.c mdct.c raw.c golomb.c cabac.c
SOURCES += dpcm.c adx.c rational.c faandct.c parser.c g726.c vp3dsp.c integer.c
SOURCES += h264idct.c rangecoder.c pnm.c h263.c msmpeg4.c h263dec.c dvdsub.c 
SOURCES += dvbsub.c

inc.path = $${PREFIX}/include/mythtv/ffmpeg/
inc.files = avcodec.h rational.h common.h

INSTALLS += inc

LIBS += $$LOCAL_LIBDIR_X11

contains( CONFIG_AASC_DECODER, yes ) {
    SOURCES += aasc.c
}

contains( CONFIG_ALAC_DECODER, yes ) {
    SOURCES += alac.c
}

DO_ASV1 = $$CONFIG_ASV1_DECODER $$CONFIG_ASV1_ENCODER $$CONFIG_ASV2_DECODER $$CONFIG_ASV2_ENCODER
contains( DO_ASV1, yes ) {
    SOURCES += asv1.c
}

contains( CONFIG_CINEPAK_DECODER, yes ) {
    SOURCES += cinepak.c
}

DO_CLJR = $$CONFIG_CLJR_DECODER $$CONFIG_CLJR_ENCODER
contains( DO_CLJR, yes ) {
    SOURCES += cljr.c
}

contains( CONFIG_CYUV_DECODER, yes ) {
    SOURCES += cyuv.c
}

DO_DV = $$CONFIG_DVVIDEO_DECODER $$CONFIG_DVVIDEO_ENCODER
contains( DO_DV, yes ) {
    SOURCES += dv.c
}

contains( CONFIG_EIGHTBPS_DECODER, yes ) {
    SOURCES += 8bps.c
}

DO_FFV1 = $$CONFIG_FFV1_DECODER $$CONFIG_FFV1_ENCODER
contains( DO_FFV1, yes ) {
    SOURCES += ffv1.c
}

contains( CONFIG_FLAC_DECODER, yes ) {
    SOURCES += flac.c
}

contains( CONFIG_FLIC_DECODER, yes ) {
    SOURCES += flicvideo.c
}

contains( CONFIG_FOURXM_DECODER, yes ) {
    SOURCES += 4xm.c
}

contains( CONFIG_FRAPS_DECODER, yes ) {
    SOURCES += fraps.c
}

DO_H261 = $$CONFIG_H261_DECODER $$CONFIG_H261_ENCODER
contains( DO_H261, yes ) {
    SOURCES += h261.c
}

DO_H264 = $$CONFIG_H264_DECODER $$CONFIG_SVQ3_DECODER
contains( DO_H264, yes ) {
    SOURCES += h264.c
}

DO_HUFFYUV = $$CONFIG_HUFFYUV_DECODER $$CONFIG_HUFFYUV_ENCODER $$CONFIG_FFVHUFF_DECODER $$CONFIG_FFVHUFF_ENCODER
contains( DO_HUFFYUV, yes ) {
    SOURCES += huffyuv.c
}

contains( CONFIG_IDCIN_DECODER, yes ) {
    SOURCES += idcinvideo.c
}

contains( CONFIG_INDEO2_DECODER, yes ) {
    SOURCES += indeo2.c
}

contains( CONFIG_INDEO3_DECODER, yes ) {
    SOURCES += indeo3.c
}

contains( CONFIG_INTERPLAY_VIDEO_DECODER, yes ) {
    SOURCES += interplayvideo.c
}

DO_LCL = $$CONFIG_MSZH_DECODER $$CONFIG_ZLIB_DECODER $$CONFIG_ZLIB_ENCODER
contains( DO_LCL, yes ) {
    SOURCES += lcl.c
}

contains( CONFIG_LOCO_DECODER, yes ) {
    SOURCES += loco.c
}

DO_MACE = $$CONFIG_MACE3_DECODER $$CONFIG_MACE6_DECODER
contains( DO_MACE, yes ) {
    SOURCES += mace.c
}

contains( CONFIG_MSRLE_DECODER, yes ) {
    SOURCES += msrle.c
}

contains( CONFIG_MSVIDEO1_DECODER, yes ) {
    SOURCES += msvideo1.c
}

DO_PNG = $$CONFIG_PNG_DECODER $$CONFIG_PNG_ENCODER
contains( DO_PNG, yes ) {
    SOURCES += png.c
}

contains( CONFIG_QDRAW_DECODER, yes ) {
    SOURCES += qdrw.c
}

contains( CONFIG_QPEG_DECODER, yes ) {
    SOURCES += qpeg.c
}

contains( CONFIG_QTRLE_DECODER, yes ) {
    SOURCES += qtrle.c
}

contains( CONFIG_RA_144_DECODER, yes ) {
    SOURCES += ra144.c
}

contains( CONFIG_RA_288_DECODER, yes ) {
    SOURCES += ra288.c
}

contains( CONFIG_ROQ_DECODER, yes ) {
    SOURCES += roqvideo.c
}

contains( CONFIG_RPZA_DECODER, yes ) {
    SOURCES += rpza.c
}

DO_RV10 = $$CONFIG_RV10_DECODER $$CONFIG_RV20_DECODER $$CONFIG_RV10_ENCODER $$CONFIG_RV20_ENCODER
contains( DO_RV10, yes ) {
    SOURCES += rv10.c
}

contains( CONFIG_SHORTEN_DECODER, yes ) {
    SOURCES += shorten.c
}

contains( CONFIG_SMC_DECODER, yes ) {
    SOURCES += smc.c
}

DO_SNOW = $$CONFIG_SNOW_DECODER $$CONFIG_SNOW_ENCODER
contains( DO_SNOW, yes ) {
    SOURCES += snow.c
}

DO_SONIC = $$CONFIG_SONIC_DECODER $$CONFIG_SONIC_ENCODER $$CONFIG_SONIC_LS_ENCODER
contains( DO_SONIC, yes ) {
    SOURCES += sonic.c
}

DO_SVQ1 = $$CONFIG_SVQ1_DECODER $$CONFIG_SVQ1_ENCODER
contains( DO_SVQ1, yes ) {
    SOURCES += svq1.c
}

contains( CONFIG_TRUEMOTION1_DECODER, yes ) {
    SOURCES += truemotion1.c
}

contains( CONFIG_TSCC_DECODER, yes ) {
    SOURCES += tscc.c
}

contains( CONFIG_ULTI_DECODER, yes ) {
    SOURCES += ulti.c
}

DO_VC9 = $$CONFIG_VC9_DECODER $$CONFIG_WMV3_DECODER 
contains( DO_VC9, yes ) {
    SOURCES += vc9.c
}

DO_VCR1 = $$CONFIG_VCR1_DECODER $$CONFIG_VCR1_ENCODER
contains( DO_VCR1, yes ) {
    SOURCES += vcr1.c
}

DO_VMDAV = $$CONFIG_VMDVIDEO_DECODER $$CONFIG_VMDAUDIO_DECODER
contains( DO_VMDAV, yes ) {
    SOURCES += vmdav.c
}

contains( CONFIG_VORBIS_DECODER, yes ) {
    SOURCES += vorbis.c
}

DO_VP3 = $$CONFIG_VP3_DECODER $$CONFIG_THEORA_DECODER
contains( DO_VP3, yes ) {
    SOURCES += vp3.c
}

contains( CONFIG_VQA_DECODER, yes ) {
    SOURCES += vqavideo.c
}

DO_WMA = $$CONFIG_WMAV1_DECODER $$CONFIG_WMAV2_DECODER
contains( DO_WMA, yes ) {
    SOURCES += wmadec.c
}

contains( CONFIG_WNV1_DECODER, yes ) {
    SOURCES += wnv1.c
}

contains( CONFIG_WS_SND1_DECODER, yes ) {
    SOURCES += ws-snd1.c
}

DO_XAN = $$CONFIG_XAN_WC3_DECODER $$CONFIG_XAN_WC4_DECODER
contains( DO_XAN, yes ) {
    SOURCES += xan.c
}

contains( CONFIG_XL_DECODER, yes ) {
    SOURCES += xl.c
}

contains( HAVE_PTHREADS, yes ) {
    SOURCES += pthread.c
}

contains( HAVE_W32THREADS, yes ) {
    SOURCES += w32thread.c
}

contains( HAVE_BEOSTHREADS, yes ) {
    SOURCES += beosthread.c
}

contains( CONFIG_AC3, yes ) {
    SOURCES += a52dec.c
    !contains( CONFIG_A52BIN, yes ) {
        SOURCES += liba52/bit_allocate.c liba52/a52_bitstream.c liba52/downmix.c
        SOURCES += liba52/imdct.c liba52/parse.c liba52/crc.c liba52/resample.c
    }
}

contains( CONFIG_DTS, yes ) {
    SOURCES += dtsdec.c
    LIBS += -ldts
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

contains( CONFIG_FAAD, yes ) {
    SOURCES += faad.c
    !contains( CONFIG_FAADBIN, yes) {
        LIBS += -lfaad
    }
}

contains( CONFIG_FAAC, yes ) {
    SOURCES += faac.c
    LIBS += -lfaac
}

contains( CONFIG_XVID, yes ) {
    SOURCES += xvidff.c
    LIBS += -lxvidcore
}

contains( CONFIG_X264, yes ) {
    SOURCES += x264.c
    LIBS += -lx264
} 

contains( CONFIG_LIBOGG, yes ) {
    contains( CONFIG_LIBVORBIS, yes ) {
        SOURCES += oggvorbis.c
        LIBS += -lvorbisenc -lvorbis
    }
    contains( CONFIG_LIBTHEORA, yes ) {
        SOURCES += oggtheora.c
        LIBS += -ltheora
    }
    LIBS += -logg
}

contains( CONFIG_LIBGSM, yes ) {
    SOURCES += libgsm.c
    LIBS += -lgsm
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
