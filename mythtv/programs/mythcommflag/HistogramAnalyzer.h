/*
 * HistogramAnalyzer
 *
 * Detect frame-to-frame changes in histograms.
 */

#ifndef __HISTOGRAMANALYZER_H__
#define __HISTOGRAMANALYZER_H__

#include "FrameAnalyzer.h"

typedef struct AVPicture AVPicture;
class PGMConverter;
class BorderDetector;
class TemplateFinder;

class HistogramAnalyzer : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    HistogramAnalyzer(PGMConverter *pgmc, BorderDetector *bd,
            QString debugdir);
    ~HistogramAnalyzer(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const { return "HistogramAnalyzer"; }
    enum analyzeFrameResult nuppelVideoPlayerInited(NuppelVideoPlayer *nvp);
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame);
    int finished(void);
    int reportTime(void) const;
    bool isContent(long long frameno) const { return iscontent[frameno] > 0; }

    /* HistogramAnalyzer interface. */
    void setTemplateState(TemplateFinder *tf);
    bool getSkipBlanks(void) const { return skipblanks; }
    const FrameAnalyzer::FrameMap getBlanks(void) const { return blankMap; }
    void clear(void);

    static const unsigned int   HISTOGRAM_MAXCOLOR = UCHAR_MAX;
    typedef unsigned int        Histogram[HISTOGRAM_MAXCOLOR + 1];

private:
    PGMConverter            *pgmConverter;
    BorderDetector          *borderDetector;
    TemplateFinder          *templateFinder;
    long long               nframes;                /* total frame count */
    float                   fps;
    unsigned int            npixels;
    bool                    skipblanks;             /* blanks as commercials */

    /* Per-frame info. */
#ifdef LATER
    Histogram               histogram;
#endif /* LATER */
    unsigned long long      *histval;
    unsigned char           *iscontent;             /* boolean result: 1/0 */

    FrameAnalyzer::FrameMap blankMap;               /* frameno => nframes */
    FrameAnalyzer::FrameMap breakMap;               /* frameno => nframes */

    /* Debugging */
    int                     debugLevel;
    QString                 debugdir;
    QString                 debugdata;              /* filename */
    bool                    debug_histval;
    bool                    histval_done;
    struct timeval          analyze_time;
};

#endif  /* !__HISTOGRAMANALYZER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
