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

    void GetCommercialBreakList(frm_dir_map_t &marks) override; // ClassicCommDetector
    ~PrePostRollFlagger() override = default;
    bool go() override; // ClassicCommDetector

private:
    long long m_myTotalFrames     {0};
    long long m_closestAfterPre   {0};
    long long m_closestBeforePre  {0};
    long long m_closestAfterPost  {0};
    long long m_closestBeforePost {0};

    void Init();

    long long findBreakInrange(long long start, long long stopFrame,
                               long long totalFrames,
                               long long &framesProcessed,
                               QElapsedTimer &flagTime, bool findLast);
};

#endif // PREPOSTROLLFLAGGER_H
