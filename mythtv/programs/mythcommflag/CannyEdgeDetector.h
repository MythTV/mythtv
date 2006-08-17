/*
 * CannyEdgeDetector
 *
 * Implement the Canny edge detection algorithm.
 */

#ifndef __CANNYEDGEDETECTOR_H__
#define __CANNYEDGEDETECTOR_H__

#include "avcodec.h"    /* AVPicture */
#include "EdgeDetector.h"

typedef struct VideoFrame_ VideoFrame;
class NuppelVideoPlayer;

class CannyEdgeDetector : public EdgeDetector
{
public:
    CannyEdgeDetector(void);
    ~CannyEdgeDetector(void);
    int nuppelVideoPlayerInited(const NuppelVideoPlayer *nvp,
            int width, int height);
    const AVPicture *detectEdges(const AVPicture *pgm, int pgmheight,
            int percentile);

private:
    int resetBuffers(int pgmwidth, int pgmheight);

    double          *mask;                  /* pre-computed Gaussian mask */
    int             mask_radius;            /* radius of mask */

    unsigned int    *sgm, *sgmsorted;       /* squared-gradient magnitude */
    AVPicture       s1, s2, convolved;      /* smoothed grayscale frame */
    int             ewidth, eheight;        /* dimensions */
    AVPicture       edges;                  /* detected edges */
};

#endif  /* !__CANNYEDGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
