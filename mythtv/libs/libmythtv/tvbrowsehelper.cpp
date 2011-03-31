#include "mythcorecontext.h"

#include <QCoreApplication>

#include "tvbrowsehelper.h"
#include "playercontext.h"
#include "remoteencoder.h"
#include "recordinginfo.h"
#include "mythplayer.h"
#include "tv_play.h"

#define GetPlayer(X,Y) GetPlayerHaveLock(X, Y, __FILE__ , __LINE__)
#define GetOSDLock(X) GetOSDL(X, __FILE__, __LINE__)

static void format_time(int seconds, QString &tMin, QString &tHrsMin)
{
    int minutes     = seconds / 60;
    int hours       = minutes / 60;
    int min         = minutes % 60;

    tMin = TV::tr("%n minute(s)", "", minutes);
    tHrsMin.sprintf("%d:%02d", hours, min);
}

TVBrowseHelper::TVBrowseHelper(
    TV      *tv,
    QString  time_format,
    QString  short_date_format,
    uint     browse_max_forward,
    bool     browse_all_tuners,
    bool     use_channel_groups,
    QString  db_channel_ordering) :
    m_tv(tv),
    db_time_format(time_format),
    db_short_date_format(short_date_format),
    db_browse_max_forward(browse_max_forward),
    db_browse_all_tuners(browse_all_tuners),
    db_use_channel_groups(use_channel_groups),
    m_ctx(NULL),
    m_chanid(0),
    m_run(true)
{
    if (db_browse_all_tuners)
    {
        db_browse_all_channels = ChannelUtil::GetChannels(
            0, true, "channum, callsign");
        ChannelUtil::SortChannels(
            db_browse_all_channels, db_channel_ordering, true);
    }

    start();
}


/// \brief Begins channel browsing.
/// \note This may only be called from the UI thread.
bool TVBrowseHelper::BrowseStart(PlayerContext *ctx, bool skip_browse)
{
    VERBOSE(VB_IMPORTANT, "BrowseStart()");

    if (!gCoreContext->IsUIThread())
        return false;

    QMutexLocker locker(&m_lock);

    if (m_ctx)
        return m_ctx == ctx;

    bool paused = false;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->player)
        paused = ctx->player->IsPaused();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    if (paused)
        return false;

    m_tv->ClearOSD(ctx);

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (ctx->playingInfo)
    {
        m_ctx = ctx;
        BrowseInfo bi(BROWSE_SAME,
                      ctx->playingInfo->GetChanNum(),
                      ctx->playingInfo->GetChanID(),
                      ctx->playingInfo->GetScheduledStartTime(ISODate));
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        if (!skip_browse)
        {
            locker.unlock();
            BrowseDispInfo(ctx, bi);
        }
        return true;
    }
    else
    {
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }
}

/** \brief Ends channel browsing.
 *         Changing the channel if change_channel is true.
 *  \note This may only be called from the UI thread.
 *  \param ctx PlayerContext to end browsing on
 *  \param change_channel iff true we call ChangeChannel()
 */
void TVBrowseHelper::BrowseEnd(PlayerContext *ctx, bool change_channel)
{
    VERBOSE(VB_IMPORTANT, "BrowseEnd()");

    if (!gCoreContext->IsUIThread())
        return;

    QMutexLocker locker(&m_lock);

    if (ctx && m_ctx != ctx)
        return;

    if (!m_ctx)
        return;

    {
        QMutexLocker locker(&m_tv->timerIdLock);
        if (m_tv->browseTimerId)
        {
            m_tv->KillTimer(m_tv->browseTimerId);
            m_tv->browseTimerId = 0;
        }
    }

    m_list.clear();
    m_wait.wakeAll();

    OSD *osd = m_tv->GetOSDLock(ctx);
    if (osd)
        osd->HideWindow("browse_info");
    m_tv->ReturnOSDLock(ctx, osd);

    if (change_channel)
        m_tv->ChangeChannel(ctx, 0, m_channum);

    m_ctx = NULL;
}

