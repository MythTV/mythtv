#ifndef MYTHSOCKET_H
#define MYTHSOCKET_H

#include <pthread.h>

#include <QTextStream>
#include <QString>
#include <QStringList>
#include <QList>

#include "mythexp.h"
#include "msocketdevice.h"

class QHostAddress;
class MythSocket;

class MPUBLIC MythSocketCBs
{
  public:
    virtual ~MythSocketCBs() {}
    virtual void connected(MythSocket*) = 0;
    virtual void readyRead(MythSocket*) = 0;
    virtual void connectionFailed(MythSocket*) = 0;
    virtual void connectionClosed(MythSocket*) = 0;
};

class MPUBLIC MythSocket : public MSocketDevice
{
    friend void readyReadThread_iffound(MythSocket*);

  public:
    MythSocket(int socket = -1, MythSocketCBs *cb = NULL);

    enum State {
        Connected,
        Connecting,
        HostLookup,
        Idle
    };

    void close(void);
    void deleteLater(void);
 
    void UpRef(void);
    bool DownRef(void);

    State   state(void);
    QString stateToString(void) { return stateToString(state()); }
    QString stateToString(const State state);

    QString errorToString(void) { return errorToString(error()); }
    QString errorToString(const Error error);

    void setSocket(int socket, Type type = MSocketDevice::Stream);
    void setCallbacks(MythSocketCBs *cb);

    qint64 readBlock(char *data, quint64 len);
    qint64 writeBlock(const char *data, quint64 len);

    bool readStringList(QStringList &list, bool quickTimeout = false);
    bool writeStringList(QStringList &list);
    bool writeData(const char *data, quint64 len);

    bool connect(const QHostAddress &addr, quint16 port);
    bool connect(const QString &host, quint16 port);

    void Lock();
    void Unlock();

  protected:
   ~MythSocket();  // force refcounting

    void  setState(const State state);

    MythSocketCBs  *m_cb;
    State           m_state;
    QHostAddress    m_addr;
    quint16         m_port;
    int             m_ref_count;

    bool            m_notifyread;

    static const uint kSocketBufferSize;

    QMutex          m_ref_lock;
    QMutex          m_lock;

    static pthread_t            m_readyread_thread;
    static bool                 m_readyread_run;
    static QMutex               m_readyread_lock;
    static QList<MythSocket*>   m_readyread_list;
    static QList<MythSocket*>   m_readyread_dellist;
    static QList<MythSocket*>   m_readyread_addlist;
    static int                  m_readyread_pipe[2];

    static void StartReadyReadThread(void);
    static void *readyReadThread(void *);

    static void AddToReadyRead(MythSocket *sock);
    static void RemoveFromReadyRead(MythSocket *sock);
    static void WakeReadyReadThread(void);
    static void ShutdownReadyReadThread(void);

    friend class QList<MythSocket*>;
};

#endif

