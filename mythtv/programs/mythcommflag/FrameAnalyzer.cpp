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

        /*
         * QMap'd as 0-based index, but display as 1-based index to match "Edit
         * Recording" OSD.
         */
        bb = ii.key() + 1;
        if (ii.data())
        {
            ee = bb + ii.data();
            len = ee - bb;

            VERBOSE(VB_COMMFLAG, QString("%1: %2-%3 (%4-%5, %6)")
                    .arg(comment)
                    .arg(bb, 6).arg(ee - 1, 6)
                    .arg(frameToTimestamp(bb, fps))
                    .arg(frameToTimestamp(ee - 1, fps))
                    .arg(frameToTimestamp(len, fps)));
        }
        else
        {
            VERBOSE(VB_COMMFLAG, QString("%1: %2 (%3)")
                    .arg(comment)
                    .arg(bb, 6)
                    .arg(frameToTimestamp(bb, fps)));
        }
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

        /*
         * QMap'd as 0-based index, but display as 1-based index to match "Edit
         * Recording" OSD.
         */
        bb = ii.key() + 1;
        if (ii.data())
        {
            ee = bb + ii.data();
            len = ee - bb;

            VERBOSE(VB_COMMFLAG, QString("%1: %2-%3 (%4-%5, %6)")
                    .arg(comment)
                    .arg(bb, 6).arg(ee - 1, 6)
                    .arg(frameToTimestamp(bb, fps))
                    .arg(frameToTimestamp(ee - 1, fps))
                    .arg(frameToTimestampms(len, fps)));
        }
        else
        {
            VERBOSE(VB_COMMFLAG, QString("%1: %2 (%3)")
                    .arg(comment)
                    .arg(bb, 6)
                    .arg(frameToTimestamp(bb, fps)));
        }
    }
}

long long
frameAnalyzerMapSum(const FrameAnalyzer::FrameMap *frameMap)
{
    long long sum = 0;
    for (FrameAnalyzer::FrameMap::const_iterator ii = frameMap->begin();
            ii != frameMap->end();
            ++ii)
        sum += ii.data();
    return sum;
}

};  /* namespace */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
