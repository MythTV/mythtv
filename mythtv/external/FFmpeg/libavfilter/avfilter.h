/*
 * filter layer
 * Copyright (c) 2007 Bobby Bingham
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

#ifndef AVFILTER_AVFILTER_H
#define AVFILTER_AVFILTER_H

#include "libavutil/avutil.h"
#include "libavutil/log.h"
#include "libavutil/samplefmt.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"
#include "libavcodec/avcodec.h"


#ifndef FF_API_OLD_VSINK_API
#define FF_API_OLD_VSINK_API        (LIBAVFILTER_VERSION_MAJOR < 3)
#endif
#ifndef FF_API_OLD_ALL_FORMATS_API
#define FF_API_OLD_ALL_FORMATS_API (LIBAVFILTER_VERSION_MAJOR < 3)
#endif

#include <stddef.h>

#include "libavfilter/version.h"

/**
 * Return the LIBAVFILTER_VERSION_INT constant.
 */
unsigned avfilter_version(void);

/**
 * Return the libavfilter build-time configuration.
 */
const char *avfilter_configuration(void);

/**
 * Return the libavfilter license.
 */
const char *avfilter_license(void);


typedef struct AVFilterContext AVFilterContext;
typedef struct AVFilterLink    AVFilterLink;
typedef struct AVFilterPad     AVFilterPad;

/**
 * A reference-counted buffer data type used by the filter system. Filters
 * should not store pointers to this structure directly, but instead use the
 * AVFilterBufferRef structure below.
 */
typedef struct AVFilterBuffer {
    uint8_t *data[8];           ///< buffer data for each plane/channel
    int linesize[8];            ///< number of bytes per line

    unsigned refcount;          ///< number of references to this buffer

    /** private data to be used by a custom free function */
    void *priv;
    /**
     * A pointer to the function to deallocate this buffer if the default
     * function is not sufficient. This could, for example, add the memory
     * back into a memory pool to be reused later without the overhead of
     * reallocating it from scratch.
     */
    void (*free)(struct AVFilterBuffer *buf);

    int format;                 ///< media format
    int w, h;                   ///< width and height of the allocated buffer

    /**
     * pointers to the data planes/channels.
     *
     * For video, this should simply point to data[].
     *
     * For planar audio, each channel has a separate data pointer, and
     * linesize[0] contains the size of each channel buffer.
     * For packed audio, there is just one data pointer, and linesize[0]
     * contains the total size of the buffer for all channels.
     *
     * Note: Both data and extended_data will always be set, but for planar
     * audio with more channels that can fit in data, extended_data must be used
     * in order to access all channels.
     */
    uint8_t **extended_data;
} AVFilterBuffer;

#define AV_PERM_READ     0x01   ///< can read from the buffer
#define AV_PERM_WRITE    0x02   ///< can write to the buffer
#define AV_PERM_PRESERVE 0x04   ///< nobody else can overwrite the buffer
#define AV_PERM_REUSE    0x08   ///< can output the buffer multiple times, with the same contents each time
#define AV_PERM_REUSE2   0x10   ///< can output the buffer multiple times, modified each time
#define AV_PERM_NEG_LINESIZES 0x20  ///< the buffer requested can have negative linesizes
#define AV_PERM_ALIGN    0x40   ///< the buffer must be aligned

#define AVFILTER_ALIGN 16 //not part of ABI

/**
 * Audio specific properties in a reference to an AVFilterBuffer. Since
 * AVFilterBufferRef is common to different media formats, audio specific
 * per reference properties must be separated out.
 */
typedef struct AVFilterBufferRefAudioProps {
    uint64_t channel_layout;    ///< channel layout of audio buffer
    int nb_samples;             ///< number of audio samples per channel
    int sample_rate;            ///< audio buffer sample rate
#if FF_API_PACKING
    int planar;                 ///< audio buffer - planar or packed
#endif
} AVFilterBufferRefAudioProps;

/**
 * Video specific properties in a reference to an AVFilterBuffer. Since
 * AVFilterBufferRef is common to different media formats, video specific
 * per reference properties must be separated out.
 */
