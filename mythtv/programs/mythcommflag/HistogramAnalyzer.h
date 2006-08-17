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

class HistogramAnalyzer
{
public:
    /* Ctor/dtor. */
    HistogramAnalyzer(PGMConverter *pgmc, BorderDetector *bd,
            QString debugdir);
    ~HistogramAnalyzer(void);

    enum FrameAnalyzer::analyzeFrameResult nuppelVideoPlayerInited(
            NuppelVideoPlayer *nvp);
    void setLogoState(TemplateFinder *finder);
    enum FrameAnalyzer::analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno);
    int finished(void);
    int reportTime(void) const;

    const unsigned char *getBlanks(void) const { return blank; }
    const float *getMeans(void) const { return mean; }
    const unsigned char *getMedians(void) const { return median; }
    const float *getStdDevs(void) const { return stddev; }

private:
    PGMConverter            *pgmConverter;
    BorderDetector          *borderDetector;

    TemplateFinder          *logoFinder;
    const struct AVPicture  *logo;
    int                     logowidth, logoheight;
    int                     logorr1, logocc1, logorr2, logocc2;

    long long               nframes;                /* total frame count */

    /* Per-frame info. */
    unsigned char           *blank;                 /* boolean */
    float                   *mean;                  /* mean pixel value */
    unsigned char           *median;                /* median pixel value */
    float                   *stddev;                /* standard deviation */
    int                     *frow, *fcol;           /* position of borders */
    int                     *fwidth, *fheight;      /* area of borders */
    unsigned char           *buf;                   /* temporary buffer */

    /* Debugging */
    int                     debugLevel;
    QString                 debugdata;              /* filename */
    bool                    debug_histval;
    bool                    histval_done;
    struct timeval          analyze_time;
};

#endif  /* !__HISTOGRAMANALYZER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
