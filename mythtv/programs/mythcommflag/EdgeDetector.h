/*
 * EdgeDetector
 *
 * Declare routines that are likely to be useful for any edge-detection
 * implementation.
 */

#ifndef __EDGEDETECTOR_H__
#define __EDGEDETECTOR_H__

using AVFrame = struct AVFrame;

namespace edgeDetector {

/* Pass all zeroes to not exclude any areas from examination. */

unsigned int *sgm_init_exclude(unsigned int *sgm,
        const AVFrame *src, int srcheight,
        int excluderow, int excludecol, int excludewidth, int excludeheight);

int edge_mark_uniform_exclude(AVFrame *dst, int dstheight, int extramargin,
        const unsigned int *sgm, unsigned int *sgmsorted, int percentile,
        int excluderow, int excludecol, int excludewidth, int excludeheight);

};  /* namespace */

class EdgeDetector
{
public:
    virtual ~EdgeDetector(void) = default;

    /* Exclude an area from edge detection. */
    virtual int setExcludeArea(int row, int col, int width, int height);

    /* Detect edges in "pgm" image. */
    virtual const AVFrame *detectEdges(const AVFrame *pgm, int pgmheight,
            int percentile) = 0;
};

#endif  /* !__EDGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