typedef struct AVFilterBufferRefVideoProps {
    int w;                      ///< image width
    int h;                      ///< image height
    AVRational sample_aspect_ratio; ///< sample aspect ratio
    int interlaced;             ///< is frame interlaced
    int top_field_first;        ///< field order
    enum AVPictureType pict_type; ///< picture type of the frame
    int key_frame;              ///< 1 -> keyframe, 0-> not
} AVFilterBufferRefVideoProps;

/**
 * A reference to an AVFilterBuffer. Since filters can manipulate the origin of
 * a buffer to, for example, crop image without any memcpy, the buffer origin
 * and dimensions are per-reference properties. Linesize is also useful for
 * image flipping, frame to field filters, etc, and so is also per-reference.
 *
 * TODO: add anything necessary for frame reordering
 */
typedef struct AVFilterBufferRef {
    AVFilterBuffer *buf;        ///< the buffer that this is a reference to
    uint8_t *data[8];           ///< picture/audio data for each plane
    int linesize[8];            ///< number of bytes per line
    int format;                 ///< media format

    /**
     * presentation timestamp. The time unit may change during
     * filtering, as it is specified in the link and the filter code
     * may need to rescale the PTS accordingly.
     */
    int64_t pts;
    int64_t pos;                ///< byte position in stream, -1 if unknown

    int perms;                  ///< permissions, see the AV_PERM_* flags

    enum AVMediaType type;      ///< media type of buffer data
    AVFilterBufferRefVideoProps *video; ///< video buffer specific properties
    AVFilterBufferRefAudioProps *audio; ///< audio buffer specific properties

    /**
     * pointers to the data planes/channels.
     *
     * For video, this should simply point to data[].
     *
     * For planar audio, each channel has a separate data pointer, and
     * linesize[0] contains the size of each channel buffer.
     * For packed audio, there is just one data pointer, and linesize[0]
     * contains the total size of the buffer for all channels.
     *
     * Note: Both data and extended_data will always be set, but for planar
     * audio with more channels that can fit in data, extended_data must be used
     * in order to access all channels.
     */
    uint8_t **extended_data;
} AVFilterBufferRef;

/**
 * Copy properties of src to dst, without copying the actual data
 */
void avfilter_copy_buffer_ref_props(AVFilterBufferRef *dst, AVFilterBufferRef *src);

/**
 * Add a new reference to a buffer.
 *
 * @param ref   an existing reference to the buffer
 * @param pmask a bitmask containing the allowable permissions in the new
 *              reference
 * @return      a new reference to the buffer with the same properties as the
 *              old, excluding any permissions denied by pmask
 */
AVFilterBufferRef *avfilter_ref_buffer(AVFilterBufferRef *ref, int pmask);

/**
 * Remove a reference to a buffer. If this is the last reference to the
 * buffer, the buffer itself is also automatically freed.
 *
 * @param ref reference to the buffer, may be NULL
 */
void avfilter_unref_buffer(AVFilterBufferRef *ref);

/**
 * Remove a reference to a buffer and set the pointer to NULL.
 * If this is the last reference to the buffer, the buffer itself
 * is also automatically freed.
 *
 * @param ref pointer to the buffer reference
 */
void avfilter_unref_bufferp(AVFilterBufferRef **ref);

/**
 * A list of supported formats for one end of a filter link. This is used
 * during the format negotiation process to try to pick the best format to
 * use to minimize the number of necessary conversions. Each filter gives a
 * list of the formats supported by each input and output pad. The list
 * given for each pad need not be distinct - they may be references to the
 * same list of formats, as is often the case when a filter supports multiple
 * formats, but will always output the same format as it is given in input.
 *
 * In this way, a list of possible input formats and a list of possible
 * output formats are associated with each link. When a set of formats is
 * negotiated over a link, the input and output lists are merged to form a
 * new list containing only the common elements of each list. In the case
 * that there were no common elements, a format conversion is necessary.
 * Otherwise, the lists are merged, and all other links which reference
 * either of the format lists involved in the merge are also affected.
 *
 * For example, consider the filter chain:
 * filter (a) --> (b) filter (b) --> (c) filter
 *
 * where the letters in parenthesis indicate a list of formats supported on
 * the input or output of the link. Suppose the lists are as follows:
 * (a) = {A, B}
 * (b) = {A, B, C}
 * (c) = {B, C}
 *
 * First, the first link's lists are merged, yielding:
 * filter (a) --> (a) filter (a) --> (c) filter
 *
 * Notice that format list (b) now refers to the same list as filter list (a).
 * Next, the lists for the second link are merged, yielding:
 * filter (a) --> (a) filter (a) --> (a) filter
 *
 * where (a) = {B}.
 *
 * Unfortunately, when the format lists at the two ends of a link are merged,
 * we must ensure that all links which reference either pre-merge format list
 * get updated as well. Therefore, we have the format list structure store a
 * pointer to each of the pointers to itself.
 */
