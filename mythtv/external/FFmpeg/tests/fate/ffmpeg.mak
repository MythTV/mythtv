FATE_MAPCHAN-$(call FILTERDEMDECENCMUX, PAN, WAV, PCM_S16LE, PCM_S16LE, WAV, MD5_PROTOCOL) += fate-mapchan-6ch-extract-2
fate-mapchan-6ch-extract-2: tests/data/asynth-22050-6.wav
fate-mapchan-6ch-extract-2: CMD = ffmpeg -i $(TARGET_PATH)/tests/data/asynth-22050-6.wav -map_channel 0.0.0 -fflags +bitexact -f wav md5: -map_channel 0.0.1 -fflags +bitexact -f wav md5:

FATE_MAPCHAN-$(call FILTERDEMDECENCMUX, PAN ARESAMPLE, WAV, PCM_S16LE, PCM_S16LE, WAV) += fate-mapchan-6ch-extract-2-downmix-mono
fate-mapchan-6ch-extract-2-downmix-mono: tests/data/asynth-22050-6.wav
fate-mapchan-6ch-extract-2-downmix-mono: CMD = md5 -auto_conversion_filters -i $(TARGET_PATH)/tests/data/asynth-22050-6.wav -map_channel 0.0.1 -map_channel 0.0.0 -ac 1 -fflags +bitexact -f wav

FATE_MAPCHAN-$(call FILTERDEMDECENCMUX, PAN, WAV, PCM_S16LE, PCM_S16LE, WAV) += fate-mapchan-silent-mono
fate-mapchan-silent-mono: tests/data/asynth-22050-1.wav
fate-mapchan-silent-mono: CMD = md5 -i $(TARGET_PATH)/tests/data/asynth-22050-1.wav -map_channel -1 -map_channel 0.0.0 -fflags +bitexact -f wav

FATE_MAPCHAN-$(call FILTERDEMDECENCMUX, PAN, WAV, PCM_S16LE, PCM_S16LE, WAV) += fate-mapchan-2ch-extract-ch0-ch2-trailing
fate-mapchan-2ch-extract-ch0-ch2-trailing: tests/data/asynth-44100-2.wav
fate-mapchan-2ch-extract-ch0-ch2-trailing: CMD = md5 -i $(TARGET_PATH)/tests/data/asynth-44100-2.wav -map_channel 0.0.0 -map_channel 0.0.2? -fflags +bitexact -f wav

FATE_MAPCHAN-$(call FILTERDEMDECENCMUX, PAN, WAV, PCM_S16LE, PCM_S16LE, WAV) += fate-mapchan-3ch-extract-ch0-ch2-trailing
fate-mapchan-3ch-extract-ch0-ch2-trailing: tests/data/asynth-44100-3.wav
fate-mapchan-3ch-extract-ch0-ch2-trailing: CMD = md5 -i $(TARGET_PATH)/tests/data/asynth-44100-3.wav -map_channel 0.0.0 -map_channel 0.0.2? -fflags +bitexact -f wav

FATE_MAPCHAN = $(FATE_MAPCHAN-yes)

FATE_FFMPEG += $(FATE_MAPCHAN)
fate-mapchan: $(FATE_MAPCHAN)

FATE_FFMPEG-$(call FILTERFRAMECRC, COLOR) += fate-ffmpeg-filter_complex
fate-ffmpeg-filter_complex: CMD = framecrc -filter_complex color=d=1:r=5 -fflags +bitexact

# Ticket 6603
FATE_FFMPEG-$(call FILTERFRAMECRC, AEVALSRC ASETNSAMPLES ARESAMPLE, AC3_FIXED_ENCODER) += fate-ffmpeg-filter_complex_audio
fate-ffmpeg-filter_complex_audio: CMD = framecrc -auto_conversion_filters -filter_complex "aevalsrc=0:d=0.1,asetnsamples=1537" -c ac3_fixed

# Ticket 6375, use case of NoX
FATE_SAMPLES_FFMPEG-$(call FRAMECRC, MOV, PNG ALAC, ARESAMPLE_FILTER) += fate-ffmpeg-attached_pics
fate-ffmpeg-attached_pics: CMD = threads=2 framecrc -i $(TARGET_SAMPLES)/lossless-audio/inside.m4a -threads 1 -max_muxing_queue_size 16 -af aresample

