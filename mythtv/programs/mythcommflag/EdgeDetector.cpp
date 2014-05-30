// ANSI C headers
#include <cstdlib>

// C++ headers
#include <algorithm>
using namespace std;

#include "mythconfig.h"

// avlib/ffmpeg headers
extern "C" {
#include "libavcodec/avcodec.h"        // AVPicture
}

// MythTV headers
#include "mythframe.h"          // VideoFrame
#include "mythplayer.h"

// Commercial Flagging headers
#include "FrameAnalyzer.h"
#include "EdgeDetector.h"

namespace edgeDetector {

using namespace frameAnalyzer;

unsigned int *
sgm_init_exclude(unsigned int *sgm, const AVPicture *src, int srcheight,
        int excluderow, int excludecol, int excludewidth, int excludeheight)
{
    /*
     * Squared Gradient Magnitude (SGM) calculations: use a 45-degree rotated
     * set of axes.
     *
     * Intuitively, the SGM of a pixel is a measure of the "edge intensity" of
     * that pixel: how much it differs from its neighbors.
     */
    const int       srcwidth = src->linesize[0];
    int             rr, cc, dx, dy, rr2, cc2;
    unsigned char   *rr0, *rr1;

    memset(sgm, 0, srcwidth * srcheight * sizeof(*sgm));
    rr2 = srcheight - 1;
    cc2 = srcwidth - 1;
    for (rr = 0; rr < rr2; rr++)
    {
        for (cc = 0; cc < cc2; cc++)
        {
            if (!rrccinrect(rr, cc, excluderow, excludecol,
                        excludewidth, excludeheight))
            {
                rr0 = &src->data[0][rr * srcwidth + cc];
                rr1 = &src->data[0][(rr + 1) * srcwidth + cc];
                dx = rr1[1] - rr0[0];   /* southeast - northwest */
                dy = rr1[0] - rr0[1];   /* southwest - northeast */
                sgm[rr * srcwidth + cc] = dx * dx + dy * dy;
            }
        }
    }
    return sgm;
}

#ifdef LATER
unsigned int *
sgm_init(unsigned int *sgm, const AVPicture *src, int srcheight)
{
    return sgm_init_exclude(sgm, src, srcheight, 0, 0, 0, 0);
}
#endif /* LATER */

static int sort_ascending(const void *aa, const void *bb)
{
    return *(unsigned int*)aa - *(unsigned int*)bb;
}

static int
edge_mark(AVPicture *dst, int dstheight,
        int extratop, int extraright, int extrabottom, int extraleft,
        const unsigned int *sgm, unsigned int *sgmsorted, int percentile,
        int excluderow, int excludecol, int excludewidth, int excludeheight)
{
    /*
     * TUNABLE:
     *
     * Conventionally, the Canny edge detector should select for intensities at
     * the 95th percentile or higher. In case the requested percentile actually
     * yields something lower (degenerate cases), pick the next unique
     * intensity, to try to salvage useful data.
     */
    static const int    MINTHRESHOLDPCT = 95;

    const int           dstwidth = dst->linesize[0];
    const int           padded_width = extraleft + dstwidth + extraright;
    unsigned int        thresholdval;
    int                 nn, dstnn, ii, rr, cc, first, last, last2;

    (void)extrabottom;  /* gcc */

    /*
     * sgm: SGM values of padded (convolved) image
     *
     * sgmsorted: sorted SGM values of unexcluded areas of unpadded image (same
     * dimensions as "dst").
     */
    nn = 0;
    for (rr = 0; rr < dstheight; rr++)
    {
        for (cc = 0; cc < dstwidth; cc++)
        {
            if (!rrccinrect(rr, cc, excluderow, excludecol,
                        excludewidth, excludeheight))
            {
                sgmsorted[nn++] = sgm[(extratop + rr) * padded_width +
                    extraleft + cc];
            }
        }
    }

    dstnn = dstwidth * dstheight;
#if 0
    assert(nn == dstnn -
            (min(max(0, excluderow + excludeheight), dstheight) -
                min(max(0, excluderow), dstheight)) *
            (min(max(0, excludecol + excludewidth), dstwidth) -
                min(max(0, excludecol), dstwidth)));
#endif
    memset(dst->data[0], 0, dstnn * sizeof(*dst->data[0]));

    if (!nn)
    {
            /* Degenerate case (entire area excluded from analysis). */
            return 0;
    }

    qsort(sgmsorted, nn, sizeof(*sgmsorted), sort_ascending);

    ii = percentile * nn / 100;
    thresholdval = sgmsorted[ii];

    /*
     * Try not to pick up too many edges, and eliminate degenerate edge-less
     * cases.
     */
    for (first = ii; first > 0 && sgmsorted[first] == thresholdval; first--) ;
    if (sgmsorted[first] != thresholdval)
        first++;
    if (first * 100 / nn < MINTHRESHOLDPCT)
    {
        unsigned int    newthresholdval;

        last2 = nn - 1;
        for (last = ii; last < last2 && sgmsorted[last] == thresholdval;
                last++) ;
        if (sgmsorted[last] != thresholdval)
            last--;

        newthresholdval = sgmsorted[min(last + 1, nn - 1)];
        if (thresholdval == newthresholdval)
        {
            /* Degenerate case; no edges (e.g., blank frame). */
            return 0;
        }

        thresholdval = newthresholdval;
    }

    /* sgm is a padded matrix; dst is the unpadded matrix. */
    for (rr = 0; rr < dstheight; rr++)
    {
        for (cc = 0; cc < dstwidth; cc++)
        {
            if (!rrccinrect(rr, cc, excluderow, excludecol,
                        excludewidth, excludeheight) &&
                    sgm[(extratop + rr) * padded_width + extraleft + cc] >=
                        thresholdval)
                dst->data[0][rr * dstwidth + cc] = UCHAR_MAX;
        }
    }
    return 0;
}

#ifdef LATER
int edge_mark_uniform(AVPicture *dst, int dstheight, int extramargin,
        const unsigned int *sgm, unsigned int *sgmsorted,
        int percentile)
{
    return edge_mark(dst, dstheight,
            extramargin, extramargin, extramargin, extramargin,
            sgm, sgmsorted, percentile, 0, 0, 0, 0);
}
#endif /* LATER */

int edge_mark_uniform_exclude(AVPicture *dst, int dstheight, int extramargin,
        const unsigned int *sgm, unsigned int *sgmsorted, int percentile,
        int excluderow, int excludecol, int excludewidth, int excludeheight)
{
    return edge_mark(dst, dstheight,
            extramargin, extramargin, extramargin, extramargin,
            sgm, sgmsorted, percentile,
            excluderow, excludecol, excludewidth, excludeheight);
}

};  /* namespace */

EdgeDetector::~EdgeDetector(void)
{
}

int
EdgeDetector::setExcludeArea(int row, int col, int width, int height)
{
    (void)row;  /* gcc */
    (void)col;  /* gcc */
    (void)width;    /* gcc */
    (void)height;   /* gcc */
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