typedef struct AVFilterFormats {
    unsigned format_count;      ///< number of formats
    int *formats;               ///< list of media formats

    unsigned refcount;          ///< number of references to this list
    struct AVFilterFormats ***refs; ///< references to this list
}  AVFilterFormats;

/**
 * Create a list of supported formats. This is intended for use in
 * AVFilter->query_formats().
 *
 * @param fmts list of media formats, terminated by -1. If NULL an
 *        empty list is created.
 * @return the format list, with no existing references
 */
AVFilterFormats *avfilter_make_format_list(const int *fmts);

/**
 * Add fmt to the list of media formats contained in *avff.
 * If *avff is NULL the function allocates the filter formats struct
 * and puts its pointer in *avff.
 *
 * @return a non negative value in case of success, or a negative
 * value corresponding to an AVERROR code in case of error
 */
int avfilter_add_format(AVFilterFormats **avff, int64_t fmt);

#if FF_API_OLD_ALL_FORMATS_API
/**
 * @deprecated Use avfilter_make_all_formats() instead.
 */
attribute_deprecated
AVFilterFormats *avfilter_all_formats(enum AVMediaType type);
#endif

/**
 * Return a list of all formats supported by FFmpeg for the given media type.
 */
AVFilterFormats *avfilter_make_all_formats(enum AVMediaType type);

/**
 * A list of all channel layouts supported by libavfilter.
 */
extern const int64_t avfilter_all_channel_layouts[];

#if FF_API_PACKING
/**
 * Return a list of all audio packing formats.
 */
AVFilterFormats *avfilter_make_all_packing_formats(void);
#endif

/**
 * Return a format list which contains the intersection of the formats of
 * a and b. Also, all the references of a, all the references of b, and
 * a and b themselves will be deallocated.
 *
 * If a and b do not share any common formats, neither is modified, and NULL
 * is returned.
 */
AVFilterFormats *avfilter_merge_formats(AVFilterFormats *a, AVFilterFormats *b);

/**
 * Add *ref as a new reference to formats.
 * That is the pointers will point like in the ASCII art below:
 *   ________
 *  |formats |<--------.
 *  |  ____  |     ____|___________________
 *  | |refs| |    |  __|_
 *  | |* * | |    | |  | |  AVFilterLink
 *  | |* *--------->|*ref|
 *  | |____| |    | |____|
 *  |________|    |________________________
 */
void avfilter_formats_ref(AVFilterFormats *formats, AVFilterFormats **ref);

/**
 * If *ref is non-NULL, remove *ref as a reference to the format list
 * it currently points to, deallocates that list if this was the last
 * reference, and sets *ref to NULL.
 *
 *         Before                                 After
 *   ________                               ________         NULL
 *  |formats |<--------.                   |formats |         ^
 *  |  ____  |     ____|________________   |  ____  |     ____|________________
 *  | |refs| |    |  __|_                  | |refs| |    |  __|_
 *  | |* * | |    | |  | |  AVFilterLink   | |* * | |    | |  | |  AVFilterLink
 *  | |* *--------->|*ref|                 | |*   | |    | |*ref|
 *  | |____| |    | |____|                 | |____| |    | |____|
 *  |________|    |_____________________   |________|    |_____________________
 */
void avfilter_formats_unref(AVFilterFormats **ref);

/**
 *
 *         Before                                 After
 *   ________                         ________
 *  |formats |<---------.            |formats |<---------.
 *  |  ____  |       ___|___         |  ____  |       ___|___
 *  | |refs| |      |   |   |        | |refs| |      |   |   |   NULL
 *  | |* *--------->|*oldref|        | |* *--------->|*newref|     ^
 *  | |* * | |      |_______|        | |* * | |      |_______|  ___|___
 *  | |____| |                       | |____| |                |   |   |
 *  |________|                       |________|                |*oldref|
 *                                                             |_______|
 */
