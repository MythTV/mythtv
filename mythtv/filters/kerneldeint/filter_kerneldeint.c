/* Rewrite of neuron2's KernelDeint filter for Avisynth */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "mythconfig.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <math.h>
#include <pthread.h>

#include "filter.h"
#include "mythframe.h"

#include "mythlogging.h"

#include "../mm_arch.h"

#undef ABS
#define ABS(A) ( (A) > 0 ? (A) : -(A) )
#define CLAMP(A,L,U) ((A)>(U)?(U):((A)<(L)?(L):(A)))

#if HAVE_MMX
#include "ffmpeg-mmx.h"
#define THRESHOLD 12
static const mmx_t mm_lthr = { .w={ -THRESHOLD, -THRESHOLD,
                                   -THRESHOLD, -THRESHOLD} };
static const mmx_t mm_hthr = { .w={ THRESHOLD - 1, THRESHOLD - 1,
                                   THRESHOLD - 1, THRESHOLD - 1} };
static const mmx_t mm_cpool[] = { { 0x0000000000000000LL }, };
#else
#define mmx_t int
#endif

struct DeintThread
{
    int       m_ready;
    pthread_t m_id;
    int       m_exists;
};

typedef struct ThisFilter
{
    VideoFilter m_vf;

    struct DeintThread *m_threads;
    VideoFrame         *m_frame;
    int                 m_field;
    int                 m_ready;
    int                 m_killThreads;
    int                 m_actualThreads;
    int                 m_requestedThreads;
    pthread_mutex_t     m_mutex;

    int                 m_skipChroma;
    int                 m_mmFlags;
    int                 m_width;
    int                 m_height;
    long long           m_lastFrameNr;
    uint8_t            *m_ref[3];
    int                 m_refStride[3];

    int                 m_dirtyFrame;
    int                 m_doubleRate;
    int                 m_doubleCall;
    void (*m_lineFilter)(uint8_t *dst, int width, int start_width,
                        const uint8_t *src1, const uint8_t *src2, const uint8_t *src3,
                        const uint8_t *src4, const uint8_t *src5);
    void (*m_lineFilterFast)(uint8_t *dst, int width, int start_width,
                             uint8_t *src1, const uint8_t *src2, const uint8_t *src3,
                             const uint8_t *src4, const uint8_t *src5);
    TF_STRUCT;
} ThisFilter;

static void line_filter_c_fast(uint8_t *dst, int width, int start_width,
                           uint8_t *buf, const uint8_t *src2, const uint8_t *src3,
                           const uint8_t *src4, const uint8_t *src5)
{
    for (int X = start_width; X < width; X++)
    {
        uint8_t tmp = buf[X];
        buf[X] = src3[X];
        if (ABS((int)src3[X] - (int)src2[X]) > 11)
            dst[X] = CLAMP((src2[X] * 4 + src4[X] * 4
                    + src3[X] * 2 - tmp - src5[X])
                    / 8, 0, 255);
    }
}

static void line_filter_c(uint8_t *dst, int width, int start_width,
                          const uint8_t *src1, const uint8_t *src2, const uint8_t *src3,
                          const uint8_t *src4, const uint8_t *src5)
{
    for (int X = start_width; X < width; X++)
    {
        if (ABS((int)src3[X] - (int)src2[X]) > 11)
            dst[X] = CLAMP((src2[X] * 4 + src4[X] * 4
                    + src3[X] * 2 - src1[X] - src5[X])
                    / 8, 0, 255);
        else
            dst[X] = src3[X];
    }
}

#if HAVE_MMX
static inline void mmx_start(const uint8_t *src1, const uint8_t *src2,
                             const uint8_t *src3, const uint8_t *src4,
                             int X)
{
    movq_m2r (src2[X], mm0);
    movq_m2r (src2[X], mm1);
    movq_m2r (src4[X], mm2);
    movq_m2r (src4[X], mm3);
    movq_m2r (src3[X], mm4);
    movq_m2r (src3[X], mm5);
    punpcklbw_m2r (mm_cpool[0], mm0);
    punpckhbw_m2r (mm_cpool[0], mm1);
    punpcklbw_m2r (mm_cpool[0], mm2);
    punpckhbw_m2r (mm_cpool[0], mm3);
    movq_r2r (mm0, mm6);
    movq_r2r (mm1, mm7);
    paddw_r2r (mm2, mm0);
    paddw_r2r (mm3, mm1);
    movq_m2r (src3[X], mm2);
    movq_m2r (src3[X], mm3);
    psllw_i2r (2, mm0);
    psllw_i2r (2, mm1);
    punpcklbw_m2r (mm_cpool[0], mm2);
    punpckhbw_m2r (mm_cpool[0], mm3);
    psllw_i2r (1, mm2);
    psllw_i2r (1, mm3);
    paddw_r2r (mm2, mm0);
    paddw_r2r (mm3, mm1);
    movq_m2r (src1[X], mm2);
    movq_m2r (src1[X], mm3);
    punpcklbw_m2r (mm_cpool[0], mm2);
    punpckhbw_m2r (mm_cpool[0], mm3);
}

static inline void mmx_end(const uint8_t *src3, const uint8_t *src5,
                           uint8_t *dst, int X)
{
    punpcklbw_m2r (mm_cpool[0], mm4);
    punpckhbw_m2r (mm_cpool[0], mm5);
    psubusw_r2r (mm2, mm0);
    psubusw_r2r (mm3, mm1);
    movq_m2r (src5[X], mm2);
    movq_m2r (src5[X], mm3);
    punpcklbw_m2r (mm_cpool[0], mm2);
    punpckhbw_m2r (mm_cpool[0], mm3);
    psubusw_r2r (mm2, mm0);
    psubusw_r2r (mm3, mm1);
    psrlw_i2r (3, mm0);
    psrlw_i2r (3, mm1);
    psubw_r2r (mm6, mm4);
    psubw_r2r (mm7, mm5);
    packuswb_r2r (mm1,mm0);
    movq_r2r (mm4, mm6);
    movq_r2r (mm5, mm7);
    pcmpgtw_m2r (mm_lthr, mm4);
    pcmpgtw_m2r (mm_lthr, mm5);
    pcmpgtw_m2r (mm_hthr, mm6);
    pcmpgtw_m2r (mm_hthr, mm7);
    packsswb_r2r (mm5, mm4);
    packsswb_r2r (mm7, mm6);
    pxor_r2r (mm6, mm4);
    movq_r2r (mm4, mm5);
    pandn_r2r (mm0, mm4);
    pand_m2r (src3[X], mm5);
    por_r2r (mm4, mm5);
    movq_r2m (mm5, dst[X]);
}

static void line_filter_mmx_fast(uint8_t *dst, int width, int start_width,
                                 uint8_t *buf, const uint8_t *src2, const uint8_t *src3,
                                 const uint8_t *src4, const uint8_t *src5)
{
    int X = start_width;
    for ( ; X < width - 7; X += 8)
    {
        mmx_start(buf, src2, src3, src4, X);
        movq_r2m (mm4, buf[X]);
        mmx_end(src3, src5, dst, X);
    } 

    line_filter_c_fast(dst, width, X, buf, src2, src3, src4, src5);
}

static void line_filter_mmx(uint8_t *dst, int width, int start_width,
                            const uint8_t *src1, const uint8_t *src2, const uint8_t *src3,
                            const uint8_t *src4, const uint8_t *src5)
{
    int X = start_width;
    for ( ; X < width - 7; X += 8)
    {
        mmx_start(src1, src2, src3, src4, X);
        mmx_end(src3, src5, dst, X);
    } 

    line_filter_c(dst, width, X, src1, src2, src3, src4, src5);
}
#endif

static void store_ref(struct ThisFilter *p, uint8_t *src, int src_offsets[3],
                      int src_stride[3], int width, int height)
{
    for (int i = 0; i < 3; i++)
    {
        if (src_stride[i] < 1)
            continue;

        int is_chroma = !!i;
        int h = height >> is_chroma;
        int w = width  >> is_chroma;

        if (p->m_refStride[i] == src_stride[i])
        {
            memcpy(p->m_ref[i], src + src_offsets[i], src_stride[i] * h);
        }
        else
        {
            uint8_t *src2 = src + src_offsets[i];
            uint8_t *dest = p->m_ref[i];
            for (int j = 0; j < h; j++)
            {
                memcpy(dest, src2, w);
                src2 += src_stride[i];
                dest += p->m_refStride[i];
            }
        }
    }
}

