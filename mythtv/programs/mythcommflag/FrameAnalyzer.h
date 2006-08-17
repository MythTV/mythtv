/*
 * FrameAnalyzer
 *
 * Provide a generic interface for plugging in frame analysis algorithms.
 */

#ifndef __FRAMEANALYZER_H__
#define __FRAMEANALYZER_H__

/* Base class for commercial flagging frame analyzers. */

#include <limits.h>
#include <qmap.h>

typedef struct VideoFrame_ VideoFrame;
class NuppelVideoPlayer;

class FrameAnalyzer
{
public:
    virtual ~FrameAnalyzer(void) { }

    virtual const char *name(void) const = 0;

    /* return extra buffer time (seconds) needed for "real-time" flagging. */
    virtual int extraBuffer(int preroll) const {
        (void)preroll;
        return 0;
    }

    /* Analyze a frame. */
    enum analyzeFrameResult {
        ANALYZE_OK,         /* Analysis OK */
        ANALYZE_ERROR,      /* Recoverable error */
        ANALYZE_FINISHED,   /* Analysis complete, don't need more frames. */
        ANALYZE_FATAL,      /* Don't use this analyzer anymore. */
    };

    virtual enum analyzeFrameResult nuppelVideoPlayerInited(
            NuppelVideoPlayer *nvp) {
        (void)nvp;
        return ANALYZE_OK;
    };

    /*
     * Populate *pNextFrame with the next frame number desired by this
     * analyzer.
     */
    static const long long ANYFRAME = LONG_LONG_MAX;
    static const long long NEXTFRAME = -1;
    virtual enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame) = 0;

    /* Static analysis. */
    virtual int finished(void) { return 0; }

    /* Allow for inter-FrameAnalyzer analysis. */
    virtual int finished2(void) { return 0; }

    virtual int reportTime(void) const { return 0; }

    virtual bool isContent(long long frameno) const {
        (void)frameno;
        return false;
    }

    typedef QMap<long long, long long> FrameMap;    /* frameno => nframes */
};

namespace frameAnalyzer {

void frameAnalyzerReportMap(const FrameAnalyzer::FrameMap *frameMap,
        float fps, const char *comment);

void frameAnalyzerReportMapms(const FrameAnalyzer::FrameMap *frameMap,
        float fps, const char *comment);

}; /* namespace */

#endif  /* !__FRAMEANALYZER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
