/*
 * Quick DNR 0.8
 * (C)opyright 2003, Debabrata Banerjee
 * GNU GPL 2 or later
 *
 * Pass options as:
 * quickdnr=quality (0-255 scale adjusted)
 * quickdnr=Luma_threshold:Chroma_threshold (0-255) for single threshold
 * quickdnr=Luma_threshold1:Luma_threshold2:Chroma_threshold1:Chroma_threshold2 for double
 *
 */

#include <stdio.h>

#include "config.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "filter.h"
#include "frame.h"
#include "libavutil/mem.h"
#include "libavcodec/dsputil.h"

#ifdef MMX
#include "ffmpeg-mmx.h"
#endif

//Regular filter
#define LUMA_THRESHOLD_DEFAULT 15
#define CHROMA_THRESHOLD_DEFAULT 25

//Double thresholded filter
#define LUMA_THRESHOLD1_DEFAULT 10
#define LUMA_THRESHOLD2_DEFAULT 1
#define CHROMA_THRESHOLD1_DEFAULT 20
#define CHROMA_THRESHOLD2_DEFAULT 2

//#define QUICKDNR_DEBUG

//static const char FILTER_NAME[] = "quickdnr";

typedef struct ThisFilter
{
        VideoFilter vf;

        uint64_t Luma_threshold_mask1;
        uint64_t Luma_threshold_mask2;
        uint64_t Chroma_threshold_mask1;
        uint64_t Chroma_threshold_mask2;
        uint8_t  Luma_threshold1;
        uint8_t  Luma_threshold2;
        uint8_t  Chroma_threshold1;
        uint8_t  Chroma_threshold2;
        uint8_t *average;
        int      average_size;
        int      offsets[3];
        int      pitches[3];

        TF_STRUCT;

} ThisFilter;

static int alloc_avg(ThisFilter *filter, int size)
{
    if (filter->average_size >= size)
        return 1;

    uint8_t *tmp = realloc(filter->average, size);
    if (!tmp)
    {
        fprintf(stderr, "Couldn't allocate memory for DNR buffer\n");
        return 0;
    }

    filter->average = tmp;
    filter->average_size = size;

    return 1;
}

static int init_avg(ThisFilter *filter, VideoFrame *frame)
{
    if (!alloc_avg(filter, frame->size))
        return 0;

    if ((filter->offsets[0] != frame->offsets[0]) ||
        (filter->offsets[1] != frame->offsets[1]) ||
        (filter->offsets[2] != frame->offsets[2]) ||
        (filter->pitches[0] != frame->pitches[0]) ||
        (filter->pitches[1] != frame->pitches[1]) ||
        (filter->pitches[2] != frame->pitches[2]))
    {
        memcpy(filter->average, frame->buf, frame->size);
        memcpy(filter->offsets, frame->offsets, sizeof(int) * 3);
        memcpy(filter->pitches, frame->pitches, sizeof(int) * 3);
    }

    return 1;
}

static void init_vars(ThisFilter *tf, VideoFrame *frame,
                      int *thr1, int *thr2, int *height,
                      uint8_t **avg, uint8_t **buf)
{
    thr1[0] = tf->Luma_threshold1;
    thr1[1] = tf->Chroma_threshold1;
    thr1[2] = tf->Chroma_threshold1;

    thr2[0] = tf->Luma_threshold2;
    thr2[1] = tf->Chroma_threshold2;
    thr2[2] = tf->Chroma_threshold2;

    height[0] = frame->height;
    height[1] = frame->height >> 1;
    height[2] = frame->height >> 1;

    avg[0] = tf->average + frame->offsets[0];
    avg[1] = tf->average + frame->offsets[1];
    avg[2] = tf->average + frame->offsets[2];

    buf[0] = frame->buf + frame->offsets[0];
    buf[1] = frame->buf + frame->offsets[1];
    buf[2] = frame->buf + frame->offsets[2];
}

static int quickdnr(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *tf = (ThisFilter *)f;
    int thr1[3], thr2[3], height[3];
    uint8_t *avg[3], *buf[3];
    int i, y;

    TF_VARS;

    TF_START;

    if (!init_avg(tf, frame))
        return 0;

    init_vars(tf, frame, thr1, thr2, height, avg, buf);

    for (i = 0; i < 3; i++)
    {
        int sz = height[i] * frame->pitches[i];
        for (y = 0; y < sz; y++)
        {
            if (abs(avg[i][y] - buf[i][y]) < thr1[i])
                buf[i][y] = avg[i][y] = (avg[i][y] + buf[i][y]) >> 1;
            else
                avg[i][y] = buf[i][y];
        }
    }

    TF_END(tf, "QuickDNR: ");

    return 0;
}

