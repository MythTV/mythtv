/*
 * TemplateMatcher
 *
 * Decide whether or not an image matches a given template.
 *
 * This TemplateMatcher is tuned to yield very few false positives, at the cost
 * of slightly more false negatives.
 *
 * After all images are analyzed, attempt to remove the mis-categorized images
 * based on inter-image information (e.g., minimum sequence lengths of template
 * matching or non-matching).
 *
 * This TemplateMatcher only expects to successfully match templates in a fixed
 * location. It would be trivial to add code to search the entire frame for a
 * template known to change locations, but at a very large cost of time.
 */

#ifndef __TEMPLATEMATCHER_H__
#define __TEMPLATEMATCHER_H__

extern "C" {
#include "libavcodec/avcodec.h"    /* AVFrame */
}
#include "FrameAnalyzer.h"

typedef struct AVFrame AVFrame;
class PGMConverter;
class EdgeDetector;
class TemplateFinder;

class TemplateMatcher : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    TemplateMatcher(PGMConverter *pgmc, EdgeDetector *ed, TemplateFinder *tf,
            const QString& debugdir);
    ~TemplateMatcher(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const override // FrameAnalyzer
        { return "TemplateMatcher"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes) override; // FrameAnalyzer
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame) override; // FrameAnalyzer
    int finished(long long nframes, bool final) override; // FrameAnalyzer
    int reportTime(void) const override; // FrameAnalyzer
    FrameMap GetMap(unsigned int) const override // FrameAnalyzer
        { return m_breakMap; }

    /* TemplateMatcher interface. */
    int templateCoverage(long long nframes, bool final) const;
    const FrameAnalyzer::FrameMap *getBreaks(void) const { return &m_breakMap; }
    int adjustForBlanks(const BlankFrameDetector *blankFrameDetector, long long nframes);
    int computeBreaks(FrameMap *breaks);

private:
    PGMConverter           *m_pgmConverter      {nullptr};
    EdgeDetector           *m_edgeDetector      {nullptr};
    TemplateFinder         *m_templateFinder    {nullptr};
    const struct AVFrame   *m_tmpl              {nullptr};  /* template image */
    int                     m_tmplrow           {-1};       /* template location */
    int                     m_tmplcol           {-1};       /* template location */
    int                     m_tmplwidth         {-1};       /* template dimensions */
    int                     m_tmplheight        {-1};       /* template dimensions */

    /* Per-frame info. */
    unsigned short         *m_matches           {nullptr};  /* matching pixels */
    unsigned char          *m_match             {nullptr};  /* boolean result: 1/0 */

    float                   m_fps               {0.0F};
    AVFrame                 m_cropped           {};        /* pre-allocated buffer */
    FrameAnalyzer::FrameMap m_breakMap;                    /* frameno => nframes */

    /* Debugging */
    int                     m_debugLevel        {0};
    QString                 m_debugdir;
    QString                 m_debugdata;                   /* filename */
    MythPlayer             *m_player            {nullptr};
    bool                    m_debug_matches     {false};
    bool                    m_debug_removerunts {false};
    bool                    m_matches_done      {false};
    struct timeval          m_analyze_time      {0,0};
};

#endif  /* !__TEMPLATEMATCHER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */

