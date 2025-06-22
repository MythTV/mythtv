/*
 * TemplateFinder
 *
 * Attempt to infer the existence of a static template across a series of
 * images by looking for stable edges. Some ideas are taken from
 * http://thomashargrove.com/logo-detection/
 *
 * By operating on the edges of images rather than the image data itself, both
 * opaque and transparent templates are robustly located (if they exist).
 * Hopefully the "core" portion of animated templates is sufficiently large and
 * stable enough for animated templates to also be discovered.
 *
 * This TemplateFinder only expects to successfully discover non- or
 * minimally-moving templates. Templates that change position during the
 * sequence of images will cause this algorithm to fail.
 */

#ifndef TEMPLATEFINDER_H
#define TEMPLATEFINDER_H

extern "C" {
#include "libavcodec/avcodec.h"    /* AVFrame */
}
#include "FrameAnalyzer.h"

class PGMConverter;
class BorderDetector;
class EdgeDetector;

class TemplateFinder : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    TemplateFinder(std::shared_ptr<PGMConverter> pgmc,
                   std::shared_ptr<BorderDetector> bd,
                   std::shared_ptr<EdgeDetector> ed,
                   MythPlayer *player, std::chrono::seconds proglen,
                   const QString& debugdir);
    TemplateFinder(std::shared_ptr<PGMConverter> pgmc,
                   std::shared_ptr<BorderDetector> bd,
                   std::shared_ptr<EdgeDetector> ed,
                   MythPlayer *player, int proglen, const QString& debugdir) :
        TemplateFinder(std::move(pgmc), std::move(bd), std::move(ed),
                       player, std::chrono::seconds(proglen), debugdir) {};
    ~TemplateFinder(void) override;

    /* FrameAnalyzer interface. */
    const char *name(void) const override // FrameAnalyzer
        { return "TemplateFinder"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes) override; // FrameAnalyzer
    enum analyzeFrameResult analyzeFrame(const MythVideoFrame *frame,
            long long frameno, long long *pNextFrame) override; // FrameAnalyzer
    int finished(long long nframes, bool final) override; // FrameAnalyzer
    int reportTime(void) const override; // FrameAnalyzer
    FrameMap GetMap(unsigned int /*index*/) const override // FrameAnalyzer
        { FrameMap map; return map; }

    /* TemplateFinder implementation. */
    const struct AVFrame *getTemplate(int *prow, int *pcol,
            int *pwidth, int *pheight) const;

private:
    int resetBuffers(int newwidth, int newheight);

    std::shared_ptr<PGMConverter>   m_pgmConverter     {nullptr};
    std::shared_ptr<BorderDetector> m_borderDetector   {nullptr};
    std::shared_ptr<EdgeDetector>   m_edgeDetector     {nullptr};

    std::chrono::seconds m_sampleTime  {20min};   /* amount of time to analyze */
    int             m_frameInterval;              /* analyze every <Interval> frames */
    long long       m_endFrame         {0};       /* end of logo detection */
    long long       m_nextFrame        {0};       /* next desired frame */

    ssize_t         m_width            {-1};      /* dimensions of frames */
    ssize_t         m_height           {-1};      /* dimensions of frames */
    unsigned int   *m_scores           {nullptr}; /* pixel "edge" scores */

    int             m_minContentRow    {INT_MAX}; /* limits of content area of images */
    int             m_minContentCol    {INT_MAX};
    int             m_maxContentRow1   {INT_MIN}; /* minrow + height ("maxrow + 1") */
    int             m_maxContentCol1   {INT_MIN}; /* mincol + width ("maxcol + 1") */

    AVFrame         m_tmpl             {};        /* logo-matching template */
    int             m_tmplRow          {-1};
    int             m_tmplCol          {-1};
    int             m_tmplWidth        {-1};
    int             m_tmplHeight       {-1};

    AVFrame         m_cropped          {};        /* cropped version of frame */
    int             m_cwidth           {-1};      /* cropped width */
    int             m_cheight          {-1};      /* cropped height */

    /* Debugging. */
    int             m_debugLevel       {0};
    QString         m_debugDir;
    QString         m_debugData;                  /* filename: template location */
    QString         m_debugTmpl;                  /* filename: logo template */
    bool            m_debugTemplate    {false};
    bool            m_debugEdgeCounts  {false};
    bool            m_debugFrames      {false};
    bool            m_tmplValid        {false};
    bool            m_tmplDone         {false};
    std::chrono::microseconds  m_analyzeTime {0us};
};

#endif  /* !TEMPLATEFINDER_H */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