static int AllocFilter(ThisFilter* filter, int width, int height)
{
    if ((width != filter->m_width) || (height != filter->m_height))
    {
        for (int i = 0; i < 3; i++)
        {
            if (filter->m_ref[i])
                free(filter->m_ref[i]);

            int is_chroma= !!i;
            int w = ((width      + 31) & (~31)) >> is_chroma;
            int h = ((height + 6 + 31) & (~31)) >> is_chroma;
            int size = w * h * sizeof(uint8_t);

            filter->m_refStride[i] = w;
            filter->m_ref[i] = (uint8_t*) malloc(size);
            if (!filter->m_ref[i])
                return 0;
            memset(filter->m_ref[i], is_chroma ? 127 : 0, size);
        }
        filter->m_width  = width;
        filter->m_height = height;
    }
    return 1;
}

static void filter_func(struct ThisFilter *p, uint8_t *dst, int dst_offsets[3],
                        int dst_stride[3], int width, int height, int parity,
                        int tff, int double_rate, int dirty,
                        int this_slice, int total_slices)
{
    if (height < 8 || total_slices < 1)
        return;

    if (total_slices > 1 && !double_rate)
    {
        this_slice   = 0;
        total_slices = 1;
    }

    uint8_t *dest = NULL, *src1 = NULL, *src2 = NULL,
        *src3 = NULL, *src4 = NULL, *src5 = NULL;
    int channels = p->m_skipChroma ? 1 : 3;
    int    field = parity ^ tff;

    int first_slice  = (this_slice == 0);
    int last_slice   = 0;
    int slice_height = height / total_slices;
    slice_height     = (slice_height >> 1) << 1;
    int starth       = slice_height * this_slice;
    int endh         = starth + slice_height;

    if ((this_slice + 1) >= total_slices)
    {
        endh = height;
        last_slice = 1;
    }

    for (int i = 0; i < channels; i++)
    {

        int is_chroma = !!i;
        int w         = width  >> is_chroma;
        int start     = starth >> is_chroma;
        int end       = endh   >> is_chroma;

        if (!first_slice)
            start -= 2;
        if (last_slice)
            end -= (5 + field);

        int src_pitch = p->m_refStride[i];
        dest = dst + dst_offsets[i] + (start * dst_stride[i]);
        src1 = p->m_ref[i] + (start * src_pitch);

        if (double_rate)
        {
            src2 = src1 + src_pitch;
            src3 = src2 + src_pitch;
            src4 = src3 + src_pitch;
            src5 = src4 + src_pitch;

            if (first_slice)
            {
                if (!field)
                    p->m_lineFilter(dest, w, 0, src1, src1, src1, src2, src3);
                else if (dirty)
                    memcpy(dest, src1, w);    
                dest += dst_stride[i];

                if (field)
                    p->m_lineFilter(dest, w, 0, src1, src1, src2, src3, src4);
                else if (dirty)
                    memcpy(dest, src2, w);    
                dest += dst_stride[i];
            }
            else
            {
                dest += dst_stride[i] << 1;
            }

            for (int y = start; y < end; y++)
            {
                if ((y ^ (1 - field)) & 1)
                    p->m_lineFilter(dest, w, 0, src1, src2, src3, src4, src5);
                else if (dirty)
                    memcpy(dest, src3, w);

                dest += dst_stride[i];
                src1  = src2;
                src2  = src3;
                src3  = src4;
                src4  = src5;
                src5 += src_pitch;
            }

            if (last_slice)
            {
                if (!field)
                    p->m_lineFilter(dest, w, 0, src2, src3, src4, src5, src5);
                else if (dirty)
                    memcpy(dest, src4, w);    
                dest += dst_stride[i];

                if (field)
                    p->m_lineFilter(dest, w, 0, src3, src4, src5, src5, src5);
                else if (dirty)
                    memcpy(dest, src5, w);
            }
        }
        else
        {
            int field_stride = dst_stride[i] << 1;
            src2 = dest + dst_stride[i];
            src3 = src2 + dst_stride[i];
            src4 = src3 + dst_stride[i];
            src5 = src4 + dst_stride[i];
            memcpy(src1, dest, w);

            if (field)
            {
                dest += dst_stride[i];
                p->m_lineFilterFast(dest, w, 0, src1, src2, src2, src3, src4);
                src2 = src3;
                src3 = src4;
                src4 = src5;
                src5 += dst_stride[i];
            }
            else
            {
                p->m_lineFilterFast(dest, w, 0, src1, src2, src2, src2, src3);
            }
            dest += field_stride;

            for (int y = start; y < end; y += 2)
            {
                p->m_lineFilterFast(dest, w, 0, src1, src2, src3, src4, src5);
                dest += field_stride;
                src2 = src4;
                src3 = src5;
                src4 += field_stride;
                src5 += field_stride;
            }

            if (field)
                p->m_lineFilterFast(dest, w, 0, src1, src4, src5, src5, src5);
            else
                p->m_lineFilterFast(dest, w, 0, src1, src3, src4, src5, src5);
        }
    }
#if HAVE_MMX
    if (p->m_mmFlags & AV_CPU_FLAG_MMX)
        emms();
#endif
}

