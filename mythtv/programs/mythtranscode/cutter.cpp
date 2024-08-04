
#include "cutter.h"

#include <algorithm>                    // for min
#include <cmath>                        // for llroundf

#include <QMap>                         // for QMap

#include "libmythbase/mythlogging.h"

void Cutter::SetCutList(frm_dir_map_t &deleteMap, PlayerContext *ctx)
{
    // Break each cut into two parts, the first for
    // the player and the second for the transcode loop.
    frm_dir_map_t           remainingCutList;
    int64_t                 start = 0;
    int64_t                 leadinLength = 0;

    m_tracker.SetPlayerContext(ctx);
    m_foreshortenedCutList.clear();

    for (auto it = deleteMap.begin(); it != deleteMap.end(); ++it)
    {
        switch(it.value())
        {
            case MARK_CUT_START:
                m_foreshortenedCutList[it.key()] = MARK_CUT_START;
                start = it.key();
                break;

            case MARK_CUT_END:
                leadinLength = std::min((int64_t)(it.key() - start),
                                   (int64_t)kMaxLeadIn);
                if (leadinLength >= kMinCut)
                {
                    m_foreshortenedCutList[it.key() - leadinLength + 2] =
                        MARK_CUT_END;
                    remainingCutList[it.key() - leadinLength + 1] =
                        MARK_CUT_START;
                    remainingCutList[it.key()] = MARK_CUT_END;
                }
                else
                {
                    // Cut too short to use new method.
                    m_foreshortenedCutList[it.key()] = MARK_CUT_END;
                }
                break;

            default:
                // Ignore all other mark types.
                break;
        }
    }

    m_tracker.SetMap(remainingCutList);
}

frm_dir_map_t Cutter::AdjustedCutList() const
{
    return m_foreshortenedCutList;
}

void Cutter::Activate(float v2a, int64_t total)
{
    m_active = true;
    m_audioFramesPerVideoFrame = v2a;
    m_totalFrames = total;
    m_videoFramesToCut = 0;
    m_audioFramesToCut = 0;
    m_tracker.TrackerReset(0);
}

void Cutter::NewFrame(int64_t currentFrame)
{
    if (m_active)
    {
        if (m_videoFramesToCut == 0)
        {
            uint64_t jumpTo = 0;

            if (m_tracker.TrackerWantsToJump(currentFrame, jumpTo))
            {
                // Reset the tracker and work out how much video and audio
                // to drop
                m_tracker.TrackerReset(jumpTo);
                m_videoFramesToCut = jumpTo - currentFrame;
                m_audioFramesToCut += llroundf(m_videoFramesToCut *
                                               m_audioFramesPerVideoFrame);
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Clean cut: discarding frame from %1 to %2: "
                            "vid %3 aud %4")
                    .arg(currentFrame).arg((long)jumpTo)
                    .arg(m_videoFramesToCut)
                    .arg(m_audioFramesToCut));
            }
        }
    }
}

bool Cutter::InhibitUseVideoFrame()
{
    if (m_videoFramesToCut == 0)
    {
        return false;
    }

    // We are inside a cut. Inhibit use of this frame
    m_videoFramesToCut--;

    if(m_videoFramesToCut == 0)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Clean cut: end of video cut; audio frames left "
                    "to cut %1") .arg(m_audioFramesToCut));
    }

    return true;
}

bool Cutter::InhibitUseAudioFrames(int64_t frames, long *totalAudio)
{
    int64_t delta = m_audioFramesToCut - frames;
    if (delta < 0)
        delta = -delta;

    if (m_audioFramesToCut == 0)
    {
        return false;
    }
    if (delta < m_audioFramesToCut)
    {
        // Drop the packet containing these frames if doing
        // so gets us closer to zero left to drop
        m_audioFramesToCut -= frames;
        if(m_audioFramesToCut == 0)
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Clean cut: end of audio cut; vidio frames left "
                        "to cut %1") .arg(m_videoFramesToCut));
        }
        return true;
    }

    // Don't drop this packet even though we still have frames to cut,
    // because doing so would put us further out. Instead, inflate the
    // callers record of how many audio frames have been output.
    *totalAudio += m_audioFramesToCut;
    m_audioFramesToCut = 0;
    LOG(VB_GENERAL, LOG_INFO,
        QString("Clean cut: end of audio cut; vidio frames left to "
                "cut %1") .arg(m_videoFramesToCut));
    return false;
}

bool Cutter::InhibitDummyFrame()
{
    if (m_audioFramesToCut > 0)
    {
        // If the cutter is in the process of dropping audio then
        // it is better to drop more audio rather than insert a dummy frame
        m_audioFramesToCut += llroundf(m_audioFramesPerVideoFrame);
        return true;
    }
    return false;
}

bool Cutter::InhibitDropFrame()
{
    if (m_audioFramesToCut > llroundf(m_audioFramesPerVideoFrame))
    {
        // If the cutter is in the process of dropping audio and the
        // amount to drop is sufficient then we can drop less
        // audio rather than drop a frame
        m_audioFramesToCut -= llroundf(m_audioFramesPerVideoFrame);

        // But if it's a frame we are supposed to drop anyway, still do so,
        // and record that we have
        if (m_videoFramesToCut > 0)
        {
            m_videoFramesToCut-- ;
            return false;
        }
        return true;
    }
    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

