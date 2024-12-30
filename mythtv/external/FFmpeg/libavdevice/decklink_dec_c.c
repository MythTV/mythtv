/*
 * Blackmagic DeckLink input
 * Copyright (c) 2014 Deti Fliegl
 * Copyright (c) 2017 Akamai Technologies, Inc.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavformat/avformat.h"
#include "libavformat/demux.h"
#include "libavutil/opt.h"

#include "decklink_common_c.h"
#include "decklink_dec.h"

#define OFFSET(x) offsetof(struct decklink_cctx, x)
#define DEC AV_OPT_FLAG_DECODING_PARAM

static const AVOption options[] = {
    { "list_devices", "use ffmpeg -sources decklink instead", OFFSET(list_devices), AV_OPT_TYPE_BOOL, { .i64 = 0   }, 0, 1, DEC | AV_OPT_FLAG_DEPRECATED},
    { "list_formats", "list supported formats"  , OFFSET(list_formats), AV_OPT_TYPE_INT   , { .i64 = 0   }, 0, 1, DEC },
    { "format_code",  "set format by fourcc"    , OFFSET(format_code),  AV_OPT_TYPE_STRING, { .str = NULL}, 0, 0, DEC },
    { "raw_format",   "pixel format to be returned by the card when capturing" , OFFSET(raw_format),  AV_OPT_TYPE_INT, { .i64 = 0}, 0, 5, DEC, .unit = "raw_format" },
    { "auto",          NULL,   0,  AV_OPT_TYPE_CONST, { .i64 = 0 }, 0, 0, DEC, .unit = "raw_format"},
    { "uyvy422",       NULL,   0,  AV_OPT_TYPE_CONST, { .i64 = 1 }, 0, 0, DEC, .unit = "raw_format"},
    { "yuv422p10",     NULL,   0,  AV_OPT_TYPE_CONST, { .i64 = 2 }, 0, 0, DEC, .unit = "raw_format"},
    { "argb",          NULL,   0,  AV_OPT_TYPE_CONST, { .i64 = 3 }, 0, 0, DEC, .unit = "raw_format"},
    { "bgra",          NULL,   0,  AV_OPT_TYPE_CONST, { .i64 = 4 }, 0, 0, DEC, .unit = "raw_format"},
    { "rgb10",         NULL,   0,  AV_OPT_TYPE_CONST, { .i64 = 5 }, 0, 0, DEC, .unit = "raw_format"},
    { "enable_klv",    "output klv if present in vanc", OFFSET(enable_klv), AV_OPT_TYPE_BOOL, { .i64 = 0  }, 0, 1,   DEC },
    { "teletext_lines", "teletext lines bitmask", OFFSET(teletext_lines), AV_OPT_TYPE_INT64, { .i64 = 0   }, 0, 0x7ffffffffLL, DEC, .unit = "teletext_lines"},
    { "standard",     NULL,                                           0,  AV_OPT_TYPE_CONST, { .i64 = 0x7fff9fffeLL}, 0, 0,    DEC, .unit = "teletext_lines"},
    { "all",          NULL,                                           0,  AV_OPT_TYPE_CONST, { .i64 = 0x7ffffffffLL}, 0, 0,    DEC, .unit = "teletext_lines"},
    { "channels",     "number of audio channels", OFFSET(audio_channels), AV_OPT_TYPE_INT , { .i64 = 2   }, 2, 16, DEC },
#if BLACKMAGIC_DECKLINK_API_VERSION >= 0x0b000000
    { "duplex_mode",  "duplex mode",              OFFSET(duplex_mode),    AV_OPT_TYPE_INT,   { .i64 = 0}, 0, 5,    DEC, .unit = "duplex_mode"},
#else
    { "duplex_mode",  "duplex mode",              OFFSET(duplex_mode),    AV_OPT_TYPE_INT,   { .i64 = 0}, 0, 2,    DEC, .unit = "duplex_mode"},
#endif
    { "unset",         NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 0}, 0, 0,    DEC, .unit = "duplex_mode"},
    { "half",          NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 1}, 0, 0,    DEC, .unit = "duplex_mode"},
    { "full",          NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 2}, 0, 0,    DEC, .unit = "duplex_mode"},
#if BLACKMAGIC_DECKLINK_API_VERSION >= 0x0b000000
    { "one_sub_device_full",      NULL,                               0,  AV_OPT_TYPE_CONST, { .i64 = 2}, 0, 0,    DEC, .unit = "duplex_mode"},
    { "one_sub_device_half",      NULL,                               0,  AV_OPT_TYPE_CONST, { .i64 = 3}, 0, 0,    DEC, .unit = "duplex_mode"},
    { "two_sub_device_full",      NULL,                               0,  AV_OPT_TYPE_CONST, { .i64 = 4}, 0, 0,    DEC, .unit = "duplex_mode"},
    { "four_sub_device_half",     NULL,                               0,  AV_OPT_TYPE_CONST, { .i64 = 5}, 0, 0,    DEC, .unit = "duplex_mode"},
#endif
    { "timecode_format", "timecode format",           OFFSET(tc_format),  AV_OPT_TYPE_INT,   { .i64 = 0}, 0, 8,    DEC, .unit = "tc_format"},
    { "none",          NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 0}, 0, 0,    DEC, .unit = "tc_format"},
    { "rp188vitc",     NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 1}, 0, 0,    DEC, .unit = "tc_format"},
    { "rp188vitc2",    NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 2}, 0, 0,    DEC, .unit = "tc_format"},
    { "rp188ltc",      NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 3}, 0, 0,    DEC, .unit = "tc_format"},
    { "rp188any",      NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 4}, 0, 0,    DEC, .unit = "tc_format"},
    { "vitc",          NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 5}, 0, 0,    DEC, .unit = "tc_format"},
    { "vitc2",         NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 6}, 0, 0,    DEC, .unit = "tc_format"},
    { "serial",        NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 7}, 0, 0,    DEC, .unit = "tc_format"},
#if BLACKMAGIC_DECKLINK_API_VERSION >= 0x0b000000
    { "rp188hfr",      NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 8}, 0, 0,    DEC, .unit = "tc_format"},
#endif
    { "video_input",  "video input",              OFFSET(video_input),    AV_OPT_TYPE_INT,   { .i64 = 0}, 0, 6,    DEC, .unit = "video_input"},
    { "unset",         NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 0}, 0, 0,    DEC, .unit = "video_input"},
    { "sdi",           NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 1}, 0, 0,    DEC, .unit = "video_input"},
    { "hdmi",          NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 2}, 0, 0,    DEC, .unit = "video_input"},
    { "optical_sdi",   NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 3}, 0, 0,    DEC, .unit = "video_input"},
    { "component",     NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 4}, 0, 0,    DEC, .unit = "video_input"},
    { "composite",     NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 5}, 0, 0,    DEC, .unit = "video_input"},
    { "s_video",       NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 6}, 0, 0,    DEC, .unit = "video_input"},
    { "audio_input",  "audio input",              OFFSET(audio_input),    AV_OPT_TYPE_INT,   { .i64 = 0}, 0, 6,    DEC, .unit = "audio_input"},
    { "unset",         NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 0}, 0, 0,    DEC, .unit = "audio_input"},
    { "embedded",      NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 1}, 0, 0,    DEC, .unit = "audio_input"},
    { "aes_ebu",       NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 2}, 0, 0,    DEC, .unit = "audio_input"},
    { "analog",        NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 3}, 0, 0,    DEC, .unit = "audio_input"},
    { "analog_xlr",    NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 4}, 0, 0,    DEC, .unit = "audio_input"},
    { "analog_rca",    NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 5}, 0, 0,    DEC, .unit = "audio_input"},
    { "microphone",    NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = 6}, 0, 0,    DEC, .unit = "audio_input"},
    { "audio_pts",     "audio pts source",   OFFSET(audio_pts_source),    AV_OPT_TYPE_INT,   { .i64 = PTS_SRC_AUDIO    }, 1, PTS_SRC_NB-1, DEC, .unit = "pts_source"},
    { "video_pts",     "video pts source",   OFFSET(video_pts_source),    AV_OPT_TYPE_INT,   { .i64 = PTS_SRC_VIDEO    }, 1, PTS_SRC_NB-1, DEC, .unit = "pts_source"},
    { "audio",         NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = PTS_SRC_AUDIO    }, 0, 0, DEC, .unit = "pts_source"},
    { "video",         NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = PTS_SRC_VIDEO    }, 0, 0, DEC, .unit = "pts_source"},
    { "reference",     NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = PTS_SRC_REFERENCE}, 0, 0, DEC, .unit = "pts_source"},
    { "wallclock",     NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = PTS_SRC_WALLCLOCK}, 0, 0, DEC, .unit = "pts_source"},
    { "abs_wallclock", NULL,                                          0,  AV_OPT_TYPE_CONST, { .i64 = PTS_SRC_ABS_WALLCLOCK}, 0, 0, DEC, .unit = "pts_source"},
    { "draw_bars",     "use option signal_loss_action instead" , OFFSET(draw_bars),    AV_OPT_TYPE_BOOL,  { .i64 = 1}, 0, 1, DEC | AV_OPT_FLAG_DEPRECATED },
    { "queue_size",    "input queue buffer size",   OFFSET(queue_size),   AV_OPT_TYPE_INT64, { .i64 = (1024 * 1024 * 1024)}, 0, INT64_MAX, DEC },
    { "audio_depth",   "audio bitdepth (16 or 32)", OFFSET(audio_depth),  AV_OPT_TYPE_INT,   { .i64 = 16}, 16, 32, DEC },
    { "decklink_copyts", "copy timestamps, do not remove the initial offset", OFFSET(copyts), AV_OPT_TYPE_BOOL, { .i64 = 0 }, 0, 1, DEC },
    { "timestamp_align", "capture start time alignment (in seconds)", OFFSET(timestamp_align), AV_OPT_TYPE_DURATION, { .i64 = 0 }, 0, INT_MAX, DEC },
    { "wait_for_tc",     "drop frames till a frame with timecode is received. TC format must be set", OFFSET(wait_for_tc), AV_OPT_TYPE_BOOL, { .i64 = 0 }, 0, 1, DEC },
    { "signal_loss_action", "action on signal loss", OFFSET(signal_loss_action), AV_OPT_TYPE_INT, { .i64 = SIGNAL_LOSS_BARS }, SIGNAL_LOSS_NONE, SIGNAL_LOSS_BARS, DEC, .unit = "signal_loss_action" },
    { "none", "do not do anything (usually leads to black frames)", 0,  AV_OPT_TYPE_CONST, { .i64 = SIGNAL_LOSS_NONE }, 0, 0, DEC, .unit = "signal_loss_action"},
    { "bars", "draw color bars (only supported for 8-bit signals)", 0,  AV_OPT_TYPE_CONST, { .i64 = SIGNAL_LOSS_BARS }, 0, 0, DEC, .unit = "signal_loss_action"},
    { "repeat", "repeat the last video frame", 0,  AV_OPT_TYPE_CONST, { .i64 = SIGNAL_LOSS_REPEAT }, 0, 0, DEC, .unit = "signal_loss_action"},
    { NULL },
};

static const AVClass decklink_demuxer_class = {
    .class_name = "Blackmagic DeckLink indev",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
    .category   = AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT,
};

const FFInputFormat ff_decklink_demuxer = {
    .p.name          = "decklink",
    .p.long_name     = NULL_IF_CONFIG_SMALL("Blackmagic DeckLink input"),
    .p.flags         = AVFMT_NOFILE,
    .p.priv_class    = &decklink_demuxer_class,
    .priv_data_size = sizeof(struct decklink_cctx),
    .get_device_list = ff_decklink_list_input_devices,
    .read_header   = ff_decklink_read_header,
    .read_packet   = ff_decklink_read_packet,
    .read_close    = ff_decklink_read_close,
};