FATE_SAMPLES_FFMPEG-$(call FILTERDEMDEC, COLORKEY OVERLAY SCALE, MPEGPS IMAGE_PPM_PIPE, CAVS PPM, CAVSVIDEO_PARSER) += fate-ffmpeg-filter_colorkey
fate-ffmpeg-filter_colorkey: tests/data/filtergraphs/colorkey
fate-ffmpeg-filter_colorkey: CMD = framecrc -auto_conversion_filters -idct simple -fflags +bitexact -flags +bitexact  -sws_flags +accurate_rnd+bitexact -i $(TARGET_SAMPLES)/cavs/cavs.mpg -fflags +bitexact -flags +bitexact -sws_flags +accurate_rnd+bitexact -i $(TARGET_SAMPLES)/lena.pnm -an -filter_complex_script $(TARGET_PATH)/tests/data/filtergraphs/colorkey -sws_flags +accurate_rnd+bitexact -fflags +bitexact -flags +bitexact -qscale 2 -frames:v 10

FATE_FFMPEG-$(call FILTERFRAMECRC, COLOR) += fate-ffmpeg-lavfi
fate-ffmpeg-lavfi: CMD = framecrc -lavfi color=d=1:r=5 -fflags +bitexact

FATE_SAMPLES_FFMPEG-$(call ENCDEC2, MPEG4, RAWVIDEO, AVI, RAWVIDEO_DEMUXER FRAMECRC_MUXER) += fate-force_key_frames
fate-force_key_frames: tests/data/vsynth_lena.yuv
fate-force_key_frames: CMD = enc_dec \
  "rawvideo -s 352x288 -pix_fmt yuv420p" tests/data/vsynth_lena.yuv \
  avi "-c mpeg4 -g 240 -qscale 10 -force_key_frames 0.5,0:00:01.5" \
  framecrc "" "-skip_frame nokey"

# Tests that the video is properly autorotated using the contained
# display matrix and that the generated file does not contain
# a display matrix any more.
FATE_SAMPLES_FFMPEG_FFPROBE-$(call TRANSCODE, MPEG2VIDEO, MOV, H264_DECODER AAC_FIXED_DECODER AC3_FIXED_ENCODER EXTRACT_EXTRADATA_BSF) += fate-autorotate
fate-autorotate: CMD = transcode "mov -c:a aac_fixed" $(TARGET_SAMPLES)/filter/sample-in-issue-505.mov mov "-c:v mpeg2video -c:a ac3_fixed" "-c copy -t 0.5" "-show_entries stream_side_data_list"

FATE_SAMPLES_FFMPEG-$(call FILTERDEMDEC, OVERLAY SCALE, RAWVIDEO VOBSUB, RAWVIDEO DVDSUB, DVDSUB_ENCODER) += fate-sub2video
fate-sub2video: tests/data/vsynth_lena.yuv
fate-sub2video: CMD = framecrc -auto_conversion_filters \
  -f rawvideo -r 5 -s 352x288 -pix_fmt yuv420p -i $(TARGET_PATH)/tests/data/vsynth_lena.yuv \
  -ss 132 -i $(TARGET_SAMPLES)/sub/vobsub.idx \
  -filter_complex "sws_flags=+accurate_rnd+bitexact\;[0:0]scale=720:480[v]\;[v][1:0]overlay[v2]" \
  -map "[v2]" -c:v rawvideo -threads 1 -map 1:s -c:s dvdsub

# Very basic sub2video example, decode and convert to AVFrame with sub2video.
# Attempt to not touch timestamps.
FATE_SAMPLES_FFMPEG-$(call FRAMECRC, VOBSUB, DVDSUB, SCALE_FILTER) += fate-sub2video_basic
fate-sub2video_basic: CMD = framecrc -auto_conversion_filters \
  -i $(TARGET_SAMPLES)/sub/vobsub.idx \
  -vsync passthrough -copyts \
  -filter_complex "sws_flags=+accurate_rnd+bitexact\;[0:s:0]scale" \
  -c:v rawvideo -threads 1

# Time-limited run with a sample that doesn't require seeking and
# contains samples within the initial period.
FATE_SAMPLES_FFMPEG-$(call FRAMECRC, SUP, PGSSUB, SCALE_FILTER RAWVIDEO_ENCODER) += fate-sub2video_time_limited
fate-sub2video_time_limited: CMD = framecrc -auto_conversion_filters \
  -i $(TARGET_SAMPLES)/sub/pgs_sub.sup \
  -vsync passthrough -copyts \
  -t 15 \
  -filter_complex "sws_flags=+accurate_rnd+bitexact\;[0:s:0]scale" \
  -c:v rawvideo -threads 1

