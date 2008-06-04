include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavformat-$$LIBVERSION
CONFIG += thread dll warn_off
target.path = $${LIBDIR}
INSTALLS = target

INCLUDEPATH += ../ ../../ ../libavcodec ../libavutil ../libmythtv

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE

LIBS += $$LOCAL_LIBDIR_X11 $$EXTRALIBS

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += asf.h avformat.h avi.h avio.h dv.h mpegts.h os_support.h qtpalette.h
HEADERS += riff.h

SOURCES += utils.c cutils.c os_support.c allformats.c sdp.c
SOURCES += avio.c aviobuf.c

# not using:  beosaudio.cpp, dc1394.c, framehook.*
# not using:  grab.c v4l2.c x11grab.c libnut.c

# muxers/demuxers
contains( CONFIG_AAC_DEMUXER, yes )               { SOURCES *= raw.c }
contains( CONFIG_AC3_DEMUXER, yes )               { SOURCES *= raw.c }
contains( CONFIG_AC3_MUXER, yes )                 { SOURCES *= raw.c }
contains( CONFIG_ADTS_MUXER, yes )                { SOURCES *= adtsenc.c }
contains( CONFIG_AIFF_DEMUXER, yes )              { SOURCES *= aiff.c riff.c }
contains( CONFIG_AIFF_MUXER, yes )                { SOURCES *= aiff.c riff.c }
contains( CONFIG_AMR_DEMUXER, yes )               { SOURCES *= amr.c }
contains( CONFIG_AMR_MUXER, yes )                 { SOURCES *= amr.c }
contains( CONFIG_APC_DEMUXER, yes )               { SOURCES *= apc.c }
contains( CONFIG_APE_DEMUXER, yes )               { SOURCES *= ape.c }
contains( CONFIG_ASF_DEMUXER, yes )               { SOURCES *= asf.c asfcrypt.c riff.c }
contains( CONFIG_ASF_MUXER, yes )                 { SOURCES *= asf-enc.c riff.c }
contains( CONFIG_ASF_STREAM_MUXER, yes )          { SOURCES *= asf-enc.c riff.c }
contains( CONFIG_AU_DEMUXER, yes )                { SOURCES *= au.c riff.c }
contains( CONFIG_AU_MUXER, yes )                  { SOURCES *= au.c riff.c }
contains( CONFIG_AVI_DEMUXER, yes )               { SOURCES *= avidec.c riff.c }
contains( CONFIG_AVI_MUXER, yes )                 { SOURCES *= avienc.c riff.c }
contains( CONFIG_AVISYNTH, yes )                  { SOURCES *= avisynth.c }
contains( CONFIG_AVS_DEMUXER, yes )               { SOURCES *= avs.c vocdec.c voc.c riff.c }
contains( CONFIG_BETHSOFTVID_DEMUXER, yes )       { SOURCES *= bethsoftvid.c }
contains( CONFIG_BKTR_DEMUXER, yes )              { SOURCES *= bktr.c }
contains( CONFIG_C93_DEMUXER, yes )               { SOURCES *= c93.c }
contains( CONFIG_CRC_MUXER, yes )                 { SOURCES *= crcenc.c }
contains( CONFIG_DAUD_DEMUXER, yes )              { SOURCES *= daud.c }
contains( CONFIG_DC1394_DEMUXER, yes )            { SOURCES *= dc1394.c }
contains( CONFIG_DSICIN_DEMUXER, yes )            { SOURCES *= dsicin.c }
contains( CONFIG_DTS_DEMUXER, yes )               { SOURCES *= raw.c }
contains( CONFIG_DV_DEMUXER, yes )                { SOURCES *= dv.c }
contains( CONFIG_DV_MUXER, yes )                  { SOURCES *= dvenc.c }
contains( CONFIG_DV1394_DEMUXER, yes )            { SOURCES *= dv1394.c }
contains( CONFIG_DXA_DEMUXER, yes )               { SOURCES *= dxa.c }
contains( CONFIG_EA_CDATA_DEMUXER, yes )          { SOURCES *= eacdata.c }
contains( CONFIG_EA_DEMUXER, yes )                { SOURCES *= electronicarts.c }
contains( CONFIG_FFM_DEMUXER, yes )               { SOURCES *= ffm.c }
contains( CONFIG_FFM_MUXER, yes )                 { SOURCES *= ffm.c }
contains( CONFIG_FLAC_DEMUXER, yes )              { SOURCES *= raw.c }
contains( CONFIG_FLAC_MUXER, yes )                { SOURCES *= raw.c }
contains( CONFIG_FLIC_DEMUXER, yes )              { SOURCES *= flic.c }
contains( CONFIG_FLV_DEMUXER, yes )               { SOURCES *= flvdec.c }
contains( CONFIG_FLV_MUXER, yes )                 { SOURCES *= flvenc.c }
contains( CONFIG_FOURXM_DEMUXER, yes )            { SOURCES *= 4xm.c }
contains( CONFIG_FRAMECRC_MUXER, yes )            { SOURCES *= framecrcenc.c }
contains( CONFIG_GIF_MUXER, yes )                 { SOURCES *= gif.c }
contains( CONFIG_GIF_DEMUXER, yes )               { SOURCES *= gifdec.c }
contains( CONFIG_GXF_DEMUXER, yes )               { SOURCES *= gxf.c }
contains( CONFIG_GXF_MUXER, yes )                 { SOURCES *= gxfenc.c }
contains( CONFIG_H261_DEMUXER, yes )              { SOURCES *= raw.c }
contains( CONFIG_H261_MUXER, yes )                { SOURCES *= raw.c }
contains( CONFIG_H263_DEMUXER, yes )              { SOURCES *= raw.c }
contains( CONFIG_H263_MUXER, yes )                { SOURCES *= raw.c }
contains( CONFIG_H264_DEMUXER, yes )              { SOURCES *= raw.c }
contains( CONFIG_H264_MUXER, yes )                { SOURCES *= raw.c }
contains( CONFIG_IDCIN_DEMUXER, yes )             { SOURCES *= idcin.c }
contains( CONFIG_IMAGE2_DEMUXER, yes )            { SOURCES *= img2.c }
contains( CONFIG_IMAGE2_MUXER, yes )              { SOURCES *= img2.c }
contains( CONFIG_IMAGE2PIPE_DEMUXER, yes )        { SOURCES *= img2.c }
contains( CONFIG_IMAGE2PIPE_MUXER, yes )          { SOURCES *= img2.c }
contains( CONFIG_INGENIENT_DEMUXER, yes )         { SOURCES *= raw.c }
contains( CONFIG_IPMOVIE_DEMUXER, yes )           { SOURCES *= ipmovie.c }
contains( CONFIG_M4V_DEMUXER, yes )               { SOURCES *= raw.c }
contains( CONFIG_M4V_MUXER, yes )                 { SOURCES *= raw.c }
contains( CONFIG_MATROSKA_AUDIO_MUXER, yes )      { SOURCES *= matroskaenc.c matroska.c riff.c }
contains( CONFIG_MATROSKA_DEMUXER, yes )          { SOURCES *= matroskadec.c matroska.c riff.c }
contains( CONFIG_MATROSKA_MUXER, yes )            { SOURCES *= matroskaenc.c matroska.c riff.c }
contains( CONFIG_MJPEG_DEMUXER, yes )             { SOURCES *= raw.c }
contains( CONFIG_MJPEG_MUXER, yes )               { SOURCES *= raw.c }
contains( CONFIG_MM_DEMUXER, yes )                { SOURCES *= mm.c }
contains( CONFIG_MMF_DEMUXER, yes )               { SOURCES *= mmf.c riff.c }
contains( CONFIG_MMF_MUXER, yes )                 { SOURCES *= mmf.c riff.c }
contains( CONFIG_MOV_DEMUXER, yes )               { SOURCES *= mov.c riff.c isom.c }
contains( CONFIG_MOV_MUXER, yes )                 { SOURCES *= movenc.c riff.c isom.c }
contains( CONFIG_MP2_MUXER, yes )                 { SOURCES *= mp3.c }
contains( CONFIG_MP3_DEMUXER, yes )               { SOURCES *= mp3.c }
contains( CONFIG_MP3_MUXER, yes )                 { SOURCES *= mp3.c }
contains( CONFIG_MP4_MUXER, yes )                 { SOURCES *= movenc.c riff.c isom.c }
contains( CONFIG_MPC_DEMUXER, yes )               { SOURCES *= mpc.c }
contains( CONFIG_MPC8_DEMUXER, yes )              { SOURCES *= mpc8.c }
contains( CONFIG_MPEG1SYSTEM_MUXER, yes )         { SOURCES *= mpegenc.c }
contains( CONFIG_MPEG1VCD_MUXER, yes )            { SOURCES *= mpegenc.c }
contains( CONFIG_MPEG2DVD_MUXER, yes )            { SOURCES *= mpegenc.c }
contains( CONFIG_MPEG2VOB_MUXER, yes )            { SOURCES *= mpegenc.c }
contains( CONFIG_MPEG2SVCD_MUXER, yes )           { SOURCES *= mpegenc.c }
contains( CONFIG_MPEG1VIDEO_MUXER, yes )          { SOURCES *= raw.c }
contains( CONFIG_MPEG2VIDEO_MUXER, yes )          { SOURCES *= raw.c }
contains( CONFIG_MPEGPS_DEMUXER, yes )            { SOURCES *= mpeg.c }
contains( CONFIG_MPEGTS_DEMUXER, yes )            { SOURCES *= mpegts.c }
contains( CONFIG_MPEGTS_MUXER, yes )              { SOURCES *= mpegtsenc.c }
contains( CONFIG_MPEGVIDEO_DEMUXER, yes )         { SOURCES *= raw.c }
contains( CONFIG_MPJPEG_MUXER, yes )              { SOURCES *= mpjpeg.c }
contains( CONFIG_MTV_DEMUXER, yes )               { SOURCES *= mtv.c }
contains( CONFIG_MXF_DEMUXER, yes )               { SOURCES *= mxf.c }
contains( CONFIG_NSV_DEMUXER, yes )               { SOURCES *= nsvdec.c riff.c }
contains( CONFIG_NULL_MUXER, yes )                { SOURCES *= raw.c }
contains( CONFIG_NUT_DEMUXER, yes )               { SOURCES *= nutdec.c riff.c }
contains( CONFIG_NUT_MUXER, yes )                 { SOURCES *= nutenc.c nut.c riff.c }
contains( CONFIG_NUV_DEMUXER, yes )               { SOURCES *= nuv.c riff.c }
contains( CONFIG_OGG_DEMUXER, yes )               { SOURCES *= oggdec.c oggparsevorbis.c oggparsetheora.c oggparseflac.c oggparseogm.c riff.c }
contains( CONFIG_OGG_MUXER, yes )                 { SOURCES *= oggenc.c }
contains( CONFIG_OSS_DEMUXER, yes )               { SOURCES *= audio.c }
contains( CONFIG_OSS_MUXER, yes )                 { SOURCES *= audio.c }
contains( CONFIG_PSP_MUXER, yes )                 { SOURCES *= movenc.c riff.c isom.c }
contains( CONFIG_RAWVIDEO_DEMUXER, yes )          { SOURCES *= raw.c }
contains( CONFIG_RAWVIDEO_MUXER, yes )            { SOURCES *= raw.c }
contains( CONFIG_REDIR_DEMUXER, yes )             { SOURCES *= rtsp.c }
contains( CONFIG_RM_DEMUXER, yes )                { SOURCES *= rmdec.c }
contains( CONFIG_RM_MUXER, yes )                  { SOURCES *= rmenc.c }
contains( CONFIG_ROQ_DEMUXER, yes )               { SOURCES *= idroq.c }
contains( CONFIG_ROQ_MUXER, yes )                 { SOURCES *= raw.c }
contains( CONFIG_RTP_MUXER, yes )                 { SOURCES *= rtp.c rtp_h264.c rtp_mpv.c rtp_aac.c }
contains( CONFIG_RTSP_DEMUXER, yes )              { SOURCES *= rtsp.c }
contains( CONFIG_SDP_DEMUXER, yes )               { SOURCES *= rtsp.c }
contains( CONFIG_SEGAFILM_DEMUXER, yes )          { SOURCES *= segafilm.c }
contains( CONFIG_SIFF_DEMUXER, yes )              { SOURCES *= siff.c }
contains( CONFIG_SHORTEN_DEMUXER, yes )           { SOURCES *= raw.c }
contains( CONFIG_SMACKER_DEMUXER, yes )           { SOURCES *= smacker.c }
contains( CONFIG_SOL_DEMUXER, yes )               { SOURCES *= sol.c }
contains( CONFIG_STR_DEMUXER, yes )               { SOURCES *= psxstr.c }
contains( CONFIG_SWF_DEMUXER, yes )               { SOURCES *= swf.c }
contains( CONFIG_SWF_MUXER, yes )                 { SOURCES *= swf.c }
contains( CONFIG_TG2_MUXER, yes )                 { SOURCES *= movenc.c riff.c isom.c }
contains( CONFIG_TGP_MUXER, yes )                 { SOURCES *= movenc.c riff.c isom.c }
contains( CONFIG_THP_DEMUXER, yes )               { SOURCES *= thp.c }
contains( CONFIG_TIERTEXSEQ_DEMUXER, yes )        { SOURCES *= tiertexseq.c }
contains( CONFIG_TTA_DEMUXER, yes )               { SOURCES *= tta.c }
contains( CONFIG_TXD_DEMUXER, yes )               { SOURCES *= txd.c }
contains( CONFIG_V4L2_DEMUXER, yes )              { SOURCES *= v4l2.c }
contains( CONFIG_V4L_DEMUXER, yes )               { SOURCES *= v4l.c }
contains( CONFIG_VMD_DEMUXER, yes )               { SOURCES *= sierravmd.c }
contains( CONFIG_VOC_DEMUXER, yes )               { SOURCES *= vocdec.c voc.c riff.c }
contains( CONFIG_VOC_MUXER, yes )                 { SOURCES *= vocenc.c voc.c riff.c }
contains( CONFIG_WAV_DEMUXER, yes )               { SOURCES *= wav.c riff.c }
contains( CONFIG_WAV_MUXER, yes )                 { SOURCES *= wav.c riff.c }
contains( CONFIG_WC3_DEMUXER, yes )               { SOURCES *= wc3movie.c }
contains( CONFIG_WSAUD_DEMUXER, yes )             { SOURCES *= westwood.c }
contains( CONFIG_WSVQA_DEMUXER, yes )             { SOURCES *= westwood.c }
contains( CONFIG_WV_DEMUXER, yes )                { SOURCES *= wv.c }
contains( CONFIG_X11_GRAB_DEVICE_DEMUXER, yes )   { SOURCES *= x11grab.c }
contains( CONFIG_YUV4MPEGPIPE_MUXER, yes )        { SOURCES *= yuv4mpeg.c }
contains( CONFIG_YUV4MPEGPIPE_DEMUXER, yes )      { SOURCES *= yuv4mpeg.c }

