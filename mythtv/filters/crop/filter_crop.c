/*
 * crop v 0.2
 * (C)opyright 2003, Debabrata Banerjee
 * GNU GPL 2 or later
 * 
 * Pass options as crop=top:left:bottom:right as number of 16 pixel blocks.
 * 
 */

#include <stdio.h>

#include "mythconfig.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "filter.h"
#include "mythframe.h"
#include "libavutil/mem.h"
#include "libavutil/cpu.h"

#ifdef MMX
#include "ffmpeg-mmx.h"
#endif

//static const char FILTER_NAME[] = "crop";

typedef struct ThisFilter
{
        VideoFilter m_vf;

        int m_yp1, m_yp2, m_xp1, m_xp2;

        TF_STRUCT;

} ThisFilter;

static int crop(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *tf = (ThisFilter*) f;
    uint64_t *ybuf = (uint64_t*) (frame->buf + frame->offsets[0]);
    uint64_t *ubuf = (uint64_t*) (frame->buf + frame->offsets[1]);
    uint64_t *vbuf = (uint64_t*) (frame->buf + frame->offsets[2]);
    const uint64_t Y_black  = 0x1010101010101010LL; // 8 bytes
    const uint64_t UV_black = 0x8080808080808080LL; // 8 bytes

    TF_VARS;

    TF_START;

    if (frame->pitches[1] != frame->pitches[2])
        return -1;

    // Luma top
    int sz = (frame->pitches[0] * frame->height) >> 3; // div 8 bytes
    for (int y = 0; (y < tf->m_yp1 * frame->pitches[0] << 1) && (y < sz); y += 2)
    {
        ybuf[y + 0] = Y_black;
        ybuf[y + 1] = Y_black;
    }

    // Luma bottom
    for (int y = ((frame->height >> 4) - tf->m_yp2) * frame->pitches[0] << 1;
         y < sz; y += 2)
    {
        ybuf[y + 0] = Y_black;
        ybuf[y + 1] = Y_black;
    }

    // Chroma top
    sz = (frame->pitches[1] * (frame->height >> 1)) >> 3; // div 8 bytes
    for (int y = 0; (y < tf->m_yp1 * frame->pitches[1]) && (y < sz); y++)
    {
        ubuf[y] = UV_black;
        vbuf[y] = UV_black;
    }

    // Chroma bottom
    for (int y = ((frame->height >> 4) - tf->m_yp2) * frame->pitches[1]; y < sz; y++)
    {
        ubuf[y] = UV_black;
        vbuf[y] = UV_black;
    }

    // Luma left and right
    sz = (frame->pitches[0] * frame->height) >> 3; // div 8 bytes
    int t1 = frame->pitches[0] << 1;
    int t2 = frame->pitches[0] >> 3;
    for (int y = tf->m_yp1 * t1;
         (y < ((frame->height >> 4) - tf->m_yp2) * t1) && (y < sz); y += t2)
    {
        for (int x = 0; (x < (tf->m_xp1 << 1)) && (x < t1); x += 2)
        {
            ybuf[y + x + 0] = Y_black;
            ybuf[y + x + 1] = Y_black;
        }

        for (int x = t2 - (tf->m_xp2 << 1); (x < t2) && (x < t1); x += 2)
        {
            ybuf[y + x + 0] = Y_black;
            ybuf[y + x + 1] = Y_black;
        }
    }

    // Chroma left and right
    sz = (frame->pitches[1] * (frame->height >> 1)) >> 3; // div 8 bytes
    t1 = (frame->pitches[1] * ((frame->height >> 4) - tf->m_yp2) << 2) >> 2;
    t2 = frame->pitches[1] >> 3;
    for (int y = (frame->pitches[1] * tf->m_yp1) >> 1; (y < t1) && (y < sz); y += t2)
    {
        for (int x = 0; x < tf->m_xp1; x++)
        {
            ubuf[y + x] = UV_black;
            vbuf[y + x] = UV_black;
        }

        for (int x = t2 - tf->m_xp2; x < t2; x++)
        {
            ubuf[y + x] = UV_black;
            vbuf[y + x] = UV_black;
        }
    }

    TF_END(tf, "Crop: ");
    return 0;
}

