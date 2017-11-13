
#include "cutter.h"

#include <algorithm>                    // for min
using namespace std;

#include <QMap>                         // for QMap

#include "mythlogging.h"

void Cutter::SetCutList(frm_dir_map_t &deleteMap, PlayerContext *ctx)
{
    // Break each cut into two parts, the first for
    // the player and the second for the transcode loop.
    frm_dir_map_t           remainingCutList;
    frm_dir_map_t::Iterator it;
    int64_t                 start = 0;
    int64_t                 leadinLength;

    tracker.SetPlayerContext(ctx);
    foreshortenedCutList.clear();

    for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
    {
        switch(it.value())
        {
            case MARK_CUT_START:
                foreshortenedCutList[it.key()] = MARK_CUT_START;
                start = it.key();
                break;

            case MARK_CUT_END:
                leadinLength = min((int64_t)(it.key() - start),
                                   (int64_t)MAXLEADIN);
                if (leadinLength >= MINCUT)
                {
                    foreshortenedCutList[it.key() - leadinLength + 2] =
                        MARK_CUT_END;
                    remainingCutList[it.key() - leadinLength + 1] =
                        MARK_CUT_START;
                    remainingCutList[it.key()] = MARK_CUT_END;
                }
                else
                {
                    // Cut too short to use new method.
                    foreshortenedCutList[it.key()] = MARK_CUT_END;
                }
                break;

            default:
                // Ignore all other mark types.
                break;
        }
    }

    tracker.SetMap(remainingCutList);
}

frm_dir_map_t Cutter::AdjustedCutList() const
{
    return foreshortenedCutList;
}

void Cutter::Activate(float v2a, int64_t total)
{
    active = true;
    audioFramesPerVideoFrame = v2a;
    totalFrames = total;
    videoFramesToCut = 0;
    audioFramesToCut = 0;
    tracker.TrackerReset(0);
}

void Cutter::NewFrame(int64_t currentFrame)
{
    if (active)
    {
        if (videoFramesToCut == 0)
        {
            uint64_t jumpTo = 0;

            if (tracker.TrackerWantsToJump(currentFrame, jumpTo))
            {
                // Reset the tracker and work out how much video and audio
                // to drop
                tracker.TrackerReset(jumpTo);
                videoFramesToCut = jumpTo - currentFrame;
                audioFramesToCut += (int64_t)(videoFramesToCut *
                                    audioFramesPerVideoFrame + 0.5);
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Clean cut: discarding frame from %1 to %2: "
                            "vid %3 aud %4")
                    .arg((long)currentFrame).arg((long)jumpTo)
                    .arg((long)videoFramesToCut)
                    .arg((long)audioFramesToCut));
            }
        }
    }
}

bool Cutter::InhibitUseVideoFrame()
{
    if (videoFramesToCut == 0)
    {
        return false;
    }
    else
    {
        // We are inside a cut. Inhibit use of this frame
        videoFramesToCut--;

        if(videoFramesToCut == 0)
            LOG(VB_GENERAL, LOG_INFO,
                QString("Clean cut: end of video cut; audio frames left "
                        "to cut %1") .arg((long)audioFramesToCut));

        return true;
    }
}

bool Cutter::InhibitUseAudioFrames(int64_t frames, long *totalAudio)
{
    int64_t delta = audioFramesToCut - frames;
    if (delta < 0)
        delta = -delta;

    if (audioFramesToCut == 0)
    {
        return false;
    }
    else if (delta < audioFramesToCut)
    {
        // Drop the packet containing these frames if doing
        // so gets us closer to zero left to drop
        audioFramesToCut -= frames;
        if(audioFramesToCut == 0)
            LOG(VB_GENERAL, LOG_INFO,
                QString("Clean cut: end of audio cut; vidio frames left "
                        "to cut %1") .arg((long)videoFramesToCut));
        return true;
    }
    else
    {
        // Don't drop this packet even though we still have frames to cut,
        // because doing so would put us further out. Instead, inflate the
        // callers record of how many audio frames have been output.
        *totalAudio += audioFramesToCut;
        audioFramesToCut = 0;
        LOG(VB_GENERAL, LOG_INFO,
            QString("Clean cut: end of audio cut; vidio frames left to "
                    "cut %1") .arg((long)videoFramesToCut));
        return false;
    }
}

bool Cutter::InhibitDummyFrame()
{
    if (audioFramesToCut > 0)
    {
        // If the cutter is in the process of dropping audio then
        // it is better to drop more audio rather than insert a dummy frame
        audioFramesToCut += (int64_t)(audioFramesPerVideoFrame + 0.5);
        return true;
    }
    else
    {
        return false;
    }
}

bool Cutter::InhibitDropFrame()
{
    if (audioFramesToCut > (int64_t)(audioFramesPerVideoFrame + 0.5))
    {
        // If the cutter is in the process of dropping audio and the
        // amount to drop is sufficient then we can drop less
        // audio rather than drop a frame
        audioFramesToCut -= (int64_t)(audioFramesPerVideoFrame + 0.5);

        // But if it's a frame we are supposed to drop anyway, still do so,
        // and record that we have
        if (videoFramesToCut > 0)
        {
            videoFramesToCut-- ;
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

