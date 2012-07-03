/** -*- Mode: c++ -*- */
#ifndef MYTHSOCKET_H
#define MYTHSOCKET_H

#include <QStringList>
#include <QMutex>

#include "referencecounter.h"
#include "msocketdevice.h"
#include "mythsocket_cb.h"
#include "mythbaseexp.h"

template<class T >
class QList;
class QString;
class QHostAddress;
class MythSocketThread;

class MBASE_PUBLIC MythSocket :
    public MSocketDevice, public ReferenceCounter
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
    bool closedByRemote(void);
    void deleteLater(void);

    virtual int DecrRef(void); // ReferenceCounter

    State   state(void) const;
    QString stateToString(void) const { return stateToString(state()); }
    QString stateToString(const State state) const;

    QString errorToString(void) const { return errorToString(error()); }
    QString errorToString(const Error error) const;

    bool    Validate(uint timeout_ms = kMythSocketLongTimeout,
                     bool error_dialog_desired = false);
    void setValidated(bool isValidated=true)    { m_isValidated = isValidated; }
    bool  isValidated(void)                     { return m_isValidated; }

    bool           Announce(QStringList &strlist);
    QStringList getAnnounce(void)               { return m_announce; }
    void        setAnnounce(QStringList &strlist);
    bool         isAnnounced(void)              { return m_isAnnounced; }

    bool isExpectingReply(void)                 { return m_expectingreply; }

    void setSocket(int socket, Type type = MSocketDevice::Stream);
    void setCallbacks(MythSocketCBs *cb);
    void useReadyReadCallback(bool useReadyReadCallback = true)
        { m_useReadyReadCallback = useReadyReadCallback; }

    qint64 readBlock(char *data, quint64 len);
    qint64 writeBlock(const char *data, quint64 len);

    bool readStringList(QStringList &list, uint timeoutMS = kLongTimeout);
    bool readStringList(QStringList &list, bool quicTimeout)
    {
        return readStringList(
            list, quicTimeout ? kShortTimeout : kLongTimeout);
    }
    bool writeStringList(QStringList &list);
    bool SendReceiveStringList(QStringList &list, uint min_reply_length = 0);
    bool readData(char *data, quint64 len);
    bool writeData(const char *data, quint64 len);

    bool connect(const QHostAddress &hadr, quint16 port);
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
    bool            m_useReadyReadCallback;
    State           m_state;
    QHostAddress    m_addr;
    quint16         m_port;

    bool            m_notifyread;
    mutable QMutex  m_lock; // externally accessible lock

    bool            m_expectingreply;
    bool            m_isValidated;
    bool            m_isAnnounced;
    QStringList     m_announce;

    static const uint kSocketBufferSize;
    static QMutex s_readyread_thread_lock;
    static MythSocketThread *s_readyread_thread;
    
    static QMap<QString, QHostAddress::SpecialAddress> s_loopback_cache;
};

#endif

