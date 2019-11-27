/*
 * HistogramAnalyzer
 *
 * Detect frame-to-frame changes in histograms.
 */

#ifndef __HISTOGRAMANALYZER_H__
#define __HISTOGRAMANALYZER_H__

#include "FrameAnalyzer.h"

class PGMConverter;
class BorderDetector;
class TemplateFinder;

class HistogramAnalyzer
{
public:
    /* Ctor/dtor. */
    HistogramAnalyzer(PGMConverter *pgmc, BorderDetector *bd,
            const QString& debugdir);
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
    using Histogram = unsigned char[UCHAR_MAX + 1];

    const float *getMeans(void) const { return m_mean; }
    const unsigned char *getMedians(void) const { return m_median; }
    const float *getStdDevs(void) const { return m_stddev; }
    const Histogram *getHistograms(void) const { return m_histogram; }
    const unsigned char *getMonochromatics(void) const { return m_monochromatic; }

private:
    PGMConverter         *m_pgmConverter   {nullptr};
    BorderDetector       *m_borderDetector {nullptr};

    TemplateFinder       *m_logoFinder     {nullptr};
    const struct AVFrame *m_logo           {nullptr};
    int                   m_logowidth      {-1};
    int                   m_logoheight     {-1};
    int                   m_logorr1        {-1};
    int                   m_logocc1        {-1};
    int                   m_logorr2        {-1};
    int                   m_logocc2        {-1};

    /* Per-frame info. */
    float                *m_mean          {nullptr}; /* mean pixel value */
    unsigned char        *m_median        {nullptr}; /* median pixel value */
    float                *m_stddev        {nullptr}; /* standard deviation */
    int                  *m_frow          {nullptr}; /* position of borders */
    int                  *m_fcol          {nullptr}; /* position of borders */
    int                  *m_fwidth        {nullptr}; /* area of borders */
    int                  *m_fheight       {nullptr}; /* area of borders */
    Histogram            *m_histogram     {nullptr}; /* histogram */
    unsigned char        *m_monochromatic {nullptr}; /* computed boolean */
    int                   m_histval[UCHAR_MAX + 1] {0}; /* temporary buffer */
    unsigned char        *m_buf           {nullptr}; /* temporary buffer */
    long long             m_lastframeno   {-1};

    /* Debugging */
    int                   m_debugLevel    {0};
    QString               m_debugdata;              /* filename */
    bool                  m_debug_histval {false};
    bool                  m_histval_done  {false};
    struct timeval        m_analyze_time  {0,0};
};

#endif  /* !__HISTOGRAMANALYZER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