static int quickdnr2(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *tf = (ThisFilter *)f;
    int thr1[3], thr2[3], height[3];
    uint8_t *avg[3], *buf[3];
    int i, y;

    TF_VARS;

    TF_START;

    if (!init_avg(tf, frame))
        return 0;

    init_vars(tf, frame, thr1, thr2, height, avg, buf);

    for (i = 0; i < 3; i++)
    {
        int sz = height[i] * frame->pitches[i];
        for (y = 0; y < sz; y++)
        {
            int t = abs(avg[i][y] - buf[i][y]);
            if (t < thr1[i])
            {
                if (t > thr2[i])
                    avg[i][y] = (avg[i][y] + buf[i][y]) >> 1;
                buf[i][y] = avg[i][y];
            }
            else
            {
                avg[i][y] = buf[i][y];
            }
        }
    }

    TF_END(tf, "QuickDNR2: ");

    return 0;
}

#ifdef MMX

static int quickdnrMMX(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *tf = (ThisFilter *)f;
    const uint64_t sign_convert = 0x8080808080808080LL;
    int thr1[3], thr2[3], height[3];
    uint64_t *avg[3], *buf[3];
    int i, y;

    TF_VARS;

    TF_START;

    if (!init_avg(tf, frame))
        return 0;

    init_vars(tf, frame, thr1, thr2, height, (uint8_t**) avg, (uint8_t**) buf);

    /*
      Removed all the prefetches. These don't do anything when
      you are processing an array with sequential accesses because the
      processor automatically does a prefetchT0 in these cases. The
      instruction is meant to be used to specify a different prefetch
      cache level, or to prefetch non-sequental data.

      These prefetches are not available on all MMX processors so if
      we wanted to use them we would need to test for a prefetch
      capable processor before using them. -- dtk
    */

    __asm__ volatile("emms\n\t");

    __asm__ volatile("movq (%0), %%mm4" : : "r" (&sign_convert));

    for (i = 0; i < 3; i++)
    {
        int sz = (height[i] * frame->pitches[i]) >> 3;

        if (0 == i)
            __asm__ volatile("movq (%0), %%mm5" : : "r" (&tf->Luma_threshold_mask1));
        else
            __asm__ volatile("movq (%0), %%mm5" : : "r" (&tf->Chroma_threshold_mask1));

        for (y = 0; y < sz; y++)
        {
            __asm__ volatile(
            "movq (%0), %%mm0     \n\t" // avg[i]
            "movq (%1), %%mm1     \n\t" // buf[i]
            "movq %%mm0, %%mm2    \n\t"
            "movq %%mm1, %%mm3    \n\t"
            "movq %%mm1, %%mm7    \n\t"

            "pcmpgtb %%mm0, %%mm1 \n\t" // 1 if av greater
            "psubb %%mm0, %%mm3   \n\t" // mm3=buf-av
            "psubb %%mm7, %%mm0   \n\t" // mm0=av-buf
            "pand %%mm1, %%mm3    \n\t" // select buf
            "pandn %%mm0,%%mm1    \n\t" // select av
            "por %%mm1, %%mm3     \n\t" // mm3=abs()

            "paddb %%mm4, %%mm3   \n\t" // hack! No proper unsigned mmx compares!
            "pcmpgtb %%mm5, %%mm3 \n\t" // compare buf with mask

            "pavgb %%mm7, %%mm2   \n\t"
            "pand %%mm3, %%mm7    \n\t"
            "pandn %%mm2,%%mm3    \n\t"
            "por %%mm7, %%mm3     \n\t"
            "movq %%mm3, (%0)     \n\t"
            "movq %%mm3, (%1)     \n\t"
            : : "r" (avg[i]), "r" (buf[i])
            );
            buf[i]++;
            avg[i]++;
        }
    }

    __asm__ volatile("emms\n\t");

    // filter the leftovers from the mmx rutine
    for (i = 0; i < 3; i++)
    {
        int thr1[3], thr2[3], height[3];
        uint8_t *avg8[3], *buf8[3];
        int end, beg;

        init_vars(tf, frame, thr1, thr2, height, avg8, buf8);

        end = height[i] * frame->pitches[i];
        beg = end & ~0x7;

        if (beg == end)
            continue;

        for (y = beg; y < end; y++)
        {
            if (abs(avg8[i][y] - buf8[i][y]) < thr1[i])
                buf8[i][y] = avg8[i][y] = (avg8[i][y] + buf8[i][y]) >> 1;
            else
                avg8[i][y] = buf8[i][y];
        }
    }

    TF_END(tf, "QuickDNRmmx: ");

    return 0;
}


