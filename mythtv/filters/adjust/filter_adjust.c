/* format conversion filter
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

typedef struct ThisFilter
{
    VideoFilter vf;

    int yend;
    int cend;

    uint8_t ytable[256];
    uint8_t ctable[256];

    TF_STRUCT;
} ThisFilter;

int adjustFilter (VideoFilter *vf, VideoFrame *frame)
{
    ThisFilter *filter = (ThisFilter *) vf;
    uint8_t *pptr = frame->buf;
    TF_VARS;

    TF_START;
    while (pptr < frame->buf + filter->yend)
    {
        *pptr = filter->ytable[*pptr];
        pptr++;
    }
    while (pptr < frame->buf + filter->cend)
    {
        *pptr = filter->ctable[*pptr];
        pptr++;
    }

    TF_END(filter, "Adjust: ");
    return 0;
}

void fill_table(uint8_t *table, int in_min, int in_max, int out_min, int out_max, float gamma)
{
    int i;
    float f;

    for (i = 0; i < 256; i++)
    {
        f = ((float)i - in_min) / (in_max - in_min);
        f = f < 0.0 ? 0.0 : f;
        f = f > 1.0 ? 1.0 : f;
        table[i] = (pow (f, gamma) * (out_max - out_min) + out_min + 0.5);
    }
}

VideoFilter *
newAdjustFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                    int *width, int *height, char *options)
{
    ThisFilter *filter;
    int numopts, ymin, ymax, cmin, cmax;
    float ygamma, cgamma;

    if (inpixfmt != outpixfmt ||
        (inpixfmt != FMT_YV12 && inpixfmt != FMT_YUV422P))
    {
        fprintf(stderr, "adjust: only YV12->YV12 and YUV422P->YUV422P"
                " conversions are supported\n");
        return NULL;
    }

    numopts = 0;
    if (options)
        numopts = sscanf(options, "%d:%d:%f:%d:%d:%f", &ymin, &ymax, &ygamma,
                         &cmin, &cmax, &cgamma);

    if (numopts != 6)
    {
        ymin = 16;
        ymax = 253;
        ygamma = 1.0;
        cmin = 2;
        cmax = 253;
        cgamma = 1.0;
    }

    filter = malloc (sizeof (ThisFilter));

    if (filter == NULL)
    {
        fprintf (stderr, "adjust: failed to allocate memory for filter\n");
        return NULL;
    }

    fill_table (filter->ytable, ymin, ymax, 16, 235, ygamma);
    fill_table (filter->ctable, cmin, cmax, 16, 240, cgamma);

    filter->yend = *width * *height;
    
    switch (inpixfmt)
    {
        case FMT_YV12:
            filter->cend = filter->yend + *width * *height / 2;
            break;
        case FMT_YUV422P:
            filter->cend = filter->yend + *width * *height;
            break;
        default:
            break;
    }

    filter->vf.filter = &adjustFilter;
    filter->vf.cleanup = NULL;
    
    TF_INIT(filter);
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    { FMT_YUV422P, FMT_YUV422P },
    FMT_NULL
};

FilterInfo filter_table[] = 
{
    {
        symbol:     "newAdjustFilter",
        name:       "adjust",
        descript:   "adjust range and gamma of video",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
