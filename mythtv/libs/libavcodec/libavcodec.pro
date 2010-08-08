include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavcodec-$$LIBVERSION
CONFIG += thread dll warn_off
CONFIG -= qt
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_LFLAGS += $$LDFLAGS

INCLUDEPATH = .. ../..

# remove MMX define since it clashes with a libmp3lame enum
DEFINES -= MMX
DEFINES += HAVE_AV_CONFIG_H

# Debug mode on x86 must compile without -fPIC and with -O,
# otherwise gcc runs out of registers.
# libavcodec rev 7671 needs another register
# -fomit-frame-pointer frees it. gcc-4 enables "location lists"
# which allows debugging without frame pointer
debug:contains(ARCH_X86_32, yes) {
        QMAKE_CFLAGS_DEBUG += -fomit-frame-pointer
}
# gcc-4.2 and newer can not compile with PIC on x86
contains(ARCH_X86_32, yes) {
	QMAKE_CFLAGS_SHLIB =
}

!profile:QMAKE_CFLAGS_DEBUG += -O

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
SOURCES += allcodecs.c
SOURCES += audioconvert.c
SOURCES += avpacket.c
SOURCES += bitstream.c
SOURCES += bitstream_filter.c
SOURCES += dsputil.c
SOURCES += eval.c
SOURCES += faanidct.c
SOURCES += h263.c
SOURCES += imgconvert.c
SOURCES += jrevdct.c
SOURCES += motion_est.c
SOURCES += opt.c
SOURCES += options.c
SOURCES += parser.c
SOURCES += rangecoder.c
SOURCES += ratecontrol.c
SOURCES += raw.c
SOURCES += resample.c
SOURCES += resample2.c
SOURCES += simple_idct.c
SOURCES += utils.c

SOURCES += myth_utils.c

inc.path = $${PREFIX}/include/mythtv/libavcodec/
inc.files = avcodec.h x86/mmx.h dxva2.h opt.h vaapi.h vdpau.h xvmc.h

INSTALLS += inc

LIBS += $$LOCAL_LIBDIR_X11
LIBS += -L../libavutil -lmythavutil-$$LIBVERSION $$EXTRALIBS


# parts needed for many different codecs
contains( CONFIG_AANDCT, yes )                  { SOURCES *= aandcttab.c }
contains( CONFIG_ENCODERS, yes )                { SOURCES *= faandct.c jfdctfst.c jfdctint.c }
contains( CONFIG_DCT, yes )                     { SOURCES *= dct.c }
contains( CONFIG_DXVA2, yes )                   { SOURCES *= dxva2.c }
contains( CONFIG_FFT, yes )                     { SOURCES *= fft.c }
contains( CONFIG_GOLOMB, yes )                  { SOURCES *= golomb.c }
contains( CONFIG_LPC, yes )                     { SOURCES *= lpc.c }
contains( CONFIG_MDCT, yes )                    { SOURCES *= mdct.c }
contains( CONFIG_RDFT, yes )                    { SOURCES *= rdft.c }
contains( CONFIG_VAAPI, yes )                   { SOURCES *= vaapi.c }
contains( CONFIG_VDPAU, yes )                   { SOURCES *= vdpau.c }

#hardcoded tables
contains( CONFIG_HARDCODED_TABLES, yes ) {
    contains( CONFIG_FFT, yes )                 { SOURCES *= cos_tables.c }
    contains( CONFIG_RDFT, yes )                { SOURCES *= sin_tables.c }
}