static void *KernelThread(void *args)
{
    ThisFilter *filter = (ThisFilter*)args;

    pthread_mutex_lock(&(filter->m_mutex));
    int num = filter->m_actualThreads;
    filter->m_actualThreads = num + 1;
    pthread_mutex_unlock(&(filter->m_mutex));

    while (!filter->m_killThreads)
    {
        usleep(1000);
        if (filter->m_ready &&
            filter->m_frame != NULL &&
            filter->m_threads[num].m_ready)
        {
            filter_func(
                filter, filter->m_frame->buf, filter->m_frame->offsets,
                filter->m_frame->pitches, filter->m_frame->width,
                filter->m_frame->height, filter->m_field,
                filter->m_frame->top_field_first, filter->m_doubleRate,
                filter->m_dirtyFrame, num, filter->m_actualThreads);

            pthread_mutex_lock(&(filter->m_mutex));
            filter->m_ready = filter->m_ready - 1;
            filter->m_threads[num].m_ready = 0;
            pthread_mutex_unlock(&(filter->m_mutex));
        }
    }
    pthread_exit(NULL);
    return NULL;
}

static int KernelDeint(VideoFilter *f, VideoFrame *frame, int field)
{
    ThisFilter *filter = (ThisFilter *) f;
    TF_VARS;

    if (!AllocFilter(filter, frame->width, frame->height))
    {
        LOG(VB_GENERAL, LOG_ERR, "KernelDeint: failed to allocate buffers.");
        return -1;
    }

    TF_START;

    filter->m_dirtyFrame = 1;
    if (filter->m_lastFrameNr == frame->frameNumber)
    {
        filter->m_doubleCall = 1;
    }
    else
    {
        filter->m_doubleRate = filter->m_doubleCall;
        filter->m_doubleCall = 0;
        filter->m_dirtyFrame = 0;
        if (filter->m_doubleRate)
        {
            store_ref(filter, frame->buf,  frame->offsets,
                      frame->pitches, frame->width, frame->height);
        }
    }

    if (filter->m_actualThreads > 1 && filter->m_doubleRate)
    {
        for (int i = 0; i < filter->m_actualThreads; i++)
            filter->m_threads[i].m_ready = 1;
        filter->m_frame = frame;
        filter->m_field = field;
        filter->m_ready = filter->m_actualThreads;
        int i = 0;
        while (filter->m_ready > 0 && i < 1000)
        {
            usleep(1000);
            i++;
        }
    }
    else
    {
        filter_func(
            filter, frame->buf, frame->offsets, frame->pitches,
            frame->width, frame->height, field, frame->top_field_first,
            filter->m_doubleRate, filter->m_dirtyFrame, 0, 1);
    }

    filter->m_lastFrameNr = frame->frameNumber;

    TF_END(filter, "KernelDeint: ");

    return 0;
}

static void CleanupKernelDeintFilter(VideoFilter *f)
{
    ThisFilter *filter = (ThisFilter *) f;

    for (int i = 0; i < 3; i++)
    {
        uint8_t **p= &filter->m_ref[i];
        if (*p)
            free(*p);
        *p= NULL;
    }

    if (filter->m_threads != NULL)
    {
        filter->m_killThreads = 1;
        for (int i = 0; i < filter->m_requestedThreads; i++)
            if (filter->m_threads[i].m_exists)
                pthread_join(filter->m_threads[i].m_id, NULL);
        free(filter->m_threads);
    }
}

