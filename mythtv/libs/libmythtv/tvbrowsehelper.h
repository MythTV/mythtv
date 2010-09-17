// -*- Mode: c++ -*-

#ifndef _TV_BROWSE_HELPER_H_
#define _TV_BROWSE_HELPER_H_

#include <QWaitCondition>
#include <QThread>
#include <QString>

#include "dbchannelinfo.h" // for DBChanList
#include "programtypes.h"  // for InfoMap
#include "tv.h"            // for BrowseDirection

class PlayerContext;
class RemoteEncoder;
class TV;

class BrowseInfo
{
  public:
    BrowseInfo(BrowseDirection dir) :
        m_dir(dir), m_chanid(0)
    {
    }
    BrowseInfo(BrowseDirection dir,
               const QString &channum,
               uint           chanid,
               const QString &starttime) :
        m_dir(dir),  m_channum(channum),
        m_chanid(0), m_starttime(starttime)
    {
    }

    BrowseDirection m_dir;
    QString         m_channum;
    uint            m_chanid;
    QString         m_starttime;
};


class TVBrowseHelper : public QThread
{
  public:
    TVBrowseHelper(TV      *tv,
                   QString  time_format,
                   QString  short_date_format,
                   uint     browse_max_forward,
                   bool     browse_all_tuners,
                   bool     use_channel_groups,
                   QString  db_channel_ordering);

    virtual ~TVBrowseHelper()
    {
        Stop();
        Wait();
    }

    virtual void deleteLater()
    {
        Stop();
        QThread::deleteLater();
    }

    void Stop()
    {
        QMutexLocker locker(&m_lock);
        m_list.clear();
        m_run = false;
        m_wait.wakeAll();
    }

    void Wait() { QThread::wait(); }

    bool BrowseStart(PlayerContext *ctx, bool skip_browse = false);
    void BrowseEnd(PlayerContext *ctx, bool change_channel);
    void BrowseDispInfo(PlayerContext *ctx, BrowseInfo &bi);

    void BrowseDispInfo(PlayerContext *ctx, BrowseDirection direction)
    {
        BrowseInfo bi(direction);
        if (BROWSE_SAME != direction)
            BrowseDispInfo(ctx, bi);
    }

    void BrowseChannel(PlayerContext *ctx, const QString &channum)
    {
        BrowseInfo bi(BROWSE_SAME, channum, 0, "");
        BrowseDispInfo(ctx, bi);
    }

    BrowseInfo GetBrowsedInfo(void) const;
    bool IsBrowsing(void) const;
    uint BrowseAllGetChanId(const QString &channum) const;

  protected:
    void GetNextProgram(BrowseDirection direction, InfoMap &infoMap) const;
    void GetNextProgramDB(BrowseDirection direction, InfoMap &infoMap) const;

    virtual void run();

    TV               *m_tv;
    DBChanList        db_browse_all_channels;
    QString           db_time_format;
    QString           db_short_date_format;
    uint              db_browse_max_forward;
    bool              db_browse_all_tuners;
    bool              db_use_channel_groups;

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