# decoders/encoders/hardware accelerators
contains( CONFIG_AAC_DECODER, yes )             { SOURCES *= aac.c aactab.c }
contains( CONFIG_AAC_ENCODER, yes )             { SOURCES *= aacenc.c aaccoder.c aacpsy.c aactab.c psymodel.c iirfilter.c mpeg4audio.c }
contains( CONFIG_AASC_DECODER, yes )            { SOURCES *= aasc.c msrledec.c }
contains( CONFIG_AC3_DECODER, yes )             { SOURCES *= ac3dec.c ac3dec_data.c ac3.c }
contains( CONFIG_AC3_ENCODER, yes )             { SOURCES *= ac3enc.c ac3tab.c ac3.c }
contains( CONFIG_ALAC_DECODER, yes )            { SOURCES *= alac.c }
contains( CONFIG_ALAC_ENCODER, yes )            { SOURCES *= alacenc.c }
contains( CONFIG_ALS_DECODER, yes )             { SOURCES *= alsdec.c }
contains( CONFIG_AMV_DECODER, yes )             { SOURCES *= sp5xdec.c mjpegdec.c mjpeg.c }
contains( CONFIG_ANM_DECODER, yes )             { SOURCES *= anm.c }
contains( CONFIG_APE_DECODER, yes )             { SOURCES *= apedec.c }
contains( CONFIG_ASV1_DECODER, yes )            { SOURCES *= asv1.c mpeg12data.c }
contains( CONFIG_ASV1_ENCODER, yes )            { SOURCES *= asv1.c mpeg12data.c }
contains( CONFIG_ASV2_DECODER, yes )            { SOURCES *= asv1.c mpeg12data.c }
contains( CONFIG_ASV2_ENCODER, yes )            { SOURCES *= asv1.c mpeg12data.c }
contains( CONFIG_ATRAC1_DECODER, yes )          { SOURCES *= atrac1.c atrac.c }
contains( CONFIG_ATRAC3_DECODER, yes )          { SOURCES *= atrac3.c atrac.c }
contains( CONFIG_AURA_DECODER, yes )            { SOURCES *= cyuv.c }
contains( CONFIG_AURA2_DECODER, yes )           { SOURCES *= aura.c }
contains( CONFIG_AVS_DECODER, yes )             { SOURCES *= avs.c }
contains( CONFIG_BETHSOFTVID_DECODER, yes )     { SOURCES *= bethsoftvideo.c }
contains( CONFIG_BFI_DECODER, yes )             { SOURCES *= bfi.c }
contains( CONFIG_BINKAUDIO_DCT_DECODER, yes )   { SOURCES *= binkaudio.c wma.c }
contains( CONFIG_BINKAUDIO_RDFT_DECODER, yes )  { SOURCES *= binkaudio.c wma.c }
contains( CONFIG_BMP_DECODER, yes )             { SOURCES *= bmp.c msrledec.c }
contains( CONFIG_BMP_ENCODER, yes )             { SOURCES *= bmpenc.c }
contains( CONFIG_C93_DECODER, yes )             { SOURCES *= c93.c }
contains( CONFIG_CAVS_DECODER, yes )            { SOURCES *= cavs.c cavsdec.c cavsdsp.c mpeg12data.c mpegvideo.c }
contains( CONFIG_CINEPAK_DECODER, yes )         { SOURCES *= cinepak.c }
contains( CONFIG_CDGRAPHICS_DECODER, yes )      { SOURCES *= cdgraphics.c }
contains( CONFIG_CLJR_DECODER, yes )            { SOURCES *= cljr.c }
contains( CONFIG_CLJR_ENCODER, yes )            { SOURCES *= cljr.c }
contains( CONFIG_COOK_DECODER, yes )            { SOURCES *= cook.c }
contains( CONFIG_CSCD_DECODER, yes )            { SOURCES *= cscd.c }
contains( CONFIG_CYUV_DECODER, yes )            { SOURCES *= cyuv.c }
contains( CONFIG_DCA_DECODER, yes )             { SOURCES *= dca.c synth_filter.c }
contains( CONFIG_DNXHD_DECODER, yes )           { SOURCES *= dnxhddec.c dnxhddata.c }
contains( CONFIG_DNXHD_ENCODER, yes )           { SOURCES *= dnxhdenc.c dnxhddata.c mpegvideo_enc.c motion_est.c ratecontrol.c mpeg12data.c mpegvideo.c }
contains( CONFIG_DPX_DECODER, yes )             { SOURCES *= dpx.c }
contains( CONFIG_DSICINAUDIO_DECODER, yes )     { SOURCES *= dsicinav.c }
contains( CONFIG_DSICINVIDEO_DECODER, yes )     { SOURCES *= dsicinav.c }
contains( CONFIG_DVBSUB_DECODER, yes )          { SOURCES *= dvbsubdec.c }
contains( CONFIG_DVBSUB_ENCODER, yes )          { SOURCES *= dvbsub.c }
contains( CONFIG_DVDSUB_DECODER, yes )          { SOURCES *= dvdsubdec.c }
contains( CONFIG_DVDSUB_ENCODER, yes )          { SOURCES *= dvdsubenc.c }
contains( CONFIG_DVVIDEO_DECODER, yes )         { SOURCES *= dv.c dvdata.c }
contains( CONFIG_DVVIDEO_ENCODER, yes )         { SOURCES *= dv.c dvdata.c }
contains( CONFIG_DXA_DECODER, yes )             { SOURCES *= dxa.c }
contains( CONFIG_EAC3_DECODER, yes )            { SOURCES *= eac3dec.c eac3dec_data.c }
contains( CONFIG_EACMV_DECODER, yes )           { SOURCES *= eacmv.c }
contains( CONFIG_EAMAD_DECODER, yes )           { SOURCES *= eamad.c eaidct.c mpeg12.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_EATGQ_DECODER, yes )           { SOURCES *= eatgq.c eaidct.c }
contains( CONFIG_EATGV_DECODER, yes )           { SOURCES *= eatgv.c }
contains( CONFIG_EATQI_DECODER, yes )           { SOURCES *= eatqi.c eaidct.c mpeg12.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_EIGHTBPS_DECODER, yes )        { SOURCES *= 8bps.c }
contains( CONFIG_EIGHTSVX_EXP_DECODER, yes )    { SOURCES *= 8svx.c }
contains( CONFIG_EIGHTSVX_FIB_DECODER, yes )    { SOURCES *= 8svx.c }
contains( CONFIG_ESCAPE124_DECODER, yes )       { SOURCES *= escape124.c }
contains( CONFIG_FFV1_DECODER, yes )            { SOURCES *= ffv1.c }
contains( CONFIG_FFV1_ENCODER, yes )            { SOURCES *= ffv1.c }
contains( CONFIG_FFVHUFF_DECODER, yes )         { SOURCES *= huffyuv.c }
contains( CONFIG_FFVHUFF_ENCODER, yes )         { SOURCES *= huffyuv.c }
contains( CONFIG_FLAC_DECODER, yes )            { SOURCES *= flac.c flacdata.c }
contains( CONFIG_FLAC_ENCODER, yes )            { SOURCES *= flacenc.c flacdata.c }
contains( CONFIG_FLASHSV_DECODER, yes )         { SOURCES *= flashsv.c }
contains( CONFIG_FLASHSV_ENCODER, yes )         { SOURCES *= flashsvenc.c }
contains( CONFIG_FLIC_DECODER, yes )            { SOURCES *= flicvideo.c }
contains( CONFIG_FOURXM_DECODER, yes )          { SOURCES *= 4xm.c }
contains( CONFIG_FRAPS_DECODER, yes )           { SOURCES *= fraps.c huffman.c }
contains( CONFIG_FRWU_DECODER, yes )            { SOURCES *= frwu.c }
contains( CONFIG_GIF_DECODER, yes )             { SOURCES *= gifdec.c lzw.c }
contains( CONFIG_GIF_ENCODER, yes )             { SOURCES *= gif.c }
contains( CONFIG_H261_DECODER, yes )            { SOURCES *= h261dec.c h261.c mpegvideo.c error_resilience.c }
contains( CONFIG_H261_ENCODER, yes )            { SOURCES *= h261enc.c h261.c mpegvideo_enc.c motion_est.c ratecontrol.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_H263_DECODER, yes )            { SOURCES *= h263dec.c h263.c ituh263dec.c mpeg4video.c mpeg4videodec.c flvdec.c intelh263dec.c mpegvideo.c error_resilience.c }
contains( CONFIG_H263_VAAPI_HWACCEL, yes )      { SOURCES *= vaapi_mpeg4.c }
contains( CONFIG_H263_ENCODER, yes )            { SOURCES *= mpegvideo_enc.c mpeg4video.c mpeg4videoenc.c motion_est.c ratecontrol.c h263.c ituh263enc.c flvenc.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_H264_DECODER, yes )            { SOURCES *= h264.c h264idct.c h264pred.c h264_loopfilter.c h264_direct.c cabac.c h264_sei.c h264_ps.c h264_refs.c h264_cavlc.c h264_cabac.c mpegvideo.c error_resilience.c }
contains( CONFIG_H264_DXVA2_HWACCEL, yes )      { SOURCES *= dxva2_h264.c }
contains( CONFIG_H264_ENCODER, yes )            { SOURCES *= h264enc.c h264dspenc.c }
contains( CONFIG_H264_VAAPI_HWACCEL, yes )      { SOURCES *= vaapi_h264.c }
contains( CONFIG_HUFFYUV_DECODER, yes )         { SOURCES *= huffyuv.c }
contains( CONFIG_HUFFYUV_ENCODER, yes )         { SOURCES *= huffyuv.c }
contains( CONFIG_IDCIN_DECODER, yes )           { SOURCES *= idcinvideo.c }
contains( CONFIG_IFF_BYTERUN1_DECODER, yes )    { SOURCES *= iff.c }
contains( CONFIG_IFF_ILBM_DECODER, yes )        { SOURCES *= iff.c }
contains( CONFIG_IMC_DECODER, yes )             { SOURCES *= imc.c }
contains( CONFIG_INDEO2_DECODER, yes )          { SOURCES *= indeo2.c }
contains( CONFIG_INDEO3_DECODER, yes )          { SOURCES *= indeo3.c }
contains( CONFIG_INTERPLAY_DPCM_DECODER, yes )  { SOURCES *= dpcm.c }
contains( CONFIG_INTERPLAY_VIDEO_DECODER, yes ) { SOURCES *= interplayvideo.c }
contains( CONFIG_JPEGLS_DECODER, yes )          { SOURCES *= jpeglsdec.c jpegls.c mjpegdec.c mjpeg.c }
contains( CONFIG_JPEGLS_ENCODER, yes )          { SOURCES *= jpeglsenc.c jpegls.c }
contains( CONFIG_KMVC_DECODER, yes )            { SOURCES *= kmvc.c }
contains( CONFIG_LJPEG_ENCODER, yes )           { SOURCES *= ljpegenc.c mjpegenc.c mjpeg.c mpegvideo_enc.c motion_est.c ratecontrol.c mpeg12data.c mpegvideo.c }
contains( CONFIG_LOCO_DECODER, yes )            { SOURCES *= loco.c }
contains( CONFIG_MACE3_DECODER, yes )           { SOURCES *= mace.c }
contains( CONFIG_MACE6_DECODER, yes )           { SOURCES *= mace.c }
contains( CONFIG_MDEC_DECODER, yes )            { SOURCES *= mdec.c mpeg12.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_MIMIC_DECODER, yes )           { SOURCES *= mimic.c }
contains( CONFIG_MJPEG_DECODER, yes )           { SOURCES *= mjpegdec.c mjpeg.c }
contains( CONFIG_MJPEG_ENCODER, yes )           { SOURCES *= mjpegenc.c mjpeg.c mpegvideo_enc.c motion_est.c ratecontrol.c mpeg12data.c mpegvideo.c }
contains( CONFIG_MJPEGB_DECODER, yes )          { SOURCES *= mjpegbdec.c mjpegdec.c mjpeg.c }
contains( CONFIG_MLP_DECODER, yes )             { SOURCES *= mlpdec.c mlpdsp.c }
contains( CONFIG_MMVIDEO_DECODER, yes )         { SOURCES *= mmvideo.c }
contains( CONFIG_MOTIONPIXELS_DECODER, yes )    { SOURCES *= motionpixels.c }
contains( CONFIG_MP1_DECODER, yes )             { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MP2_DECODER, yes )             { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MP2_ENCODER, yes )             { SOURCES *= mpegaudioenc.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MP3ADU_DECODER, yes )          { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MP3ON4_DECODER, yes )          { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c  mpeg4audio.c }
contains( CONFIG_MP3_DECODER, yes )             { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MPC7_DECODER, yes )            { SOURCES *= mpc7.c mpc.c mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MPC8_DECODER, yes )            { SOURCES *= mpc8.c mpc.c mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MPEG_XVMC_DECODER, yes )       { SOURCES *= mpegvideo_xvmc.c }
contains( CONFIG_MPEG_XVMC_VLD_DECODER, yes )   { SOURCES *= xvmcvldvideo.c mpeg12.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_MPEG1VIDEO_DECODER, yes )      { SOURCES *= mpeg12.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_MPEG1VIDEO_ENCODER, yes )      { SOURCES *= mpeg12enc.c mpegvideo_enc.c motion_est.c ratecontrol.c mpeg12.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_MPEG2_VAAPI_HWACCEL, yes )     { SOURCES *= vaapi_mpeg2.c }
contains( CONFIG_MPEG2VIDEO_DECODER, yes )      { SOURCES *= mpeg12.c mpegvideo.c error_resilience.c }
contains( CONFIG_MPEG2VIDEO_ENCODER, yes )      { SOURCES *= mpeg12enc.c mpegvideo_enc.c motion_est.c ratecontrol.c mpeg12.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_MPEG4_VAAPI_HWACCEL, yes )     { SOURCES *= vaapi_mpeg4.c }
contains( CONFIG_MSMPEG4V1_DECODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c }
contains( CONFIG_MSMPEG4V1_ENCODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c }
contains( CONFIG_MSMPEG4V2_DECODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c }
contains( CONFIG_MSMPEG4V2_ENCODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c }
contains( CONFIG_MSMPEG4V3_DECODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c }
contains( CONFIG_MSMPEG4V3_ENCODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c }
contains( CONFIG_MSRLE_DECODER, yes )           { SOURCES *= msrle.c msrledec.c }
contains( CONFIG_MSVIDEO1_DECODER, yes )        { SOURCES *= msvideo1.c }
contains( CONFIG_MSZH_DECODER, yes )            { SOURCES *= lcldec.c }
contains( CONFIG_NELLYMOSER_DECODER, yes )      { SOURCES *= nellymoserdec.c nellymoser.c }
contains( CONFIG_NELLYMOSER_ENCODER, yes )      { SOURCES *= nellymoserenc.c nellymoser.c }
contains( CONFIG_NUV_DECODER, yes )             { SOURCES *= nuv.c rtjpeg.c }
contains( CONFIG_PAM_DECODER, yes )             { SOURCES *= pnmdec.c pnm.c }
contains( CONFIG_PAM_ENCODER, yes )             { SOURCES *= pamenc.c pnm.c  }
contains( CONFIG_PBM_DECODER, yes )             { SOURCES *= pnmdec.c pnm.c }
contains( CONFIG_PBM_ENCODER, yes )             { SOURCES *= pnmenc.c pnm.c  }
contains( CONFIG_PCX_DECODER, yes )             { SOURCES *= pcx.c }
contains( CONFIG_PCX_ENCODER, yes )             { SOURCES *= pcxenc.c }
contains( CONFIG_PGM_DECODER, yes )             { SOURCES *= pnmdec.c pnm.c }
contains( CONFIG_PGM_ENCODER, yes )             { SOURCES *= pnmenc.c pnm.c }
contains( CONFIG_PGMYUV_DECODER, yes )          { SOURCES *= pnmdec.c pnm.c }
contains( CONFIG_PGMYUV_ENCODER, yes )          { SOURCES *= pnmenc.c pnm.c }
contains( CONFIG_PGSSUB_DECODER, yes )          { SOURCES *= pgssubdec.c }
contains( CONFIG_PNG_DECODER, yes )             { SOURCES *= png.c pngdec.c }
contains( CONFIG_PNG_ENCODER, yes )             { SOURCES *= png.c pngenc.c }
contains( CONFIG_PPM_DECODER, yes )             { SOURCES *= pnmdec.c pnm.c }
contains( CONFIG_PPM_ENCODER, yes )             { SOURCES *= pnmenc.c pnm.c }
contains( CONFIG_PTX_DECODER, yes )             { SOURCES *= ptx.c }
contains( CONFIG_QCELP_DECODER, yes )           { SOURCES *= qcelpdec.c lsp.c celp_math.c celp_filters.c acelp_vectors.c }
contains( CONFIG_QDM2_DECODER, yes )            { SOURCES *= qdm2.c mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_QDRAW_DECODER, yes )           { SOURCES *= qdrw.c }
contains( CONFIG_QPEG_DECODER, yes )            { SOURCES *= qpeg.c }
contains( CONFIG_QTRLE_DECODER, yes )           { SOURCES *= qtrle.c }
contains( CONFIG_QTRLE_ENCODER, yes)            { SOURCES *= qtrleenc.c }
contains( CONFIG_R210_DECODER, yes)             { SOURCES *= r210dec.c }
contains( CONFIG_RA_144_DECODER, yes )          { SOURCES *= ra144.c celp_filters.c }
contains( CONFIG_RA_288_DECODER, yes )          { SOURCES *= ra288.c celp_math.c celp_filters.c }
contains( CONFIG_RAWVIDEO_DECODER, yes )        { SOURCES *= rawdec.c }
contains( CONFIG_RAWVIDEO_ENCODER, yes )        { SOURCES *= rawenc.c }
contains( CONFIG_RL2_DECODER, yes )             { SOURCES *= rl2.c }
contains( CONFIG_ROQ_DECODER, yes )             { SOURCES *= roqvideodec.c roqvideo.c }
contains( CONFIG_ROQ_ENCODER, yes )             { SOURCES *= roqvideoenc.c roqvideo.c elbg.c }
contains( CONFIG_ROQ_DPCM_DECODER, yes )        { SOURCES *= dpcm.c }
contains( CONFIG_ROQ_DPCM_ENCODER, yes )        { SOURCES *= roqaudioenc.c }
contains( CONFIG_RPZA_DECODER, yes )            { SOURCES *= rpza.c }
contains( CONFIG_RV10_DECODER, yes )            { SOURCES *= rv10.c }
contains( CONFIG_RV10_ENCODER, yes )            { SOURCES *= rv10enc.c }
contains( CONFIG_RV20_DECODER, yes )            { SOURCES *= rv10.c }
contains( CONFIG_RV20_ENCODER, yes )            { SOURCES *= rv20enc.c }
contains( CONFIG_RV30_DECODER, yes )            { SOURCES *= rv30.c rv34.c h264pred.c rv30dsp.c  mpegvideo.c error_resilience.c }
contains( CONFIG_RV40_DECODER, yes )            { SOURCES *= rv40.c rv34.c h264pred.c rv40dsp.c  mpegvideo.c error_resilience.c }
contains( CONFIG_SGI_DECODER, yes )             { SOURCES *= sgidec.c }
contains( CONFIG_SGI_ENCODER, yes )             { SOURCES *= sgienc.c rle.c }
contains( CONFIG_SIPR_DECODER, yes )            { SOURCES *= sipr.c acelp_pitch_delay.c celp_math.c acelp_vectors.c acelp_filters.c celp_filters.c lsp.c sipr16k.c }
contains( CONFIG_SHORTEN_DECODER, yes )         { SOURCES *= shorten.c }
contains( CONFIG_SMACKAUD_DECODER, yes )        { SOURCES *= smacker.c }
contains( CONFIG_SMACKER_DECODER, yes )         { SOURCES *= smacker.c }
contains( CONFIG_SMC_DECODER, yes )             { SOURCES *= smc.c }
contains( CONFIG_SNOW_DECODER, yes )            { SOURCES *= snow.c rangecoder.c }
contains( CONFIG_SNOW_ENCODER, yes )            { SOURCES *= snow.c rangecoder.c motion_est.c mpegvideo.c error_resilience.c ituh263enc.c mpegvideo_enc.c }
contains( CONFIG_SOL_DPCM_DECODER, yes )        { SOURCES *= dpcm.c }
contains( CONFIG_SONIC_DECODER, yes )           { SOURCES *= sonic.c }
contains( CONFIG_SONIC_ENCODER, yes )           { SOURCES *= sonic.c }
contains( CONFIG_SONIC_LS_ENCODER, yes )        { SOURCES *= sonic.c }
contains( CONFIG_SP5X_DECODER, yes )            { SOURCES *= sp5xdec.c mjpegdec.c mjpeg.c }
contains( CONFIG_SUNRAST_DECODER, yes )         { SOURCES *= sunrast.c }
contains( CONFIG_SVQ1_DECODER, yes )            { SOURCES *= svq1dec.c svq1.c h263.c mpegvideo.c error_resilience.c }
contains( CONFIG_SVQ1_ENCODER, yes )            { SOURCES *= svq1enc.c svq1.c motion_est.c h263.c mpegvideo.c error_resilience.c }
contains( CONFIG_SVQ3_DECODER, yes )            { SOURCES *= h264.c svq3.c h264idct.c h264pred.c h264_loopfilter.c h264_direct.c h264_sei.c h264_ps.c h264_refs.c h264_cavlc.c h264_cabac.c cabac.c mpegvideo.c error_resilience.c svq1dec.c svq1.c h263.c }
contains( CONFIG_TARGA_DECODER, yes )           { SOURCES *= targa.c }
contains( CONFIG_TARGA_ENCODER, yes )           { SOURCES *= targaenc.c rle.c }
contains( CONFIG_THEORA_DECODER, yes )          { SOURCES *= xiph.c }
contains( CONFIG_THP_DECODER, yes )             { SOURCES *= mjpegdec.c mjpeg.c }
contains( CONFIG_TIERTEXSEQVIDEO_DECODER, yes ) { SOURCES *= tiertexseqv.c }
contains( CONFIG_TIFF_DECODER, yes )            { SOURCES *= tiff.c lzw.c faxcompr.c }
contains( CONFIG_TIFF_ENCODER, yes )            { SOURCES *= tiffenc.c rle.c lzwenc.c }
contains( CONFIG_TMV_DECODER, yes )             { SOURCES *= tmv.c cga_data.c }
contains( CONFIG_TRUEMOTION1_DECODER, yes )     { SOURCES *= truemotion1.c }
contains( CONFIG_TRUEMOTION2_DECODER, yes )     { SOURCES *= truemotion2.c }
contains( CONFIG_TRUESPEECH_DECODER, yes )      { SOURCES *= truespeech.c }
contains( CONFIG_TSCC_DECODER, yes )            { SOURCES *= tscc.c msrledec.c }
contains( CONFIG_TTA_DECODER, yes )             { SOURCES *= tta.c }
contains( CONFIG_TWINVQ_DECODER, yes )          { SOURCES *= twinvq.c }
contains( CONFIG_TXD_DECODER, yes )             { SOURCES *= txd.c s3tc.c }
contains( CONFIG_V210X_DECODER, yes )           { SOURCES *= v210x.c }
contains( CONFIG_ULTI_DECODER, yes )            { SOURCES *= ulti.c }
contains( CONFIG_V210_DECODER, yes )            { SOURCES *= v210dec.c }
contains( CONFIG_V210_ENCODER, yes )            { SOURCES *= v210enc.c }
contains( CONFIG_VB_DECODER, yes )              { SOURCES *= vb.c }
contains( CONFIG_VC1_DECODER, yes )             { SOURCES *= vc1dec.c vc1.c vc1data.c vc1dsp.c msmpeg4.c msmpeg4data.c intrax8.c intrax8dsp.c }
contains( CONFIG_VC1_DXVA2_HWACCEL, yes )       { SOURCES *= dxva2_vc1.c }
contains( CONFIG_VC1_VAAPI_HWACCEL, yes )       { SOURCES *= vaapi_vc1.c }
contains( CONFIG_VCR1_DECODER, yes )            { SOURCES *= vcr1.c }
contains( CONFIG_VCR1_ENCODER, yes )            { SOURCES *= vcr1.c }
contains( CONFIG_VMDAUDIO_DECODER, yes )        { SOURCES *= vmdav.c }
contains( CONFIG_VMDVIDEO_DECODER, yes )        { SOURCES *= vmdav.c }
contains( CONFIG_VMNC_DECODER, yes )            { SOURCES *= vmnc.c }
contains( CONFIG_VORBIS_DECODER, yes )          { SOURCES *= vorbis_dec.c vorbis.c vorbis_data.c xiph.c }
contains( CONFIG_VORBIS_ENCODER, yes )          { SOURCES *= vorbis_enc.c vorbis.c vorbis_data.c }
contains( CONFIG_VP3_DECODER, yes )             { SOURCES *= vp3.c vp3dsp.c }
contains( CONFIG_VP5_DECODER, yes )             { SOURCES *= vp5.c vp56.c vp56data.c vp3dsp.c }
contains( CONFIG_VP6_DECODER, yes )             { SOURCES *= vp6.c vp56.c vp56data.c vp3dsp.c vp6dsp.c huffman.c }
contains( CONFIG_VQA_DECODER, yes )             { SOURCES *= vqavideo.c }
contains( CONFIG_WAVPACK_DECODER, yes )         { SOURCES *= wavpack.c }
contains( CONFIG_WMAPRO_DECODER, yes )          { SOURCES *= wmaprodec.c wma.c }
contains( CONFIG_WMAV1_DECODER, yes )           { SOURCES *= wmadec.c wma.c }
contains( CONFIG_WMAV1_ENCODER, yes )           { SOURCES *= wmaenc.c wma.c }
contains( CONFIG_WMAV2_DECODER, yes )           { SOURCES *= wmadec.c wma.c }
contains( CONFIG_WMAV2_ENCODER, yes )           { SOURCES *= wmaenc.c wma.c }
contains( CONFIG_WMV2_DECODER, yes )            { SOURCES *= wmv2dec.c wmv2.c msmpeg4.c msmpeg4data.c intrax8.c intrax8dsp.c }
contains( CONFIG_WMV2_ENCODER, yes )            { SOURCES *= wmv2enc.c wmv2.c msmpeg4.c msmpeg4data.c }
contains( CONFIG_WNV1_DECODER, yes )            { SOURCES *= wnv1.c }
contains( CONFIG_WS_SND1_DECODER, yes )         { SOURCES *= ws-snd1.c }
contains( CONFIG_XAN_DPCM_DECODER, yes )        { SOURCES *= dpcm.c }
contains( CONFIG_XAN_WC3_DECODER, yes )         { SOURCES *= xan.c }
contains( CONFIG_XAN_WC4_DECODER, yes )         { SOURCES *= xan.c }
contains( CONFIG_XL_DECODER, yes )              { SOURCES *= xl.c }
contains( CONFIG_XSUB_DECODER, yes )            { SOURCES *= xsubdec.c }
contains( CONFIG_XSUB_ENCODER, yes )            { SOURCES *= xsubenc.c }
contains( CONFIG_ZLIB_DECODER, yes )            { SOURCES *= lcldec.c }
contains( CONFIG_ZLIB_ENCODER, yes )            { SOURCES *= lclenc.c }
contains( CONFIG_ZMBV_DECODER, yes )            { SOURCES *= zmbv.c }
contains( CONFIG_ZMBV_ENCODER, yes )            { SOURCES *= zmbvenc.c }

