// ANSI C headers
#include <cmath>

// MythTV headers
#include "mythplayer.h"
#include "mythframe.h"          // VideoFrame
#include "mythlogging.h"

// Commercial Flagging headers
#include "pgm.h"
#include "CannyEdgeDetector.h"

using namespace edgeDetector;

CannyEdgeDetector::CannyEdgeDetector(void)
    : sgm(NULL)
    , sgmsorted(NULL)
    , ewidth(-1)
    , eheight(-1)
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

    double          val, sum;
    int             mask_width, rr, ii;

    /* The SGM computations require that mask_radius >= 2. */
    mask_radius = max(2, (int)roundf(TRUNCATION * sigma));
    mask_width = 2 * mask_radius + 1;

    /* Compute Gaussian mask. */
    mask = new double[mask_width];
    val = 1.0;  /* Initialize center of Gaussian mask (rr=0 => exp(0)). */
    mask[mask_radius] = val;
    sum = val;
    for (rr = 1; rr <= mask_radius; rr++)
    {
        val = exp(-(rr * rr) / TWO_SIGMA2); // Gaussian weight(rr,sigma)
        mask[mask_radius + rr] = val;
        mask[mask_radius - rr] = val;
        sum += 2 * val;
    }
    for (ii = 0; ii < mask_width; ii++)
        mask[ii] /= sum;    /* normalize to [0,1] */

    memset(&s1, 0, sizeof(s1));
    memset(&s2, 0, sizeof(s2));
    memset(&convolved, 0, sizeof(convolved));
    memset(&edges, 0, sizeof(edges));
    memset(&exclude, 0, sizeof(exclude));
}

CannyEdgeDetector::~CannyEdgeDetector(void)
{
    avpicture_free(&edges);
    avpicture_free(&convolved);
    avpicture_free(&s2);
    avpicture_free(&s1);
    if (sgmsorted)
        delete []sgmsorted;
    if (sgm)
        delete []sgm;
    if (mask)
        delete []mask;
}

int
CannyEdgeDetector::resetBuffers(int newwidth, int newheight)
{
    if (ewidth == newwidth && eheight == newheight)
        return 0;

    if (sgm) {
        /*
         * Sentinel value to determine whether or not stuff has already been
         * allocated.
         */
        avpicture_free(&s1);
        avpicture_free(&s2);
        avpicture_free(&convolved);
        avpicture_free(&edges);
        delete []sgm;
        delete []sgmsorted;
        sgm = NULL;
    }

    const int   padded_width = newwidth + 2 * mask_radius;
    const int   padded_height = newheight + 2 * mask_radius;

    if (avpicture_alloc(&s1, PIX_FMT_GRAY8, padded_width, padded_height))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CannyEdgeDetector::resetBuffers "
                                  "avpicture_alloc s1 failed");
        return -1;
    }

    if (avpicture_alloc(&s2, PIX_FMT_GRAY8, padded_width, padded_height))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CannyEdgeDetector::resetBuffers "
                                  "avpicture_alloc s2 failed");
        goto free_s1;
    }

    if (avpicture_alloc(&convolved, PIX_FMT_GRAY8, padded_width, padded_height))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CannyEdgeDetector::resetBuffers "
                                  "avpicture_alloc convolved failed");
        goto free_s2;
    }

    if (avpicture_alloc(&edges, PIX_FMT_GRAY8, newwidth, newheight))
    {
        LOG(VB_COMMFLAG, LOG_ERR, "CannyEdgeDetector::resetBuffers "
                                  "avpicture_alloc edges failed");
        goto free_convolved;
    }

    sgm = new unsigned int[padded_width * padded_height];
    sgmsorted = new unsigned int[newwidth * newheight];

    ewidth = newwidth;
    eheight = newheight;

    return 0;

free_convolved:
    avpicture_free(&convolved);
free_s2:
    avpicture_free(&s2);
free_s1:
    avpicture_free(&s1);
    return -1;
}

int
CannyEdgeDetector::setExcludeArea(int row, int col, int width, int height)
{
    exclude.row = row;
    exclude.col = col;
    exclude.width = width;
    exclude.height = height;
    return 0;
}

const AVPicture *
CannyEdgeDetector::detectEdges(const AVPicture *pgm, int pgmheight,
        int percentile)
{
    /*
     * Canny edge detection
     *
     * See
     * http://www.cs.cornell.edu/courses/CS664/2003fa/handouts/664-l6-edges-03.pdf
     */

    const int   pgmwidth = pgm->linesize[0];
    const int   padded_height = pgmheight + 2 * mask_radius;

    if (resetBuffers(pgmwidth, pgmheight))
        return NULL;

    if (pgm_convolve_radial(&convolved, &s1, &s2, pgm, pgmheight,
                mask, mask_radius))
        return NULL;

    if (edge_mark_uniform_exclude(&edges, pgmheight, mask_radius,
                sgm_init_exclude(sgm, &convolved, padded_height,
                    exclude.row + mask_radius, exclude.col + mask_radius,
                    exclude.width, exclude.height),
                sgmsorted, percentile,
                exclude.row, exclude.col, exclude.width, exclude.height))
        return NULL;

    return &edges;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