# external libraries
contains( CONFIG_LIBDC1394_DEMUXER, yes )         { SOURCES *= libdc1394.c }
contains( CONFIG_LIBNUT_DEMUXER, yes )            { SOURCES *= libnut.c riff.c }
contains( CONFIG_LIBNUT_MUXER, yes )              { SOURCES *= libnut.c riff.c }

# protocols I/O
contains( CONFIG_FILE_PROTOCOL, yes )             { SOURCES *= file.c }
contains( CONFIG_HTTP_PROTOCOL, yes )             { SOURCES *= http.c }
contains( CONFIG_PIPE_PROTOCOL, yes )             { SOURCES *= file.c }
contains( CONFIG_RTP_PROTOCOL, yes )              { SOURCES *= rtpproto.c }
contains( CONFIG_TCP_PROTOCOL, yes )              { SOURCES *= tcp.c }
contains( CONFIG_UDP_PROTOCOL, yes )              { SOURCES *= udp.c }


inc.path = $${PREFIX}/include/mythtv/ffmpeg/
inc.files = avformat.h avio.h rtp.h rtsp.h rtspcodes.h

INSTALLS += inc

LIBS += -L../libavcodec -lmythavcodec-$$LIBVERSION -L../libavutil -lmythavutil-$$LIBVERSION
LIBS += -lz
using_xvmc:LIBS += $$CONFIG_XVMC_LIBS

macx {
    QMAKE_LFLAGS_SHLIB += -single_module
    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC4000000
    SOURCES            -= audio.c
}

mingw:SOURCES -= audio.c

include ( ../libs-targetfix.pro )