FATE_FFMPEG-$(call ENCDEC, PCM_S16LE, PCM_S16LE) += fate-unknown_layout-pcm
fate-unknown_layout-pcm: $(AREF)
fate-unknown_layout-pcm: CMD = md5 \
  -guess_layout_max 0 -f s16le -ac 1 -ar 44100 -i $(TARGET_PATH)/$(AREF) -f s16le

FATE_FFMPEG-$(call FILTERDEMDECENCMUX, ARESAMPLE, PCM_S32LE, PCM_S32LE, AC3_FIXED, AC3) += fate-unknown_layout-ac3
fate-unknown_layout-ac3: $(AREF)
fate-unknown_layout-ac3: CMD = md5 -auto_conversion_filters \
  -guess_layout_max 0 -f s32le -ac 1 -ar 44100 -i $(TARGET_PATH)/$(AREF) \
  -f ac3 -flags +bitexact -c ac3_fixed

FATE_SAMPLES_FFMPEG-$(call FILTERDEMDEC, AMIX ARESAMPLE SINE, RAWVIDEO, \
                           PCM_S16LE RAWVIDEO, LAVFI_INDEV  \
                           MPEG4_ENCODER AC3_FIXED_ENCODER) \
                           += fate-shortest
fate-shortest: tests/data/vsynth_lena.yuv
fate-shortest: CMD = framecrc -auto_conversion_filters -f lavfi -i "sine=3000:d=10" -f lavfi -i "sine=1000:d=1" -sws_flags +accurate_rnd+bitexact -fflags +bitexact -flags +bitexact -idct simple -f rawvideo -s 352x288 -pix_fmt yuv420p -i $(TARGET_PATH)/tests/data/vsynth_lena.yuv -filter_complex "[0:a:0][1:a:0]amix=inputs=2[audio]" -map 2:v:0 -map "[audio]" -sws_flags +accurate_rnd+bitexact -fflags +bitexact -flags +bitexact -idct simple -dct fastint -qscale 10 -threads 1 -c:v mpeg4 -c:a ac3_fixed -shortest

# Basic test for fix_sub_duration, which calculates duration based on the
# following subtitle's pts.
FATE_SAMPLES_FFMPEG-$(call FILTERDEMDECENCMUX, MOVIE, MPEGVIDEO, \
                           MPEG2VIDEO, SUBRIP, SRT, LAVFI_INDEV  \
                           MPEGVIDEO_PARSER CCAPTION_DECODER PIPE_PROTOCOL) \
                           += fate-ffmpeg-fix_sub_duration
fate-ffmpeg-fix_sub_duration: CMD = fmtstdout srt -fix_sub_duration \
  -real_time 1 -f lavfi \
  -i "movie=$(TARGET_SAMPLES)/sub/Closedcaption_rollup.m2v[out0+subcc]"

FATE_STREAMCOPY-$(call REMUX, MP4 MOV, EAC3_DEMUXER) += fate-copy-trac3074
fate-copy-trac3074: CMD = transcode eac3 $(TARGET_SAMPLES)/eac3/csi_miami_stereo_128_spx.eac3\
                     mp4 "-codec copy -map 0" "-codec copy"

FATE_STREAMCOPY-$(call TRANSCODE, RAWVIDEO DVVIDEO, MOV, PCM_S16LE_DECODER) += fate-copy-trac236
fate-copy-trac236: CMD = transcode mov $(TARGET_SAMPLES)/mov/fcp_export8-236.mov\
                     mov "-codec copy -map 0"

FATE_STREAMCOPY-$(call TRANSCODE, RAWVIDEO MPEG2VIDEO, MXF, MPEGTS_DEMUXER MPEGVIDEO_PARSER MPEGAUDIO_PARSER MP2_DECODER ARESAMPLE_FILTER PCM_S16LE_DECODER) += fate-copy-trac4914
fate-copy-trac4914: CMD = transcode mpegts $(TARGET_SAMPLES)/mpeg2/xdcam8mp2-1s_small.ts\
                      mxf "-c:a pcm_s16le -af aresample -c:v copy"

FATE_STREAMCOPY-$(call TRANSCODE, RAWVIDEO MPEG2VIDEO, AVI, MPEGTS_DEMUXER MPEGVIDEO_PARSER MPEGAUDIO_PARSER EXTRACT_EXTRADATA_BSF MP2_DECODER ARESAMPLE_FILTER) += fate-copy-trac4914-avi
fate-copy-trac4914-avi: CMD = transcode mpegts $(TARGET_SAMPLES)/mpeg2/xdcam8mp2-1s_small.ts\
                          avi "-c:a copy -c:v copy" "-af aresample"

