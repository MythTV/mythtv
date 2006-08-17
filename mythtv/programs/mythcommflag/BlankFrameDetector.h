/*
 * BlankFrameDetector
 *
 * Detect blank frames by examining whether all pixel values fall within some
 * narrow range of values.
 */

#ifndef __BLANKFRAMEDETECTOR_H__
#define __BLANKFRAMEDETECTOR_H__

#include "FrameAnalyzer.h"

typedef struct AVPicture AVPicture;
class PGMConverter;
class BorderDetector;
class TemplateFinder;

class BlankFrameDetector : public FrameAnalyzer
{
public:
    /* Ctor/dtor. */
    BlankFrameDetector(PGMConverter *pgmc, BorderDetector *bd,
            TemplateFinder *tf, QString debugdir);
    ~BlankFrameDetector(void);

    /* FrameAnalyzer interface. */
    const char *name(void) const { return "BlankFrameDetector"; }
    enum analyzeFrameResult nuppelVideoPlayerInited(NuppelVideoPlayer *nvp);
    enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame);
    int finished(void);
    bool isContent(long long frameno) const { return iscontent[frameno] > 0; }

private:
    PGMConverter            *pgmConverter;
    BorderDetector          *borderDetector;
    TemplateFinder          *templateFinder;
    long long               nframes;                /* total frame count */
    float                   fps;
    const struct AVPicture  *tmpl;                  /* template image */
    int                     tmplrow, tmplcol;       /* template location */
    int                     tmplwidth, tmplheight;  /* template dimensions */

    unsigned char           maxavgblack, maxavggrey, maxgrey;
    bool                    skipblanks;             /* blanks as commercials */

    /* Per-frame info. */
    unsigned char           *minval;                /* dimmest pixel */
    unsigned char           *avgval;                /* average value */
    unsigned char           *maxval;                /* brighest pixel */
    unsigned char           *iscontent;             /* boolean result: 1/0 */

    /* Debugging */
    int                     debugLevel;
    QString                 debugdir;
    QString                 debugdata;              /* filename */
    bool                    debug_blanks;
    bool                    blanks_done;
    struct timeval          analyze_time;
};

#endif  /* !__BLANKFRAMEDETECTOR_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
