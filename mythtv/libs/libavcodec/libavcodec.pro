include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavcodec-$$LIBVERSION
CONFIG += thread dll warn_off
target.path = $${LIBDIR}
INSTALLS = target

INCLUDEPATH = ./ ../ ../../ ../libavutil ../libswscale

# remove MMX define since it clashes with a libmp3lame enum
DEFINES -= MMX
DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE

# Debug mode on x86 must compile without -fPIC and with -O, 
# otherwise gcc runs out of registers.
# libavcodec rev 7671 needs another register
# -fomit-frame-pointer frees it. gcc-4 enables "location lists"
# which allows debugging without frame pointer
debug:contains(ARCH_X86_32, yes) {
        QMAKE_CFLAGS_SHLIB = 
        QMAKE_CFLAGS_DEBUG += -fomit-frame-pointer
}
# "-Os" can not compiled with PIC 
contains(CONFIG_SMALL, yes):contains(ARCH_X86_32, yes) {
	QMAKE_CFLAGS_SHLIB =
}

!profile:QMAKE_CFLAGS_DEBUG += -O

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
SOURCES += bitstream.c utils.c allcodecs.c mpegvideo.c jrevdct.c jfdctfst.c
SOURCES += jfdctint.c resample.c resample2.c dsputil.c motion_est.c
SOURCES += imgconvert.c mpeg12.c mpeg12data.c simple_idct.c ratecontrol.c
SOURCES += eval.c error_resilience.c raw.c faandct.c parser.c rangecoder.c
SOURCES += h263.c opt.c bitstream_filter.c audioconvert.c
SOURCES += myth_utils.c

inc.path = $${PREFIX}/include/mythtv/ffmpeg/
inc.files = avcodec.h i386/mmx.h opt.h

INSTALLS += inc

LIBS += $$LOCAL_LIBDIR_X11
LIBS += -L../libavutil -lmythavutil-$$LIBVERSION $$EXTRALIBS
LIBS += -lz

