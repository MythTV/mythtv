#ifndef _LIVETVCHAIN_H_
#define _LIVETVCHAIN_H_

#include <qstring.h>
#include <qvaluelist.h>
#include <qdatetime.h>
#include <qmutex.h>
#include <qptrlist.h>

class ProgramInfo;
class QSocket;

struct LiveTVChainEntry
{
    QString chanid;
    QDateTime starttime;
    bool discontinuity; // if true, can't play smooth from last entry
    QString hostprefix;
    QString cardtype;
    QString channum;
    QString inputname;
};

class LiveTVChain
{
  public:
    LiveTVChain();
   ~LiveTVChain();

    QString InitializeNewChain(const QString &seed);
    void LoadFromExistingChain(const QString &id);

    void SetHostPrefix(const QString &prefix);
    void SetCardType(const QString &type);

    QString GetID(void);

    void DestroyChain(void);

    void AppendNewProgram(ProgramInfo *pginfo, QString channum,
                          QString inputname, bool discont);
    void DeleteProgram(ProgramInfo *pginfo);

    void ReloadAll();

    int GetCurPos(void);
    ProgramInfo *GetProgramAt(int at); // -1 for last, new's the program caller must delete
    int ProgramIsAt(const QString &chanid, const QDateTime &starttime);
    int ProgramIsAt(ProgramInfo *pginfo); // -1 for not found
    int TotalSize(void);

    void SetProgram(ProgramInfo *pginfo);
    bool HasNext(void);
    bool HasPrev(void);

    bool NeedsToSwitch(void);
    ProgramInfo *GetSwitchProgram(bool &discont, bool &newtype); // clears switch flag

    void ClearSwitch(void);

    void SwitchTo(int num);
    void SwitchToNext(bool up); // true up, false down

    bool NeedsToJump(void);

    void JumpTo(int num, int pos);
    void JumpToNext(bool up, int pos);
    int  GetJumpPos(void);  // clears jumppos

    QString GetChannelName(int pos = -1);
    QString GetInputName(int pos = -1);

    void SetHostSocket(QSocket *sock);
    bool IsHostSocket(QSocket *sock);
    int HostSocketCount(void);
    void DelHostSocket(QSocket *sock);
 
  private:
    void BroadcastUpdate();
    void GetEntryAt(int at, LiveTVChainEntry &entry);
    ProgramInfo *EntryToProgram(LiveTVChainEntry &entry);

    QString m_id;
    QValueList<LiveTVChainEntry> m_chain;
    int m_maxpos;
    QMutex m_lock;

    QString m_hostprefix;
    QString m_cardtype;

    int m_curpos;
    QString m_cur_chanid;
    QDateTime m_cur_startts;

    int m_switchid;
    int m_jumppos;

    QMutex m_sockLock;
    QPtrList<QSocket> m_inUseSocks;
};

#endif
    