void avfilter_formats_changeref(AVFilterFormats **oldref,
                                AVFilterFormats **newref);

/**
 * A filter pad used for either input or output.
 *
 * See doc/filter_design.txt for details on how to implement the methods.
 */
struct AVFilterPad {
    /**
     * Pad name. The name is unique among inputs and among outputs, but an
     * input may have the same name as an output. This may be NULL if this
     * pad has no need to ever be referenced by name.
     */
    const char *name;

    /**
     * AVFilterPad type.
     */
    enum AVMediaType type;

    /**
     * Minimum required permissions on incoming buffers. Any buffer with
     * insufficient permissions will be automatically copied by the filter
     * system to a new buffer which provides the needed access permissions.
     *
     * Input pads only.
     */
    int min_perms;

    /**
     * Permissions which are not accepted on incoming buffers. Any buffer
     * which has any of these permissions set will be automatically copied
     * by the filter system to a new buffer which does not have those
     * permissions. This can be used to easily disallow buffers with
     * AV_PERM_REUSE.
     *
     * Input pads only.
     */
    int rej_perms;

    /**
     * Callback called before passing the first slice of a new frame. If
     * NULL, the filter layer will default to storing a reference to the
     * picture inside the link structure.
     *
     * Input video pads only.
     */
    void (*start_frame)(AVFilterLink *link, AVFilterBufferRef *picref);

    /**
     * Callback function to get a video buffer. If NULL, the filter system will
     * use avfilter_default_get_video_buffer().
     *
     * Input video pads only.
     */
    AVFilterBufferRef *(*get_video_buffer)(AVFilterLink *link, int perms, int w, int h);

    /**
     * Callback function to get an audio buffer. If NULL, the filter system will
     * use avfilter_default_get_audio_buffer().
     *
     * Input audio pads only.
     */
    AVFilterBufferRef *(*get_audio_buffer)(AVFilterLink *link, int perms,
                                           int nb_samples);

    /**
     * Callback called after the slices of a frame are completely sent. If
     * NULL, the filter layer will default to releasing the reference stored
     * in the link structure during start_frame().
     *
     * Input video pads only.
     */
    void (*end_frame)(AVFilterLink *link);

    /**
     * Slice drawing callback. This is where a filter receives video data
     * and should do its processing.
     *
     * Input video pads only.
     */
    void (*draw_slice)(AVFilterLink *link, int y, int height, int slice_dir);

    /**
     * Samples filtering callback. This is where a filter receives audio data
     * and should do its processing.
     *
     * Input audio pads only.
     */
    void (*filter_samples)(AVFilterLink *link, AVFilterBufferRef *samplesref);

    /**
     * Frame poll callback. This returns the number of immediately available
     * samples. It should return a positive value if the next request_frame()
     * is guaranteed to return one frame (with no delay).
     *
     * Defaults to just calling the source poll_frame() method.
     *
     * Output pads only.
     */
    int (*poll_frame)(AVFilterLink *link);

    /**
     * Frame request callback. A call to this should result in at least one
     * frame being output over the given link. This should return zero on
     * success, and another value on error.
     * See avfilter_request_frame() for the error codes with a specific
     * meaning.
     *
     * Output pads only.
     */
    int (*request_frame)(AVFilterLink *link);

    /**
     * Link configuration callback.
     *
     * For output pads, this should set the following link properties:
     * video: width, height, sample_aspect_ratio, time_base
     * audio: sample_rate.
     *
     * This should NOT set properties such as format, channel_layout, etc which
     * are negotiated between filters by the filter system using the
     * query_formats() callback before this function is called.
     *
     * For input pads, this should check the properties of the link, and update
     * the filter's internal state as necessary.
     *
     * For both input and output pads, this should return zero on success,
     * and another value on error.
     */
    int (*config_props)(AVFilterLink *link);
};

#if FF_API_FILTERS_PUBLIC
/** default handler for start_frame() for video inputs */
attribute_deprecated
void avfilter_default_start_frame(AVFilterLink *link, AVFilterBufferRef *picref);

/** default handler for draw_slice() for video inputs */
attribute_deprecated
void avfilter_default_draw_slice(AVFilterLink *link, int y, int h, int slice_dir);