contains( CONFIG_AASC_DECODER, yes )            { SOURCES *= aasc.c }
contains( CONFIG_AC3_DECODER, yes )             { SOURCES *= ac3dec.c ac3tab.c ac3.c }
contains( CONFIG_AC3_ENCODER, yes )             { SOURCES *= ac3enc.c ac3tab.c ac3.c }
contains( CONFIG_ALAC_DECODER, yes )            { SOURCES *= alac.c }
contains( CONFIG_AMV_DECODER, yes )             { SOURCES *= sp5xdec.c mjpegdec.c mjpeg.c }
contains( CONFIG_APE_DECODER, yes )             { SOURCES *= apedec.c }
contains( CONFIG_ASV1_DECODER, yes )            { SOURCES *= asv1.c }
contains( CONFIG_ASV1_ENCODER, yes )            { SOURCES *= asv1.c }
contains( CONFIG_ASV2_DECODER, yes )            { SOURCES *= asv1.c }
contains( CONFIG_ASV2_ENCODER, yes )            { SOURCES *= asv1.c }
contains( CONFIG_ATRAC3_DECODER, yes )          { SOURCES *= atrac3.c mdct.c fft.c }
contains( CONFIG_AVS_DECODER, yes )             { SOURCES *= avs.c }
contains( CONFIG_BETHSOFTVID_DECODER, yes )     { SOURCES *= bethsoftvideo.c }
contains( CONFIG_BMP_DECODER, yes )             { SOURCES *= bmp.c }
contains( CONFIG_BMP_ENCODER, yes )             { SOURCES *= bmpenc.c }
contains( CONFIG_C93_DECODER, yes )             { SOURCES *= c93.c }
contains( CONFIG_CAVS_DECODER, yes )            { SOURCES *= cavs.c cavsdec.c cavsdsp.c golomb.c }
contains( CONFIG_CINEPAK_DECODER, yes )         { SOURCES *= cinepak.c }
contains( CONFIG_CLJR_DECODER, yes )            { SOURCES *= cljr.c }
contains( CONFIG_CLJR_ENCODER, yes )            { SOURCES *= cljr.c }
contains( CONFIG_COOK_DECODER, yes )            { SOURCES *= cook.c mdct.c fft.c }
contains( CONFIG_CSCD_DECODER, yes )            { SOURCES *= cscd.c }
contains( CONFIG_CYUV_DECODER, yes )            { SOURCES *= cyuv.c }
contains( CONFIG_DCA_DECODER, yes )             { SOURCES *= dca.c }
contains( CONFIG_DNXHD_DECODER, yes )           { SOURCES *= dnxhddec.c dnxhddata.c }
contains( CONFIG_DNXHD_ENCODER, yes )           { SOURCES *= dnxhdenc.c dnxhddata.c mpegvideo_enc.c motion_est.c ratecontrol.c }
contains( CONFIG_DSICINVIDEO_DECODER, yes )     { SOURCES *= dsicinav.c }
contains( CONFIG_DSICINAUDIO_DECODER, yes )     { SOURCES *= dsicinav.c }
contains( CONFIG_DVBSUB_DECODER, yes )          { SOURCES *= dvbsubdec.c }
contains( CONFIG_DVBSUB_ENCODER, yes )          { SOURCES *= dvbsub.c }
contains( CONFIG_DVDSUB_DECODER, yes )          { SOURCES *= dvdsubdec.c }
contains( CONFIG_DVDSUB_ENCODER, yes )          { SOURCES *= dvdsubenc.c }
contains( CONFIG_DVVIDEO_DECODER, yes )         { SOURCES *= dv.c }
contains( CONFIG_DVVIDEO_ENCODER, yes )         { SOURCES *= dv.c }
contains( CONFIG_DXA_DECODER, yes )             { SOURCES *= dxa.c }
contains( CONFIG_EIGHTBPS_DECODER, yes )        { SOURCES *= 8bps.c }
contains( CONFIG_FFV1_DECODER, yes )            { SOURCES *= ffv1.c golomb.c }
contains( CONFIG_FFV1_ENCODER, yes )            { SOURCES *= ffv1.c }
contains( CONFIG_FFVHUFF_DECODER, yes )         { SOURCES *= huffyuv.c }
contains( CONFIG_FFVHUFF_ENCODER, yes )         { SOURCES *= huffyuv.c }
contains( CONFIG_FLAC_DECODER, yes )            { SOURCES *= flac.c golomb.c }
contains( CONFIG_FLAC_ENCODER, yes )            { SOURCES *= flacenc.c golomb.c }
contains( CONFIG_FLASHSV_DECODER, yes )         { SOURCES *= flashsv.c }
contains( CONFIG_FLASHSV_ENCODER, yes )         { SOURCES *= flashsvenc.c }
contains( CONFIG_FLIC_DECODER, yes )            { SOURCES *= flicvideo.c }
contains( CONFIG_FLV_DECODER, yes )             { SOURCES *= h263dec.c h263.c }
contains( CONFIG_FLV_ENCODER, yes )             { SOURCES *= mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_FOURXM_DECODER, yes )          { SOURCES *= 4xm.c }
contains( CONFIG_FRAPS_DECODER, yes )           { SOURCES *= fraps.c huffman.c }
contains( CONFIG_GIF_DECODER, yes )             { SOURCES *= gifdec.c lzw.c }
contains( CONFIG_GIF_ENCODER, yes )             { SOURCES *= gif.c }
contains( CONFIG_H261_DECODER, yes )            { SOURCES *= h261dec.c h261.c }
contains( CONFIG_H261_ENCODER, yes )            { SOURCES *= h261enc.c h261.c mpegvideo_enc.c motion_est.c ratecontrol.c }
contains( CONFIG_H263_DECODER, yes )            { SOURCES *= h263dec.c h263.c }
contains( CONFIG_H263I_DECODER, yes )           { SOURCES *= h263dec.c h263.c }
contains( CONFIG_H263_ENCODER, yes )            { SOURCES *= mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_H263P_ENCODER, yes )           { SOURCES *= mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_H264_DECODER, yes )            { SOURCES *= h264.c h264idct.c h264pred.c cabac.c golomb.c }
contains( CONFIG_H264_ENCODER, yes )            { SOURCES *= h264enc.c h264dspenc.c }
contains( CONFIG_HUFFYUV_DECODER, yes )         { SOURCES *= huffyuv.c }
contains( CONFIG_HUFFYUV_ENCODER, yes )         { SOURCES *= huffyuv.c }
contains( CONFIG_IDCIN_DECODER, yes )           { SOURCES *= idcinvideo.c }
contains( CONFIG_IMC_DECODER, yes )             { SOURCES *= imc.c mdct.c fft.c }
contains( CONFIG_INDEO2_DECODER, yes )          { SOURCES *= indeo2.c }
contains( CONFIG_INDEO3_DECODER, yes )          { SOURCES *= indeo3.c }
contains( CONFIG_INTERPLAY_VIDEO_DECODER, yes ) { SOURCES *= interplayvideo.c }
contains( CONFIG_INTERPLAY_DPCM_DECODER, yes )  { SOURCES *= dpcm.c }
contains( CONFIG_JPEGLS_DECODER, yes )          { SOURCES *= jpeglsdec.c jpegls.c mjpegdec.c mjpeg.c golomb.c }
contains( CONFIG_JPEGLS_ENCODER, yes )          { SOURCES *= jpeglsenc.c jpegls.c golomb.c }
contains( CONFIG_KMVC_DECODER, yes )            { SOURCES *= kmvc.c }
contains( CONFIG_LJPEG_ENCODER, yes )           { SOURCES *= ljpegenc.c mjpegenc.c mjpeg.c mpegvideo_enc.c motion_est.c ratecontrol.c }
contains( CONFIG_LOCO_DECODER, yes )            { SOURCES *= loco.c golomb.c }
contains( CONFIG_MACE3_DECODER, yes )           { SOURCES *= mace.c }
contains( CONFIG_MACE6_DECODER, yes )           { SOURCES *= mace.c }
contains( CONFIG_MJPEG_DECODER, yes )           { SOURCES *= mjpegdec.c mjpeg.c }
contains( CONFIG_MJPEG_ENCODER, yes )           { SOURCES *= mjpegenc.c mjpeg.c mpegvideo_enc.c motion_est.c ratecontrol.c }
contains( CONFIG_MJPEGB_DECODER, yes )          { SOURCES *= mjpegbdec.c mjpegdec.c mjpeg.c }
contains( CONFIG_MMVIDEO_DECODER, yes )         { SOURCES *= mmvideo.c }
contains( CONFIG_MP2_DECODER, yes )             { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MP2_ENCODER, yes )             { SOURCES *= mpegaudioenc.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MP3_DECODER, yes )             { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MP3ADU_DECODER, yes )          { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MP3ON4_DECODER, yes )          { SOURCES *= mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MPC7_DECODER, yes )            { SOURCES *= mpc7.c mpc.c mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MPC7_DECODER, yes )            { SOURCES *= mpc8.c mpc.c mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_MPEG1VIDEO_ENCODER, yes )      { SOURCES *= mpeg12enc.c mpeg12data.c mpegvideo_enc.c motion_est.c ratecontrol.c }
contains( CONFIG_MPEG2VIDEO_ENCODER, yes )      { SOURCES *= mpeg12enc.c mpeg12data.c mpegvideo_enc.c motion_est.c ratecontrol.c }
contains( CONFIG_MPEG4_DECODER, yes)            { SOURCES *= h263dec.c h263.c }
contains( CONFIG_MPEG4_ENCODER, yes)            { SOURCES *= mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_MSMPEG4V1_DECODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c h263dec.c h263.c }
contains( CONFIG_MSMPEG4V1_ENCODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_MSMPEG4V2_DECODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c h263dec.c h263.c }
contains( CONFIG_MSMPEG4V2_ENCODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_MSMPEG4V3_DECODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c h263dec.c h263.c }
contains( CONFIG_MSMPEG4V3_ENCODER, yes )       { SOURCES *= msmpeg4.c msmpeg4data.c mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_MSRLE_DECODER, yes )           { SOURCES *= msrle.c }
contains( CONFIG_MSVIDEO1_DECODER, yes )        { SOURCES *= msvideo1.c }
contains( CONFIG_MSZH_DECODER, yes )            { SOURCES *= lcldec.c }
contains( CONFIG_NELLYMOSER_DECODER, yes )      { SOURCES *= nellymoserdec.c }
contains( CONFIG_NUV_DECODER, yes )             { SOURCES *= nuv.c rtjpeg.c }
contains( CONFIG_PAM_ENCODER, yes )             { SOURCES *= pnmenc.c pnm.c }
contains( CONFIG_PBM_ENCODER, yes )             { SOURCES *= pnmenc.c pnm.c }
contains( CONFIG_PGM_ENCODER, yes )             { SOURCES *= pnmenc.c pnm.c }
contains( CONFIG_PGMYUV_ENCODER, yes )          { SOURCES *= pnmenc.c pnm.c }
contains( CONFIG_PNG_DECODER, yes )             { SOURCES *= png.c pngdec.c }
contains( CONFIG_PNG_ENCODER, yes )             { SOURCES *= png.c pngenc.c }
contains( CONFIG_PPM_ENCODER, yes )             { SOURCES *= pnmenc.c pnm.c }
contains( CONFIG_PTX_DECODER, yes )             { SOURCES *= ptx.c }
contains( CONFIG_QDM2_DECODER, yes )            { SOURCES *= qdm2.c mdct.c fft.c mpegaudiodec.c mpegaudiodecheader.c mpegaudio.c mpegaudiodata.c }
contains( CONFIG_QDRAW_DECODER, yes )           { SOURCES *= qdrw.c }
contains( CONFIG_QPEG_DECODER, yes )            { SOURCES *= qpeg.c }
contains( CONFIG_QTRLE_DECODER, yes )           { SOURCES *= qtrle.c }
contains( CONFIG_QTRLE_ENCODER, yes)            { SOURCES *= qtrleenc.c }
contains( CONFIG_RA_144_DECODER, yes )          { SOURCES *= ra144.c }
contains( CONFIG_RA_288_DECODER, yes )          { SOURCES *= ra288.c }
contains( CONFIG_RAWVIDEO_DECODER, yes )        { SOURCES *= rawdec.c }
contains( CONFIG_RAWVIDEO_ENCODER, yes )        { SOURCES *= rawenc.c }
contains( CONFIG_ROQ_DECODER, yes )             { SOURCES *= roqvideodec.c roqvideo.c }
contains( CONFIG_ROQ_ENCODER, yes )             { SOURCES *= roqvideoenc.c roqvideo.c elbg.c }
contains( CONFIG_ROQ_DPCM_DECODER, yes )        { SOURCES *= dpcm.c }
contains( CONFIG_ROQ_DPCM_ENCODER, yes )        { SOURCES *= roqaudioenc.c }
contains( CONFIG_RPZA_DECODER, yes )            { SOURCES *= rpza.c }
contains( CONFIG_RV10_DECODER, yes )            { SOURCES *= rv10.c h263.c }
contains( CONFIG_RV10_ENCODER, yes )            { SOURCES *= rv10.c mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_RV20_DECODER, yes )            { SOURCES *= rv10.c h263.c }
contains( CONFIG_RV20_ENCODER, yes )            { SOURCES *= rv10.c mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_SGI_DECODER, yes )             { SOURCES *= sgidec.c }
contains( CONFIG_SGI_ENCODER, yes )             { SOURCES *= sgienc.c rle.c }
contains( CONFIG_SHORTEN_DECODER, yes )         { SOURCES *= shorten.c golomb.c }
contains( CONFIG_SMACKAUD_DECODER, yes )        { SOURCES *= smacker.c }
contains( CONFIG_SMACKER_DECODER, yes )         { SOURCES *= smacker.c }
contains( CONFIG_SMC_DECODER, yes )             { SOURCES *= smc.c }
contains( CONFIG_SNOW_DECODER, yes )            { SOURCES *= snow.c }
contains( CONFIG_SNOW_ENCODER, yes )            { SOURCES *= snow.c }
contains( CONFIG_SOL_DPCM_DECODER, yes )        { SOURCES *= dpcm.c }
contains( CONFIG_SONIC_DECODER, yes )           { SOURCES *= sonic.c golomb.c }
contains( CONFIG_SONIC_ENCODER, yes )           { SOURCES *= sonic.c golomb.c }
contains( CONFIG_SONIC_LS_ENCODER, yes )        { SOURCES *= sonic.c golomb.c }
contains( CONFIG_SP5X_DECODER, yes )            { SOURCES *= sp5xdec.c mjpegdec.c mjpeg.c }
contains( CONFIG_SVQ1_DECODER, yes )            { SOURCES *= svq1dec.c svq1.c h263.c }
contains( CONFIG_SVQ1_ENCODER, yes )            { SOURCES *= svq1enc.c svq1.c motion_est.c h263.c }
contains( CONFIG_SVQ3_DECODER, yes )            { SOURCES *= h264.c h264idct.c h264pred.c cabac.c golomb.c }
contains( CONFIG_TARGA_DECODER, yes )           { SOURCES *= targa.c }
contains( CONFIG_TARGA_ENCODER, yes )           { SOURCES *= targaenc.c rle.c }
contains( CONFIG_THEORA_DECODER, yes )          { SOURCES *= vp3.c xiph.c vp3dsp.c }
contains( CONFIG_THP_DECODER, yes )             { SOURCES *= mjpegdec.c mjpeg.c }
contains( CONFIG_TIERTEXSEQVIDEO_DECODER, yes ) { SOURCES *= tiertexseqv.c }
contains( CONFIG_TIFF_DECODER, yes )            { SOURCES *= tiff.c lzw.c }
contains( CONFIG_TIFF_ENCODER, yes )            { SOURCES *= tiffenc.c rle.c lzwenc.c }
contains( CONFIG_TRUEMOTION1_DECODER, yes )     { SOURCES *= truemotion1.c }
contains( CONFIG_TRUEMOTION2_DECODER, yes )     { SOURCES *= truemotion2.c }
contains( CONFIG_TRUESPEECH_DECODER, yes )      { SOURCES *= truespeech.c }
contains( CONFIG_TSCC_DECODER, yes )            { SOURCES *= tscc.c }
contains( CONFIG_TTA_DECODER, yes )             { SOURCES *= tta.c }
contains( CONFIG_TXD_DECODER, yes )             { SOURCES *= txd.c s3tc.c }
contains( CONFIG_ULTI_DECODER, yes )            { SOURCES *= ulti.c }
contains( CONFIG_VB_DECODER, yes )              { SOURCES *= vb.c }
contains( CONFIG_VC1_DECODER, yes )             { SOURCES *= vc1.c vc1data.c vc1dsp.c msmpeg4data.c intrax8.c intrax8dsp.c }
contains( CONFIG_VCR1_DECODER, yes )            { SOURCES *= vcr1.c }
contains( CONFIG_VCR1_ENCODER, yes )            { SOURCES *= vcr1.c }
contains( CONFIG_VMDAUDIO_DECODER, yes )        { SOURCES *= vmdav.c }
contains( CONFIG_VMDVIDEO_DECODER, yes )        { SOURCES *= vmdav.c }
contains( CONFIG_VMNC_DECODER, yes )            { SOURCES *= vmnc.c }
contains( CONFIG_VORBIS_DECODER, yes )          { SOURCES *= vorbis_dec.c vorbis.c vorbis_data.c xiph.c mdct.c fft.c }
contains( CONFIG_VORBIS_ENCODER, yes )          { SOURCES *= vorbis_enc.c vorbis.c vorbis_data.c mdct.c fft.c }
contains( CONFIG_VP3_DECODER, yes )             { SOURCES *= vp3.c vp3dsp.c }
contains( CONFIG_VP5_DECODER, yes )             { SOURCES *= vp5.c vp56.c vp56data.c vp3dsp.c }
contains( CONFIG_VP6_DECODER, yes )             { SOURCES *= vp6.c vp56.c vp56data.c vp3dsp.c }
contains( CONFIG_VP6A_DECODER, yes )            { SOURCES *= vp6.c vp56.c vp56data.c vp3dsp.c }
contains( CONFIG_VP6F_DECODER, yes )            { SOURCES *= vp6.c vp56.c vp56data.c vp3dsp.c }
contains( CONFIG_VQA_DECODER, yes )             { SOURCES *= vqavideo.c }
contains( CONFIG_WAVPACK_DECODER, yes )         { SOURCES *= wavpack.c }
contains( CONFIG_WMAV1_DECODER, yes )           { SOURCES *= wmadec.c wma.c mdct.c fft.c }
contains( CONFIG_WMAV2_DECODER, yes )           { SOURCES *= wmadec.c wma.c mdct.c fft.c }
contains( CONFIG_WMAV1_ENCODER, yes )           { SOURCES *= wmaenc.c wma.c mdct.c fft.c }
contains( CONFIG_WMAV2_ENCODER, yes )           { SOURCES *= wmaenc.c wma.c mdct.c fft.c }
contains( CONFIG_WMV1_DECODER, yes )            { SOURCES *= h263dec.c h263.c }
contains( CONFIG_WMV1_ENCODER, yes )            { SOURCES *= mpegvideo_enc.c motion_est.c ratecontrol.c h263.c }
contains( CONFIG_WMV2_DECODER, yes )            { SOURCES *= wmv2.c msmpeg4.c msmpeg4data.c h263dec.c h263.c intrax8.c intrax8dsp.c }
contains( CONFIG_WMV2_ENCODER, yes )            { SOURCES *= wmv2.c msmpeg4.c msmpeg4data.c mpegvideo_enc.c motion_est.c ratecontrol.c h263.c intrax8.c intrax8dsp.c }
contains( CONFIG_WMV3_DECODER, yes )            { SOURCES *= vc1.c vc1data.c vc1dsp.c }
contains( CONFIG_WNV1_DECODER, yes )            { SOURCES *= wnv1.c }
contains( CONFIG_WS_SND1_DECODER, yes )         { SOURCES *= ws-snd1.c }
contains( CONFIG_XAN_DPCM_DECODER, yes )        { SOURCES *= dpcm.c }
contains( CONFIG_XAN_WC3_DECODER, yes )         { SOURCES *= xan.c }
contains( CONFIG_XAN_WC4_DECODER, yes )         { SOURCES *= xan.c }
contains( CONFIG_XL_DECODER, yes )              { SOURCES *= xl.c }
contains( CONFIG_XSUB_DECODER, yes )            { SOURCES *= xsubdec.c }
contains( CONFIG_ZLIB_DECODER, yes )            { SOURCES *= lcldec.c }
contains( CONFIG_ZLIB_ENCODER, yes )            { SOURCES *= lclenc.c }
contains( CONFIG_ZMBV_DECODER, yes )            { SOURCES *= zmbv.c }
contains( CONFIG_ZMBV_ENCODER, yes )            { SOURCES *= zmbvenc.c }