FATE_STREAMCOPY-$(call TRANSCODE, RAWVIDEO H264, AVI, H264_DEMUXER H264_PARSER EXTRACT_EXTRADATA_BSF) += fate-copy-trac2211-avi
fate-copy-trac2211-avi: CMD = transcode "h264 -r 14" $(TARGET_SAMPLES)/h264/bbc2.sample.h264\
                          avi "-c:v copy"

ifneq (,$(filter fate-lavf-apng,$(FATE_LAVF_VIDEO)))
FATE_STREAMCOPY-$(call TRANSCODE, RAWVIDEO APNG, APNG) += fate-copy-apng
endif
fate-copy-apng: fate-lavf-apng
fate-lavf-apng: KEEP_FILES ?= 1
fate-copy-apng: CMD = transcode apng tests/data/lavf/lavf.apng apng "-c:v copy"

FATE_STREAMCOPY-$(call DEMMUX, OGG, OGG) += fate-limited_input_seek fate-limited_input_seek-copyts
fate-limited_input_seek: CMD = md5 -ss 1.5 -t 1.3 -i $(TARGET_SAMPLES)/vorbis/moog_small.ogg -c:a copy -fflags +bitexact -f ogg
fate-limited_input_seek-copyts: CMD = md5 -ss 1.5 -t 1.3 -i $(TARGET_SAMPLES)/vorbis/moog_small.ogg -c:a copy -copyts -fflags +bitexact -f ogg

FATE_STREAMCOPY-$(call REMUX, PSP MOV, H264_PARSER H264_DECODER) += fate-copy-psp
fate-copy-psp: CMD = transcode "mov" $(TARGET_SAMPLES)/h264/wwwq_cut.mp4\
                      psp "-c copy" "-codec copy"

FATE_STREAMCOPY-$(call FRAMEMD5, FLV, H264) += fate-ffmpeg-streamloop
fate-ffmpeg-streamloop: CMD = framemd5 -stream_loop 2 -i $(TARGET_SAMPLES)/flv/streamloop.flv -c copy

tests/data/audio_shorter_than_video.nut: TAG = GEN
tests/data/audio_shorter_than_video.nut: tests/data/vsynth_lena.yuv
tests/data/audio_shorter_than_video.nut: ffmpeg$(PROGSSUF)$(EXESUF) | tests/data
	$(M)$(TARGET_EXEC) $(TARGET_PATH)/$< -nostdin \
        -sws_flags +accurate_rnd+bitexact -fflags +bitexact -flags +bitexact -idct simple -f rawvideo -s 352x288 -pix_fmt yuv420p -i $(TARGET_PATH)/tests/data/vsynth_lena.yuv \
        -f lavfi -i "sine=1000:d=1" \
        -sws_flags +accurate_rnd+bitexact -fflags +bitexact -flags +bitexact -idct simple -dct fastint -qscale 10 -c:v mpeg4 -threads 1 -c:a pcm_s16le -bitexact \
        -y $(TARGET_PATH)/tests/data/audio_shorter_than_video.nut 2>/dev/null

FATE_STREAMCOPY-$(call FRAMEMD5, NUT, RAWVIDEO PCM_S16LE MPEG4,  \
                                 RAWVIDEO_DEMUXER LAVFI_INDEV    \
                                 MPEG4_ENCODER PCM_S16LE_ENCODER \
                                 SINE_FILTER ARESAMPLE_FILTER AMIX_FILTER \
                                 NUT_MUXER AC3_FIXED_ENCODER)    \
                               += fate-copy-shortest1 fate-copy-shortest2
fate-copy-shortest1 fate-copy-shortest2: tests/data/audio_shorter_than_video.nut
fate-copy-shortest1: CMD = framemd5 -auto_conversion_filters -fflags +bitexact -flags +bitexact -f lavfi -i "sine=3000:d=10" -f lavfi -i "sine=1000:d=1" -i $(TARGET_PATH)/tests/data/audio_shorter_than_video.nut -filter_complex "[0:a:0][1:a:0]amix=inputs=2[audio]" -map 2:v:0 -map "[audio]" -fflags +bitexact -flags +bitexact -c:v copy -c:a ac3_fixed -shortest
fate-copy-shortest2: CMD = framemd5 -auto_conversion_filters -fflags +bitexact -flags +bitexact -f lavfi -i "sine=3000:d=10" -i $(TARGET_PATH)/tests/data/audio_shorter_than_video.nut -filter_complex "[0:a:0][1:a:0]amix=inputs=2[audio]" -map 1:v:0 -map "[audio]" -fflags +bitexact -flags +bitexact -c:v copy -c:a ac3_fixed -shortest