static VideoFilter *NewKernelDeintFilter(VideoFrameType inpixfmt,
                                         VideoFrameType outpixfmt,
                                         const int *width, const int *height,
                                         const char *options, int threads)
{
    (void) options;
    (void) threads;

    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
    {
        LOG(VB_GENERAL, LOG_ERR, "KernelDeint: valid formats are YV12->YV12");
        return NULL;
    }

    ThisFilter *filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "KernelDeint: failed to allocate memory for filter.");
        return NULL;
    }

    filter->m_mmFlags = 0;
    filter->m_lineFilter = &line_filter_c;
    filter->m_lineFilterFast = &line_filter_c_fast;
#if HAVE_MMX
    filter->m_mmFlags = av_get_cpu_flags();
    if (filter->m_mmFlags & AV_CPU_FLAG_MMX)
    {
        filter->m_lineFilter = &line_filter_mmx;
        filter->m_lineFilterFast = &line_filter_mmx_fast;
    }
#endif

    filter->m_skipChroma   = 0;
    filter->m_width        = 0;
    filter->m_height       = 0;
    filter->m_lastFrameNr  = -1;
    filter->m_doubleCall   = 0;
    filter->m_doubleRate   = 1;
    memset(filter->m_ref, 0, sizeof(filter->m_ref));
    if (!AllocFilter(filter, *width, *height))
    {
        LOG(VB_GENERAL, LOG_ERR, "KernelDeint: failed to allocate buffers.");
        free (filter);
        return NULL;
    }

    TF_INIT(filter);

    filter->m_vf.filter  = &KernelDeint;
    filter->m_vf.cleanup = &CleanupKernelDeintFilter;

    filter->m_frame = NULL;
    filter->m_field = 0;
    filter->m_ready = 0;
    filter->m_killThreads = 0;
    filter->m_actualThreads  = 0;
    filter->m_requestedThreads  = threads;
    filter->m_threads = NULL;

    if (filter->m_requestedThreads > 1)
    {
        filter->m_threads = (struct DeintThread *) calloc(threads,
                          sizeof(struct DeintThread));
        if (filter->m_threads == NULL)
        {
            LOG(VB_GENERAL, LOG_ERR, "KernelDeint: failed to allocate memory "
                    "for threads - falling back to existing, single thread.");
            filter->m_requestedThreads = 1;
        }
    }

    if (filter->m_requestedThreads > 1)
    {
        pthread_mutex_init(&(filter->m_mutex), NULL);
        int success = 0;
        for (int i = 0; i < filter->m_requestedThreads; i++)
        {
            if (pthread_create(&(filter->m_threads[i].m_id), NULL,
                               KernelThread, (void*)filter) != 0)
                filter->m_threads[i].m_exists = 0;
            else
            {
                success++;
                filter->m_threads[i].m_exists = 1;
            }
        }

        if (success < filter->m_requestedThreads)
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                "KernelDeint: failed to create all threads - "
                "falling back to existing, single thread.");
        }
        else
        {
            int timeout = 0;
            while (filter->m_actualThreads != filter->m_requestedThreads)
            {
                timeout++;
                if (timeout > 5000)
                {
                    LOG(VB_GENERAL, LOG_NOTICE,
                        "KernelDeint: waited too long for "
                        "threads to start.- continuing.");
                    break;
                }
                usleep(1000);
            }
            LOG(VB_PLAYBACK, LOG_INFO, "KernelDeint: Created threads.");
        }
    }

    if (filter->m_actualThreads < 1 )
        LOG(VB_PLAYBACK, LOG_INFO, "KernelDeint: Using existing thread.");

    return (VideoFilter *) filter;
}

static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        .filter_init= &NewKernelDeintFilter,
        .name=       (char*)"kerneldeint",
        .descript=   (char*)"combines data from several fields to deinterlace "
                    "with less motion blur",
        .formats=    FmtList,
        .libname=    NULL
    },
    {
        .filter_init= &NewKernelDeintFilter,
        .name=       (char*)"kerneldoubleprocessdeint",
        .descript=   (char*)"combines data from several fields to deinterlace "
                    "with less motion blur",
        .formats=    FmtList,
        .libname=    NULL
    },
    FILT_NULL
};
