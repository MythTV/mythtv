// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"

#include "commbreakmap.h"

#define LOC QString("CommBreakMap: ")

CommBreakMap::CommBreakMap(void)
  : m_autocommercialskip(static_cast<CommSkipMode>(gCoreContext->GetNumSetting("AutoCommercialSkip", kCommSkipOff))),
    m_commrewindamount(gCoreContext->GetDurSetting<std::chrono::seconds>("CommRewindAmount",0s)),
    m_commnotifyamount(gCoreContext->GetDurSetting<std::chrono::seconds>("CommNotifyAmount",0s)),
    m_lastIgnoredManualSkip(MythDate::current().addSecs(-10)),
    m_maxskip(gCoreContext->GetDurSetting<std::chrono::seconds>("MaximumCommercialSkip", 1h)),
    m_maxShortMerge(gCoreContext->GetDurSetting<std::chrono::seconds>("MergeShortCommBreaks", 0s)),
    m_commBreakIter(m_commBreakMap.end())
{    
}

CommSkipMode CommBreakMap::GetAutoCommercialSkip(void) const
{
    QMutexLocker locker(&m_commBreakMapLock);
    return m_autocommercialskip;
}

void CommBreakMap::ResetLastSkip(void)
{
    m_lastSkipTime = SystemClock::now();
}

void CommBreakMap::SetAutoCommercialSkip(CommSkipMode autoskip, uint64_t framesplayed)
{
    QMutexLocker locker(&m_commBreakMapLock);
    SetTracker(framesplayed);
    uint next = (kCommSkipIncr == autoskip) ?
        (uint) m_autocommercialskip + 1 : (uint) autoskip;
    m_autocommercialskip = (CommSkipMode) (next % kCommSkipCount);
}

void CommBreakMap::SkipCommercials(int direction)
{
    m_commBreakMapLock.lock();
    if ((m_skipcommercials == 0 && direction != 0) ||
        (m_skipcommercials != 0 && direction == 0))
        m_skipcommercials = direction;
    m_commBreakMapLock.unlock();
}

