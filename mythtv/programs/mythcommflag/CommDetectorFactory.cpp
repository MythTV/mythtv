#include "CommDetectorFactory.h"
#include "ClassicCommDetector.h"
#include "CommDetector2.h"
#include "PrePostRollFlagger.h"

class MythPlayer;
class RemoteEncoder;

CommDetectorBase*
CommDetectorFactory::makeCommDetector(
    SkipType commDetectMethod,
    bool showProgress, bool fullSpeed,
    MythPlayer* player,
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
                                      player, startedAt, stopsAt,
                                      recordingStartedAt, recordingStopsAt);
    }

    if ((commDetectMethod & COMM_DETECT_2))
    {
        return new CommDetector2(
            commDetectMethod, showProgress, fullSpeed,
            player, chanid, startedAt, stopsAt,
            recordingStartedAt, recordingStopsAt, useDB);
    }

    return new ClassicCommDetector(commDetectMethod, showProgress, fullSpeed,
            player, startedAt, stopsAt, recordingStartedAt, recordingStopsAt);
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