static int quickdnr2MMX(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *tf = (ThisFilter *)f;
    const uint64_t sign_convert = 0x8080808080808080LL;
    int thr1[3], thr2[3], height[3];
    uint64_t *avg[3], *buf[3];
    int i, y;

    TF_VARS;

    TF_START;

    if (!init_avg(tf, frame))
        return 0;

    init_vars(tf, frame, thr1, thr2, height, (uint8_t**) avg, (uint8_t**) buf);

    __asm__ volatile("emms\n\t");

    __asm__ volatile("movq (%0), %%mm4" : : "r" (&sign_convert));

    for (i = 0; i < 3; i++)
    {
        int sz = (height[i] * frame->pitches[i]) >> 3;

        if (0 == i)
            __asm__ volatile("movq (%0), %%mm5" : : "r" (&tf->Luma_threshold_mask1));
        else
            __asm__ volatile("movq (%0), %%mm5" : : "r" (&tf->Chroma_threshold_mask1));

        for (y = 0; y < sz; y++)
        {
            uint64_t *mask2 = (0 == i) ?
                &tf->Luma_threshold_mask2 : &tf->Chroma_threshold_mask2;

            __asm__ volatile(
                "movq (%0), %%mm0     \n\t" // avg[i]
                "movq (%1), %%mm1     \n\t" // buf[i]
                "movq %%mm0, %%mm2    \n\t"
                "movq %%mm1, %%mm3    \n\t"
                "movq %%mm1, %%mm6    \n\t"
                "movq %%mm1, %%mm7    \n\t"

                "pcmpgtb %%mm0, %%mm1 \n\t" // 1 if av greater
                "psubb %%mm0, %%mm3   \n\t" // mm3=buf-av
                "psubb %%mm7, %%mm0   \n\t" // mm0=av-buf
                "pand %%mm1, %%mm3    \n\t" // select buf
                "pandn %%mm0,%%mm1    \n\t" // select av
                "por %%mm1, %%mm3     \n\t" // mm3=abs(buf-av)

                "paddb %%mm4, %%mm3   \n\t" // hack! No proper unsigned mmx compares!
                "pcmpgtb %%mm5, %%mm3 \n\t" // compare diff with mask

                "movq %%mm2, %%mm0    \n\t" // reload registers
                "movq %%mm7, %%mm1    \n\t"

                "pcmpgtb %%mm0, %%mm1 \n\t" // Secondary threshold
                "psubb %%mm0, %%mm6   \n\t"
                "psubb %%mm7, %%mm0   \n\t"
                "pand %%mm1, %%mm6    \n\t"
                "pandn %%mm0,%%mm1    \n\t"
                "por %%mm1, %%mm6     \n\t"

                "paddb %%mm4, %%mm6   \n\t"
                "pcmpgtb (%2), %%mm6  \n\t"

                "movq %%mm2, %%mm0    \n\t"

                "pavgb %%mm7, %%mm2   \n\t"

                "pand %%mm6, %%mm2    \n\t"
                "pandn %%mm0,%%mm6    \n\t"
                "por %%mm2, %%mm6     \n\t" // Combined new/keep average

                "pand %%mm3, %%mm7    \n\t"
                "pandn %%mm6,%%mm3    \n\t"
                "por %%mm7, %%mm3     \n\t" // Combined new/keep average

                "movq %%mm3, (%0)     \n\t"
                "movq %%mm3, (%1)     \n\t"
                : :
                "r" (avg[i]),
                "r" (buf[i]),
                "r" (mask2)
                );
            buf[i]++;
            avg[i]++;
        }
    }

    __asm__ volatile("emms\n\t");

    // filter the leftovers from the mmx rutine
    for (i = 0; i < 3; i++)
    {
        int thr1[3], thr2[3], height[3];
        uint8_t *avg8[3], *buf8[3];
        int end, beg;

        init_vars(tf, frame, thr1, thr2, height, avg8, buf8);

        end = height[i] * frame->pitches[i];
        beg = end & ~0x7;

        if (beg == end)
            continue;

        for (y = beg; y < end; y++)
        {
            int t = abs(avg8[i][y] - buf8[i][y]);
            if (t < thr1[i])
            {
                if (t > thr2[i])
                    avg8[i][y] = (avg8[i][y] + buf8[i][y]) >> 1;
                buf8[i][y] = avg8[i][y];
            }
            else
            {
                avg8[i][y] = buf8[i][y];
            }
        }
    }

    TF_END(tf, "QuickDNR2mmx: ");

    return 0;
}
#endif /* MMX */

