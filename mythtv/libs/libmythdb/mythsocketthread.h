#ifndef _MYTH_SOCKET_THREAD_H_
#define _MYTH_SOCKET_THREAD_H_

#include <QWaitCondition>
#include <QThread>
#include <QMutex>
#include <QList>

class MythSocket;
class MythSocketThread : public QThread
{
  public:
    MythSocketThread();

    virtual void run(void);

    void StartReadyReadThread(void);
    void WakeReadyReadThread(void) const;
    void ShutdownReadyReadThread(void);

    void AddToReadyRead(MythSocket *sock);
    void RemoveFromReadyRead(MythSocket *sock);

  private:
    void ProcessAddRemoveQueues(void);
    void ReadyToBeRead(MythSocket *sock);
    void CloseReadyReadPipe(void) const;

    bool                   m_readyread_run;
    mutable QMutex         m_readyread_lock;
    mutable QWaitCondition m_readyread_wait;
    mutable QWaitCondition m_readyread_started_wait;

    mutable int            m_readyread_pipe[2];
    mutable long           m_readyread_pipe_flags[2];

    QList<MythSocket*> m_readyread_list;
    QList<MythSocket*> m_readyread_dellist;
    QList<MythSocket*> m_readyread_addlist;
    QList<MythSocket*> m_readyread_downref_list;

    static const uint kShortWait;
};

#endif // _MYTH_SOCKET_THREAD_H_