/** default handler for end_frame() for video inputs */
attribute_deprecated
void avfilter_default_end_frame(AVFilterLink *link);

/** default handler for get_video_buffer() for video inputs */
attribute_deprecated
AVFilterBufferRef *avfilter_default_get_video_buffer(AVFilterLink *link,
                                                     int perms, int w, int h);

/** Default handler for query_formats() */
attribute_deprecated
int avfilter_default_query_formats(AVFilterContext *ctx);
#endif

/**
 * Helpers for query_formats() which set all links to the same list of
 * formats/layouts. If there are no links hooked to this filter, the list
 * of formats is freed.
 */
void avfilter_set_common_formats(AVFilterContext *ctx, AVFilterFormats *formats);
void avfilter_set_common_pixel_formats(AVFilterContext *ctx, AVFilterFormats *formats);
void avfilter_set_common_sample_formats(AVFilterContext *ctx, AVFilterFormats *formats);
void avfilter_set_common_channel_layouts(AVFilterContext *ctx, AVFilterFormats *formats);
#if FF_API_PACKING
void avfilter_set_common_packing_formats(AVFilterContext *ctx, AVFilterFormats *formats);
#endif

#if FF_API_FILTERS_PUBLIC
/** start_frame() handler for filters which simply pass video along */
attribute_deprecated
void avfilter_null_start_frame(AVFilterLink *link, AVFilterBufferRef *picref);

/** draw_slice() handler for filters which simply pass video along */
attribute_deprecated
void avfilter_null_draw_slice(AVFilterLink *link, int y, int h, int slice_dir);

/** end_frame() handler for filters which simply pass video along */
attribute_deprecated
void avfilter_null_end_frame(AVFilterLink *link);

/** get_video_buffer() handler for filters which simply pass video along */
attribute_deprecated
AVFilterBufferRef *avfilter_null_get_video_buffer(AVFilterLink *link,
                                                  int perms, int w, int h);
#endif

/**
 * Filter definition. This defines the pads a filter contains, and all the
 * callback functions used to interact with the filter.
 */
typedef struct AVFilter {
    const char *name;         ///< filter name

    int priv_size;      ///< size of private data to allocate for the filter

    /**
     * Filter initialization function. Args contains the user-supplied
     * parameters. FIXME: maybe an AVOption-based system would be better?
     * opaque is data provided by the code requesting creation of the filter,
     * and is used to pass data to the filter.
     */
    int (*init)(AVFilterContext *ctx, const char *args, void *opaque);

    /**
     * Filter uninitialization function. Should deallocate any memory held
     * by the filter, release any buffer references, etc. This does not need
     * to deallocate the AVFilterContext->priv memory itself.
     */
    void (*uninit)(AVFilterContext *ctx);

    /**
     * Queries formats/layouts supported by the filter and its pads, and sets
     * the in_formats/in_chlayouts for links connected to its output pads,
     * and out_formats/out_chlayouts for links connected to its input pads.
     *
     * @return zero on success, a negative value corresponding to an
     * AVERROR code otherwise
     */
    int (*query_formats)(AVFilterContext *);

    const AVFilterPad *inputs;  ///< NULL terminated list of inputs. NULL if none
    const AVFilterPad *outputs; ///< NULL terminated list of outputs. NULL if none

    /**
     * A description for the filter. You should use the
     * NULL_IF_CONFIG_SMALL() macro to define it.
     */
    const char *description;

    /**
     * Make the filter instance process a command.
     *
     * @param cmd    the command to process, for handling simplicity all commands must be alphanumeric only
     * @param arg    the argument for the command
     * @param res    a buffer with size res_size where the filter(s) can return a response. This must not change when the command is not supported.
     * @param flags  if AVFILTER_CMD_FLAG_FAST is set and the command would be
     *               time consuming then a filter should treat it like an unsupported command
     *
     * @returns >=0 on success otherwise an error code.
     *          AVERROR(ENOSYS) on unsupported commands
     */
    int (*process_command)(AVFilterContext *, const char *cmd, const char *arg, char *res, int res_len, int flags);
} AVFilter;

/** An instance of a filter */
struct AVFilterContext {
    const AVClass *av_class;        ///< needed for av_log()

    AVFilter *filter;               ///< the AVFilter of which this is an instance

    char *name;                     ///< name of this filter instance

