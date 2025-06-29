// C++ headers
#include <algorithm>
#include <cstdlib>

#include "libmythbase/mythconfig.h"

// avlib/ffmpeg headers
extern "C" {
#include "libavcodec/avcodec.h"        // AVFrame
}

// MythTV headers
#include "libmythtv/mythframe.h"       // VideoFrame
#include "libmythtv/mythplayer.h"

// Commercial Flagging headers
#include "EdgeDetector.h"
#include "FrameAnalyzer.h"

namespace edgeDetector {

using namespace frameAnalyzer;

unsigned int *
sgm_init_exclude(unsigned int *sgm, const AVFrame *src, int srcheight,
        int excluderow, int excludecol, int excludewidth, int excludeheight)
{
    /*
     * Squared Gradient Magnitude (SGM) calculations: use a 45-degree rotated
     * set of axes.
     *
     * Intuitively, the SGM of a pixel is a measure of the "edge intensity" of
     * that pixel: how much it differs from its neighbors.
     */
    const size_t    srcwidth = src->linesize[0];

    memset(sgm, 0, srcwidth * srcheight * sizeof(*sgm));
    int rr2 = srcheight - 1;
    int cc2 = srcwidth - 1;
    for (int rr = 0; rr < rr2; rr++)
    {
        for (int cc = 0; cc < cc2; cc++)
        {
            if (!rrccinrect(rr, cc, excluderow, excludecol,
                        excludewidth, excludeheight))
            {
                uchar *rr0 = &src->data[0][(rr * srcwidth) + cc];
                uchar *rr1 = &src->data[0][((rr + 1) * srcwidth) + cc];
                int dx = rr1[1] - rr0[0];   /* southeast - northwest */
                int dy = rr1[0] - rr0[1];   /* southwest - northeast */
                sgm[(rr * srcwidth) + cc] = (dx * dx) + (dy * dy);
            }
        }
    }
    return sgm;
}

#ifdef LATER
unsigned int *
sgm_init(unsigned int *sgm, const AVFrame *src, int srcheight)
{
    return sgm_init_exclude(sgm, src, srcheight, 0, 0, 0, 0);
}
#endif /* LATER */

static int sort_ascending(const void *aa, const void *bb)
{
    return *(unsigned int*)aa - *(unsigned int*)bb;
}

static int
edge_mark(AVFrame *dst, int dstheight,
        int extratop, int extraright,
        [[maybe_unused]] int extrabottom,
        int extraleft,
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
    static constexpr int kMinThresholdPct = 95;

    const int           dstwidth = dst->linesize[0];
    const int           padded_width = extraleft + dstwidth + extraright;

    /*
     * sgm: SGM values of padded (convolved) image
     *
     * sgmsorted: sorted SGM values of unexcluded areas of unpadded image (same
     * dimensions as "dst").
     */
    int nn = 0;
    for (int rr = 0; rr < dstheight; rr++)
    {
        for (int cc = 0; cc < dstwidth; cc++)
        {
            if (!rrccinrect(rr, cc, excluderow, excludecol,
                        excludewidth, excludeheight))
            {
                sgmsorted[nn++] = sgm[((extratop + rr) * padded_width) +
                    extraleft + cc];
            }
        }
    }

    int dstnn = dstwidth * dstheight;
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

    int ii = percentile * nn / 100;
    uint thresholdval = sgmsorted[ii];

    /*
     * Try not to pick up too many edges, and eliminate degenerate edge-less
     * cases.
     */
    int first = ii;
    for ( ; first > 0 && sgmsorted[first] == thresholdval; first--) ;
    if (sgmsorted[first] != thresholdval)
        first++;
    if (first * 100 / nn < kMinThresholdPct)
    {
        int last = ii;
        int last2 = nn - 1;
        for ( ; last < last2 && sgmsorted[last] == thresholdval;
                last++) ;
        if (sgmsorted[last] != thresholdval)
            last--;

        uint newthresholdval = sgmsorted[std::min(last + 1, nn - 1)];
        if (thresholdval == newthresholdval)
        {
            /* Degenerate case; no edges (e.g., blank frame). */
            return 0;
        }

        thresholdval = newthresholdval;
    }

    /* sgm is a padded matrix; dst is the unpadded matrix. */
    for (int rr = 0; rr < dstheight; rr++)
    {
        for (int cc = 0; cc < dstwidth; cc++)
        {
            if (!rrccinrect(rr, cc, excluderow, excludecol,
                        excludewidth, excludeheight) &&
                    sgm[((extratop + rr) * padded_width) + extraleft + cc] >=
                        thresholdval)
                dst->data[0][(rr * dstwidth) + cc] = UCHAR_MAX;
        }
    }
    return 0;
}

#ifdef LATER
int edge_mark_uniform(AVFrame *dst, int dstheight, int extramargin,
        const unsigned int *sgm, unsigned int *sgmsorted,
        int percentile)
{
    return edge_mark(dst, dstheight,
            extramargin, extramargin, extramargin, extramargin,
            sgm, sgmsorted, percentile, 0, 0, 0, 0);
}
#endif /* LATER */

int edge_mark_uniform_exclude(AVFrame *dst, int dstheight, int extramargin,
        const unsigned int *sgm, unsigned int *sgmsorted, int percentile,
        int excluderow, int excludecol, int excludewidth, int excludeheight)
{
    return edge_mark(dst, dstheight,
            extramargin, extramargin, extramargin, extramargin,
            sgm, sgmsorted, percentile,
            excluderow, excludecol, excludewidth, excludeheight);
}

};  /* namespace */

int
EdgeDetector::setExcludeArea([[maybe_unused]] int row,
                             [[maybe_unused]] int col,
                             [[maybe_unused]] int width,
                             [[maybe_unused]] int height)
{
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