contains( CONFIG_PCM_S32LE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S32LE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S32BE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S32BE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U32LE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U32LE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U32BE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U32BE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24LE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24LE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24BE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24BE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U24LE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U24LE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U24BE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U24BE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24DAUD_DECODER, yes )     { SOURCES *= pcm.c }
contains( CONFIG_PCM_S24DAUD_ENCODER, yes )     { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16LE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16LE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16BE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S16BE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U16LE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U16LE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U16BE_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_U16BE_ENCODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_S8_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_S8_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U8_DECODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_U8_ENCODER, yes )          { SOURCES *= pcm.c }
contains( CONFIG_PCM_ALAW_DECODER, yes )        { SOURCES *= pcm.c }
contains( CONFIG_PCM_ALAW_ENCODER, yes )        { SOURCES *= pcm.c }
contains( CONFIG_PCM_MULAW_DECODER, yes )       { SOURCES *= pcm.c }
contains( CONFIG_PCM_MULAW_ENCODER, yes )       { SOURCES *= pcm.c }

contains( CONFIG_ADPCM_4XM_DECODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_4XM_ENCODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_ADX_DECODER, yes )       { SOURCES *= adxdec.c }
contains( CONFIG_ADPCM_ADX_ENCODER, yes )       { SOURCES *= adxenc.c }
contains( CONFIG_ADPCM_CT_DECODER, yes )        { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_DECODER, yes )        { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_R1_DECODER, yes )     { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_R2_DECODER, yes )     { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_R3_DECODER, yes )     { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_EA_XAS_DECODER, yes )    { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_G726_DECODER, yes )      { SOURCES *= g726.c }
contains( CONFIG_ADPCM_G726_ENCODER, yes )      { SOURCES *= g726.c }
contains( CONFIG_ADPCM_IMA_AMV_DECODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_DK3_DECODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_DK4_DECODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_EA_EACS_DECODER, yes ) { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_EA_SEAD_DECODER, yes ) { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_QT_DECODER, yes )    { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_SMJPEG_DECODER, yes ) { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_WAV_DECODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_WAV_ENCODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_IMA_WS_DECODER, yes )    { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_MS_DECODER, yes )        { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_MS_ENCODER, yes )        { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SBPRO_2_DECODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SBPRO_3_DECODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SBPRO_4_DECODER, yes )   { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SWF_DECODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_SWF_ENCODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_THP_DECODER, yes )       { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_XA_DECODER, yes )        { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_YAMAHA_DECODER, yes )    { SOURCES *= adpcm.c }
contains( CONFIG_ADPCM_YAMAHA_ENCODER, yes )    { SOURCES *= adpcm.c }