    unsigned input_count;           ///< number of input pads
    AVFilterPad   *input_pads;      ///< array of input pads
    AVFilterLink **inputs;          ///< array of pointers to input links

    unsigned output_count;          ///< number of output pads
    AVFilterPad   *output_pads;     ///< array of output pads
    AVFilterLink **outputs;         ///< array of pointers to output links

    void *priv;                     ///< private data for use by the filter

    struct AVFilterCommand *command_queue;
};

#if FF_API_PACKING
enum AVFilterPacking {
    AVFILTER_PACKED = 0,
    AVFILTER_PLANAR,
};
#endif

/**
 * A link between two filters. This contains pointers to the source and
 * destination filters between which this link exists, and the indexes of
 * the pads involved. In addition, this link also contains the parameters
 * which have been negotiated and agreed upon between the filter, such as
 * image dimensions, format, etc.
 */
struct AVFilterLink {
    AVFilterContext *src;       ///< source filter
    AVFilterPad *srcpad;        ///< output pad on the source filter

    AVFilterContext *dst;       ///< dest filter
    AVFilterPad *dstpad;        ///< input pad on the dest filter

    /** stage of the initialization of the link properties (dimensions, etc) */
    enum {
        AVLINK_UNINIT = 0,      ///< not started
        AVLINK_STARTINIT,       ///< started, but incomplete
        AVLINK_INIT             ///< complete
    } init_state;

    enum AVMediaType type;      ///< filter media type

    /* These parameters apply only to video */
    int w;                      ///< agreed upon image width
    int h;                      ///< agreed upon image height
    AVRational sample_aspect_ratio; ///< agreed upon sample aspect ratio
    /* These parameters apply only to audio */
    uint64_t channel_layout;    ///< channel layout of current buffer (see libavutil/audioconvert.h)
#if FF_API_SAMPLERATE64
    int64_t sample_rate;        ///< samples per second
#else
    int sample_rate;            ///< samples per second
#endif
#if FF_API_PACKING
    int planar;                 ///< agreed upon packing mode of audio buffers. true if planar.
#endif

    int format;                 ///< agreed upon media format

    /**
     * Lists of formats and channel layouts supported by the input and output
     * filters respectively. These lists are used for negotiating the format
     * to actually be used, which will be loaded into the format and
     * channel_layout members, above, when chosen.
     *
     */
    AVFilterFormats *in_formats;
    AVFilterFormats *out_formats;

#if FF_API_PACKING
    AVFilterFormats *in_packing;
    AVFilterFormats *out_packing;
#endif

    /**
     * The buffer reference currently being sent across the link by the source
     * filter. This is used internally by the filter system to allow
     * automatic copying of buffers which do not have sufficient permissions
     * for the destination. This should not be accessed directly by the
     * filters.
     */
    AVFilterBufferRef *src_buf;

    AVFilterBufferRef *cur_buf;
    AVFilterBufferRef *out_buf;

    /**
     * Define the time base used by the PTS of the frames/samples
     * which will pass through this link.
     * During the configuration stage, each filter is supposed to
     * change only the output timebase, while the timebase of the
     * input link is assumed to be an unchangeable property.
     */
    AVRational time_base;

    /*****************************************************************
     * All fields below this line are not part of the public API. They
     * may not be used outside of libavfilter and can be changed and
     * removed at will.
     * New public fields should be added right above.
     *****************************************************************
     */
    /**
     * Lists of channel layouts and sample rates used for automatic
     * negotiation.
     */
    AVFilterFormats  *in_samplerates;
    AVFilterFormats *out_samplerates;
    struct AVFilterChannelLayouts  *in_channel_layouts;
    struct AVFilterChannelLayouts *out_channel_layouts;

    struct AVFilterPool *pool;

    /**
     * Graph the filter belongs to.
     */
    struct AVFilterGraph *graph;

    /**
     * Current timestamp of the link, as defined by the most recent
     * frame(s), in AV_TIME_BASE units.
     */
    int64_t current_pts;

    /**
     * Index in the age array.
     */
    int age_index;

};

/**
 * Link two filters together.
 *
 * @param src    the source filter
 * @param srcpad index of the output pad on the source filter
 * @param dst    the destination filter
 * @param dstpad index of the input pad on the destination filter
 * @return       zero on success
 */
