#include "mythcontext.h"
#include "programinfo.h"
#include "playercontext.h"
#include "commbreakmap.h"

#define LOC QString("CommBreakMap: ")

CommBreakMap::CommBreakMap(void)
   : commBreakMapLock(QMutex::Recursive),
     skipcommercials(0),       autocommercialskip(kCommSkipOff),
     commrewindamount(0),      commnotifyamount(0),
     lastCommSkipDirection(0), lastCommSkipTime(0/*1970*/),
     lastCommSkipStart(0),     lastSkipTime(0 /*1970*/),
     hascommbreaktable(false), maxskip(3600),
     maxShortMerge(0),         commBreakIter(commBreakMap.end())
{
    commrewindamount = gCoreContext->GetNumSetting("CommRewindAmount",0);
    commnotifyamount = gCoreContext->GetNumSetting("CommNotifyAmount",0);
    lastIgnoredManualSkip = QDateTime::currentDateTime().addSecs(-10);
    autocommercialskip = (CommSkipMode)
        gCoreContext->GetNumSetting("AutoCommercialSkip", kCommSkipOff);
    maxskip = gCoreContext->GetNumSetting("MaximumCommercialSkip", 3600);
    maxShortMerge = gCoreContext->GetNumSetting("MergeShortCommBreaks", 0);
}

CommSkipMode CommBreakMap::GetAutoCommercialSkip(void)
{
    QMutexLocker locker(&commBreakMapLock);
    return autocommercialskip;
}

void CommBreakMap::ResetLastSkip(void)
{
    lastSkipTime = time(NULL);
}

void CommBreakMap::SetAutoCommercialSkip(CommSkipMode autoskip, uint64_t framesplayed)
{
    QMutexLocker locker(&commBreakMapLock);
    SetTracker(framesplayed);
    uint next = (kCommSkipIncr == autoskip) ?
        (uint) autocommercialskip + 1 : (uint) autoskip;
    autocommercialskip = (CommSkipMode) (next % kCommSkipCount);
}

void CommBreakMap::SkipCommercials(int direction)
{
    commBreakMapLock.lock();
    if (skipcommercials == 0 && direction != 0)
        skipcommercials = direction;
    else if (skipcommercials != 0 && direction == 0)
        skipcommercials = direction;
    commBreakMapLock.unlock();
}

