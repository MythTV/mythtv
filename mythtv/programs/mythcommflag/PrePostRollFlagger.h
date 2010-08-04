#ifndef PREPOSTROLLFLAGGER_H
#define PREPOSTROLLFLAGGER_H

#include "ClassicCommDetector.h"

class PrePostRollFlagger : public ClassicCommDetector
{
public:
    PrePostRollFlagger(SkipType commDetectMethod, bool showProgress,
                            bool fullSpeed, MythPlayer* player,
                            const QDateTime& startedAt_in,
                            const QDateTime& stopsAt_in,
                            const QDateTime& recordingStartedAt_in,
                            const QDateTime& recordingStopsAt_in);

    virtual void GetCommercialBreakList(frm_dir_map_t &comms);
    virtual ~PrePostRollFlagger(){}
    bool go();

private:
    long long myTotalFrames;
    long long closestAfterPre;
    long long closestBeforePre;
    long long closestAfterPost;
    long long closestBeforePost;

    void Init();

    long long findBreakInrange(long long start, long long end,
                               long long totalFrames,
                               long long &framesProcessed,
                               QTime &flagTime, bool findLast);
};

#endif // PREPOSTROLLFLAGGER_H