int avfilter_link(AVFilterContext *src, unsigned srcpad,
                  AVFilterContext *dst, unsigned dstpad);

/**
 * Free the link in *link, and set its pointer to NULL.
 */
void avfilter_link_free(AVFilterLink **link);

/**
 * Negotiate the media format, dimensions, etc of all inputs to a filter.
 *
 * @param filter the filter to negotiate the properties for its inputs
 * @return       zero on successful negotiation
 */
int avfilter_config_links(AVFilterContext *filter);

/**
 * Request a picture buffer with a specific set of permissions.
 *
 * @param link  the output link to the filter from which the buffer will
 *              be requested
 * @param perms the required access permissions
 * @param w     the minimum width of the buffer to allocate
 * @param h     the minimum height of the buffer to allocate
 * @return      A reference to the buffer. This must be unreferenced with
 *              avfilter_unref_buffer when you are finished with it.
 */
AVFilterBufferRef *avfilter_get_video_buffer(AVFilterLink *link, int perms,
                                          int w, int h);

/**
 * Create a buffer reference wrapped around an already allocated image
 * buffer.
 *
 * @param data pointers to the planes of the image to reference
 * @param linesize linesizes for the planes of the image to reference
 * @param perms the required access permissions
 * @param w the width of the image specified by the data and linesize arrays
 * @param h the height of the image specified by the data and linesize arrays
 * @param format the pixel format of the image specified by the data and linesize arrays
 */
AVFilterBufferRef *
avfilter_get_video_buffer_ref_from_arrays(uint8_t * const data[4], const int linesize[4], int perms,
                                          int w, int h, enum PixelFormat format);

/**
 * Create an audio buffer reference wrapped around an already
 * allocated samples buffer.
 *
 * @param data           pointers to the samples plane buffers
 * @param linesize       linesize for the samples plane buffers
 * @param perms          the required access permissions
 * @param nb_samples     number of samples per channel
 * @param sample_fmt     the format of each sample in the buffer to allocate
 * @param channel_layout the channel layout of the buffer
 */
AVFilterBufferRef *avfilter_get_audio_buffer_ref_from_arrays(uint8_t **data,
                                                             int linesize,
                                                             int perms,
                                                             int nb_samples,
                                                             enum AVSampleFormat sample_fmt,
                                                             uint64_t channel_layout);

/**
 * Request an input frame from the filter at the other end of the link.
 *
 * @param link the input link
 * @return     zero on success or a negative error code; in particular:
 *             AVERROR_EOF means that the end of frames have been reached;
 *             AVERROR(EAGAIN) means that no frame could be immediately
 *             produced.
 */
int avfilter_request_frame(AVFilterLink *link);

/**
 * Poll a frame from the filter chain.
 *
 * @param  link the input link
 * @return the number of immediately available frames, a negative
 * number in case of error
 */
int avfilter_poll_frame(AVFilterLink *link);

/**
 * Notify the next filter of the start of a frame.
 *
 * @param link   the output link the frame will be sent over
 * @param picref A reference to the frame about to be sent. The data for this
 *               frame need only be valid once draw_slice() is called for that
 *               portion. The receiving filter will free this reference when
 *               it no longer needs it.
 */
void avfilter_start_frame(AVFilterLink *link, AVFilterBufferRef *picref);

/**
 * Notify the next filter that the current frame has finished.
 *
 * @param link the output link the frame was sent over
 */
void avfilter_end_frame(AVFilterLink *link);

/**
 * Send a slice to the next filter.
 *
 * Slices have to be provided in sequential order, either in
 * top-bottom or bottom-top order. If slices are provided in
 * non-sequential order the behavior of the function is undefined.
 *
 * @param link the output link over which the frame is being sent
 * @param y    offset in pixels from the top of the image for this slice
 * @param h    height of this slice in pixels
 * @param slice_dir the assumed direction for sending slices,
 *             from the top slice to the bottom slice if the value is 1,
 *             from the bottom slice to the top slice if the value is -1,
 *             for other values the behavior of the function is undefined.
 */
void avfilter_draw_slice(AVFilterLink *link, int y, int h, int slice_dir);

#define AVFILTER_CMD_FLAG_ONE   1 ///< Stop once a filter understood the command (for target=all for example), fast filters are favored automatically
#define AVFILTER_CMD_FLAG_FAST  2 ///< Only execute command when its fast (like a video out that supports contrast adjustment in hw)

