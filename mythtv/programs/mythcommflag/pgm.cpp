#include <climits>

#include "mythconfig.h"

extern "C" {
#include "libavcodec/avcodec.h"
}
#include "frame.h"
#include "mythlogging.h"
#include "myth_imgconvert.h"
#include "pgm.h"

// TODO: verify this
/*
 * N.B.: this is really C code, but LOG, #define'd in mythlogging.h, is in
 * a C++ header file, so this has to be compiled with a C++ compiler, which
 * means this has to be a C++ source file.
 */

static enum PixelFormat pixelTypeOfVideoFrameType(VideoFrameType codec)
{
    /* XXX: how to map VideoFrameType values to PixelFormat values??? */
    switch (codec) {
    case FMT_YV12:      return PIX_FMT_YUV420P;
    default:            break;
    }
    return PIX_FMT_NONE;
}

int pgm_fill(AVPicture *dst, const VideoFrame *frame)
{
    enum PixelFormat        srcfmt;
    AVPicture               src;

    if ((srcfmt = pixelTypeOfVideoFrameType(frame->codec)) == PIX_FMT_NONE)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_fill unknown codec: %1")
                .arg(frame->codec));
        return -1;
    }

    if (avpicture_fill(&src, frame->buf, srcfmt, frame->width,
                frame->height) < 0)
    {
        LOG(VB_COMMFLAG, LOG_ERR, "pgm_fill avpicture_fill failed");
        return -1;
    }

    if (myth_sws_img_convert(dst, PIX_FMT_GRAY8, &src, srcfmt, frame->width,
                             frame->height))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "pgm_fill img_convert failed");
        return -1;
    }

    return 0;
}

int pgm_read(unsigned char *buf, int width, int height, const char *filename)
{
    FILE        *fp;
    int         nn, fwidth, fheight, maxgray, rr;

    if (!(fp = fopen(filename, "r")))
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_read fopen %1 failed: %2")
                .arg(filename).arg(strerror(errno)));
        return -1;
    }

    if ((nn = fscanf(fp, "P5\n%20d %20d\n%20d\n",
                     &fwidth, &fheight, &maxgray)) != 3)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_read fscanf %1 failed: %2")
                .arg(filename).arg(strerror(errno)));
        goto error;
    }

    if (fwidth != width || fheight != height || maxgray != UCHAR_MAX)
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("pgm_read header (%1x%2,%3) != (%4x%5,%6)")
                .arg(fwidth).arg(fheight).arg(maxgray)
                .arg(width).arg(height).arg(UCHAR_MAX));
        goto error;
    }

    for (rr = 0; rr < height; rr++)
    {
        if (fread(buf + rr * width, 1, width, fp) != (size_t)width)
        {
            LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_read fread %1 failed: %2")
                    .arg(filename).arg(strerror(errno)));
            goto error;
        }
    }

    (void)fclose(fp);
    return 0;

error:
    (void)fclose(fp);
    return -1;
}

int pgm_write(const unsigned char *buf, int width, int height,
              const char *filename)
{
    /* Copied from libavcodec/apiexample.c */
    FILE        *fp;
    int         rr;

    if (!(fp = fopen(filename, "w")))
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_write fopen %1 failed: %2")
                .arg(filename).arg(strerror(errno)));
        return -1;
    }

    (void)fprintf(fp, "P5\n%d %d\n%d\n", width, height, UCHAR_MAX);
    for (rr = 0; rr < height; rr++)
    {
        if (fwrite(buf + rr * width, 1, width, fp) != (size_t)width)
        {
            LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_write fwrite %1 failed: %2")
                    .arg(filename).arg(strerror(errno)));
            goto error;
        }
    }

    (void)fclose(fp);
    return 0;

error:
    (void)fclose(fp);
    return -1;
}

static int pgm_expand(AVPicture *dst, const AVPicture *src, int srcheight,
                      int extratop, int extraright, int extrabottom,
                      int extraleft)
{
    /* Pad all edges with the edge color. */
    const int           srcwidth = src->linesize[0];
    const int           newwidth = srcwidth + extraleft + extraright;
    const int           newheight = srcheight + extratop + extrabottom;
    const unsigned char *srcdata;
    int                 rr;

    /* Copy the image. */
    for (rr = 0; rr < srcheight; rr++)
        memcpy(dst->data[0] + (rr + extratop) * newwidth + extraleft,
                src->data[0] + rr * srcwidth,
                srcwidth);

    /* Pad the top. */
    srcdata = src->data[0];
    for (rr = 0; rr < extratop; rr++)
        memcpy(dst->data[0] + rr * newwidth + extraleft, srcdata, srcwidth);

    /* Pad the bottom. */
    srcdata = src->data[0] + (srcheight - 1) * srcwidth;
    for (rr = extratop + srcheight; rr < newheight; rr++)
        memcpy(dst->data[0] + rr * newwidth + extraleft, srcdata, srcwidth);

    /* Pad the left. */
    for (rr = 0; rr < newheight; rr++)
        memset(dst->data[0] + rr * newwidth,
                dst->data[0][rr * newwidth + extraleft],
                extraleft);

    /* Pad the right. */
    for (rr = 0; rr < newheight; rr++)
        memset(dst->data[0] + rr * newwidth + extraleft + srcwidth,
                dst->data[0][rr * newwidth + extraleft + srcwidth - 1],
                extraright);

    return 0;
}