void CommBreakMap::LoadMap(PlayerContext *player_ctx, uint64_t framesPlayed)
{
    if (!player_ctx)
        return;

    QMutexLocker locker(&m_commBreakMapLock);
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->m_playingInfo)
    {
        m_commBreakMap.clear();
        player_ctx->m_playingInfo->QueryCommBreakList(m_commBreakMap);
        m_hascommbreaktable = !m_commBreakMap.isEmpty();
        SetTracker(framesPlayed);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void CommBreakMap::SetTracker(uint64_t framesPlayed)
{
    QMutexLocker locker(&m_commBreakMapLock);
    if (!m_hascommbreaktable)
        return;

    m_commBreakIter = m_commBreakMap.begin();
    while (m_commBreakIter != m_commBreakMap.end())
    {
        if (framesPlayed <= m_commBreakIter.key())
            break;

        m_commBreakIter++;
    }

    if (m_commBreakIter != m_commBreakMap.end())
    {
        LOG(VB_COMMFLAG, LOG_INFO, LOC +
            QString("new commBreakIter = %1 @ frame %2, framesPlayed = %3")
                .arg(*m_commBreakIter).arg(m_commBreakIter.key())
                .arg(framesPlayed));
    }
}

void CommBreakMap::GetMap(frm_dir_map_t &map) const
{
    QMutexLocker locker(&m_commBreakMapLock);
    map.clear();
    map = m_commBreakMap;
}

bool CommBreakMap::IsInCommBreak(uint64_t frameNumber) const
{
    QMutexLocker locker(&m_commBreakMapLock);
    if (m_commBreakMap.isEmpty())
        return false;

    frm_dir_map_t::const_iterator it = m_commBreakMap.find(frameNumber);
    if (it != m_commBreakMap.end())
        return true;

    int lastType = MARK_UNSET;
    for (it = m_commBreakMap.begin(); it != m_commBreakMap.end(); ++it)
    {
        if (it.key() > frameNumber)
        {
            int type = *it;

            if (((type == MARK_COMM_END) ||
                 (type == MARK_CUT_END)) &&
                ((lastType == MARK_COMM_START) ||
                 (lastType == MARK_CUT_START)))
                return true;

            if ((type == MARK_COMM_START) ||
                (type == MARK_CUT_START))
                return false;
        }

        lastType = *it;
    }
    return false;
}

void CommBreakMap::SetMap(const frm_dir_map_t &newMap, uint64_t framesPlayed)
{
    QMutexLocker locker(&m_commBreakMapLock);
    LOG(VB_COMMFLAG, LOG_INFO, LOC +
        QString("Setting New Commercial Break List, old size %1, new %2")
                    .arg(m_commBreakMap.size()).arg(newMap.size()));

    m_commBreakMap.clear();
    m_commBreakMap = newMap;
    m_hascommbreaktable = !m_commBreakMap.isEmpty();
    SetTracker(framesPlayed);
}

bool CommBreakMap::AutoCommercialSkip(uint64_t &jumpToFrame,
                                      uint64_t framesPlayed,
                                      double video_frame_rate,
                                      uint64_t totalFrames,
                                      QString &comm_msg)
{
    QMutexLocker locker(&m_commBreakMapLock);
    if (!m_hascommbreaktable)
        return false;

    if ((SystemClock::now() <= m_lastSkipTime + kAutoSkipDeadZone) ||
        (SystemClock::now() <= m_lastCommSkipTime + kAutoSkipDeadZone))
    {
        SetTracker(framesPlayed);
        return false;
    }

    if (m_commBreakIter == m_commBreakMap.end())
        return false;

    if (*m_commBreakIter == MARK_COMM_END)
        m_commBreakIter++;

    if (m_commBreakIter == m_commBreakMap.end())
        return false;

    if ((*m_commBreakIter != MARK_COMM_START) ||
          (((kCommSkipOn != m_autocommercialskip) ||
            (framesPlayed < m_commBreakIter.key())) &&
           ((kCommSkipNotify != m_autocommercialskip) ||
            (framesPlayed + (m_commnotifyamount.count() * video_frame_rate) <
             m_commBreakIter.key()))))
    {
        return false;
    }

    LOG(VB_COMMFLAG, LOG_INFO, LOC +
        QString("AutoCommercialSkip(), current framesPlayed %1, commBreakIter "
                "frame %2, incrementing commBreakIter")
            .arg(framesPlayed).arg(m_commBreakIter.key()));

    ++m_commBreakIter;

    MergeShortCommercials(video_frame_rate);

    if (m_commBreakIter == m_commBreakMap.end())
    {
        LOG(VB_COMMFLAG, LOG_INFO, LOC + "AutoCommercialSkip(), at end of "
                                       "commercial break list, will not skip.");
        return false;
    }

    if (*m_commBreakIter == MARK_COMM_START)
    {
        LOG(VB_COMMFLAG, LOG_INFO, LOC + "AutoCommercialSkip(), new "
                                         "commBreakIter mark is another start, "
                                         "will not skip.");
        return false;
    }

    if (totalFrames &&
        ((m_commBreakIter.key() + (10 * video_frame_rate)) > totalFrames))
    {
        LOG(VB_COMMFLAG, LOG_INFO, LOC + "AutoCommercialSkip(), skipping would "
                                         "take us to the end of the file, will "
                                         "not skip.");
        return false;
    }

    LOG(VB_COMMFLAG, LOG_INFO, LOC +
        QString("AutoCommercialSkip(), new commBreakIter frame %1")
            .arg(m_commBreakIter.key()));

    auto skipped_seconds = std::chrono::seconds((int)((m_commBreakIter.key() -
                                 framesPlayed) / video_frame_rate));
    QString skipTime = MythDate::formatTime(skipped_seconds, "m:ss");
    if (kCommSkipOn == m_autocommercialskip)
    {
        //: %1 is the skip time
        comm_msg = tr("Skip %1").arg(skipTime);
    }
    else
    {
        //: %1 is the skip time
        comm_msg = tr("Commercial: %1").arg(skipTime);
    }

    if (kCommSkipOn == m_autocommercialskip)
    {
        int framediff = (int)(m_commrewindamount.count() * video_frame_rate);
        LOG(VB_COMMFLAG, LOG_INFO, LOC +
            QString("AutoCommercialSkip(), auto-skipping to frame %1")
                .arg(m_commBreakIter.key() - framediff));

        m_lastCommSkipDirection = 1;
        m_lastCommSkipStart = framesPlayed;
        m_lastCommSkipTime = SystemClock::now();

        jumpToFrame = m_commBreakIter.key() - framediff;
        return true;
    }
    ++m_commBreakIter;
    return false;
}

bool CommBreakMap::DoSkipCommercials(uint64_t &jumpToFrame,
                                     uint64_t framesPlayed,
                                     double video_frame_rate,
                                     uint64_t totalFrames, QString &comm_msg)
{
    QMutexLocker locker(&m_commBreakMapLock);
    if ((m_skipcommercials == (0 - m_lastCommSkipDirection)) &&
        (SystemClock::now() <= m_lastCommSkipTime + kSkipBackWindow))
    {
        comm_msg = tr("Skipping Back.");

        if (m_lastCommSkipStart > (2.0 * video_frame_rate))
            m_lastCommSkipStart -= (long long) (2.0 * video_frame_rate);
        m_lastCommSkipDirection = 0;
        m_lastCommSkipTime = SystemClock::now();
        jumpToFrame = m_lastCommSkipStart;
        return true;
    }
    m_lastCommSkipDirection = m_skipcommercials;
    m_lastCommSkipStart     = framesPlayed;
    m_lastCommSkipTime      = SystemClock::now();

    SetTracker(framesPlayed);

    if ((m_commBreakIter == m_commBreakMap.begin()) &&
        (m_skipcommercials < 0))
    {
        comm_msg = tr("Start of program.");
        jumpToFrame = 0;
        return true;
    }

    if ((m_skipcommercials > 0) &&
        ((m_commBreakIter == m_commBreakMap.end()) ||
         (((totalFrames) != 0U) &&
          ((m_commBreakIter.key() + (10 * video_frame_rate)) > totalFrames))))
    {
        comm_msg = tr("At End, cannot Skip.");
        return false;
    }

    if (m_skipcommercials < 0)
    {
        m_commBreakIter--;

        int skipped_seconds = (int)(((int64_t)(m_commBreakIter.key()) -
                              (int64_t)framesPlayed) / video_frame_rate);

        // special case when hitting 'skip backwards' <3 seconds after break
        if (skipped_seconds > -3)
        {
            if (m_commBreakIter == m_commBreakMap.begin())
            {
                comm_msg = tr("Start of program.");
                jumpToFrame = 0;
                return true;
            }
            m_commBreakIter--;
        }
    }
    else
    {
        int64_t framediff = m_commBreakIter.key() - framesPlayed;
        auto skipped_seconds = std::chrono::seconds((int)(framediff / video_frame_rate));

        // special case when hitting 'skip' within 20 seconds of the break
        // start or within commrewindamount of the break end
        // Even though commrewindamount has a max of 10 per the settings UI,
        // check for MARK_COMM_END to make the code generic
        MarkTypes type = *m_commBreakIter;
        if (((type == MARK_COMM_START) && (skipped_seconds < 20s)) ||
            ((type == MARK_COMM_END) && (skipped_seconds < m_commrewindamount)))
        {
            m_commBreakIter++;

            if ((m_commBreakIter == m_commBreakMap.end()) ||
                (((totalFrames) != 0U) &&
                 ((m_commBreakIter.key() + (10 * video_frame_rate)) >
                                                                totalFrames)))
            {
                comm_msg = tr("At End, cannot Skip.");
                return false;
            }
        }
    }

    if (m_skipcommercials > 0)
        MergeShortCommercials(video_frame_rate);
    int64_t framediff = m_commBreakIter.key() - framesPlayed;
    auto skipped_seconds = std::chrono::seconds((int)(framediff / video_frame_rate));
    QString skipTime = MythDate::formatTime(skipped_seconds, "m:ss");

    if ((MythDate::secsInPast(m_lastIgnoredManualSkip) > 3s) &&
        (abs(skipped_seconds) >= m_maxskip))
    {
        //: %1 is the skip time
        comm_msg = tr("Too Far %1").arg(skipTime);
        m_lastIgnoredManualSkip = MythDate::current();
        return false;
    }

    //: %1 is the skip time
    comm_msg = tr("Skip %1").arg(skipTime);

    uint64_t jumpto = (m_skipcommercials > 0) ?
        m_commBreakIter.key() - (long long)(m_commrewindamount.count() * video_frame_rate):
        m_commBreakIter.key();
    m_commBreakIter++;
    jumpToFrame = jumpto;
    return true;
}

void CommBreakMap::MergeShortCommercials(double video_frame_rate)
{
    double maxMerge = m_maxShortMerge.count() * video_frame_rate;
    if (maxMerge <= 0.0 || (m_commBreakIter == m_commBreakMap.end()))
        return;

    long long lastFrame = m_commBreakIter.key();
    ++m_commBreakIter;
    while ((m_commBreakIter != m_commBreakMap.end()) &&
           (m_commBreakIter.key() - lastFrame < maxMerge))
    {
        ++m_commBreakIter;
    }
    --m_commBreakIter;
}
