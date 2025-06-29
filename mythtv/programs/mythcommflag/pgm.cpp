#include <climits>

// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/mythframe.h"

// Commercial Flagging headers
#include "pgm.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
}

// TODO: verify this
/*
 * N.B.: this is really C code, but LOG, #define'd in mythlogging.h, is in
 * a C++ header file, so this has to be compiled with a C++ compiler, which
 * means this has to be a C++ source file.
 */

#if 0 /* compiler says its unused */
static enum PixelFormat pixelTypeOfVideoFrameType(VideoFrameType codec)
{
    /* XXX: how to map VideoFrameType values to PixelFormat values??? */
    switch (codec) {
    case FMT_YV12:      return AV_PIX_FMT_YUV420P;
    default:            break;
    }
    return AV_PIX_FMT_NONE;
}
#endif

int pgm_read(unsigned char *buf, int width, int height, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == nullptr)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_read fopen %1 failed: %2")
                .arg(filename, strerror(errno)));
        return -1;
    }
    // Automatically close file at function exit
    auto close_fp = [](FILE *fp2) { fclose(fp2); };
    std::unique_ptr<FILE,decltype(close_fp)> cleanup { fp, close_fp };

    int fwidth = 0;
    int fheight = 0;
    int maxgray = 0;
    int nn = fscanf(fp, "P5\n%20d %20d\n%20d\n", &fwidth, &fheight, &maxgray);
    if (nn != 3)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_read fscanf %1 failed: %2")
                .arg(filename, strerror(errno)));
        return -1;
    }

    if (fwidth != width || fheight != height || maxgray != UCHAR_MAX)
    {
        LOG(VB_COMMFLAG, LOG_ERR,
            QString("pgm_read header (%1x%2,%3) != (%4x%5,%6)")
                .arg(fwidth).arg(fheight).arg(maxgray)
                .arg(width).arg(height).arg(UCHAR_MAX));
        return -1;
    }

    for (ptrdiff_t rr = 0; rr < height; rr++)
    {
        if (fread(buf + (rr * width), 1, width, fp) != (size_t)width)
        {
            LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_read fread %1 failed: %2")
                    .arg(filename, strerror(errno)));
            return -1;
        }
    }
    return 0;
}

int pgm_write(const unsigned char *buf, int width, int height,
              const char *filename)
{
    /* Copied from libavcodec/apiexample.c */

    FILE *fp = fopen(filename, "w");
    if (fp == nullptr)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_write fopen %1 failed: %2")
                .arg(filename, strerror(errno)));
        return -1;
    }
    // Automatically close file at function exit
    auto close_fp = [](FILE *fp2) { fclose(fp2); };
    std::unique_ptr<FILE,decltype(close_fp)> cleanup { fp, close_fp };

    (void)fprintf(fp, "P5\n%d %d\n%d\n", width, height, UCHAR_MAX);
    for (ptrdiff_t rr = 0; rr < height; rr++)
    {
        if (fwrite(buf + (rr * width), 1, width, fp) != (size_t)width)
        {
            LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_write fwrite %1 failed: %2")
                    .arg(filename, strerror(errno)));
            return -1;
        }
    }
    return 0;
}

static int pgm_expand(AVFrame *dst, const AVFrame *src, int srcheight,
                      int extratop, int extraright, int extrabottom,
                      int extraleft)
{
    /* Pad all edges with the edge color. */
    const int           srcwidth = src->linesize[0];
    const int           newwidth = srcwidth + extraleft + extraright;
    const int           newheight = srcheight + extratop + extrabottom;

    /* Copy the image. */
    for (ptrdiff_t rr = 0; rr < srcheight; rr++)
    {
        memcpy(dst->data[0] + ((rr + extratop) * newwidth) + extraleft,
                src->data[0] + (rr * srcwidth),
                srcwidth);
    }

    /* Pad the top. */
    const uchar *srcdata = src->data[0];
    for (ptrdiff_t rr = 0; rr < extratop; rr++)
        memcpy(dst->data[0] + (rr * newwidth) + extraleft, srcdata, srcwidth);

    /* Pad the bottom. */
    srcdata = src->data[0] + (static_cast<ptrdiff_t>(srcheight - 1) * srcwidth);
    for (ptrdiff_t rr = extratop + srcheight; rr < newheight; rr++)
        memcpy(dst->data[0] + (rr * newwidth) + extraleft, srcdata, srcwidth);

    /* Pad the left. */
    for (ptrdiff_t rr = 0; rr < newheight; rr++)
    {
        memset(dst->data[0] + (rr * newwidth),
                dst->data[0][(rr * newwidth) + extraleft],
                extraleft);
    }

    /* Pad the right. */
    for (ptrdiff_t rr = 0; rr < newheight; rr++)
    {
        memset(dst->data[0] + (rr * newwidth) + extraleft + srcwidth,
                dst->data[0][(rr * newwidth) + extraleft + srcwidth - 1],
                extraright);
    }

    return 0;
}

