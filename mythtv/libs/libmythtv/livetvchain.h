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
};

class LiveTVChain
{
  public:
    LiveTVChain();
   ~LiveTVChain();

    QString InitializeNewChain(const QString &seed);
    void LoadFromExistingChain(const QString &id);

    void DestroyChain(void);

    void AppendNewProgram(ProgramInfo *pginfo, bool discont);
    void DeleteProgram(ProgramInfo *pginfo);

    void ReloadAll();

  private:
    void BroadcastUpdate();

    QString m_id;
    QValueList<LiveTVChainEntry> m_chain;
    int m_maxpos;
    QMutex m_lock;
};

#endif
    
