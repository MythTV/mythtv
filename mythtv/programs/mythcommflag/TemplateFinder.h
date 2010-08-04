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

#ifndef __TEMPLATEFINDER_H__
#define __TEMPLATEFINDER_H__

extern "C" {
#include "avcodec.h"    /* AVPicture */
}
#include "FrameAnalyzer.h"

class PGMConverter;
class BorderDetector;
class EdgeDetector;

class TemplateFinder : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    TemplateFinder(PGMConverter *pgmc, BorderDetector *bd, EdgeDetector *ed,
            MythPlayer *player, int proglen, QString debugdir);
    ~TemplateFinder(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const { return "TemplateFinder"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes);
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame);
    int finished(long long nframes, bool final);
    int reportTime(void) const;
    FrameMap GetMap(unsigned int) const { FrameMap map; return map; }

    /* TemplateFinder implementation. */
    const struct AVPicture *getTemplate(int *prow, int *pcol,
            int *pwidth, int *pheight) const;

private:
    int resetBuffers(int newcwidth, int newcheight);

    PGMConverter    *pgmConverter;
    BorderDetector  *borderDetector;
    EdgeDetector    *edgeDetector;

    unsigned int    sampleTime;         /* amount of time to analyze */
    int             frameInterval;      /* analyze every <Interval> frames */
    long long       endFrame;           /* end of logo detection */
    long long       nextFrame;          /* next desired frame */

    int             width, height;      /* dimensions of frames */
    unsigned int    *scores;            /* pixel "edge" scores */

    int             mincontentrow;      /* limits of content area of images */
    int             mincontentcol;
    int             maxcontentrow1;     /* minrow + height ("maxrow + 1") */
    int             maxcontentcol1;     /* mincol + width ("maxcol + 1") */

    AVPicture       tmpl;               /* logo-matching template */
    int             tmplrow, tmplcol;
    int             tmplwidth, tmplheight;

    AVPicture       cropped;            /* cropped version of frame */
    int             cwidth, cheight;    /* cropped height */

    /* Debugging. */
    int             debugLevel;
    QString         debugdir;
    QString         debugdata;          /* filename: template location */
    QString         debugtmpl;          /* filename: logo template */
    bool            debug_template;
    bool            debug_edgecounts;
    bool            debug_frames;
    bool            tmpl_valid;
    bool            tmpl_done;
    struct timeval  analyze_time;
};

#endif  /* !__TEMPLATEFINDER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
