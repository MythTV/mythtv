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
#include "libavcodec/avcodec.h"    /* AVPicture */
}
#include "FrameAnalyzer.h"

typedef struct AVPicture AVPicture;
class PGMConverter;
class EdgeDetector;
class TemplateFinder;

class TemplateMatcher : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    TemplateMatcher(PGMConverter *pgmc, EdgeDetector *ed, TemplateFinder *tf,
            QString debugdir);
    ~TemplateMatcher(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const { return "TemplateMatcher"; }
    enum analyzeFrameResult MythPlayerInited(MythPlayer *player,
            long long nframes);
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame);
    int finished(long long nframes, bool final);
    int reportTime(void) const;
    FrameMap GetMap(unsigned int) const { return breakMap; }

    /* TemplateMatcher interface. */
    int templateCoverage(long long nframes, bool final) const;
    const FrameAnalyzer::FrameMap *getBreaks(void) const { return &breakMap; }
    int adjustForBlanks(const BlankFrameDetector *bf, long long nframes);
    int computeBreaks(FrameMap *breaks);

private:
    PGMConverter            *pgmConverter;
    EdgeDetector            *edgeDetector;
    TemplateFinder          *templateFinder;
    const struct AVPicture  *tmpl;                  /* template image */
    int                     tmplrow, tmplcol;       /* template location */
    int                     tmplwidth, tmplheight;  /* template dimensions */

    /* Per-frame info. */
    unsigned short          *matches;               /* matching pixels */
    unsigned char           *match;                 /* boolean result: 1/0 */

    float                   fps;
    AVPicture               cropped;                /* pre-allocated buffer */
    FrameAnalyzer::FrameMap breakMap;               /* frameno => nframes */

    /* Debugging */
    int                     debugLevel;
    QString                 debugdir;
    QString                 debugdata;              /* filename */
    MythPlayer             *player;
    bool                    debug_matches;
    bool                    debug_removerunts;
    bool                    matches_done;
    struct timeval          analyze_time;
};

#endif  /* !__TEMPLATEMATCHER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */

