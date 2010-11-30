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
class TemplateMatcher;

class BlankFrameDetector : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    BlankFrameDetector(HistogramAnalyzer *ha, QString debugdir);

    /* FrameAnalyzer interface. */
    const char *name(void) const { return "BlankFrameDetector"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes);
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame);
    int finished(long long nframes, bool final);
    int reportTime(void) const;
    FrameMap GetMap(unsigned int index) const
        { return (index) ? blankMap : breakMap; }

    /* BlankFrameDetector interface. */
    const FrameAnalyzer::FrameMap *getBlanks(void) const { return &blankMap; }
    int computeForLogoSurplus(const TemplateMatcher *tm);
    int computeForLogoDeficit(const TemplateMatcher *tm);
    int computeBreaks(FrameMap *breaks);

private:
    HistogramAnalyzer       *histogramAnalyzer;
    float                   fps;

    FrameAnalyzer::FrameMap blankMap;
    FrameAnalyzer::FrameMap breakMap;

    /* Debugging */
    int                     debugLevel;
};

#endif  /* !__BLANKFRAMEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
