/** -*- Mode: c++ -*- */
#ifndef MYTHSOCKET_H
#define MYTHSOCKET_H

#include <QStringList>
#include <QMutex>

#include "mythexp.h"
#include "msocketdevice.h"

class QHostAddress;
class MythSocket;
class MythSocketThread;

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
    friend class MythSocketThread;
    friend class QList<MythSocket*>;
    friend void ShutdownRRT(void);

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

    bool readStringList(QStringList &list, uint timeoutMS = kLongTimeout);
    bool readStringList(QStringList &list, bool quickTimeout)
    {
        return readStringList(
            list, quickTimeout ? kShortTimeout : kLongTimeout);
    }
    bool writeStringList(QStringList &list);
    bool readData(char *data, quint64 len);
    bool writeData(const char *data, quint64 len);

    bool connect(const QHostAddress &addr, quint16 port);
    bool connect(const QString &host, quint16 port);

    void Lock(void) const;
    bool TryLock(bool wakereadyread) const;
    void Unlock(bool wakereadyread = true) const;

    static const uint kShortTimeout;
    static const uint kLongTimeout;

  protected:
   ~MythSocket();  // force refcounting

    void  setState(const State state);

    MythSocketCBs  *m_cb;
    State           m_state;
    QHostAddress    m_addr;
    quint16         m_port;
    int             m_ref_count;

    bool            m_notifyread;
    QMutex          m_ref_lock;
    mutable QMutex  m_lock; // externally accessible lock

    static const uint kSocketBufferSize;
    static MythSocketThread *s_readyread_thread;
};

#endif

