#ifndef TV_BROWSE_HELPER_H
#define TV_BROWSE_HELPER_H

// Qt
#include <QHash>
#include <QMultiMap>
#include <QString>
#include <QWaitCondition>

// MythTV
#include "channelinfo.h"
#include "programtypes.h"
#include "mthread.h"
#include "tv.h"

// Std
#include <utility>

class PlayerContext;
class RemoteEncoder;
class TV;

class BrowseInfo
{
  public:
    explicit BrowseInfo(BrowseDirection Dir)
      : m_dir(Dir) {}
    BrowseInfo(BrowseDirection Dir, QString Channum, uint Chanid, QString Starttime)
      : m_dir(Dir),
        m_chanNum(std::move(Channum)),
        m_chanId(Chanid),
        m_startTime(std::move(Starttime)) { }
    BrowseInfo(QString Channum, uint SourceId)
      : m_chanNum(std::move(Channum)),
        m_sourceId(SourceId) { }

    BrowseDirection m_dir      { BROWSE_SAME };
    QString         m_chanNum;
    uint            m_chanId   { 0 };
    QString         m_startTime;
    uint            m_sourceId { 0 };
};


class TVBrowseHelper : public MThread
{
  public:
    TVBrowseHelper(TV* Tv, uint BrowseMaxForward, bool BrowseAllTuners,
                   bool UseChannelGroups, const QString& DBChannelOrdering);
    ~TVBrowseHelper() override;

    void Stop();
    void Wait();
    bool BrowseStart(PlayerContext* Ctx, bool SkipBrowse = false);
    void BrowseEnd(PlayerContext* Ctx, bool ChangeChannel);
    void BrowseDispInfo(PlayerContext* Ctx, const BrowseInfo& Browseinfo);
    void BrowseDispInfo(PlayerContext* Ctx, BrowseDirection Direction);
    void BrowseChannel(PlayerContext* Ctx, const QString& Channum);
    BrowseInfo GetBrowsedInfo() const;
    bool IsBrowsing() const;
    uint GetChanId(const QString& Channum, uint PrefCardid, uint PrefSourceid) const;

  protected:
    void GetNextProgram(BrowseDirection Direction, InfoMap& Infomap) const;
    void GetNextProgramDB(BrowseDirection Direction, InfoMap& Infomap) const;

    void run() override;

    TV*                     m_tv { nullptr };
    ChannelInfoList         m_dbAllChannels;
    ChannelInfoList         m_dbAllVisibleChannels;
    uint                    m_dbBrowseMaxForward;
    bool                    m_dbBrowseAllTuners;
    bool                    m_dbUseChannelGroups;
    QHash<uint,QString>     m_dbChanidToChannum;
    QHash<uint,uint>        m_dbChanidToSourceid;
    QMultiMap<QString,uint> m_dbChannumToChanids;

    mutable QMutex          m_lock;
    PlayerContext*          m_ctx       { nullptr };
    QString                 m_chanNum;
    uint                    m_chanId    { 0 };
    QString                 m_startTime;
    bool                    m_run       { true };
    QWaitCondition          m_wait;
    QList<BrowseInfo>       m_list;
};

#endif