void TVBrowseHelper::BrowseDispInfo(PlayerContext *ctx, BrowseInfo &bi)
{
    VERBOSE(VB_IMPORTANT, "BrowseDispInfo()");

    if (!gCoreContext->IsUIThread())
        return;

    if (!BrowseStart(ctx, true))
        return;

    {
        QMutexLocker locker(&m_tv->timerIdLock);
        if (m_tv->browseTimerId)
        {
            m_tv->KillTimer(m_tv->browseTimerId);
            m_tv->browseTimerId =
                m_tv->StartTimer(TV::kBrowseTimeout, __LINE__);
        }
    }

    QMutexLocker locker(&m_lock);
    if (BROWSE_SAME == bi.m_dir)
        m_list.clear();
    m_list.push_back(bi);
    m_wait.wakeAll();
}

BrowseInfo TVBrowseHelper::GetBrowsedInfo(void) const
{
    QMutexLocker locker(&m_lock);
    BrowseInfo bi(BROWSE_SAME);
    if (m_ctx != NULL)
    {
        bi.m_channum   = m_channum;
        bi.m_chanid    = m_chanid;
        bi.m_starttime = m_starttime;
    }
    return bi;
}

/**
 *  \note This may only be called from the UI thread.
 */
bool TVBrowseHelper::IsBrowsing(void) const
{
    if (!gCoreContext->IsUIThread())
        return true;

    return m_ctx != NULL;
}

/// Returns chanid of random channel with with matching channum
uint TVBrowseHelper::BrowseAllGetChanId(const QString &channum) const
{
    DBChanList::const_iterator it = db_browse_all_channels.begin();
    for (; it != db_browse_all_channels.end(); ++it)
    {
        if ((*it).channum == channum)
            return (*it).chanid;
    }
    return 0;
}

/**
 *  \brief Fetches information on the desired program from the backend.
 *  \param enc RemoteEncoder to query, if null query the actx->recorder.
 *  \param direction BrowseDirection to get information on.
 *  \param infoMap InfoMap to fill in with returned data
 */
void TVBrowseHelper::GetNextProgram(
    BrowseDirection direction, InfoMap &infoMap) const
{
    if (!m_ctx->recorder)
        return;

    QString title, subtitle, desc, category, endtime, callsign, iconpath;
    QDateTime begts, endts;

    QString starttime = infoMap["dbstarttime"];
    QString chanid    = infoMap["chanid"];
    QString channum   = infoMap["channum"];
    QString seriesid  = infoMap["seriesid"];
    QString programid = infoMap["programid"];

    m_ctx->recorder->GetNextProgram(
        direction,
        title,     subtitle,  desc,      category,
        starttime, endtime,   callsign,  iconpath,
        channum,   chanid,    seriesid,  programid);

    if (!starttime.isEmpty())
        begts = QDateTime::fromString(starttime, Qt::ISODate);
    else
        begts = QDateTime::fromString(infoMap["dbstarttime"], Qt::ISODate);

    infoMap["starttime"] = begts.toString(db_time_format);
    infoMap["startdate"] = begts.toString(db_short_date_format);

    infoMap["endtime"] = infoMap["enddate"] = "";
    if (!endtime.isEmpty())
    {
        endts = QDateTime::fromString(endtime, Qt::ISODate);
        infoMap["endtime"] = endts.toString(db_time_format);
        infoMap["enddate"] = endts.toString(db_short_date_format);
    }

    infoMap["lenmins"] = TV::tr("%n minute(s)", "", 0);
    infoMap["lentime"] = "0:00";
    if (begts.isValid() && endts.isValid())
    {
        QString lenM, lenHM;
        format_time(begts.secsTo(endts), lenM, lenHM);
        infoMap["lenmins"] = lenM;
        infoMap["lentime"] = lenHM;
    }

    infoMap["dbstarttime"] = starttime;
    infoMap["dbendtime"]   = endtime;
    infoMap["title"]       = title;
    infoMap["subtitle"]    = subtitle;
    infoMap["description"] = desc;
    infoMap["category"]    = category;
    infoMap["callsign"]    = callsign;
    infoMap["channum"]     = channum;
    infoMap["chanid"]      = chanid;
    infoMap["iconpath"]    = iconpath;
    infoMap["seriesid"]    = seriesid;
    infoMap["programid"]   = programid;
}