# (AD)PCM decoders/encoders
contains( CONFIG_PCM_ALAW_DECODER, yes )           { SOURCES *= pcm.c }
contains( CONFIG_PCM_ALAW_ENCODER, yes )           { SOURCES *= pcm.c }
contains( CONFIG_PCM_BLURAY_DECODER, yes )         { SOURCES *= pcm-mpeg.c }
contains( CONFIG_PCM_DVD_DECODER, yes )            { SOURCES *= pcm.c }
contains( CONFIG_PCM_DVD_ENCODER, yes )            { SOURCES *= pcm.c }
contains( CONFIG_PCM_F32BE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_F32BE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_F32LE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_F32LE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_F64BE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_F64BE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_F64LE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_F64LE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_MULAW_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_MULAW_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S8_DECODER, yes )             { SOURCES *= pcm.c }
contains( CONFIG_PCM_S8_ENCODER, yes )             { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16BE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16BE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16LE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16LE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16LE_PLANAR_DECODER, yes )   { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24BE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24BE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24DAUD_DECODER, yes )        { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24DAUD_ENCODER, yes )        { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24LE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24LE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S32BE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S32BE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S32LE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S32LE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U8_DECODER, yes )             { SOURCES *= pcm.c }
contains( CONFIG_PCM_U8_ENCODER, yes )             { SOURCES *= pcm.c }
contains( CONFIG_PCM_U16BE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U16BE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U16LE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U16LE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U24BE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U24BE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U24LE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U24LE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U32BE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U32BE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U32LE_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U32LE_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_ZORK_DECODER, yes )           { SOURCES *= pcm.c }
contains( CONFIG_PCM_ZORK_ENCODER, yes )           { SOURCES *= pcm.c }