# libavformat dependencies
contains( CONFIG_OGG_MUXER, yes )               { SOURCES *= xiph.c }

# external codec libraries
contains( CONFIG_LIBA52, yes )                  { SOURCES *= liba52.c }
contains( CONFIG_LIBAMR, yes )                  { SOURCES *= libamr.c }
contains( CONFIG_LIBFAAC, yes )                 { SOURCES *= libfaac.c }
contains( CONFIG_LIBFAAD, yes )                 { SOURCES *= libfaad.c }
contains( CONFIG_LIBGSM, yes )                  { SOURCES *= libgsm.c }
contains( CONFIG_LIBMP3LAME, yes )              { SOURCES *= libmp3lame.c }
contains( CONFIG_LIBTHEORA, yes )               { SOURCES *= libtheoraenc.c }
contains( CONFIG_LIBVORBIS, yes )               { SOURCES *= libvorbis.c }
contains( CONFIG_LIBX264, yes )                 { SOURCES *= libx264.c }
contains( CONFIG_LIBXVID, yes )                 { SOURCES *= libxvidff.c libxvid_rc.c }

contains( CONFIG_AAC_PARSER, yes )              { SOURCES *= aac_parser.c aac_ac3_parser.c }
contains( CONFIG_AC3_PARSER, yes )              { SOURCES *= ac3_parser.c ac3tab.c aac_ac3_parser.c }
contains( CONFIG_CAVSVIDEO_PARSER, yes )        { SOURCES *= cavs_parser.c }
contains( CONFIG_DCA_PARSER, yes )              { SOURCES *= dca_parser.c }
contains( CONFIG_DVBSUB_PARSER, yes )           { SOURCES *= dvbsub_parser.c }
contains( CONFIG_DVDSUB_PARSER, yes )           { SOURCES *= dvdsub_parser.c }
contains( CONFIG_H261_PARSER, yes )             { SOURCES *= h261_parser.c }
contains( CONFIG_H263_PARSER, yes )             { SOURCES *= h263_parser.c }
contains( CONFIG_H264_PARSER, yes )             { SOURCES *= h264_parser.c }
contains( CONFIG_MJPEG_PARSER, yes )            { SOURCES *= mjpeg_parser.c }
contains( CONFIG_MPEG4VIDEO_PARSER, yes )       { SOURCES *= mpeg4video_parser.c }
contains( CONFIG_MPEGAUDIO_PARSER, yes )        { SOURCES *= mpegaudio_parser.c mpegaudiodecheader.c }
contains( CONFIG_MPEGVIDEO_PARSER, yes )        { SOURCES *= mpegvideo_parser.c }
contains( CONFIG_PNM_PARSER, yes )              { SOURCES *= pnm_parser.c pnm.c }
contains( CONFIG_VC1_PARSER, yes )              { SOURCES *= vc1_parser.c }

