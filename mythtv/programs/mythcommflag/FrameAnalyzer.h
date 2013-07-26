/*
 * FrameAnalyzer
 *
 * Provide a generic interface for plugging in video frame analysis algorithms.
 */

#ifndef __FRAMEANALYZER_H__
#define __FRAMEANALYZER_H__

/* Base class for commercial flagging video frame analyzers. */

#include <limits.h>

#include <QMap>

/*  
 * At least FreeBSD doesn't define LONG_LONG_MAX, but it does define  
 * __LONG_LONG_MAX__.  Who knows what other systems do the same?  
 */  
#ifndef LONG_LONG_MAX  
#define LONG_LONG_MAX  __LONG_LONG_MAX__  
#endif

typedef struct VideoFrame_ VideoFrame;
class MythPlayer;

class FrameAnalyzer
{
public:
    virtual ~FrameAnalyzer(void) { }

    virtual const char *name(void) const = 0;

    /* Analyze a video frame. */
    enum analyzeFrameResult {
        ANALYZE_OK,         /* Analysis OK */
        ANALYZE_ERROR,      /* Recoverable error */
        ANALYZE_FINISHED,   /* Analysis complete, don't need more frames. */
        ANALYZE_FATAL,      /* Don't use this analyzer anymore. */
    };


    /* 0-based frameno => nframes */
    typedef QMap<long long, long long> FrameMap;

    virtual enum analyzeFrameResult MythPlayerInited(
            MythPlayer *player, long long nframes) {
        (void)player;
        (void)nframes;
        return ANALYZE_OK;
    };

    /*
     * Populate *pNextFrame with the next frame number desired by this
     * analyzer.
     */
    static const long long ANYFRAME = LLONG_MAX;
    static const long long NEXTFRAME = -1;
    virtual enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame /* [out] */) = 0;

    virtual int finished(long long nframes, bool final) {
        (void)nframes;
        (void)final;
        return 0;
    }
    virtual int reportTime(void) const { return 0; }

    virtual FrameMap GetMap(unsigned int) const = 0;
};

namespace frameAnalyzer {

bool rrccinrect(int rr, int cc, int rrow, int rcol, int rwidth, int rheight);

void frameAnalyzerReportMap(const FrameAnalyzer::FrameMap *frameMap,
        float fps, const char *comment);

void frameAnalyzerReportMapms(const FrameAnalyzer::FrameMap *frameMap,
        float fps, const char *comment);

long long frameAnalyzerMapSum(const FrameAnalyzer::FrameMap *frameMap);

bool removeShortBreaks(FrameAnalyzer::FrameMap *breakMap, float fps,
        int minbreaklen, bool verbose);

bool removeShortSegments(FrameAnalyzer::FrameMap *breakMap, long long nframes,
    float fps, int minseglen, bool verbose);

FrameAnalyzer::FrameMap::const_iterator frameMapSearchForwards(
        const FrameAnalyzer::FrameMap *frameMap, long long mark,
        long long markend);

FrameAnalyzer::FrameMap::const_iterator frameMapSearchBackwards(
        const FrameAnalyzer::FrameMap *frameMap, long long markbegin,
        long long mark);

}; /* namespace */

#endif  /* !__FRAMEANALYZER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
