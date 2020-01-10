/*
 * CannyEdgeDetector
 *
 * Implement the Canny edge detection algorithm.
 */

#ifndef __CANNYEDGEDETECTOR_H__
#define __CANNYEDGEDETECTOR_H__

extern "C" {
#include "libavcodec/avcodec.h"    /* AVFrame */
}
#include "EdgeDetector.h"

class MythPlayer;

class CannyEdgeDetector : public EdgeDetector
{
public:
    CannyEdgeDetector(void);
    ~CannyEdgeDetector(void) override;
    CannyEdgeDetector(const CannyEdgeDetector &) = delete;            // not copyable
    CannyEdgeDetector &operator=(const CannyEdgeDetector &) = delete; // not copyable
    int MythPlayerInited(const MythPlayer *player, int width, int height);
    int setExcludeArea(int row, int col, int width, int height) override; // EdgeDetector
    const AVFrame *detectEdges(const AVFrame *pgm, int pgmheight,
            int percentile) override; // EdgeDetector

private:
    int resetBuffers(int newwidth, int newheight);

    double         *m_mask        {nullptr}; /* pre-computed Gaussian mask */
    int             m_maskRadius  {2};       /* radius of mask */

    unsigned int   *m_sgm         {nullptr}; /* squared-gradient magnitude */
    unsigned int   *m_sgmSorted   {nullptr}; /* squared-gradient magnitude */
    AVFrame         m_s1          {};        /* smoothed grayscale frame */
    AVFrame         m_s2          {};        /* smoothed grayscale frame */
    AVFrame         m_convolved   {};        /* smoothed grayscale frame */
    int             m_ewidth      {-1};      /* dimensions */
    int             m_eheight     {-1};      /* dimensions */
    AVFrame         m_edges       {};        /* detected edges */

    struct {
        int         row, col, width, height;
    }               m_exclude {};
};

#endif  /* !__CANNYEDGEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
