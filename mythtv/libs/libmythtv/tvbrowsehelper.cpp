#include <algorithm>
using namespace std;

#include "mythcorecontext.h"

#include <QCoreApplication>

#include "tvbrowsehelper.h"
#include "playercontext.h"
#include "remoteencoder.h"
#include "recordinginfo.h"
#include "mythplayer.h"
#include "cardutil.h"
#include "tv_play.h"
#include "mythlogging.h"

#define LOC      QString("BH: ")

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
    uint     browse_max_forward,
    bool     browse_all_tuners,
    bool     use_channel_groups,
    QString  db_channel_ordering) :
    MThread("TVBrowseHelper"),
    m_tv(tv),
    db_browse_max_forward(browse_max_forward),
    db_browse_all_tuners(browse_all_tuners),
    db_use_channel_groups(use_channel_groups),
    m_ctx(NULL),
    m_chanid(0),
    m_run(true)
{
    db_all_channels = ChannelUtil::GetChannels(
        0, true, "channum, callsign");
    ChannelUtil::SortChannels(
        db_all_channels, db_channel_ordering, false);

    ChannelInfoList::const_iterator it = db_all_channels.begin();
    for (; it != db_all_channels.end(); ++it)
    {
        db_chanid_to_channum[(*it).chanid] = (*it).channum;
        db_chanid_to_sourceid[(*it).chanid] = (*it).sourceid;
        db_channum_to_chanids.insert((*it).channum,(*it).chanid);
    }

    db_all_visible_channels = ChannelUtil::GetChannels(
        0, true, "channum, callsign");
    ChannelUtil::SortChannels(
        db_all_visible_channels, db_channel_ordering, false);

    start();
}


