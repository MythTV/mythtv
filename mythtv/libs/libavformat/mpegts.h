/*
 * MPEG2 transport stream defines
 * Copyright (c) 2003 Fabrice Bellard.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef __cplusplus
extern "C" {
#endif

#define TS_FEC_PACKET_SIZE 204
#define TS_DVHS_PACKET_SIZE 192
#define TS_PACKET_SIZE 188
#define NB_PID_MAX 8192
#define MAX_SECTION_SIZE 4096

/* pids */
#define PAT_PID                 0x0000
#define SDT_PID                 0x0011

/* table ids */
#define PAT_TID   0x00
#define PMT_TID   0x02
#define SDT_TID   0x42

/* descriptor ids */
#define DVB_CAROUSEL_ID             0x13
#define DVB_VBI_DESCID              0x56
#define DVB_SUBT_DESCID             0x59
#define DVB_BROADCAST_ID            0x66
#define DVB_DATA_STREAM             0x52

#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_DSMCC_B         0x0b
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x8a

#define STREAM_TYPE_SUBTITLE_DVB    0x100
#define STREAM_TYPE_VBI_DVB         0x101

extern AVOutputFormat mpegts_mux;

typedef struct MpegTSContext MpegTSContext;

MpegTSContext *mpegts_parse_open(AVFormatContext *s);
int mpegts_parse_packet(MpegTSContext *ts, AVPacket *pkt,
                        const uint8_t *buf, int len);
void mpegts_parse_close(MpegTSContext *ts);
void mpegts_remove_stream(MpegTSContext *ts, int pid);

#ifdef __cplusplus
}
#endif
