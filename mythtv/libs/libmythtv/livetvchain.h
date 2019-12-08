#ifndef _LIVETVCHAIN_H_
#define _LIVETVCHAIN_H_

#include <climits>

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QMutex>
#include <QList>

#include "mythtvexp.h"
#include "referencecounter.h"


class ProgramInfo;
class MythSocket;

struct MTV_PUBLIC LiveTVChainEntry
{
    uint chanid;
    QDateTime starttime;
    QDateTime endtime;
    bool discontinuity; // if true, can't play smooth from last entry
    QString hostprefix;
    QString inputtype;
    QString channum;
    QString inputname;
};

class MTV_PUBLIC LiveTVChain : public ReferenceCounter
{
  public:
    LiveTVChain();
   ~LiveTVChain();

    QString InitializeNewChain(const QString &seed);
    void LoadFromExistingChain(const QString &id);

    void SetHostPrefix(const QString &prefix);
    void SetInputType(const QString &type);

    void DestroyChain(void);

    void AppendNewProgram(ProgramInfo *pginfo, const QString& channum,
                          const QString& inputname, bool discont);
    void FinishedRecording(ProgramInfo *pginfo);
    void DeleteProgram(ProgramInfo *pginfo);

    void ReloadAll(const QStringList &data = QStringList());

    // const gets
    QString GetID(void)  const { return m_id; }
    int  GetCurPos(void) const { return m_curPos; }
    int  ProgramIsAt(uint chanid, const QDateTime &starttime) const;
    int  ProgramIsAt(const ProgramInfo &pginfo) const;
    int  GetLengthAtCurPos(void);
    int  GetLengthAtPos(int pos);
    int  TotalSize(void) const;
    bool HasNext(void)   const;
    bool HasPrev(void)   const { return (m_curPos > 0); }
    ProgramInfo *GetProgramAt(int at) const;
    /// Returns true iff a switch is required but no jump is required
    /// m_jumppos sets to INT_MAX means not set
    bool NeedsToSwitch(void) const
        { return (m_switchId >= 0 && m_jumpPos == INT_MAX); }
    /// Returns true iff a switch and jump are required
    bool NeedsToJump(void)   const
        { return (m_switchId >= 0 && m_jumpPos != INT_MAX); }
    QString GetChannelName(int pos = -1) const;
    QString GetInputName(int pos = -1) const;
    QString GetInputType(int pos = -1) const;

    // sets/gets program to switch to
    void SetProgram(const ProgramInfo &pginfo);
    void SwitchTo(int num);
    void SwitchToNext(bool up);
    void ClearSwitch(void);
    ProgramInfo *GetSwitchProgram(bool &discont, bool &newtype, int &newid);

    // sets/gets program to jump to
    void JumpTo(int num, int pos);
    void JumpToNext(bool up, int pos);
    int  GetJumpPos(void);

    // socket stuff
    void SetHostSocket(MythSocket *sock);
    bool IsHostSocket(MythSocket *sock);
    uint HostSocketCount(void) const;
    void DelHostSocket(MythSocket *sock);
 
    QString toString() const;

    // serialize m_maxpos and m_chain to a stringlist
    QStringList entriesToStringList() const;
    // deserialize m_maxpos and m_chain from a stringlist
    bool entriesFromStringList(const QStringList &items);

  private:
    void BroadcastUpdate();
    void GetEntryAt(int at, LiveTVChainEntry &entry) const;
    static ProgramInfo *EntryToProgram(const LiveTVChainEntry &entry);
    ProgramInfo *DoGetNextProgram(bool up, int curpos, int &newid,
                                  bool &discont, bool &newtype);

    QString                 m_id;
    QList<LiveTVChainEntry> m_chain;
    int                     m_maxPos      {0};
    mutable QMutex          m_lock        {QMutex::Recursive};

    QString                 m_hostPrefix;
    QString                 m_inputType;

    int                     m_curPos      {0};
    uint                    m_curChanId   {0};
    QDateTime               m_curStartTs;

    int                     m_switchId    {-1};
    LiveTVChainEntry        m_switchEntry;

    int                     m_jumpPos     {INT_MAX};

    mutable QMutex          m_sockLock;
    QList<MythSocket*>      m_inUseSocks;
};

#endif
    