static int pgm_expand_uniform(AVFrame *dst, const AVFrame *src,
                              int srcheight, int extramargin)
{
    return pgm_expand(dst, src, srcheight,
            extramargin, extramargin, extramargin, extramargin);
}

int pgm_crop(AVFrame *dst, const AVFrame *src,
             [[maybe_unused]] int srcheight,
             int srcrow, int srccol, int cropwidth, int cropheight)
{
    const int   srcwidth = src->linesize[0];

    if (dst->linesize[0] != cropwidth)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_crop want width %1, have %2")
                .arg(cropwidth).arg(dst->linesize[0]));
        return -1;
    }

    for (ptrdiff_t rr = 0; rr < cropheight; rr++)
    {
        memcpy(dst->data[0] + (rr * cropwidth),
                src->data[0] + ((srcrow + rr) * srcwidth) + srccol,
                cropwidth);
    }

    return 0;
}

int pgm_overlay(AVFrame *dst, const AVFrame *s1, int s1height,
                int s1row, int s1col, const AVFrame *s2, int s2height)
{
    const int   dstwidth = dst->linesize[0];
    const int   s1width = s1->linesize[0];
    const int   s2width = s2->linesize[0];

    if (dstwidth != s1width)
    {
        LOG(VB_COMMFLAG, LOG_ERR, QString("pgm_overlay want width %1, have %2")
                .arg(s1width).arg(dst->linesize[0]));
        return -1;
    }

    // av_image_copy is badly designed to require writeable
    // pointers to the read-only data, so copy the pointers here
    std::array<const uint8_t*,4> src_data
        {s1->data[0], s1->data[1], s1->data[2], s1->data[3]};

    av_image_copy(dst->data, dst->linesize, src_data.data(), s1->linesize,
        AV_PIX_FMT_GRAY8, s1width, s1height);

    /* Overwrite overlay area of "dst" with "s2". */
    for (ptrdiff_t rr = 0; rr < s2height; rr++)
    {
        memcpy(dst->data[0] + ((s1row + rr) * s1width) + s1col,
                s2->data[0] + (rr * s2width),
                s2width);
    }

    return 0;
}

int pgm_convolve_radial(AVFrame *dst, AVFrame *s1, AVFrame *s2,
                        const AVFrame *src, int srcheight,
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
    const int       newwidth = srcwidth + (2 * mask_radius);
    const int       newheight = srcheight + (2 * mask_radius);

    /* Get a padded copy of the src image for use by the convolutions. */
    if (pgm_expand_uniform(s1, src, srcheight, mask_radius))
        return -1;

    /* copy s1 to s2 and dst */

    // av_image_copy is badly designed to require writeable
    // pointers to the read-only data, so copy the pointers here
    std::array<const uint8_t*,4> src_data
        {s1->data[0], s1->data[1], s1->data[2], s1->data[3]};

    av_image_copy(s2->data, s2->linesize, src_data.data(), s1->linesize,
        AV_PIX_FMT_GRAY8, newwidth, newheight);
    av_image_copy(dst->data, dst->linesize, src_data.data(), s1->linesize,
        AV_PIX_FMT_GRAY8, newwidth, newheight);

    /* "s1" convolve with column vector => "s2" */
    int rr2 = mask_radius + srcheight;
    int cc2 = mask_radius + srcwidth;
    for (int rr = mask_radius; rr < rr2; rr++)
    {
        for (int cc = mask_radius; cc < cc2; cc++)
        {
            double sum = 0;
            for (int ii = -mask_radius; ii <= mask_radius; ii++)
            {
                sum += mask[ii + mask_radius] *
                    s1->data[0][((rr + ii) * newwidth) + cc];
            }
            s2->data[0][(rr * newwidth) + cc] = lround(sum);
        }
    }

    /* "s2" convolve with row vector => "dst" */
    for (int rr = mask_radius; rr < rr2; rr++)
    {
        for (int cc = mask_radius; cc < cc2; cc++)
        {
            double sum = 0;
            for (int ii = -mask_radius; ii <= mask_radius; ii++)
            {
                sum += mask[ii + mask_radius] *
                    s2->data[0][(rr * newwidth) + cc + ii];
            }
            dst->data[0][(rr * newwidth) + cc] = lround(sum);
        }
    }

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
