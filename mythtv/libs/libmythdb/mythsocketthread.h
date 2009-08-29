#ifndef _MYTH_SOCKET_THREAD_H_
#define _MYTH_SOCKET_THREAD_H_

#include <QThread>
#include <QMutex>
#include <QList>

#ifdef USING_MINGW
#include "compat.h"   /// for HANDLE
#endif

class MythSocket;
class MythSocketThread : public QThread
{
  public:
    MythSocketThread();

    virtual void run();

    void StartReadyReadThread(void);
    void WakeReadyReadThread(void);
    void ShutdownReadyReadThread(void);

    void AddToReadyRead(MythSocket *sock);
    void RemoveFromReadyRead(MythSocket *sock);

  private:
    void iffound(MythSocket *sock);
    bool isLocked(QMutex &mutex);

    bool m_readyread_run;
    QMutex m_readyread_lock;
    QList<MythSocket*> m_readyread_list;
    QList<MythSocket*> m_readyread_dellist;
    QList<MythSocket*> m_readyread_addlist;
#ifdef USING_MINGW
    HANDLE readyreadevent;
#else
    int m_readyread_pipe[2];
#endif
};

#endif // _MYTH_SOCKET_THREAD_H_