void TVBrowseHelper::GetNextProgramDB(
    BrowseDirection direction, InfoMap &infoMap) const
{
    uint chanid = infoMap["chanid"].toUInt();
    if (!chanid)
        chanid = BrowseAllGetChanId(infoMap["channum"]);

    int chandir = -1;
    switch (direction)
    {
        case BROWSE_UP:       chandir = CHANNEL_DIRECTION_UP;       break;
        case BROWSE_DOWN:     chandir = CHANNEL_DIRECTION_DOWN;     break;
        case BROWSE_FAVORITE: chandir = CHANNEL_DIRECTION_FAVORITE; break;
    }
    if (direction != BROWSE_INVALID)
    {
        chanid = ChannelUtil::GetNextChannel(
            db_browse_all_channels, chanid, 0, chandir);
    }

    infoMap["chanid"] = QString::number(chanid);

    DBChanList::const_iterator it = db_browse_all_channels.begin();
    for (; it != db_browse_all_channels.end(); ++it)
    {
        if ((*it).chanid == chanid)
        {
            QString tmp = (*it).channum;
            tmp.detach();
            infoMap["channum"] = tmp;
            break;
        }
    }

    QDateTime nowtime = QDateTime::currentDateTime();
    QDateTime latesttime = nowtime.addSecs(6*60*60);
    QDateTime browsetime = QDateTime::fromString(
        infoMap["dbstarttime"], Qt::ISODate);

    MSqlBindings bindings;
    bindings[":CHANID"] = chanid;
    bindings[":NOWTS"] = nowtime.toString("yyyy-MM-ddThh:mm:ss");
    bindings[":LATESTTS"] = latesttime.toString("yyyy-MM-ddThh:mm:ss");
    bindings[":BROWSETS"] = browsetime.toString("yyyy-MM-ddThh:mm:ss");
    bindings[":BROWSETS2"] = browsetime.toString("yyyy-MM-ddThh:mm:ss");

    QString querystr = " WHERE program.chanid = :CHANID ";
    switch (direction)
    {
        case BROWSE_LEFT:
            querystr += " AND program.endtime <= :BROWSETS "
                " AND program.endtime > :NOWTS ";
            break;

        case BROWSE_RIGHT:
            querystr += " AND program.starttime > :BROWSETS "
                " AND program.starttime < :LATESTTS ";
            break;

        default:
            querystr += " AND program.starttime <= :BROWSETS "
                " AND program.endtime > :BROWSETS2 ";
    };

    ProgramList progList;
    ProgramList dummySched;
    LoadFromProgram(progList, querystr, bindings, dummySched, false);

    if (progList.empty())
    {
        infoMap["dbstarttime"] = "";
        return;
    }

    const ProgramInfo *prog = (direction == BROWSE_LEFT) ?
        progList[progList.size() - 1] : progList[0];

    infoMap["dbstarttime"] = prog->GetScheduledStartTime(ISODate);
}

