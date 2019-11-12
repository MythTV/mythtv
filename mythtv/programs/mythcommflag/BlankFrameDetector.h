/*
 * BlankFrameDetector
 *
 * Detect blank frames based on histogram analysis.
 */

#ifndef __BLANKFRAMEDETECTOR_H__
#define __BLANKFRAMEDETECTOR_H__

#include "FrameAnalyzer.h"

class HistogramAnalyzer;
class TemplateMatcher;

class BlankFrameDetector : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    BlankFrameDetector(HistogramAnalyzer *ha, const QString& debugdir);

    /* FrameAnalyzer interface. */
    const char *name(void) const override // FrameAnalyzer
        { return "BlankFrameDetector"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes) override; // FrameAnalyzer
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame) override; // FrameAnalyzer
    int finished(long long nframes, bool final) override; // FrameAnalyzer
    int reportTime(void) const override; // FrameAnalyzer
    FrameMap GetMap(unsigned int index) const override // FrameAnalyzer
        { return (index) ? m_blankMap : m_breakMap; }

    /* BlankFrameDetector interface. */
    const FrameAnalyzer::FrameMap *getBlanks(void) const { return &m_blankMap; }
    int computeForLogoSurplus(const TemplateMatcher *templateMatcher);
    static int computeForLogoDeficit(const TemplateMatcher *templateMatcher);
    int computeBreaks(FrameMap *breaks);

private:
    HistogramAnalyzer      *m_histogramAnalyzer;
    float                   m_fps        {0.0F};

    FrameAnalyzer::FrameMap m_blankMap;
    FrameAnalyzer::FrameMap m_breakMap;

    /* Debugging */
    int                     m_debugLevel {0};
};

#endif  /* !__BLANKFRAMEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