contains( CONFIG_DUMP_EXTRADATA_BSF, yes )      { SOURCES *= dump_extradata_bsf.c }
contains( CONFIG_H264_MP4TOANNEXB_BSF, yes )    { SOURCES *= h264_mp4toannexb_bsf.c }
contains( CONFIG_REMOVE_EXTRADATA_BSF, yes )    { SOURCES *= remove_extradata_bsf.c }
contains( CONFIG_NOISE_BSF, yes )               { SOURCES *= noise_bsf.c }
contains( CONFIG_MP3_HEADER_COMPRESS_BSF, yes ) { SOURCES *= mp3_header_compress_bsf.c }
contains( CONFIG_MP3_HEADER_DECOMPRESS_BSF, yes ) { SOURCES *= mp3_header_decompress_bsf.c mpegaudiodata.c }
contains( CONFIG_MJPEGA_DUMP_HEADER_BSF, yes )  { SOURCES *= mjpega_dump_header_bsf.c }
contains( CONFIG_IMX_DUMP_HEADER_BSF, yes )     { SOURCES *= imx_dump_header_bsf.c }


contains( HAVE_PTHREADS, yes )                  { SOURCES *= pthread.c }
contains( HAVE_W32THREADS, yes )                { SOURCES *= w32thread.c }
contains( HAVE_OS2THREADS, yes )                { SOURCES *= os2thread.c }
contains( HAVE_BEOSTHREADS, yes )               { SOURCES *= beosthread.c }