void TVBrowseHelper::run()
{
    QMutexLocker locker(&m_lock);
    while (true)
    {
        while (m_list.empty() && m_run)
            m_wait.wait(&m_lock);

        if (!m_run)
            break;

        BrowseInfo bi = m_list.front();
        m_list.pop_front();

        PlayerContext *ctx = m_ctx;

        if (BROWSE_SAME == bi.m_dir)
        {
            m_channum   = bi.m_channum;
            m_chanid    = bi.m_chanid;
            m_starttime = bi.m_starttime;
        }

        BrowseDirection direction = bi.m_dir;

        QDateTime lasttime = QDateTime::fromString(
            m_starttime, Qt::ISODate);
        QDateTime curtime  = QDateTime::currentDateTime();
        if (lasttime < curtime)
            m_starttime = curtime.toString(Qt::ISODate);

        QDateTime maxtime  = curtime.addSecs(db_browse_max_forward);
        if ((lasttime > maxtime) && (direction == BROWSE_RIGHT))
            continue;

        m_lock.unlock();

        // if browsing channel groups is enabled or
        // direction if BROWSE_FAVORITES
        // Then pick the next channel in the channel group list to browse
        // If channel group is ALL CHANNELS (-1), then bypass picking from
        // the channel group list
        if ((db_use_channel_groups || (direction == BROWSE_FAVORITE)) &&
            (direction != BROWSE_RIGHT) && (direction != BROWSE_LEFT) &&
            (direction != BROWSE_SAME))
        {
            m_tv->channelGroupLock.lock();
            if (m_tv->channelGroupId > -1)
            {
                int dir = direction;
                if ((direction == BROWSE_UP) || (direction == BROWSE_FAVORITE))
                    dir = CHANNEL_DIRECTION_UP;
                else if (direction == BROWSE_DOWN)
                    dir = CHANNEL_DIRECTION_DOWN;

                uint chanid = ChannelUtil::GetNextChannel(
                    m_tv->channelGroupChannelList, m_chanid, 0, dir);
                direction = BROWSE_SAME;

                m_tv->channelGroupLock.unlock();

                m_lock.lock();
                m_chanid  = chanid;
                m_channum = QString::null;
                m_lock.unlock();
            }
            else
                m_tv->channelGroupLock.unlock();
        }

        if (direction == BROWSE_FAVORITE)
            direction = BROWSE_UP;

        InfoMap infoMap;
        infoMap["dbstarttime"] = m_starttime;
        infoMap["channum"]     = m_channum;
        infoMap["chanid"]      = QString::number(m_chanid);

        m_tv->GetPlayerReadLock(0,__FILE__,__LINE__);
        bool still_there = false;
        for (uint i = 0; i < m_tv->player.size() && !still_there; i++)
            still_there |= (ctx == m_tv->player[0]);
        if (still_there)
        {
            if (!db_browse_all_tuners)
            {
                GetNextProgram(direction, infoMap);
            }
            else
            {
                GetNextProgramDB(direction, infoMap);
                while (!m_tv->IsTunable(ctx, infoMap["chanid"].toUInt()) &&
                       (infoMap["channum"] != m_channum))
                {
                    GetNextProgramDB(direction, infoMap);
                }
            }
        }
        m_tv->ReturnPlayerLock(ctx);

        m_lock.lock();
        if (!m_ctx && !still_there)
            continue;

        VERBOSE(VB_IMPORTANT, QString("browsechanid: %1 -> %2")
                .arg(m_chanid).arg(infoMap["chanid"].toUInt()));

        m_channum = infoMap["channum"];
        m_chanid  = infoMap["chanid"].toUInt();

        if (((direction == BROWSE_LEFT) || (direction == BROWSE_RIGHT)) &&
            !infoMap["dbstarttime"].isEmpty())
        {
            m_starttime = infoMap["dbstarttime"];
        }

        if (!m_list.empty())
        {
            // send partial info to UI for appearance of responsiveness
            QCoreApplication::postEvent(
                m_tv, new UpdateBrowseInfoEvent(infoMap));
            continue;
        }
        m_lock.unlock();

        // pull in additional data from the DB...
        QDateTime startts = QDateTime::fromString(
            m_starttime, Qt::ISODate);
        RecordingInfo recinfo(m_chanid, startts, false);
        recinfo.ToMap(infoMap);
        infoMap["iconpath"] = ChannelUtil::GetIcon(recinfo.GetChanID());

        m_lock.lock();
        if (m_ctx)
        {
            QCoreApplication::postEvent(
                m_tv, new UpdateBrowseInfoEvent(infoMap));
        }
    }
}