contains( CONFIG_ADPCM_4XM_DECODER, yes )          { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_ADX_DECODER, yes )          { SOURCES *= adxdec.c }
contains( CONFIG_ADPCM_ADX_ENCODER, yes )          { SOURCES *= adxenc.c }
contains( CONFIG_ADPCM_CT_DECODER, yes )           { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_DECODER, yes )           { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_MAXIS_XA_DECODER, yes )  { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_R1_DECODER, yes )        { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_R2_DECODER, yes )        { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_R3_DECODER, yes )        { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_XAS_DECODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_G726_DECODER, yes )         { SOURCES *= g726.c }
contains( CONFIG_ADPCM_G726_ENCODER, yes )         { SOURCES *= g726.c }
contains( CONFIG_ADPCM_IMA_AMV_DECODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_DK3_DECODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_DK4_DECODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_EA_EACS_DECODER, yes )  { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_EA_SEAD_DECODER, yes )  { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_ISS_DECODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_QT_DECODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_QT_ENCODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_SMJPEG_DECODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_WAV_DECODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_WAV_ENCODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_WS_DECODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_MS_DECODER, yes )           { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_MS_ENCODER, yes )           { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SBPRO_2_DECODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SBPRO_3_DECODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SBPRO_4_DECODER, yes )      { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SWF_DECODER, yes )          { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SWF_ENCODER, yes )          { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_THP_DECODER, yes )          { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_XA_DECODER, yes )           { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_YAMAHA_DECODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_YAMAHA_ENCODER, yes )       { SOURCES *= adpcm.c }