#ifdef MMX
static int cropMMX(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *tf = (ThisFilter*) f;  
    uint64_t *ybuf = (uint64_t*) (frame->buf + frame->offsets[0]);
    uint64_t *ubuf = (uint64_t*) (frame->buf + frame->offsets[1]);
    uint64_t *vbuf = (uint64_t*) (frame->buf + frame->offsets[2]);
    const uint64_t Y_black  = 0x1010101010101010LL;
    const uint64_t UV_black = 0x8080808080808080LL;

    TF_VARS;

    TF_START;

    if (frame->pitches[1] != frame->pitches[2])
        return -1;

    __asm__ volatile("emms\n\t");

    __asm__ volatile("movq (%1),%%mm0    \n\t"
                 "movq (%0),%%mm1    \n\t"
                 : : "r" (&UV_black), "r"(&Y_black));
  
    // Luma top
    int sz = (frame->pitches[0] * frame->height) >> 3; // div 8 bytes
    for (int y = 0; (y < tf->m_yp1 * frame->pitches[0] << 1) && (y < sz); y += 2)
    {
        __asm__ volatile("movq %%mm0, (%0)     \n\t"
                     "movq %%mm0, 8(%0)    \n\t"
                     : : "r" (ybuf + y));
    }

    // Luma bottom
    for (int y = ((frame->height >> 4) - tf->m_yp2) * frame->pitches[0] << 1;
         y < sz; y += 2)
    {
        __asm__ volatile("movq %%mm0, (%0)     \n\t"
                     "movq %%mm0, 8(%0)    \n\t"
                     : : "r" (ybuf + y));
    }

    // Chroma top
    sz = (frame->pitches[1] * (frame->height >> 1)) >> 3; // div 8 bytes
    for (int y = 0; (y < tf->m_yp1 * frame->pitches[1]) && (y < sz); y++)
    {
        __asm__ volatile("movq %%mm1, (%0)    \n\t"
                     "movq %%mm1, (%1)    \n\t"
                     : : "r" (ubuf + y), "r" (vbuf + y));
    }

    // Chroma bottom
    for (int y = ((frame->height >> 4) - tf->m_yp2) * frame->pitches[1]; y < sz; y++)
    {
        __asm__ volatile("movq %%mm1, (%0)    \n\t"
                     "movq %%mm1, (%1)    \n\t"
                     : : "r" (ubuf + y), "r" (vbuf + y));
    }
 
    // Luma left and right
    sz = (frame->pitches[0] * frame->height) >> 3; // div 8 bytes
    int t1 = frame->pitches[0] << 1;
    int t2 = frame->pitches[0] >> 3;
    for (int y = tf->m_yp1 * t1;
         (y < ((frame->height >> 4) - tf->m_yp2) * t1) && (y < sz); y += t2)
    {
        for (int x = 0; (x < (tf->m_xp1 << 1)) && (x < t1); x += 2)
        {
            __asm__ volatile("movq %%mm0, (%0)     \n\t"
                         "movq %%mm0, 8(%0)    \n\t"
                         : : "r" (ybuf + y + x));
        }

        for (int x = t2 - (tf->m_xp2 << 1); (x < t2) && (x < t1); x += 2)
        {
            __asm__ volatile("movq %%mm0, (%0)     \n\t"
                         "movq %%mm0, 8(%0)    \n\t"
                         : : "r" (ybuf + y + x));
        }
    }

    // Chroma left and right
    sz = (frame->pitches[1] * (frame->height >> 1)) >> 3; // div 8 bytes
    t1 = (frame->pitches[1] * ((frame->height >> 4) - tf->m_yp2) << 2) >> 2;
    t2 = frame->pitches[1] >> 3;
    for (int y = (frame->pitches[1] * tf->m_yp1) >> 1; (y < t1) && (y < sz); y += t2)
    {
        for (int x = 0; x < tf->m_xp1; x++)
        {
            __asm__ volatile("movq %%mm1, (%0)    \n\t"
                         "movq %%mm1, (%1)    \n\t"
                         : : "r" (ubuf + y + x), "r" (vbuf + y + x));
        }

        for (int x = t2 - tf->m_xp2; x < t2; x++)
        {
            __asm__ volatile("movq %%mm1, (%0)    \n\t"
                         "movq %%mm1, (%1)    \n\t"
                         : : "r" (ubuf + y + x), "r" (vbuf + y + x));
        }
    }

    __asm__ volatile("emms\n\t");

    TF_END(tf, "CropMMX: ");
    return 0;
}
#endif /* MMX */

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               const int *width, const int *height, const char *options,
                               int threads)
{
    (void) width;
    (void) height;
    (void) threads;

    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
    {
        fprintf(stderr,
                "crop: Attempt to initialize with unsupported format\n");

        return NULL;
    }

    ThisFilter *filter = malloc(sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf(stderr, "crop: Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->m_yp1 = filter->m_yp2 = filter->m_xp1 = filter->m_xp2 = 1;

    if (options)
    {
        unsigned int param1=0, param2=0, param3=0, param4=0;
        if (sscanf(options, "%20u:%20u:%20u:%20u",
                   &param1, &param2, &param3, &param4) == 4)
        {
            filter->m_yp1 = param1;
            filter->m_yp2 = param3;
            filter->m_xp1 = param2;
            filter->m_xp2 = param4;
        }
    }

    filter->m_vf.cleanup = NULL;
    filter->m_vf.filter  = &crop;

#ifdef MMX
    if (av_get_cpu_flags() & AV_CPU_FLAG_MMX)
        filter->m_vf.filter = &cropMMX;
#endif

    TF_INIT(filter);

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
        .filter_init= &new_filter,
        .name=       (char*)"crop",
        .descript=   (char*)"crops picture by macroblock intervals",
        .formats=    FmtList,
        .libname=    NULL
    },
    FILT_NULL
};
