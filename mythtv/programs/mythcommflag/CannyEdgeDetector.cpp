// ANSI C headers
#include <cmath>

// MythTV headers
#include "mythplayer.h"
#include "mythframe.h"          // VideoFrame
#include "mythlogging.h"

// Commercial Flagging headers
#include "pgm.h"
#include "CannyEdgeDetector.h"

extern "C" {
#include "libavutil/imgutils.h"
}

using namespace edgeDetector;

CannyEdgeDetector::CannyEdgeDetector(void)
{
    /*
     * In general, the Gaussian mask is truncated at a point where values cease
     * to make any meaningful contribution. The sigma=>truncation computation
     * is best done by table lookup (which I don't have here). For sigma=0.5,
     * the magic truncation value is 4.
     */
    const int       TRUNCATION = 4;
    const double    sigma = 0.5;
    const double    TWO_SIGMA2 = 2 * sigma * sigma;

    /* The SGM computations require that mask_radius >= 2. */
    m_maskRadius = max(2, (int)roundf(TRUNCATION * sigma));
    int mask_width = 2 * m_maskRadius + 1;

    /* Compute Gaussian mask. */
    m_mask = new double[mask_width];
    double val = 1.0;  /* Initialize center of Gaussian mask (rr=0 => exp(0)). */
    m_mask[m_maskRadius] = val;
    double sum = val;
    for (int rr = 1; rr <= m_maskRadius; rr++)
    {
        val = exp(-(rr * rr) / TWO_SIGMA2); // Gaussian weight(rr,sigma)
        m_mask[m_maskRadius + rr] = val;
        m_mask[m_maskRadius - rr] = val;
        sum += 2 * val;
    }
    for (int ii = 0; ii < mask_width; ii++)
        m_mask[ii] /= sum;    /* normalize to [0,1] */
}

CannyEdgeDetector::~CannyEdgeDetector(void)
{
    av_freep(&m_edges.data[0]);
    av_freep(&m_convolved.data[0]);
    av_freep(&m_s2.data[0]);
    av_freep(&m_s1.data[0]);
    delete []m_sgmSorted;
    delete []m_sgm;
    delete []m_mask;
}

int
CannyEdgeDetector::resetBuffers(int newwidth, int newheight)
{
    if (m_ewidth == newwidth && m_eheight == newheight)
        return 0;

    if (m_sgm) {
        /*
         * Sentinel value to determine whether or not stuff has already been
         * allocated.
         */
        av_freep(&m_s1.data[0]);
        av_freep(&m_s2.data[0]);
        av_freep(&m_convolved.data[0]);
        av_freep(&m_edges.data[0]);
        delete []m_sgm;
        delete []m_sgmSorted;
        m_sgm = nullptr;
    }

    const int   padded_width = newwidth + 2 * m_maskRadius;
    const int   padded_height = newheight + 2 * m_maskRadius;

    if (av_image_alloc(m_s1.data, m_s1.linesize,
        padded_width, padded_height, AV_PIX_FMT_GRAY8, IMAGE_ALIGN))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CannyEdgeDetector::resetBuffers "
                                  "av_image_alloc s1 failed");
        return -1;
    }

    if (av_image_alloc(m_s2.data, m_s2.linesize,
        padded_width, padded_height, AV_PIX_FMT_GRAY8, IMAGE_ALIGN))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CannyEdgeDetector::resetBuffers "
                                  "av_image_alloc s2 failed");
        goto free_s1;
    }

    if (av_image_alloc(m_convolved.data, m_convolved.linesize,
        padded_width, padded_height, AV_PIX_FMT_GRAY8, IMAGE_ALIGN))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CannyEdgeDetector::resetBuffers "
                                  "av_image_alloc convolved failed");
        goto free_s2;
    }

    if (av_image_alloc(m_edges.data, m_edges.linesize,
        newwidth, newheight, AV_PIX_FMT_GRAY8, IMAGE_ALIGN))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CannyEdgeDetector::resetBuffers "
                                  "av_image_alloc edges failed");
        goto free_convolved;
    }

    m_sgm = new unsigned int[padded_width * padded_height];
    m_sgmSorted = new unsigned int[newwidth * newheight];

    m_ewidth = newwidth;
    m_eheight = newheight;

    return 0;

free_convolved:
    av_freep(&m_convolved.data[0]);
free_s2:
    av_freep(&m_s2.data[0]);
free_s1:
    av_freep(&m_s1.data[0]);
    return -1;
}

int
CannyEdgeDetector::setExcludeArea(int row, int col, int width, int height)
{
    m_exclude.row = row;
    m_exclude.col = col;
    m_exclude.width = width;
    m_exclude.height = height;
    return 0;
}

const AVFrame *
CannyEdgeDetector::detectEdges(const AVFrame *pgm, int pgmheight,
        int percentile)
{
    /*
     * Canny edge detection
     *
     * See
     * http://www.cs.cornell.edu/courses/CS664/2003fa/handouts/664-l6-edges-03.pdf
     */

    const int   pgmwidth = pgm->linesize[0];
    const int   padded_height = pgmheight + 2 * m_maskRadius;

    if (resetBuffers(pgmwidth, pgmheight))
        return nullptr;

    if (pgm_convolve_radial(&m_convolved, &m_s1, &m_s2, pgm, pgmheight,
                m_mask, m_maskRadius))
        return nullptr;

    if (edge_mark_uniform_exclude(&m_edges, pgmheight, m_maskRadius,
                sgm_init_exclude(m_sgm, &m_convolved, padded_height,
                    m_exclude.row + m_maskRadius, m_exclude.col + m_maskRadius,
                    m_exclude.width, m_exclude.height),
                m_sgmSorted, percentile,
                m_exclude.row, m_exclude.col, m_exclude.width, m_exclude.height))
        return nullptr;

    return &m_edges;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