/**
 * Make the filter instance process a command.
 * It is recommended to use avfilter_graph_send_command().
 */
int avfilter_process_command(AVFilterContext *filter, const char *cmd, const char *arg, char *res, int res_len, int flags);

/** Initialize the filter system. Register all builtin filters. */
void avfilter_register_all(void);

/** Uninitialize the filter system. Unregister all filters. */
void avfilter_uninit(void);

/**
 * Register a filter. This is only needed if you plan to use
 * avfilter_get_by_name later to lookup the AVFilter structure by name. A
 * filter can still by instantiated with avfilter_open even if it is not
 * registered.
 *
 * @param filter the filter to register
 * @return 0 if the registration was successful, a negative value
 * otherwise
 */
int avfilter_register(AVFilter *filter);

/**
 * Get a filter definition matching the given name.
 *
 * @param name the filter name to find
 * @return     the filter definition, if any matching one is registered.
 *             NULL if none found.
 */
AVFilter *avfilter_get_by_name(const char *name);

/**
 * If filter is NULL, returns a pointer to the first registered filter pointer,
 * if filter is non-NULL, returns the next pointer after filter.
 * If the returned pointer points to NULL, the last registered filter
 * was already reached.
 */
AVFilter **av_filter_next(AVFilter **filter);

/**
 * Create a filter instance.
 *
 * @param filter_ctx put here a pointer to the created filter context
 * on success, NULL on failure
 * @param filter    the filter to create an instance of
 * @param inst_name Name to give to the new instance. Can be NULL for none.
 * @return >= 0 in case of success, a negative error code otherwise
 */
int avfilter_open(AVFilterContext **filter_ctx, AVFilter *filter, const char *inst_name);

/**
 * Initialize a filter.
 *
 * @param filter the filter to initialize
 * @param args   A string of parameters to use when initializing the filter.
 *               The format and meaning of this string varies by filter.
 * @param opaque Any extra non-string data needed by the filter. The meaning
 *               of this parameter varies by filter.
 * @return       zero on success
 */
int avfilter_init_filter(AVFilterContext *filter, const char *args, void *opaque);

/**
 * Free a filter context.
 *
 * @param filter the filter to free
 */
void avfilter_free(AVFilterContext *filter);

/**
 * Insert a filter in the middle of an existing link.
 *
 * @param link the link into which the filter should be inserted
 * @param filt the filter to be inserted
 * @param filt_srcpad_idx the input pad on the filter to connect
 * @param filt_dstpad_idx the output pad on the filter to connect
 * @return     zero on success
 */
int avfilter_insert_filter(AVFilterLink *link, AVFilterContext *filt,
                           unsigned filt_srcpad_idx, unsigned filt_dstpad_idx);

/**
 * Insert a new pad.
 *
 * @param idx Insertion point. Pad is inserted at the end if this point
 *            is beyond the end of the list of pads.
 * @param count Pointer to the number of pads in the list
 * @param padidx_off Offset within an AVFilterLink structure to the element
 *                   to increment when inserting a new pad causes link
 *                   numbering to change
 * @param pads Pointer to the pointer to the beginning of the list of pads
 * @param links Pointer to the pointer to the beginning of the list of links
 * @param newpad The new pad to add. A copy is made when adding.
 */
void avfilter_insert_pad(unsigned idx, unsigned *count, size_t padidx_off,
                         AVFilterPad **pads, AVFilterLink ***links,
                         AVFilterPad *newpad);

/** Insert a new input pad for the filter. */
static inline void avfilter_insert_inpad(AVFilterContext *f, unsigned index,
                                         AVFilterPad *p)
{
    avfilter_insert_pad(index, &f->input_count, offsetof(AVFilterLink, dstpad),
                        &f->input_pads, &f->inputs, p);
}

/** Insert a new output pad for the filter. */
static inline void avfilter_insert_outpad(AVFilterContext *f, unsigned index,
                                          AVFilterPad *p)
{
    avfilter_insert_pad(index, &f->output_count, offsetof(AVFilterLink, srcpad),
                        &f->output_pads, &f->outputs, p);
}

#endif /* AVFILTER_AVFILTER_H */