/// \brief Begins channel browsing.
/// \note This may only be called from the UI thread.
bool TVBrowseHelper::BrowseStart(PlayerContext *ctx, bool skip_browse)
{
    if (!gCoreContext->IsUIThread())
        return false;

    QMutexLocker locker(&m_lock);

    if (m_ctx)
        return m_ctx == ctx;

    m_tv->ClearOSD(ctx);

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (ctx->playingInfo)
    {
        m_ctx       = ctx;
        m_channum   = ctx->playingInfo->GetChanNum();
        m_chanid    = ctx->playingInfo->GetChanID();
        m_starttime = ctx->playingInfo->GetScheduledStartTime(MythDate::ISODate);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        if (!skip_browse)
        {
            BrowseInfo bi(BROWSE_SAME, m_channum, m_chanid, m_starttime);
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

void TVBrowseHelper::BrowseChannel(PlayerContext *ctx, const QString &channum)
{
    if (!gCoreContext->IsUIThread())
        return;

    if (db_browse_all_tuners)
    {
        BrowseInfo bi(channum, 0);
        BrowseDispInfo(ctx, bi);
        return;
    }

    if (!ctx->recorder || !ctx->last_cardid)
        return;

    QString inputname = ctx->recorder->GetInput();
    uint    inputid   = CardUtil::GetInputID(ctx->last_cardid, inputname);
    uint    sourceid  = CardUtil::GetSourceID(inputid);
    if (sourceid)
    {
        BrowseInfo bi(channum, sourceid);
        BrowseDispInfo(ctx, bi);
    }
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

/** \brief Returns a chanid for the channum, or 0 if none is available.
 *
 *  This will prefer a given sourceid first, and then a given card id,
 *  but if one or the other can not be satisfied but db_browse_all_tuners
 *  is set then it will look to see if the chanid is available on any
 *  tuner.
 */
uint TVBrowseHelper::GetChanId(
    const QString &channum, uint pref_cardid, uint pref_sourceid) const
{
    if (pref_sourceid)
    {
        ChannelInfoList::const_iterator it = db_all_channels.begin();
        for (; it != db_all_channels.end(); ++it)
        {
            if ((*it).sourceid == pref_sourceid && (*it).channum == channum)
                return (*it).chanid;
        }
    }

    if (pref_cardid)
    {
        ChannelInfoList::const_iterator it = db_all_channels.begin();
        for (; it != db_all_channels.end(); ++it)
        {
            if ((*it).GetCardIds().contains(pref_cardid) &&
                (*it).channum == channum)
                return (*it).chanid;
        }
    }

    if (db_browse_all_tuners)
    {
        ChannelInfoList::const_iterator it = db_all_channels.begin();
        for (; it != db_all_channels.end(); ++it)
        {
            if ((*it).channum == channum)
                return (*it).chanid;
        }
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
        begts = MythDate::fromString(starttime);
    else
        begts = MythDate::fromString(infoMap["dbstarttime"]);

    infoMap["starttime"] = MythDate::toString(begts, MythDate::kTime);
    infoMap["startdate"] = MythDate::toString(
        begts, MythDate::kDateFull | MythDate::kSimplify);

    infoMap["endtime"] = infoMap["enddate"] = "";
    if (!endtime.isEmpty())
    {
        endts = MythDate::fromString(endtime);
        infoMap["endtime"] = MythDate::toString(endts, MythDate::kTime);
        infoMap["enddate"] = MythDate::toString(
            endts, MythDate::kDateFull | MythDate::kSimplify);
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
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetNextProgramDB() requires a chanid");
        return;
    }

    int chandir = -1;
    switch (direction)
    {
        case BROWSE_UP:       chandir = CHANNEL_DIRECTION_UP;       break;
        case BROWSE_DOWN:     chandir = CHANNEL_DIRECTION_DOWN;     break;
        case BROWSE_FAVORITE: chandir = CHANNEL_DIRECTION_FAVORITE; break;
        default: break; // quiet -Wswitch-enum
    }
    if (chandir != -1)
    {
        chanid = ChannelUtil::GetNextChannel(
            db_all_visible_channels, chanid, 0 /*mplexid_restriction*/,
            chandir, true /*skip non visible*/, true /*skip same callsign*/);
    }

    infoMap["chanid"]  = QString::number(chanid);
    infoMap["channum"] = db_chanid_to_channum[chanid];

    QDateTime nowtime = MythDate::current();
    QDateTime latesttime = nowtime.addSecs(6*60*60);
    QDateTime browsetime = MythDate::fromString(infoMap["dbstarttime"]);

    MSqlBindings bindings;
    bindings[":CHANID"] = chanid;
    bindings[":NOWTS"] = nowtime;
    bindings[":LATESTTS"] = latesttime;
    bindings[":BROWSETS"] = browsetime;
    bindings[":BROWSETS2"] = browsetime;

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
    LoadFromProgram(progList, querystr, bindings, dummySched);

    if (progList.empty())
    {
        infoMap["dbstarttime"] = "";
        return;
    }

    const ProgramInfo *prog = (direction == BROWSE_LEFT) ?
        progList[progList.size() - 1] : progList[0];

    infoMap["dbstarttime"] = prog->GetScheduledStartTime(MythDate::ISODate);
}

inline static QString toString(const InfoMap &infoMap, const QString sep="\n")
{
    QString str("");
    InfoMap::const_iterator it = infoMap.begin();
    for (; it != infoMap.end() ; ++it)
        str += QString("[%1]:%2%3").arg(it.key()).arg(*it).arg(sep);
    return str;
}

void TVBrowseHelper::run()
{
    RunProlog();
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

        vector<uint> chanids;
        if (BROWSE_SAME == bi.m_dir)
        {
            if (!bi.m_chanid)
            {
                vector<uint> chanids_extra;
                uint sourceid = db_chanid_to_sourceid[m_chanid];
                QMultiMap<QString,uint>::iterator it;
                it = db_channum_to_chanids.lowerBound(bi.m_channum);
                for ( ; (it != db_channum_to_chanids.end()) &&
                          (it.key() == bi.m_channum); ++it)
                {
                    if (db_chanid_to_sourceid[*it] == sourceid)
                        chanids.push_back(*it);
                    else
                        chanids_extra.push_back(*it);
                }
                chanids.insert(chanids.end(),
                               chanids_extra.begin(),
                               chanids_extra.end());
            }
            m_channum   = bi.m_channum;
            m_chanid    = (chanids.empty()) ? bi.m_chanid : chanids[0];
            m_starttime = bi.m_starttime;
        }

        BrowseDirection direction = bi.m_dir;

        QDateTime lasttime = MythDate::fromString(m_starttime);
        QDateTime curtime  = MythDate::current();
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
            still_there |= (ctx == m_tv->player[i]);
        if (still_there)
        {
            if (!db_browse_all_tuners)
            {
                GetNextProgram(direction, infoMap);
            }
            else
            {
                if (!chanids.empty())
                {
                    for (uint i = 0; i < chanids.size(); i++)
                    {
                        if (m_tv->IsTunable(ctx, chanids[i]))
                        {
                            infoMap["chanid"] = QString::number(chanids[i]);
                            GetNextProgramDB(direction, infoMap);
                            break;
                        }
                    }
                }
                else
                {
                    uint orig_chanid = infoMap["chanid"].toUInt();
                    GetNextProgramDB(direction, infoMap);
                    while (!m_tv->IsTunable(ctx, infoMap["chanid"].toUInt()) &&
                           (infoMap["chanid"].toUInt() != orig_chanid))
                    {
                        GetNextProgramDB(direction, infoMap);
                    }
                }
            }
        }
        m_tv->ReturnPlayerLock(ctx);

        m_lock.lock();
        if (!m_ctx && !still_there)
            continue;

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
        QDateTime startts = MythDate::fromString(m_starttime);
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
    RunEpilog();
}
