// Spatial/temporal denoising filter shamelessly swiped from Mplayer
// Mplayer filter code Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
// Adapted to MythTV by Andrew Mahone <andrewmahone@eml.cc>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

#define LowPass(Prev, Curr, Coef) (Curr + Coef[Prev - Curr])

#define ABS(A) ( (A) > 0 ? (A) : -(A) )

static const char FILTER_NAME[] = "denoise3d";

typedef struct ThisFilter
{
    int (*filter)(VideoFilter *, VideoFrame *);
    void (*cleanup)(VideoFilter *);

    char *name;
    void *handle; // Library handle;

    /* functions and variables below here considered "private" */
    int first;
    int size;
    unsigned char *line;
    unsigned char *prev;
    int coefs[4][512];
} ThisFilter;

static inline void deNoise(unsigned char *Frame,        // mpi->planes[x]
                    unsigned char *FramePrev,    // pmpi->planes[x]
                    unsigned char *LineAnt,      // vf->priv->Line (width bytes)
                    int W, int H,
		    int *Horizontal, int *Vertical, int *Temporal)
{
    int X, Y;
    int LineOffs = 0;
    unsigned char PixelAnt;

    /* First pixel has no left nor top neightbour. Only previous frame */
    LineAnt[0] = PixelAnt = Frame[0];
    Frame[0] = LowPass(FramePrev[0], LineAnt[0], Temporal);

    /* Fist line has no top neightbour. Only left one for each pixel and
     * last frame */
    for (X = 1; X < W; X++)
    {
        PixelAnt = LowPass(PixelAnt, Frame[X], Horizontal);
        LineAnt[X] = PixelAnt;
        Frame[X] = LowPass(FramePrev[X], LineAnt[X], Temporal);
    }

    for (Y = 1; Y < H; Y++)
    {
	LineOffs += W;
        /* First pixel on each line doesn't have previous pixel */
        PixelAnt = Frame[LineOffs];
        LineAnt[0] = LowPass(LineAnt[0], PixelAnt, Vertical);
        Frame[LineOffs] = LowPass(FramePrev[LineOffs], LineAnt[0], Temporal);

        for (X = 1; X < W; X++)
        {
            /* The rest are normal */
            PixelAnt = LowPass(PixelAnt, Frame[LineOffs+X], Horizontal);
            LineAnt[X] = LowPass(LineAnt[X], PixelAnt, Vertical);
            Frame[LineOffs+X] = LowPass(FramePrev[LineOffs+X], LineAnt[X], Temporal);
        }
    }
}

static void PrecalcCoefs(int *Ct, double Dist25)
{
    int i;
    double Gamma, Simil, C;

    Gamma = log(0.25) / log(1.0 - Dist25/255.0);

    for (i = -256; i <= 255; i++)
    {
        Simil = 1.0 - ABS(i) / 255.0;
//        Ct[256+i] = lround(pow(Simil, Gamma) * (double)i);
        C = pow(Simil, Gamma) * (double)i;
        Ct[256+i] = (C<0) ? (C-0.5) : (C+0.5);
    }
}

int denoise3DFilter(VideoFilter *f, VideoFrame *frame)
{
    int width = frame->width;
    int height = frame->height;
    unsigned char *yuvptr = frame->buf;
    int uoff = width*height;
    int voff = width*height*5/4;
    int cwidth = width/2;
    int cheight = height/2;
    int fsize = width*height*3/2;
    ThisFilter *vf = (ThisFilter *)f;

    if (frame->codec != FMT_YV12)
        return 1;

    if ((vf->first) || (vf->size != fsize))
    {
        if (vf->prev)
            free(vf->prev);
        if (vf->line)
            free(vf->line);

        vf->prev = (unsigned char *)malloc(fsize);
	if(vf->prev == NULL)
	{
            fprintf(stderr,"Couldn't allocate buffer memory\n");
	    return 1;
	}
        vf->line = (unsigned char *)malloc(width);
	if(vf->line == NULL)
	{
            fprintf(stderr,"Couldn't allocate buffer memory\n");
	    free((void *)vf->prev);
	    vf->prev=NULL;
	    return 1;
	}
        memcpy((void *)vf->prev,(void *)yuvptr,fsize);
        vf->first=0;
    }

    deNoise(yuvptr, vf->prev, 
        vf->line, width, height,
        vf->coefs[0] + 256,
        vf->coefs[0] + 256,
        vf->coefs[1] + 256);
    deNoise(yuvptr+uoff, vf->prev+uoff, 
        vf->line, cwidth, cheight,
        vf->coefs[2] + 256,
        vf->coefs[2] + 256,
        vf->coefs[3] + 256);
    deNoise(yuvptr+voff, vf->prev+voff, 
        vf->line, cwidth, cheight,
        vf->coefs[2] + 256,
        vf->coefs[2] + 256,
        vf->coefs[3] + 256);
    memcpy((void *)vf->prev,(void *)yuvptr,fsize);
    return 0;
}

void cleanup(VideoFilter *filter)
{
    ThisFilter *vf = (ThisFilter *)filter;
    if(vf->line)
        free((void *)vf->line);
    if(vf->prev)
        free((void *)vf->prev);
    free(vf);
}

VideoFilter *new_filter(char *options)
{
    double LumSpac, LumTmp, ChromSpac, ChromTmp;
    double Param1, Param2, Param3;
    ThisFilter *filter = malloc(sizeof(ThisFilter));

    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->filter = &denoise3DFilter;
    filter->first = 1;
    filter->line = NULL;
    filter->prev = NULL;
    filter->size = 0;
    filter->cleanup = &cleanup;
    filter->name = (char *)FILTER_NAME;

    if (options)
    {
        switch(sscanf(options, "%lf:%lf:%lf", &Param1, &Param2, &Param3))
        {
        case 0:
            LumSpac = PARAM1_DEFAULT;
            LumTmp = PARAM3_DEFAULT;
            ChromSpac = PARAM2_DEFAULT;
            break;

        case 1:
            LumSpac = Param1;
            LumTmp = PARAM3_DEFAULT * Param1 / PARAM1_DEFAULT;
            ChromSpac = PARAM2_DEFAULT * Param1 / PARAM1_DEFAULT;
            break;

        case 2:
            LumSpac = Param1;
            LumTmp = PARAM3_DEFAULT * Param1 / PARAM1_DEFAULT;
            ChromSpac = Param2;
            break;

        case 3:
            LumSpac = Param1;
            LumTmp = Param3;
            ChromSpac = Param2;
            break;

        default:
            LumSpac = PARAM1_DEFAULT;
            LumTmp = PARAM3_DEFAULT;
            ChromSpac = PARAM2_DEFAULT;
        }
    }
    else
    {
        LumSpac = PARAM1_DEFAULT;
        LumTmp = PARAM3_DEFAULT;
        ChromSpac = PARAM2_DEFAULT;
    }

    ChromTmp = LumTmp * ChromSpac / LumSpac;
    PrecalcCoefs(filter->coefs[0], LumSpac);
    PrecalcCoefs(filter->coefs[1], LumTmp);
    PrecalcCoefs(filter->coefs[2], ChromSpac);
    PrecalcCoefs(filter->coefs[3], ChromTmp);
    return (VideoFilter *)filter;
}