# libavformat dependencies
contains( CONFIG_ADTS_MUXER, yes )              { SOURCES *= mpeg4audio.c }
contains( CONFIG_DV_DEMUXER, yes )              { SOURCES *= dvdata.c }
contains( CONFIG_DV_MUXER, yes )                { SOURCES *= dvdata.c }
contains( CONFIG_FLAC_DEMUXER, yes )            { SOURCES *= flacdec.c flacdata.c flac.c }
contains( CONFIG_FLAC_MUXER, yes )              { SOURCES *= flacdec.c flacdata.c flac.c }
contains( CONFIG_FLV_MUXER, yes )              { SOURCES *= mpeg4audio.c }
contains( CONFIG_GXF_DEMUXER, yes )             { SOURCES *= mpeg12data.c }
contains( CONFIG_MATROSKA_AUDIO_MUXER, yes )    { SOURCES *= xiph.c mpeg4audio.c flacdec.c flacdata.c flac.c }
contains( CONFIG_MATROSKA_DEMUXER, yes )        { SOURCES *= mpeg4audio.c }
contains( CONFIG_MATROSKA_MUXER, yes )          { SOURCES *= xiph.c mpeg4audio.c flacdec.c flacdata.c flac.c }
contains( CONFIG_MOV_DEMUXER, yes )             { SOURCES *= mpeg4audio.c mpegaudiodata.c }
contains( CONFIG_MPEGTS_MUXER, yes )            { SOURCES *= mpegvideo.c }
contains( CONFIG_NUT_MUXER, yes )               { SOURCES *= mpegaudiodata.c }
contains( CONFIG_OGG_DEMUXER, yes )             { SOURCES *= flacdec.c flacdata.c flac.c dirac.c mpeg12data.c }
contains( CONFIG_OGG_MUXER, yes )               { SOURCES *= xiph.c flacdec.c flacdata.c flac.c }
contains( CONFIG_RTP_MUXER, yes )               { SOURCES *= mpegvideo.c }