using_xvmc {
    SOURCES *= xvmcvideo.c
    DEFINES += HAVE_XVMC
    LIBS    += $$CONFIG_XVMC_LIBS
}

using_xvmc_vld {
    SOURCES += xvmcvldvideo.c
    DEFINES += HAVE_XVMC_VLD
}

using_dvdv {
    SOURCES += dvdv.c
    DEFINES += HAVE_DVDV
}

!contains( CONFIG_SWSCALER, yes )               { SOURCES *= imgresample.c }

contains( HAVE_GPROF, yes ) {
    QMAKE_CFLAGS_RELEASE += -p
    QMAKE_LFLAGS_RELEASE += -p
}

contains( HAVE_MMX, yes ) {
    SOURCES += i386/fdct_mmx.c i386/cputest.c i386/dsputil_mmx.c
    SOURCES += i386/mpegvideo_mmx.c i386/motion_est_mmx.c
    SOURCES += i386/simple_idct_mmx.c i386/idct_mmx_xvid.c
    SOURCES += i386/fft_sse.c i386/fft_3dn.c i386/fft_3dn2.c
    contains( CONFIG_GPL, yes )            { SOURCES += i386/idct_mmx.c }
    contains( CONFIG_CAVS_DECODER, yes )   { SOURCES += i386/cavsdsp_mmx.c }
    contains( CONFIG_SNOW_DECODER, yes )   { SOURCES += i386/snowdsp_mmx.c }
    contains( CONFIG_VP3_DECODER, yes )    { SOURCES *= i386/vp3dsp_mmx.c i386/vp3dsp_sse2.c }
    contains( CONFIG_VP5_DECODER, yes )    { SOURCES *= i386/vp3dsp_mmx.c i386/vp3dsp_sse2.c }
    contains( CONFIG_VP6_DECODER, yes )    { SOURCES *= i386/vp3dsp_mmx.c i386/vp3dsp_sse2.c }
    contains( CONFIG_VP6A_DECODER, yes )   { SOURCES *= i386/vp3dsp_mmx.c i386/vp3dsp_sse2.c }
    contains( CONFIG_VP6F_DECODER, yes )   { SOURCES *= i386/vp3dsp_mmx.c i386/vp3dsp_sse2.c }
#    contains( HAVE_BUILTIN_VECTOR, yes ) {
#        QMAKE_CFLAGS_RELEASE += -msse
#        QMAKE_CFLAGS_DEBUG += -msse
#    }
}

