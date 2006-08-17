/*
 * BlankFrameDetector
 *
 * Detect blank frames based on histogram analysis.
 */

#ifndef __BLANKFRAMEDETECTOR_H__
#define __BLANKFRAMEDETECTOR_H__

#include "FrameAnalyzer.h"

typedef struct AVPicture AVPicture;
class HistogramAnalyzer;

class BlankFrameDetector : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    BlankFrameDetector(HistogramAnalyzer *ha, QString debugdir);
    ~BlankFrameDetector(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const { return "BlankFrameDetector"; }
    enum analyzeFrameResult nuppelVideoPlayerInited(NuppelVideoPlayer *nvp);
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame);
    int finished(void);
    int reportTime(void) const;

    /* BlankFrameDetector interface. */
    bool getSkipBlanks(void) const { return skipblanks; }
    const FrameAnalyzer::FrameMap *getBlanks(void) const { return &blankMap; }
    int computeBreaks(FrameMap *breaks);

private:
    HistogramAnalyzer       *histogramAnalyzer;
    long long               nframes;                /* total frame count */
    float                   fps;
    unsigned int            npixels;
    bool                    skipblanks;             /* blanks as commercials */

    FrameAnalyzer::FrameMap blankMap;
    FrameAnalyzer::FrameMap breakMap;

    /* Debugging */
    int                     debugLevel;
};

#endif  /* !__BLANKFRAMEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
