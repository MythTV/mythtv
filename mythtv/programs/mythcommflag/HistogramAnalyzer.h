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
    ~HistogramAnalyzer();

    enum FrameAnalyzer::analyzeFrameResult MythPlayerInited(
            MythPlayer *player, long long nframes);
    void setLogoState(TemplateFinder *finder);
    static const long long UNCACHED = -1;
    enum FrameAnalyzer::analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno);
    int finished(long long nframes, bool final);
    int reportTime(void) const;

    /* Each color 0-255 gets a scaled frequency counter 0-255. */
    typedef unsigned char   Histogram[UCHAR_MAX + 1];

    const float *getMeans(void) const { return mean; }
    const unsigned char *getMedians(void) const { return median; }
    const float *getStdDevs(void) const { return stddev; }
    const Histogram *getHistograms(void) const { return histogram; }
    const unsigned char *getMonochromatics(void) const { return monochromatic; }

private:
    PGMConverter            *pgmConverter;
    BorderDetector          *borderDetector;

    TemplateFinder          *logoFinder;
    const struct AVPicture  *logo;
    int                     logowidth, logoheight;
    int                     logorr1, logocc1, logorr2, logocc2;

    /* Per-frame info. */
    float                   *mean;                  /* mean pixel value */
    unsigned char           *median;                /* median pixel value */
    float                   *stddev;                /* standard deviation */
    int                     *frow, *fcol;           /* position of borders */
    int                     *fwidth, *fheight;      /* area of borders */
    Histogram               *histogram;             /* histogram */
    unsigned char           *monochromatic;         /* computed boolean */
    int                     histval[UCHAR_MAX + 1]; /* temporary buffer */
    unsigned char           *buf;                   /* temporary buffer */
    long long               lastframeno;

    /* Debugging */
    int                     debugLevel;
    QString                 debugdata;              /* filename */
    bool                    debug_histval;
    bool                    histval_done;
    struct timeval          analyze_time;
};

#endif  /* !__HISTOGRAMANALYZER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
