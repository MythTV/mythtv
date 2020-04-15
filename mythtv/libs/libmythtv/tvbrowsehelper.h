// -*- Mode: c++ -*-

#ifndef TV_BROWSE_HELPER_H
#define TV_BROWSE_HELPER_H

#include <utility>

// Qt headers
#include <QHash>
#include <QMultiMap>
#include <QString>
#include <QWaitCondition>

// MythTV headers
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
    explicit BrowseInfo(BrowseDirection dir) :
        m_dir(dir)
    {
    }
    BrowseInfo(BrowseDirection dir,
               QString channum,
               uint           chanid,
               QString starttime) :
        m_dir(dir),         m_chanNum(std::move(channum)),
        m_chanId(chanid),   m_startTime(std::move(starttime))
    {
    }
    BrowseInfo(QString channum,
               uint           sourceid) :
        m_chanNum(std::move(channum)),
        m_sourceId(sourceid)
    {
    }

    QString toString() const
    {
        return QString("%1;%2;%3;%4;%5")
            .arg(BROWSE_SAME==m_dir?"SAME":
                 BROWSE_UP  ==m_dir?"UP":
                 BROWSE_DOWN==m_dir?"DOWN":
                 QString::number(m_dir))
            .arg(m_chanNum)
            .arg(m_chanId)
            .arg(m_startTime)
            .arg(m_sourceId);
    }

    BrowseDirection m_dir      {BROWSE_SAME};
    QString         m_chanNum;
    uint            m_chanId   {0};
    QString         m_startTime;
    uint            m_sourceId {0};
};


class TVBrowseHelper : public MThread
{
  public:
    TVBrowseHelper(TV      *tv,
                   uint     browse_max_forward,
                   bool     browse_all_tuners,
                   bool     use_channel_groups,
                   const QString&  db_channel_ordering);

    ~TVBrowseHelper() override
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

    void run() override; // MThread

    TV                      *m_tv        {nullptr};
    ChannelInfoList          m_dbAllChannels;
    ChannelInfoList          m_dbAllVisibleChannels;
    uint                     m_dbBrowseMaxForward;
    bool                     m_dbBrowseAllTuners;
    bool                     m_dbUseChannelGroups;
    QHash<uint,QString>      m_dbChanidToChannum;
    QHash<uint,uint>         m_dbChanidToSourceid;
    QMultiMap<QString,uint>  m_dbChannumToChanids;

    mutable QMutex           m_lock; // protects variables below
    PlayerContext           *m_ctx       {nullptr};
    QString                  m_chanNum;
    uint                     m_chanId    {0};
    QString                  m_startTime;
    bool                     m_run       {true};
    QWaitCondition           m_wait;
    QList<BrowseInfo>        m_list;
};

#endif // TV_BROWSE_HELPER_H
