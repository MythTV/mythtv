/*
 * CannyEdgeDetector
 *
 * Implement the Canny edge detection algorithm.
 */

#ifndef __CANNYEDGEDETECTOR_H__
#define __CANNYEDGEDETECTOR_H__

extern "C" {
#include "libavcodec/avcodec.h"    /* AVPicture */
}
#include "EdgeDetector.h"

typedef struct VideoFrame_ VideoFrame;
class MythPlayer;

class CannyEdgeDetector : public EdgeDetector
{
public:
    CannyEdgeDetector(void);
    ~CannyEdgeDetector(void);
    int MythPlayerInited(const MythPlayer *player, int width, int height);
    virtual int setExcludeArea(int row, int col, int width, int height);
    virtual const AVPicture *detectEdges(const AVPicture *pgm, int pgmheight,
            int percentile);

private:
    int resetBuffers(int pgmwidth, int pgmheight);

    double          *mask;                  /* pre-computed Gaussian mask */
    int             mask_radius;            /* radius of mask */

    unsigned int    *sgm, *sgmsorted;       /* squared-gradient magnitude */
    AVPicture       s1, s2, convolved;      /* smoothed grayscale frame */
    int             ewidth, eheight;        /* dimensions */
    AVPicture       edges;                  /* detected edges */

    struct {
        int         row, col, width, height;
    }               exclude;
};

#endif  /* !__CANNYEDGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
