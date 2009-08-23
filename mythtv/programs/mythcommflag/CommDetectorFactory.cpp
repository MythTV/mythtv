#include "CommDetectorFactory.h"
#include "ClassicCommDetector.h"
#include "CommDetector2.h"
#include "PrePostRollFlagger.h"

class NuppelVideoPlayer;
class RemoteEncoder;

CommDetectorBase*
CommDetectorFactory::makeCommDetector(
    SkipType commDetectMethod,
    bool showProgress, bool fullSpeed,
    NuppelVideoPlayer* nvp,
    int chanid,
    const QDateTime& startedAt,
    const QDateTime& stopsAt,
    const QDateTime& recordingStartedAt,
    const QDateTime& recordingStopsAt,
    bool useDB)
{
    if(commDetectMethod & COMM_DETECT_PREPOSTROLL)
    {
        return new PrePostRollFlagger(commDetectMethod, showProgress, fullSpeed,
                                      nvp, startedAt, stopsAt,
                                      recordingStartedAt, recordingStopsAt);
    }

    if ((commDetectMethod & COMM_DETECT_2))
    {
        return new CommDetector2(
            commDetectMethod, showProgress, fullSpeed,
            nvp, chanid, startedAt, stopsAt,
            recordingStartedAt, recordingStopsAt, useDB);
    }

    return new ClassicCommDetector(commDetectMethod, showProgress, fullSpeed,
            nvp, startedAt, stopsAt, recordingStartedAt, recordingStopsAt);
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
