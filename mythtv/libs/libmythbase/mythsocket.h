/** -*- Mode: c++ -*- */
#ifndef MYTH_SOCKET_H
#define MYTH_SOCKET_H

#include <QHostAddress>
#include <QStringList>
#include <QAtomicInt>
#include <QMutex>
#include <QHash>

#include "referencecounter.h"
#include "mythsocket_cb.h"
#include "mythbaseexp.h"
#include "mthread.h"

class QTcpSocket;

/** \brief Class for communcating between myth backends and frontends
 *
 *  \note Access to the methods of MythSocket must be externally
 *  serialized (i.e. the MythSocket must only be available to one
 *  thread at a time).
 *
 */
class MBASE_PUBLIC MythSocket : public QObject, public ReferenceCounter
{
    Q_OBJECT

    friend class MythSocketManager;

  public:
    MythSocket(int socket = -1, MythSocketCBs *cb = NULL,
               bool use_shared_thread = false);

    bool ConnectToHost(const QString &hostname, quint16 port);
    bool ConnectToHost(const QHostAddress &address, quint16 port);
    void DisconnectFromHost(void);

    bool Validate(uint timeout_ms = kMythSocketLongTimeout,
                  bool error_dialog_desired = false);
    bool IsValidated(void) const { return m_isValidated; }

    bool Announce(const QStringList &strlist);
    QStringList GetAnnounce(void) const { return m_announce; }
    void SetAnnounce(const QStringList &strlist);
    bool IsAnnounced(void) const { return m_isAnnounced; }

    void SetReadyReadCallbackEnabled(bool enabled)
        { m_disableReadyReadCallback.fetchAndStoreOrdered((enabled) ? 0 : 1); }

    bool SendReceiveStringList(
        QStringList &list, uint min_reply_length = 0,
        uint timeoutMS = kLongTimeout);

    bool ReadStringList(QStringList &list, uint timeoutMS = kShortTimeout);
    bool WriteStringList(const QStringList &list);

    bool IsConnected(void) const;
    bool IsDataAvailable(void) const;

    QHostAddress GetPeerAddress(void) const;
    int GetPeerPort(void) const;
    int GetSocketDescriptor(void) const;

    // RemoteFile stuff
    int Write(const char*, int size);
    int Read(char*, int size, int max_wait_ms);
    void Reset(void);

    static const uint kShortTimeout;
    static const uint kLongTimeout;

  signals:
    void CallReadyRead(void);

  protected slots:
    void ConnectHandler(void);
    void ErrorHandler(QAbstractSocket::SocketError);
    void AboutToCloseHandler(void);
    void DisconnectHandler(void);
    void ReadyReadHandler(void);
    void CallReadyReadHandler(void);

    void ReadStringListReal(QStringList *list, uint timeoutMS, bool *ret);
    void WriteStringListReal(const QStringList *list, bool *ret);
    void ConnectToHostReal(QHostAddress address, quint16 port, bool *ret);
    void DisconnectFromHostReal(void);

    void WriteReal(const char*, int size, int *ret);
    void ReadReal(char*, int size, int max_wait_ms, int *ret);
    void ResetReal(void);

    void IsDataAvailableReal(bool *ret) const;

  protected:
    ~MythSocket(); // force reference counting

    QTcpSocket     *m_tcpSocket; // only set in ctor
    MThread        *m_thread; // only set in ctor
    mutable QMutex  m_lock;
    int             m_socketDescriptor; // protected by m_lock
    QHostAddress    m_peerAddress; // protected by m_lock
    int             m_peerPort; // protected by m_lock
    MythSocketCBs  *m_callback; // only set in ctor
    bool            m_useSharedThread; // only set in ctor
    QAtomicInt      m_disableReadyReadCallback;
    bool            m_connected; // protected by m_lock
    /// This is used internally as a hint that there might be
    /// data available for reading.
    mutable QAtomicInt m_dataAvailable;
    bool            m_isValidated; // only set in thread using MythSocket
    bool            m_isAnnounced; // only set in thread using MythSocket
    QStringList     m_announce; // only set in thread using MythSocket

    static const int kSocketReceiveBufferSize;

    static QMutex s_loopbackCacheLock;
    static QHash<QString, QHostAddress::SpecialAddress> s_loopbackCache;

    static QMutex s_thread_lock;
    static MThread *s_thread; // protected by s_thread_lock
    static int s_thread_cnt; // protected by s_thread_lock
};

#endif /* MYTH_SOCKET_H */