static int pgm_expand_uniform(AVPicture *dst, const AVPicture *src,
                              int srcheight, int extramargin)
{
    return pgm_expand(dst, src, srcheight,
            extramargin, extramargin, extramargin, extramargin);
}

int pgm_crop(AVPicture *dst, const AVPicture *src, int srcheight,
             int srcrow, int srccol, int cropwidth, int cropheight)
{
    const int   srcwidth = src->linesize[0];
    int         rr;

    if (dst->linesize[0] != cropwidth)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_crop want width %1, have %2")
                .arg(cropwidth).arg(dst->linesize[0]));
        return -1;
    }

    for (rr = 0; rr < cropheight; rr++)
        memcpy(dst->data[0] + rr * cropwidth,
                src->data[0] + (srcrow + rr) * srcwidth + srccol,
                cropwidth);

    (void)srcheight;    /* gcc */
    return 0;
}

int pgm_overlay(AVPicture *dst, const AVPicture *s1, int s1height,
                int s1row, int s1col, const AVPicture *s2, int s2height)
{
    const int   dstwidth = dst->linesize[0];
    const int   s1width = s1->linesize[0];
    const int   s2width = s2->linesize[0];
    int         rr;

    if (dstwidth != s1width)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_overlay want width %1, have %2")
                .arg(s1width).arg(dst->linesize[0]));
        return -1;
    }

    av_picture_copy(dst, s1, PIX_FMT_GRAY8, s1width, s1height);

    /* Overwrite overlay area of "dst" with "s2". */
    for (rr = 0; rr < s2height; rr++)
        memcpy(dst->data[0] + (s1row + rr) * s1width + s1col,
                s2->data[0] + rr * s2width,
                s2width);

    return 0;
}

int pgm_convolve_radial(AVPicture *dst, AVPicture *s1, AVPicture *s2,
                        const AVPicture *src, int srcheight,
                        const double *mask, int mask_radius)
{
    /*
     * Pad and convolve an image.
     *
     * "s1" and "s2" are caller-pre-allocated "scratch space" (avoid repeated
     * per-frame allocation/deallocation).
     *
     * Remove noise from image; smooth by convolving with a Gaussian mask. See
     * http://www.cogs.susx.ac.uk/users/davidy/teachvision/vision0.html
     *
     * Optimization for radially-symmetric masks: implement a single
     * two-dimensional convolution with two commutative single-dimensional
     * convolutions.
     */
    const int       srcwidth = src->linesize[0];
    const int       newwidth = srcwidth + 2 * mask_radius;
    const int       newheight = srcheight + 2 * mask_radius;
    int             ii, rr, cc, rr2, cc2;
    double          sum;

    /* Get a padded copy of the src image for use by the convolutions. */
    if (pgm_expand_uniform(s1, src, srcheight, mask_radius))
        return -1;

    /* copy s1 to s2 and dst */
    av_picture_copy(s2, s1, PIX_FMT_GRAY8, newwidth, newheight);
    av_picture_copy(dst, s1, PIX_FMT_GRAY8, newwidth, newheight);

    /* "s1" convolve with column vector => "s2" */
    rr2 = mask_radius + srcheight;
    cc2 = mask_radius + srcwidth;
    for (rr = mask_radius; rr < rr2; rr++)
    {
        for (cc = mask_radius; cc < cc2; cc++)
        {
            sum = 0;
            for (ii = -mask_radius; ii <= mask_radius; ii++)
            {
                sum += mask[ii + mask_radius] *
                    s1->data[0][(rr + ii) * newwidth + cc];
            }
            s2->data[0][rr * newwidth + cc] = (unsigned char)(sum + 0.5);
        }
    }

    /* "s2" convolve with row vector => "dst" */
    for (rr = mask_radius; rr < rr2; rr++)
    {
        for (cc = mask_radius; cc < cc2; cc++)
        {
            sum = 0;
            for (ii = -mask_radius; ii <= mask_radius; ii++)
            {
                sum += mask[ii + mask_radius] *
                    s2->data[0][rr * newwidth + cc + ii];
            }
            dst->data[0][rr * newwidth + cc] = (unsigned char)(sum + 0.5);
        }
    }

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
