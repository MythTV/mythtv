// -*- Mode: c++ -*-

#ifndef _TV_BROWSE_HELPER_H_
#define _TV_BROWSE_HELPER_H_

#include <QWaitCondition>
#include <QMultiMap>
#include <QString>
#include <QHash>

#include "channelinfo.h"   // for ChannelInfoList
#include "programtypes.h"  // for InfoMap
#include "mthread.h"
#include "tv.h"            // for BrowseDirection

class PlayerContext;
class RemoteEncoder;
class TV;

class BrowseInfo
{
  public:
    BrowseInfo(BrowseDirection dir) :
        m_dir(dir), m_chanid(0), m_sourceid(0)
    {
    }
    BrowseInfo(BrowseDirection dir,
               const QString &channum,
               uint           chanid,
               const QString &starttime) :
        m_dir(dir),         m_channum(channum),
        m_chanid(chanid),   m_starttime(starttime),
        m_sourceid(0)
    {
    }
    BrowseInfo(const QString &channum,
               uint           sourceid) :
        m_dir(BROWSE_SAME), m_channum(channum),
        m_chanid(0),        m_sourceid(sourceid)
    {
    }

    QString toString() const
    {
        return QString("%1;%2;%3;%4;%5")
            .arg(BROWSE_SAME==m_dir?"SAME":
                 BROWSE_UP  ==m_dir?"UP":
                 BROWSE_DOWN==m_dir?"DOWN":
                 QString::number(m_dir))
            .arg(m_channum)
            .arg(m_chanid)
            .arg(m_starttime)
            .arg(m_sourceid);
    }

    BrowseDirection m_dir;
    QString         m_channum;
    uint            m_chanid;
    QString         m_starttime;
    uint            m_sourceid;
};


class TVBrowseHelper : public MThread
{
  public:
    TVBrowseHelper(TV      *tv,
                   uint     browse_max_forward,
                   bool     browse_all_tuners,
                   bool     use_channel_groups,
                   QString  db_channel_ordering);

    virtual ~TVBrowseHelper()
    {
        Stop();
        Wait();
    }

    void Stop()
    {
        QMutexLocker locker(&m_lock);
        m_list.clear();
        m_run = false;
        m_wait.wakeAll();
    }

    void Wait() { MThread::wait(); }

    bool BrowseStart(PlayerContext *ctx, bool skip_browse = false);
    void BrowseEnd(PlayerContext *ctx, bool change_channel);
    void BrowseDispInfo(PlayerContext *ctx, BrowseInfo &bi);

    void BrowseDispInfo(PlayerContext *ctx, BrowseDirection direction)
    {
        BrowseInfo bi(direction);
        if (BROWSE_SAME != direction)
            BrowseDispInfo(ctx, bi);
    }

    void BrowseChannel(PlayerContext *ctx, const QString &channum);

    BrowseInfo GetBrowsedInfo(void) const;
    bool IsBrowsing(void) const;
    uint GetChanId(const QString &channum,
                   uint pref_cardid, uint pref_sourceid) const;

  protected:
    void GetNextProgram(BrowseDirection direction, InfoMap &infoMap) const;
    void GetNextProgramDB(BrowseDirection direction, InfoMap &infoMap) const;

    virtual void run();

    TV               *m_tv;
    ChannelInfoList   db_all_channels;
    ChannelInfoList   db_all_visible_channels;
    uint              db_browse_max_forward;
    bool              db_browse_all_tuners;
    bool              db_use_channel_groups;
    QHash<uint,QString>     db_chanid_to_channum;
    QHash<uint,uint>        db_chanid_to_sourceid;
    QMultiMap<QString,uint> db_channum_to_chanids;

    mutable QMutex    m_lock; // protects variables below
    PlayerContext    *m_ctx;
    QString           m_channum;
    uint              m_chanid;
    QString           m_starttime;
    bool              m_run;
    QWaitCondition    m_wait;
    QList<BrowseInfo> m_list;
};

#endif // _TV_BROWSE_HELPER_H_