static void cleanup(VideoFilter *vf)
{
    ThisFilter *tf = (ThisFilter*) vf;

    if (tf->average)
        free(tf->average);
}

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               int *width, int *height, char *options,
                               int threads)
{
    unsigned int Param1, Param2, Param3, Param4;
    int i, double_threshold = 1;
    ThisFilter *filter;

    (void) width;
    (void) height;
    (void) i;
    (void) threads;

    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
    {
        fprintf(stderr, "QuickDNR: attempt to initialize "
                "with unsupported format\n");
        return NULL;
    }

    filter = malloc(sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf(stderr, "Couldn't allocate memory for filter\n");
        return NULL;
    }

    memset(filter, 0, sizeof(ThisFilter));
    filter->vf.cleanup        = &cleanup;
    filter->Luma_threshold1   = LUMA_THRESHOLD1_DEFAULT;
    filter->Chroma_threshold1 = CHROMA_THRESHOLD1_DEFAULT;
    filter->Luma_threshold2   = LUMA_THRESHOLD2_DEFAULT;
    filter->Chroma_threshold2 = CHROMA_THRESHOLD2_DEFAULT;
    double_threshold          = 1;

    if (options)
    {
        int ret = sscanf(options, "%20u:%20u:%20u:%20u",
                         &Param1, &Param2, &Param3, &Param4);
        switch (ret)
        {
            case 1:
                //These might be better as logarithmic if this gets used a lot.
                filter->Luma_threshold1   = ((uint8_t) Param1) * 40 / 255;
                filter->Luma_threshold2   = ((uint8_t) Param1) * 4/255 > 2 ?
                    2 : ((uint8_t) Param1) * 4/255;
                filter->Chroma_threshold1 = ((uint8_t) Param1) * 80 / 255;
                filter->Chroma_threshold2 = ((uint8_t) Param1) * 8/255 > 4 ?
                    4 : ((uint8_t) Param1) * 8/255;
                break;

            case 2:
                filter->Luma_threshold1   = (uint8_t) Param1;
                filter->Chroma_threshold1 = (uint8_t) Param2;
                double_threshold = 0;
                break;

            case 4:
                filter->Luma_threshold1   = (uint8_t) Param1;
                filter->Luma_threshold2   = (uint8_t) Param2;
                filter->Chroma_threshold1 = (uint8_t) Param3;
                filter->Chroma_threshold2 = (uint8_t) Param4;
                break;

            default:
                break;
        }
    }

    filter->vf.filter  = (double_threshold) ? &quickdnr2 : &quickdnr;

#ifdef MMX
    if (av_get_cpu_flags() > AV_CPU_FLAG_MMX2)
    {
        filter->vf.filter = (double_threshold) ? &quickdnr2MMX : &quickdnrMMX;
        for (i = 0; i < 8; i++)
        {
            // 8 sign-shifted bytes!
            filter->Luma_threshold_mask1 =
                (filter->Luma_threshold_mask1 << 8) +
                ((filter->Luma_threshold1 > 0x80) ?
                 (filter->Luma_threshold1 - 0x80) :
                 (filter->Luma_threshold1 + 0x80));

            filter->Chroma_threshold_mask1 =
                (filter->Chroma_threshold_mask1 << 8) +
                ((filter->Chroma_threshold1 > 0x80) ?
                 (filter->Chroma_threshold1 - 0x80) :
                 (filter->Chroma_threshold1 + 0x80));

            filter->Luma_threshold_mask2 =
                (filter->Luma_threshold_mask2 << 8) +
                ((filter->Luma_threshold2 > 0x80) ?
                 (filter->Luma_threshold2 - 0x80) :
                 (filter->Luma_threshold2 + 0x80));

            filter->Chroma_threshold_mask2 =
                (filter->Chroma_threshold_mask2 << 8) +
                ((filter->Chroma_threshold2 > 0x80) ?
                 (filter->Chroma_threshold2 - 0x80) :
                 (filter->Chroma_threshold2 + 0x80));
        }
    }
#endif

    TF_INIT(filter);

#ifdef QUICKDNR_DEBUG
    fprintf(stderr, "DNR Loaded: 0x%X Params: %u %u \n"
            "Luma1:   %3d 0x%X%X  Luma2:   0x%X%X\n"
            "Chroma1: %3d %X%X    Chroma2: 0x%X%X\n",
            av_get_cpu_flags(), Param1, Param2, filter->Luma_threshold1,
            ((int*)&filter->Luma_threshold_mask1)[1],
            ((int*)&filter->Luma_threshold_mask1)[0],
            ((int*)&filter->Luma_threshold_mask2)[1],
            ((int*)&filter->Luma_threshold_mask2)[0],
            filter->Chroma_threshold1,
            ((int*)&filter->Chroma_threshold_mask1)[1],
            ((int*)&filter->Chroma_threshold_mask1)[0],
            ((int*)&filter->Chroma_threshold_mask2)[1],
            ((int*)&filter->Chroma_threshold_mask2)[0]
        );

    fprintf(stderr, "Options:%d:%d:%d:%d\n",
            filter->Luma_threshold1, filter->Luma_threshold2,
            filter->Chroma_threshold1, filter->Chroma_threshold2);
#endif

    return (VideoFilter*) filter;
}

static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        filter_init: &new_filter,
        name:       (char*)"quickdnr",
        descript:   (char*)
        "removes noise with a fast single/double thresholded average filter",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