contains( ARCH_ARMV4L, yes ) {
    SOURCES += armv4l/jrevdct_arm.S armv4l/simple_idct_arm.S armv4l/dsputil_arm_s.S
    SOURCES += armv4l/dsputil_arm.c armv4l/mpegvideo_arm.c
}

contains( HAVE_IWMMXT, yes ) {
    SOURCES += armv4l/dsputil_iwmmxt.c armv4l/mpegvideo_iwmmxt.c
}

contains( HAVE_ARMV5TE, yes ) {
    SOURCES += armv4l/simple_idct_armv5te.S armv4l/mpegvideo_armv5te.c
}

contains( HAVE_ARMV6, yes )      { SOURCES += armv4l/simple_idct_armv6.S }

contains( HAVE_VIS, yes ) {
    SOURCES += sparc/dsputil_vis.c
    SOURCES += sparc/simple_idct_vis.c
}

contains( HAVE_MLIB, yes ) {
    SOURCES += mlib/dsputil_mlib.c
    QMAKE_CFLAGS_RELEASE += $$MLIB_INC
}

contains( ARCH_ALPHA, yes ) {
    SOURCES += alpha/dsputil_alpha.c alpha/mpegvideo_alpha.c 
    SOURCES += alpha/motion_est_alpha.c alpha/dsputil_alpha_asm.S
    SOURCES += alpha/simple_idct_alpha.c alpha/motion_est_mvi_asm.S
    QMAKE_CFLAGS_RELEASE += -fforce-addr -freduce-all-givs
}