void CommBreakMap::LoadMap(PlayerContext *player_ctx, uint64_t framesPlayed)
{
    if (!player_ctx)
        return;

    QMutexLocker locker(&commBreakMapLock);
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        commBreakMap.clear();
        player_ctx->playingInfo->QueryCommBreakList(commBreakMap);
        hascommbreaktable = !commBreakMap.isEmpty();
        SetTracker(framesPlayed);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void CommBreakMap::SetTracker(uint64_t framesPlayed)
{
    QMutexLocker locker(&commBreakMapLock);
    if (!hascommbreaktable)
        return;

    commBreakIter = commBreakMap.begin();
    while (commBreakIter != commBreakMap.end())
    {
        if (framesPlayed <= commBreakIter.key())
            break;

        commBreakIter++;
    }

    if (commBreakIter != commBreakMap.end())
    {
        VERBOSE(VB_COMMFLAG, LOC + QString("new commBreakIter = %1 @ frame %2, "
                "framesPlayed = %3")
                .arg(*commBreakIter).arg(commBreakIter.key())
                .arg(framesPlayed));
    }
}

void CommBreakMap::GetMap(frm_dir_map_t &map)
{
    QMutexLocker locker(&commBreakMapLock);
    map.clear();
    map = commBreakMap;
    map.detach();
}

bool CommBreakMap::IsInCommBreak(uint64_t frameNumber)
{
    QMutexLocker locker(&commBreakMapLock);
    if (commBreakMap.isEmpty())
        return false;

    frm_dir_map_t::Iterator it = commBreakMap.find(frameNumber);
    if (it != commBreakMap.end())
        return true;

    int lastType = MARK_UNSET;
    for (it = commBreakMap.begin(); it != commBreakMap.end(); ++it)
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
    QMutexLocker locker(&commBreakMapLock);
    VERBOSE(VB_COMMFLAG,
            QString("Setting New Commercial Break List, old size %1, new %2")
                    .arg(commBreakMap.size()).arg(newMap.size()));

    commBreakMap.clear();
    commBreakMap = newMap;
    commBreakMap.detach();
    hascommbreaktable = !commBreakMap.isEmpty();
    SetTracker(framesPlayed);
}

bool CommBreakMap::AutoCommercialSkip(uint64_t &jumpToFrame,
                                      uint64_t framesPlayed,
                                      double video_frame_rate,
                                      uint64_t totalFrames,
                                      QString &comm_msg)
{
    QMutexLocker locker(&commBreakMapLock);
    if (!hascommbreaktable)
        return false;

    if (((time(NULL) - lastSkipTime) <= 3) ||
        ((time(NULL) - lastCommSkipTime) <= 3))
    {
        SetTracker(framesPlayed);
        return false;
    }

    if (commBreakIter == commBreakMap.end())
        return false;

    if (*commBreakIter == MARK_COMM_END)
        commBreakIter++;

    if (commBreakIter == commBreakMap.end())
        return false;

    if (!((*commBreakIter == MARK_COMM_START) &&
          (((kCommSkipOn == autocommercialskip) &&
            (framesPlayed >= commBreakIter.key())) ||
           ((kCommSkipNotify == autocommercialskip) &&
            (framesPlayed + commnotifyamount * video_frame_rate >=
             commBreakIter.key())))))
    {
        return false;
    }

    VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), current "
                                       "framesPlayed %1, commBreakIter "
                                       "frame %2, incrementing "
                                       "commBreakIter")
            .arg(framesPlayed).arg(commBreakIter.key()));

    ++commBreakIter;

    MergeShortCommercials(video_frame_rate);

    if (commBreakIter == commBreakMap.end())
    {
        VERBOSE(VB_COMMFLAG, LOC + "AutoCommercialSkip(), at "
                "end of commercial break list, will not skip.");
        return false;
    }

    if (*commBreakIter == MARK_COMM_START)
    {
        VERBOSE(VB_COMMFLAG, LOC + "AutoCommercialSkip(), new "
                "commBreakIter mark is another start, will not skip.");
        return false;
    }

    if (totalFrames &&
        ((commBreakIter.key() + (10 * video_frame_rate)) > totalFrames))
    {
        VERBOSE(VB_COMMFLAG, LOC + "AutoCommercialSkip(), skipping "
                "would take us to the end of the file, will not skip.");
        return false;
    }

    VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), new "
                                       "commBreakIter frame %1")
            .arg(commBreakIter.key()));

    int skipped_seconds = (int)((commBreakIter.key() -
                                 framesPlayed) / video_frame_rate);
    QString skipTime;
    skipTime.sprintf("%d:%02d", skipped_seconds / 60,
                     abs(skipped_seconds) % 60);
    if (kCommSkipOn == autocommercialskip)
        comm_msg = QString(QObject::tr("Skip %1")).arg(skipTime);
    else
        comm_msg = QString(QObject::tr("Commercial: %1")).arg(skipTime);

    if (kCommSkipOn == autocommercialskip)
    {
        VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), "
                                           "auto-skipping to frame %1")
                .arg(commBreakIter.key() -
                     (int)(commrewindamount * video_frame_rate)));

        lastCommSkipDirection = 1;
        lastCommSkipStart = framesPlayed;
        lastCommSkipTime = time(NULL);

        jumpToFrame = commBreakIter.key() -
            (int)(commrewindamount * video_frame_rate);
        return true;
    }
    ++commBreakIter;
    return false;
}

