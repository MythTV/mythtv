/*
 * copyright (c) 2001 Fabrice Bellard
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

#ifndef AVFORMAT_DEMUX_H
#define AVFORMAT_DEMUX_H

#include <stdint.h>
#include "libavutil/rational.h"
#include "libavcodec/packet.h"
#include "avformat.h"

#define MAX_STD_TIMEBASES (30*12+30+3+6)
typedef struct FFStreamInfo {
    int64_t last_dts;
    int64_t duration_gcd;
    int duration_count;
    int64_t rfps_duration_sum;
    double (*duration_error)[2][MAX_STD_TIMEBASES];
    int64_t codec_info_duration;
    int64_t codec_info_duration_fields;
    int frame_delay_evidence;

    /**
     * 0  -> decoder has not been searched for yet.
     * >0 -> decoder found
     * <0 -> decoder with codec_id == -found_decoder has not been found
     */
    int found_decoder;

    int64_t last_duration;

    /**
     * Those are used for average framerate estimation.
     */
    int64_t fps_first_dts;
    int     fps_first_dts_idx;
    int64_t fps_last_dts;
    int     fps_last_dts_idx;
} FFStreamInfo;

/**
 * Returned by demuxers to indicate that data was consumed but discarded
 * (ignored streams or junk data). The framework will re-call the demuxer.
 */
#define FFERROR_REDO FFERRTAG('R','E','D','O')

#define RELATIVE_TS_BASE (INT64_MAX - (1LL << 48))

static av_always_inline int is_relative(int64_t ts)
{
    return ts > (RELATIVE_TS_BASE - (1LL << 48));
}

/**
 * Wrap a given time stamp, if there is an indication for an overflow
 *
 * @param st stream
 * @param timestamp the time stamp to wrap
 * @return resulting time stamp
 */
int64_t ff_wrap_timestamp(const AVStream *st, int64_t timestamp);

/**
 * Read a transport packet from a media file.
 *
 * @param s media file handle
 * @param pkt is filled
 * @return 0 if OK, AVERROR_xxx on error
 */
int ff_read_packet(AVFormatContext *s, AVPacket *pkt);

void ff_read_frame_flush(AVFormatContext *s);

/**
 * Perform a binary search using av_index_search_timestamp() and
 * AVInputFormat.read_timestamp().
 *
 * @param target_ts target timestamp in the time base of the given stream
 * @param stream_index stream number
 */
int ff_seek_frame_binary(AVFormatContext *s, int stream_index,
                         int64_t target_ts, int flags);

/**
 * Update cur_dts of all streams based on the given timestamp and AVStream.
 *
 * Stream ref_st unchanged, others set cur_dts in their native time base.
 * Only needed for timestamp wrapping or if (dts not set and pts!=dts).
 * @param timestamp new dts expressed in time_base of param ref_st
 * @param ref_st reference stream giving time_base of param timestamp
 */
void avpriv_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp);

int ff_find_last_ts(AVFormatContext *s, int stream_index, int64_t *ts, int64_t *pos,
                    int64_t (*read_timestamp)(struct AVFormatContext *, int , int64_t *, int64_t ));

/**
 * Perform a binary search using read_timestamp().
 *
 * @param target_ts target timestamp in the time base of the given stream
 * @param stream_index stream number
 */
int64_t ff_gen_search(AVFormatContext *s, int stream_index,
                      int64_t target_ts, int64_t pos_min,
                      int64_t pos_max, int64_t pos_limit,
                      int64_t ts_min, int64_t ts_max,
                      int flags, int64_t *ts_ret,
                      int64_t (*read_timestamp)(struct AVFormatContext *, int , int64_t *, int64_t ));

/**
 * Internal version of av_index_search_timestamp
 */
int ff_index_search_timestamp(const AVIndexEntry *entries, int nb_entries,
                              int64_t wanted_timestamp, int flags);

/**
 * Internal version of av_add_index_entry
 */