contains( ARCH_POWERPC, yes ) {
    SOURCES += ppc/dsputil_ppc.c
}

contains( HAVE_MMI, yes) {
    SOURCES += ps2/dsputil_mmi.c ps2/idct_mmi.c ps2/mpegvideo_mmi.c
}

contains( ARCH_SH4, yes) {
    SOURCES += sh4/idct_sh4.c sh4/dsputil_sh4.c sh4/dsputil_align.c
}

contains( HAVE_ALTIVEC, yes ) {
    SOURCES += ppc/dsputil_altivec.c ppc/mpegvideo_altivec.c ppc/idct_altivec.c
    SOURCES += ppc/fft_altivec.c ppc/gmc_altivec.c ppc/fdct_altivec.c
    SOURCES += ppc/float_altivec.c ppc/int_altivec.c ppc/check_altivec.c

    contains( CONFIG_H264_DECODER, yes ) { SOURCES += ppc/h264_altivec.c }
    contains( CONFIG_SNOW_DECODER, yes ) { SOURCES += ppc/snow_altivec.c }
    contains( CONFIG_VC1_DECODER, yes )  { SOURCES *= ppc/vc1dsp_altivec.c }
    contains( CONFIG_WMV3_DECODER, yes ) { SOURCES *= ppc/vc1dsp_altivec.c }
}

macx {
    QMAKE_LFLAGS_SHLIB += -single_module
    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC3000000
    QMAKE_LFLAGS_SHLIB += -read_only_relocs warning
}

contains( ARCH_BFIN, yes) {
    SOURCES += bfin/dsputil_bfin.c bfin/mpegvideo_bfin.c bfin/vp3_bfin.c
    SOURCES += bfin/pixels_bfin.S bfin/idct_bfin.S bfin/fdct_bfin.S
    SOURCES += bfin/vp3_idct_bfin.S
}

include ( ../libs-targetfix.pro )