bool CommBreakMap::DoSkipCommercials(uint64_t &jumpToFrame,
                                     uint64_t framesPlayed,
                                     double video_frame_rate,
                                     uint64_t totalFrames, QString &comm_msg)
{
    QMutexLocker locker(&commBreakMapLock);
    if ((skipcommercials == (0 - lastCommSkipDirection)) &&
        ((time(NULL) - lastCommSkipTime) <= 5))
    {
        comm_msg = QObject::tr("Skipping Back.");

        if (lastCommSkipStart > (2.0 * video_frame_rate))
            lastCommSkipStart -= (long long) (2.0 * video_frame_rate);
        lastCommSkipDirection = 0;
        lastCommSkipTime = time(NULL);
        jumpToFrame = lastCommSkipStart;
        return true;
    }
    lastCommSkipDirection = skipcommercials;
    lastCommSkipStart     = framesPlayed;
    lastCommSkipTime      = time(NULL);

    SetTracker(framesPlayed);

    if ((commBreakIter == commBreakMap.begin()) &&
        (skipcommercials < 0))
    {
        comm_msg = QObject::tr("Start of program.");
        jumpToFrame = 0;
        return true;
    }

    if ((skipcommercials > 0) &&
        ((commBreakIter == commBreakMap.end()) ||
         ((totalFrames) &&
          ((commBreakIter.key() + (10 * video_frame_rate)) > totalFrames))))
    {
        comm_msg = QObject::tr("At End, can not Skip.");
        return false;
    }

    if (skipcommercials < 0)
    {
        commBreakIter--;

        int skipped_seconds = (int)(((int64_t)(commBreakIter.key()) -
                              (int64_t)framesPlayed) / video_frame_rate);

        // special case when hitting 'skip backwards' <3 seconds after break
        if (skipped_seconds > -3)
        {
            if (commBreakIter == commBreakMap.begin())
            {
                comm_msg = QObject::tr("Start of program.");
                jumpToFrame = 0;
                return true;
            }
            else
                commBreakIter--;
        }
    }
    else if (*commBreakIter == MARK_COMM_START)
    {
        int skipped_seconds = (int)(((int64_t)(commBreakIter.key()) -
                              (int64_t)framesPlayed) / video_frame_rate);

        // special case when hitting 'skip' < 20 seconds before break
        if (skipped_seconds < 20)
        {
            commBreakIter++;

            if ((commBreakIter == commBreakMap.end()) ||
                ((totalFrames) &&
                 ((commBreakIter.key() + (10 * video_frame_rate)) >
                                                                totalFrames)))
            {
                comm_msg = QObject::tr("At End, can not Skip.");
                return false;
            }
        }
    }

    if (skipcommercials > 0)
        MergeShortCommercials(video_frame_rate);
    int skipped_seconds = (int)(((int64_t)(commBreakIter.key()) -
                          (int64_t)framesPlayed) / video_frame_rate);
    QString skipTime;
    skipTime.sprintf("%d:%02d", skipped_seconds / 60,
                     abs(skipped_seconds) % 60);

    if ((lastIgnoredManualSkip.secsTo(QDateTime::currentDateTime()) > 3) &&
        (abs(skipped_seconds) >= maxskip))
    {
        comm_msg = QObject::tr("Too Far %1").arg(skipTime);
        lastIgnoredManualSkip = QDateTime::currentDateTime();
        return false;
    }
    comm_msg = QObject::tr("Skip %1").arg(skipTime);

    uint64_t jumpto = (skipcommercials > 0) ?
        commBreakIter.key() - (long long)(commrewindamount * video_frame_rate):
        commBreakIter.key();
    commBreakIter++;
    jumpToFrame = jumpto;
    return true;
}

void CommBreakMap::MergeShortCommercials(double video_frame_rate)
{
    double maxMerge = maxShortMerge * video_frame_rate;
    if (maxMerge <= 0.0 || (commBreakIter == commBreakMap.end()))
        return;

    long long lastFrame = commBreakIter.key();
    ++commBreakIter;
    while ((commBreakIter != commBreakMap.end()) &&
           (commBreakIter.key() - lastFrame < maxMerge))
    {
        ++commBreakIter;
    }
    --commBreakIter;
}