# external codec libraries
contains( CONFIG_LIBDIRAC_DECODER, yes )        { SOURCES *= libdiracdec.c }
contains( CONFIG_LIBDIRAC_ENCODER, yes )        { SOURCES *= libdiracenc.c libdirac_libschro.c }
contains( CONFIG_LIBFAAC_ENCODER, yes )         { SOURCES *= libfaac.c }
contains( CONFIG_LIBFAAD_DECODER, yes )         { SOURCES *= libfaad.c }
contains( CONFIG_LIBFAAD_LATM_DECODER, yes )    { SOURCES *= latmaac.c }
contains( CONFIG_LIBGSM_DECODER, yes )          { SOURCES *= libgsm.c }
contains( CONFIG_LIBGSM_ENCODER, yes )          { SOURCES *= libgsm.c }
contains( CONFIG_LIBGSM_MS_DECODER, yes )       { SOURCES *= libgsm.c }
contains( CONFIG_LIBGSM_MS_ENCODER, yes )       { SOURCES *= libgsm.c }
contains( CONFIG_LIBMP3LAME_ENCODER, yes )      { SOURCES *= libmp3lame.c }
contains( CONFIG_LIBOPENCORE_AMRNB_DECODER, yes ) { SOURCES *= libopencore-amr.c }
contains( CONFIG_LIBOPENCORE_AMRNB_ENCODER, yes ) { SOURCES *= libopencore-amr.c }
contains( CONFIG_LIBOPENCORE_AMRWB_ENCODER, yes ) { SOURCES *= libopencore-amr.c }
contains( CONFIG_LIBOPENJPEG_DECODER, yes )     { SOURCES *= libopenjpeg.c }
contains( CONFIG_LIBSCHROEDINGER_DECODER, yes ) { SOURCES *= libschroedingerdec.c libschroedinger.c libdirac_libschro.c }
contains( CONFIG_LIBSCHROEDINGER_ENCODER, yes ) { SOURCES *= libschroedingerenc.c libschroedinger.c libdirac_libschro.c }
contains( CONFIG_LIBSPEEX_DECODER, yes )        { SOURCES *= libspeexdec.c }
contains( CONFIG_LIBTHEORA_ENCODER, yes )       { SOURCES *= libtheoraenc.c }
contains( CONFIG_LIBVORBIS_ENCODER, yes )       { SOURCES *= libvorbis.c }
contains( CONFIG_LIBX264_ENCODER, yes )         { SOURCES *= libx264.c }
contains( CONFIG_LIBXVID_ENCODER, yes )         { SOURCES *= libxvidff.c libxvid_rc.c }

