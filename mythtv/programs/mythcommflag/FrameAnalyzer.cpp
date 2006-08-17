#include "mythcontext.h"
#include "CommDetector2.h"
#include "FrameAnalyzer.h"

using namespace commDetector2;

namespace frameAnalyzer {

void
frameAnalyzerReportMap(const FrameAnalyzer::FrameMap *frameMap, float fps,
        const char *comment)
{
    for (FrameAnalyzer::FrameMap::const_iterator ii = frameMap->begin();
            ii != frameMap->end();
            ++ii)
    {
        long long   bb, ee, len;

        bb = ii.key();
        ee = bb + ii.data();
        len = ee - bb;

        VERBOSE(VB_COMMFLAG, QString("%1: %2-%3 (%4-%5, %6)")
                .arg(comment)
                .arg(bb, 6).arg(ee - 1, 6)
                .arg(frameToTimestamp(bb, fps))
                .arg(frameToTimestamp(ee - 1, fps))
                .arg(frameToTimestamp(len, fps)));
    }
}

void
frameAnalyzerReportMapms(const FrameAnalyzer::FrameMap *frameMap, float fps,
        const char *comment)
{
    for (FrameAnalyzer::FrameMap::const_iterator ii = frameMap->begin();
            ii != frameMap->end();
            ++ii)
    {
        long long   bb, ee, len;

        bb = ii.key();
        ee = bb + ii.data();
        len = ee - bb;

        VERBOSE(VB_COMMFLAG, QString("%1: %2-%3 (%4-%5, %6)")
                .arg(comment)
                .arg(bb, 6).arg(ee - 1, 6)
                .arg(frameToTimestamp(bb, fps))
                .arg(frameToTimestamp(ee - 1, fps))
                .arg(frameToTimestampms(len, fps)));
    }
}

};  /* namespace */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
