/*
 * EdgeDetector
 *
 * Declare routines that are likely to be useful for any edge-detection
 * implementation.
 */

#ifndef __EDGEDETECTOR_H__
#define __EDGEDETECTOR_H__

typedef struct AVPicture AVPicture;

namespace edgeDetector {

unsigned int *sgm_init(unsigned int *sgm, const AVPicture *src, int srcheight);
int edge_mark_uniform(AVPicture *dst, int dstheight, int extramargin,
        const unsigned int *sgm, unsigned int *sgmsorted,
        int percentile);

};  /* namespace */

class EdgeDetector
{
public:
    virtual ~EdgeDetector(void);

    /* Detect edges in "pgm" image. */
    virtual const AVPicture *detectEdges(const AVPicture *pgm, int pgmheight,
            int percentile) = 0;
};

#endif  /* !__EDGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
