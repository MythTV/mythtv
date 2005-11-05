#ifndef _LIVETVCHAIN_H_
#define _LIVETVCHAIN_H_

#include <qstring.h>
#include <qvaluelist.h>
#include <qdatetime.h>
#include <qmutex.h>

class ProgramInfo;

struct LiveTVChainEntry
{
    QString chanid;
    QDateTime starttime;
    bool discontinuity; // if true, can't play smooth from last entry
    QString hostprefix;
    QString cardtype;
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

    void AppendNewProgram(ProgramInfo *pginfo, bool discont);
    void DeleteProgram(ProgramInfo *pginfo);

    void ReloadAll();

    ProgramInfo *GetProgramAt(int at); // -1 for last, new's the program caller must delete
    int ProgramIsAt(const QString &chanid, const QDateTime &starttime);
    int ProgramIsAt(ProgramInfo *pginfo); // -1 for not found
    int TotalSize(void);

    void SetProgram(ProgramInfo *pginfo);
    bool HasNext(void);
    bool HasPrev(void);

    bool NeedsToSwitch(void);
    ProgramInfo *GetSwitchProgram(bool &discont, bool &newtype);

    void SwitchTo(int num);
    void SwitchToNext(bool up); // true up, false down
 
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
};

#endif
    
