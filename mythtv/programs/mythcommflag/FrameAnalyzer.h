/*
 * FrameAnalyzer
 *
 * Provide a generic interface for plugging in video frame analysis algorithms.
 */

#ifndef FRAMEANALYZER_H
#define FRAMEANALYZER_H

/* Base class for commercial flagging video frame analyzers. */

#include <climits>
#include <memory>

#include <QMap>
#include "libmythtv/mythframe.h"

/*  
 * At least FreeBSD doesn't define LONG_LONG_MAX, but it does define  
 * __LONG_LONG_MAX__.  Who knows what other systems do the same?  
 */  
#ifndef LONG_LONG_MAX  
#define LONG_LONG_MAX  __LONG_LONG_MAX__  
#endif

class MythPlayer;

class FrameAnalyzer
{
public:
    virtual ~FrameAnalyzer(void) = default;

    virtual const char *name(void) const = 0;

    /* Analyze a video frame. */
    enum analyzeFrameResult : std::uint8_t {
        ANALYZE_OK,         /* Analysis OK */
        ANALYZE_ERROR,      /* Recoverable error */
        ANALYZE_FINISHED,   /* Analysis complete, don't need more frames. */
        ANALYZE_FATAL,      /* Don't use this analyzer anymore. */
    };


    /* 0-based frameno => nframes */
    using FrameMap = QMap<long long, long long>;

    virtual enum analyzeFrameResult MythPlayerInited(
            [[maybe_unused]] MythPlayer *player,
            [[maybe_unused]] long long nframes)
    {
        return ANALYZE_OK;
    };

    /*
     * Populate *pNextFrame with the next frame number desired by this
     * analyzer.
     */
    static const long long kAnyFrame = LLONG_MAX;
    static const long long kNextFrame = -1;
    virtual enum analyzeFrameResult analyzeFrame(const MythVideoFrame *frame,
            long long frameno, long long *pNextFrame /* [out] */) = 0;

    virtual int finished([[maybe_unused]] long long nframes,
                         [[maybe_unused]] bool final)
    {
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

#endif  /* !FRAMEANALYZER_H */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