int ff_add_index_entry(AVIndexEntry **index_entries,
                       int *nb_index_entries,
                       unsigned int *index_entries_allocated_size,
                       int64_t pos, int64_t timestamp, int size, int distance, int flags);

void ff_configure_buffers_for_index(AVFormatContext *s, int64_t time_tolerance);

/**
 * Ensure the index uses less memory than the maximum specified in
 * AVFormatContext.max_index_size by discarding entries if it grows
 * too large.
 */
void ff_reduce_index(AVFormatContext *s, int stream_index);

/**
 * add frame for rfps calculation.
 *
 * @param dts timestamp of the i-th frame
 * @return 0 if OK, AVERROR_xxx on error
 */
int ff_rfps_add_frame(AVFormatContext *ic, AVStream *st, int64_t dts);

void ff_rfps_calculate(AVFormatContext *ic);

/**
 * Rescales a timestamp and the endpoints of an interval to which the temstamp
 * belongs, from a timebase `tb_in` to a timebase `tb_out`.
 *
 * The upper (lower) bound of the output interval is rounded up (down) such that
 * the output interval always falls within the intput interval. The timestamp is
 * rounded to the nearest integer and halfway cases away from zero, and can
 * therefore fall outside of the output interval.
 *
 * Useful to simplify the rescaling of the arguments of AVInputFormat::read_seek2()
 *
 * @param[in] tb_in      Timebase of the input `min_ts`, `ts` and `max_ts`
 * @param[in] tb_out     Timebase of the ouput `min_ts`, `ts` and `max_ts`
 * @param[in,out] min_ts Lower bound of the interval
 * @param[in,out] ts     Timestamp
 * @param[in,out] max_ts Upper bound of the interval
 */
void ff_rescale_interval(AVRational tb_in, AVRational tb_out,
                         int64_t *min_ts, int64_t *ts, int64_t *max_ts);

void avpriv_stream_set_need_parsing(AVStream *st, enum AVStreamParseType type);

/**
 * Add a new chapter.
 *
 * @param s media file handle
 * @param id unique ID for this chapter
 * @param start chapter start time in time_base units
 * @param end chapter end time in time_base units
 * @param title chapter title
 *
 * @return AVChapter or NULL on error
 */
AVChapter *avpriv_new_chapter(AVFormatContext *s, int64_t id, AVRational time_base,
                              int64_t start, int64_t end, const char *title);

/**
 * Add an attached pic to an AVStream.
 *
 * @param st   if set, the stream to add the attached pic to;
 *             if unset, a new stream will be added to s.
 * @param pb   AVIOContext to read data from if buf is unset.
 * @param buf  if set, it contains the data and size information to be used
 *             for the attached pic; if unset, data is read from pb.
 * @param size the size of the data to read if buf is unset.
 *
 * @return 0 on success, < 0 on error. On error, this function removes
 *         the stream it has added (if any).
 */
int ff_add_attached_pic(AVFormatContext *s, AVStream *st, AVIOContext *pb,
                        AVBufferRef **buf, int size);

/**
 * Add side data to a packet for changing parameters to the given values.
 * Parameters set to 0 aren't included in the change.
 */
int ff_add_param_change(AVPacket *pkt, int32_t channels,
                        uint64_t channel_layout, int32_t sample_rate,
                        int32_t width, int32_t height);

/**
 * Generate standard extradata for AVC-Intra based on width/height and field
 * order.
 */
int ff_generate_avci_extradata(AVStream *st);

/**
 * Allocate extradata with additional AV_INPUT_BUFFER_PADDING_SIZE at end
 * which is always set to 0 and fill it from pb.
 *
 * @param size size of extradata
 * @return >= 0 if OK, AVERROR_xxx on error
 */
int ff_get_extradata(void *logctx, AVCodecParameters *par, AVIOContext *pb, int size);

/**
 * Find stream index based on format-specific stream ID
 * @return stream index, or < 0 on error
 */
int ff_find_stream_index(const AVFormatContext *s, int id);

#endif /* AVFORMAT_DEMUX_H */