fate-streamcopy: $(FATE_STREAMCOPY-yes)

FATE_SAMPLES_FFMPEG-$(call TRANSCODE, RAWVIDEO, MATROSKA, MOV_DEMUXER QTRLE_DECODER) += fate-rgb24-mkv
fate-rgb24-mkv: CMD = transcode "mov" $(TARGET_SAMPLES)/qtrle/aletrek-rle.mov\
                      matroska "-c:v rawvideo -threads 1 -pix_fmt rgb24 -allow_raw_vfw 1 -frames:v 1"

FATE_SAMPLES_FFMPEG-$(call REMUX, MOV, AAC_DEMUXER AAC_DECODER AAC_PARSER AAC_ADTSTOASC_BSF) += fate-adtstoasc_ticket3715
fate-adtstoasc_ticket3715: CMD = transcode "aac" $(TARGET_SAMPLES)/aac/foo.aac\
                      mov "-c copy -bsf:a aac_adtstoasc" "-codec copy"

FATE_SAMPLES_FFMPEG-$(call REMUX, H264, MOV_DEMUXER H264_MP4TOANNEXB_BSF H264_PARSER H264_DECODER EXTRACT_EXTRADATA_BSF) += fate-h264_mp4toannexb_ticket2991 fate-h264_mp4toannexb_ticket5927 fate-h264_mp4toannexb_ticket5927_2
fate-h264_mp4toannexb_ticket2991: CMD = transcode "mp4" $(TARGET_SAMPLES)/h264/wwwq_cut.mp4\
                                  h264 "-c:v copy -bsf:v h264_mp4toannexb" "-codec copy"
fate-h264_mp4toannexb_ticket5927:   CMD = transcode "mp4" $(TARGET_SAMPLES)/h264/thezerotheorem-cut.mp4 \
                                          h264 "-c:v copy -bsf:v h264_mp4toannexb -an" "-c:v copy"
fate-h264_mp4toannexb_ticket5927_2: CMD = transcode "mp4" $(TARGET_SAMPLES)/h264/thezerotheorem-cut.mp4 \
                                          h264 "-c:v copy -an" "-c:v copy"

FATE_SAMPLES_FFMPEG-$(call TRANSCODE, MPEG4 MPEG2VIDEO, AVI, MPEGPS_DEMUXER MPEGVIDEO_DEMUXER MPEGVIDEO_PARSER EXTRACT_EXTRADATA_BSF REMOVE_EXTRADATA_BSF) += fate-ffmpeg-bsf-remove-k fate-ffmpeg-bsf-remove-r fate-ffmpeg-bsf-remove-e
fate-ffmpeg-bsf-remove-k: CMD = transcode "mpeg" $(TARGET_SAMPLES)/mpeg2/matrixbench_mpeg2.lq1.mpg\
                          avi "-vbsf remove_extra=k" "-codec copy"
fate-ffmpeg-bsf-remove-r: CMD = transcode "mpeg" $(TARGET_SAMPLES)/mpeg2/matrixbench_mpeg2.lq1.mpg\
                          avi "-vbsf remove_extra=keyframe" "-codec copy"
fate-ffmpeg-bsf-remove-e: CMD = transcode "mpeg" $(TARGET_SAMPLES)/mpeg2/matrixbench_mpeg2.lq1.mpg\
                          avi "-vbsf remove_extra=e" "-codec copy"

FATE_SAMPLES_FFMPEG-$(call DEMMUX, APNG, FRAMECRC, SETTS_BSF PIPE_PROTOCOL) += fate-ffmpeg-setts-bsf
fate-ffmpeg-setts-bsf: CMD = framecrc -i $(TARGET_SAMPLES)/apng/clock.png -c:v copy -bsf:v "setts=duration=if(eq(NEXT_PTS\,NOPTS)\,PREV_OUTDURATION\,(NEXT_PTS-PTS)/2):ts=PTS/2" -fflags +bitexact

FATE_SAMPLES_FFMPEG-yes += $(FATE_STREAMCOPY-yes)

FATE_TIME_BASE-$(call PARSERDEMDEC, MPEGVIDEO, MPEGPS, MPEG2VIDEO, MPEGVIDEO_DEMUXER MXF_MUXER) += fate-time_base
fate-time_base: CMD = md5 -i $(TARGET_SAMPLES)/mpeg2/dvd_single_frame.vob -an -sn -c:v copy -r 25 -time_base 1001:30000 -fflags +bitexact -f mxf

FATE_SAMPLES_FFMPEG-yes += $(FATE_TIME_BASE-yes)