# parsers
contains( CONFIG_AAC_PARSER, yes )              { SOURCES *= aac_parser.c aac_ac3_parser.c mpeg4audio.c }
contains( CONFIG_AC3_PARSER, yes )              { SOURCES *= ac3_parser.c ac3tab.c aac_ac3_parser.c }
contains( CONFIG_LATM_PARSER, yes )             { SOURCES *= latm_parser.c }
contains( CONFIG_CAVSVIDEO_PARSER, yes )        { SOURCES *= cavs_parser.c }
contains( CONFIG_DCA_PARSER, yes )              { SOURCES *= dca_parser.c }
contains( CONFIG_DIRAC_PARSER, yes )            { SOURCES *= dirac_parser.c }
contains( CONFIG_DNXHD_PARSER, yes )            { SOURCES *= dnxhd_parser.c }
contains( CONFIG_DVBSUB_PARSER, yes )           { SOURCES *= dvbsub_parser.c }
contains( CONFIG_DVDSUB_PARSER, yes )           { SOURCES *= dvdsub_parser.c }
contains( CONFIG_H261_PARSER, yes )             { SOURCES *= h261_parser.c }
contains( CONFIG_H263_PARSER, yes )             { SOURCES *= h263_parser.c }
contains( CONFIG_H264_PARSER, yes )             { SOURCES *= h264_parser.c h264.c h264idct.c h264pred.c cabac.c mpegvideo.c error_resilience.c }
contains( CONFIG_MJPEG_PARSER, yes )            { SOURCES *= mjpeg_parser.c }
contains( CONFIG_MLP_PARSER, yes )              { SOURCES *= mlp_parser.c mlp.c }
contains( CONFIG_MPEG4VIDEO_PARSER, yes )       { SOURCES *= mpeg4video_parser.c mpegvideo.c error_resilience.c }
contains( CONFIG_MPEGAUDIO_PARSER, yes )        { SOURCES *= mpegaudio_parser.c mpegaudiodecheader.c }
contains( CONFIG_MPEGVIDEO_PARSER, yes )        { SOURCES *= mpegvideo_parser.c mpeg12.c mpeg12data.c mpegvideo.c error_resilience.c }
contains( CONFIG_PNM_PARSER, yes )              { SOURCES *= pnm_parser.c pnm.c }
contains( CONFIG_VC1_PARSER, yes )              { SOURCES *= vc1_parser.c vc1.c vc1data.c msmpeg4.c msmpeg4data.c h263.c error_resilience.c mpegvideo.c }
contains( CONFIG_VP3_PARSER, yes )              { SOURCES *= vp3_parser.c }

# bitstream filters
contains( CONFIG_AAC_ADTSTOASC_BSF, yes )          { SOURCES *= aac_adtstoasc_bsf.c }
contains( CONFIG_DUMP_EXTRADATA_BSF, yes )         { SOURCES *= dump_extradata_bsf.c }
contains( CONFIG_H264_MP4TOANNEXB_BSF, yes )       { SOURCES *= h264_mp4toannexb_bsf.c }
contains( CONFIG_IMX_DUMP_HEADER_BSF, yes )        { SOURCES *= imx_dump_header_bsf.c }
contains( CONFIG_MJPEGA_DUMP_HEADER_BSF, yes )     { SOURCES *= mjpega_dump_header_bsf.c }
contains( CONFIG_MOV2TEXTSUB_BSF, yes )            { SOURCES *= movsub_bsf.c }
contains( CONFIG_MP3_HEADER_COMPRESS_BSF, yes )    { SOURCES *= mp3_header_compress_bsf.c }
contains( CONFIG_MP3_HEADER_DECOMPRESS_BSF, yes )  { SOURCES *= mp3_header_decompress_bsf.c mpegaudiodata.c }
contains( CONFIG_NOISE_BSF, yes )                  { SOURCES *= noise_bsf.c }
contains( CONFIG_REMOVE_EXTRADATA_BSF, yes )       { SOURCES *= remove_extradata_bsf.c }
contains( CONFIG_TEXT2MOVSUB_BSF, yes )            { SOURCES *= movsub_bsf.c }

# thread libraries
contains( HAVE_PTHREADS, yes )                  { SOURCES *= pthread.c }
contains( HAVE_W32THREADS, yes )                { SOURCES *= w32thread.c }
contains( HAVE_OS2THREADS, yes )                { SOURCES *= os2thread.c }
contains( HAVE_BEOSTHREADS, yes )               { SOURCES *= beosthread.c }

using_xvmc {
    LIBS    += $$CONFIG_XVMC_LIBS
}

contains( HAVE_GPROF, yes ) {
    QMAKE_CFLAGS_RELEASE += -p
    QMAKE_LFLAGS_RELEASE += -p
}

contains( ARCH_X86, yes ) {
    contains( CONFIG_MLP_DECODER, yes )    { SOURCES *= x86/mlpdsp_inc.c }
    contains( CONFIG_TRUEHD_DECODER, yes ) { SOURCES *= x86/mlpdsp_inc.c }
}

