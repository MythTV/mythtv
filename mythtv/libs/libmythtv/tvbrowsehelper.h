#ifndef TV_BROWSE_HELPER_H
#define TV_BROWSE_HELPER_H

// Qt
#include <QHash>
#include <QMultiMap>
#include <QString>
#include <QWaitCondition>

// MythTV
#include "libmythbase/mthread.h"
#include "libmythbase/programtypes.h"
#include "channelinfo.h"
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
    friend class TV;

  protected:
    explicit TVBrowseHelper(TV* Parent);
    ~TVBrowseHelper() override;

    void BrowseInit(std::chrono::seconds BrowseMaxForward, bool BrowseAllTuners,
                    bool UseChannelGroups, const QString& DBChannelOrdering);
    void BrowseStop();
    void BrowseWait();
    bool BrowseStart(bool SkipBrowse = false);
    void BrowseEnd(bool ChangeChannel);
    void BrowseDispInfo(const BrowseInfo& Browseinfo);
    void BrowseDispInfo(BrowseDirection Direction);
    void BrowseChannel(const QString& Channum);
    BrowseInfo GetBrowsedInfo() const;
    uint GetBrowseChanId(const QString& Channum, uint PrefCardid, uint PrefSourceid) const;

    int                     m_browseTimerId         { 0 };

  private:
    Q_DISABLE_COPY(TVBrowseHelper)
    void GetNextProgram(BrowseDirection Direction, InfoMap& Infomap) const;
    void GetNextProgramDB(BrowseDirection Direction, InfoMap& Infomap) const;
    void run() override;

    TV*                     m_parent                { nullptr };
    ChannelInfoList         m_dbAllChannels;
    std::chrono::seconds    m_dbBrowseMaxForward    { 0s };
    bool                    m_dbBrowseAllTuners     { false };
    bool                    m_dbUseChannelGroups    { false };
    QHash<uint,QString>     m_dbChanidToChannum;
    QHash<uint,uint>        m_dbChanidToSourceid;
    QMultiMap<QString,uint> m_dbChannumToChanids;

    mutable QMutex          m_browseLock;
    QString                 m_browseChanNum;
    uint                    m_browseChanId          { 0 };
    QString                 m_browseStartTime;
    bool                    m_browseRun             { true };
    QWaitCondition          m_browseWait;
    QList<BrowseInfo>       m_browseList;
};

#endif