contains( HAVE_MMX, yes ) {

    contains( CONFIG_CAVS_DECODER, yes )   { SOURCES *= x86/cavsdsp_mmx.c }
    contains( CONFIG_ENCODERS, yes )       { SOURCES *= x86/dsputilenc_mmx.c }
    contains( CONFIG_GPL, yes )            { SOURCES *= x86/idct_mmx.c }
    contains( CONFIG_LPC, yes )            { SOURCES *= x86/lpc_mmx.c }
    contains( CONFIG_SNOW_DECODER, yes )   { SOURCES *= x86/snowdsp_mmx.c }
    contains( CONFIG_VC1_DECODER, yes )    { SOURCES *= x86/vc1dsp_mmx.c }
    contains( CONFIG_VP3_DECODER, yes )    { SOURCES *= x86/vp3dsp_mmx.c x86/vp3dsp_sse2.c }
    contains( CONFIG_VP5_DECODER, yes )    { SOURCES *= x86/vp3dsp_mmx.c x86/vp3dsp_sse2.c }
    contains( CONFIG_VP6_DECODER, yes )    { SOURCES *= x86/vp3dsp_mmx.c x86/vp3dsp_sse2.c x86/vp6dsp_mmx.c x86/vp6dsp_sse2.c }

    contains( HAVE_YASM, yes ) {
        contains( CONFIG_FFT, yes ) {
            contains( HAVE_AMD3DNOW, yes )  { SOURCES += x86/fft_3dn.c }
            contains( HAVE_AMD3DNOWEXT, yes ) { SOURCES += x86/fft_3dn2_inc.c }
            contains( HAVE_SSE, yes )       { SOURCES += x86/fft_sse.c }
            YASM_SOURCES  += x86/fft_mmx.asm
        }

        YASM_SOURCES += x86/dsputil_yasm.asm

        contains( CONFIG_GPL, yes ) {
            YASM_SOURCES += x86/h264_deblock_sse2.asm
            YASM_SOURCES += x86/h264_idct_sse2.asm
        }

        yasm.output   = ${QMAKE_FILE_BASE}.o
        yasm.commands = $$YASM $$YASMFLAGS -I x86/ -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
        yasm.depends  = $$system($$YASM $$YASMFLAGS -I x86/ -M ${QMAKE_FILE_NAME} | sed \"s,^.*: ,,\")
        yasm.input    = YASM_SOURCES
        QMAKE_EXTRA_UNIX_COMPILERS += yasm
    }

    SOURCES += x86/cpuid.c
    SOURCES += x86/dnxhd_mmx.c
    SOURCES += x86/dsputil_mmx.c
    SOURCES += x86/fdct_mmx.c
    SOURCES += x86/fft_x86.c
    SOURCES += x86/idct_mmx_xvid.c
    SOURCES += x86/idct_sse2_xvid.c
    SOURCES += x86/motion_est_mmx.c
    SOURCES += x86/mpegvideo_mmx.c
    SOURCES += x86/simple_idct_mmx.c
}


contains( ARCH_ALPHA, yes ) {
    SOURCES += alpha/dsputil_alpha.c
    SOURCES += alpha/dsputil_alpha_asm.S
    SOURCES += alpha/motion_est_alpha.c
    SOURCES += alpha/motion_est_mvi_asm.S
    SOURCES += alpha/mpegvideo_alpha.c
    SOURCES += alpha/simple_idct_alpha.c
    QMAKE_CFLAGS_RELEASE += -fforce-addr -freduce-all-givs
}

contains( ARCH_ARM, yes ) {
    SOURCES += arm/dsputil_arm.S
    SOURCES += arm/dsputil_init_arm.c
    SOURCES += arm/fft_init_arm.c
    SOURCES += arm/h264pred_init_arm.c
    SOURCES += arm/jrevdct_arm.S
    SOURCES += arm/mpegvideo_arm.c
    SOURCES += arm/simple_idct_arm.S
}

contains( HAVE_ARMV5TE, yes ) {
    SOURCES += arm/dsputil_init_armv5te.c
    SOURCES += arm/mpegvideo_armv5te_s.S
    SOURCES += arm/mpegvideo_armv5te.c
    SOURCES += arm/simple_idct_armv5te.S
}

contains( HAVE_ARMV6, yes ) {
    SOURCES += arm/dsputil_init_armv6.c
    SOURCES += arm/dsputil_armv6.S
    SOURCES += arm/simple_idct_armv6.S
 }

contains( HAVE_ARMVFP, yes ) {
    SOURCES += arm/dsputil_init_vfp.c
    SOURCES += arm/dsputil_vfp.S
}

contains( HAVE_IWMMXT, yes ) {
    SOURCES += arm/dsputil_iwmmxt.c
    SOURCES += arm/mpegvideo_iwmmxt.c
}

contains( HAVE_NEON, yes ) {
    SOURCES += arm/dsputil_init_neon.c
    SOURCES += arm/dsputil_neon.S
    SOURCES += arm/simple_idct_neon.S

    contains( CONFIG_FFT, yes )            { SOURCES *= arm/fft_neon.S  }
    contains( CONFIG_MDCT, yes )           { SOURCES *= arm/mdct_neon.S }

    contains( CONFIG_H264_DECODER, yes )   { SOURCES *= arm/h264dsp_neon.S arm/h264idct_neon.S arm/h264pred_neon.S }
    contains( CONFIG_VP3_DECODER, yes )    { SOURCES *= arm/vp3dsp_neon.S }
}

contains( ARCH_BFIN, yes) {
    SOURCES += bfin/dsputil_bfin.c
    SOURCES += bfin/fdct_bfin.S
    SOURCES += bfin/idct_bfin.S
    SOURCES += bfin/mpegvideo_bfin.c
    SOURCES += bfin/pixels_bfin.S
    SOURCES += bfin/vp3_bfin.c
    SOURCES += bfin/vp3_idct_bfin.S
}

contains( ARCH_PPC, yes ) {
    SOURCES += ppc/dsputil_ppc.c
}

contains( HAVE_ALTIVEC, yes ) {
    SOURCES += ppc/check_altivec.c
    SOURCES += ppc/dsputil_altivec.c
    SOURCES += ppc/fdct_altivec.c
    SOURCES += ppc/fft_altivec.c
    SOURCES += ppc/float_altivec.c
    SOURCES += ppc/gmc_altivec.c
    SOURCES += ppc/idct_altivec.c
    SOURCES += ppc/int_altivec.c
    SOURCES += ppc/mpegvideo_altivec.c

    contains( CONFIG_H264_DECODER, yes ) { SOURCES += ppc/h264_altivec.c }
    contains( CONFIG_VC1_DECODER, yes )  { SOURCES *= ppc/vc1dsp_altivec.c }
    contains( CONFIG_VP3_DECODER, yes )  { SOURCES *= ppc/vp3dsp_altivec.c }
    contains( CONFIG_VP5_DECODER, yes )  { SOURCES *= ppc/vp3dsp_altivec.c }
    contains( CONFIG_VP6_DECODER, yes )  { SOURCES *= ppc/vp3dsp_altivec.c }
}

contains( ARCH_SH4, yes) {
    SOURCES += sh4/dsputil_align.c
    SOURCES += sh4/dsputil_sh4.c
    SOURCES += sh4/idct_sh4.c
}

contains( CONFIG_MLIB, yes ) {
    SOURCES += mlib/dsputil_mlib.c
    QMAKE_CFLAGS_RELEASE += $$MLIB_INC
}

contains( HAVE_MMI, yes) {
    SOURCES += ps2/dsputil_mmi.c
    SOURCES += ps2/idct_mmi.c
    SOURCES += ps2/mpegvideo_mmi.c
}

contains( HAVE_VIS, yes ) {
    SOURCES += sparc/dsputil_vis.c
    SOURCES += sparc/simple_idct_vis.c
}


macx {
    QMAKE_LFLAGS_SHLIB += -read_only_relocs warning
}

# TODO: generate hardcoded tables


include ( ../libs-targetfix.pro )
