#ifndef MYTHSOCKET_H
#define MYTHSOCKET_H

#include <qsocketdevice.h>
#include <qwaitcondition.h>
#include <qtimer.h>
#include <pthread.h>

class QHostAddress;
class MythSocket;

class MythSocketCBs
{
  public:
    virtual void connected(MythSocket*) = 0;
    virtual void readyRead(MythSocket*) = 0;
    virtual void connectionFailed(MythSocket*) = 0;
    virtual void connectionClosed(MythSocket*) = 0;
};

class MythSocket : public QSocketDevice
{
  public:
    MythSocket(int socket = -1);
    MythSocket(MythSocketCBs *cb, int socket = -1);

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

    void setSocket(int socket, Type type = QSocketDevice::Stream);
    void setCallbacks(MythSocketCBs *cb);

    Q_LONG  readBlock(char *data, Q_ULONG len);
    Q_LONG writeBlock(const char *data, Q_ULONG len);

    bool  readStringList(QStringList &list, bool quickTimeout = false);
    bool writeStringList(QStringList &list);
    bool writeData(const char *data, Q_ULONG len);

    bool connect(const QHostAddress &addr, Q_UINT16 port);
    bool connect(const QString &host, Q_UINT16 port);

  protected:
   ~MythSocket();  // force refcounting

    void  setState(const State state);
    static void *readyReadThreadStart(void *param);
    void  readyReadThread(void);

    MythSocketCBs  *m_cb;
    State           m_state;
    QHostAddress    m_addr;
    Q_UINT16        m_port;
    int             m_ref_count;
    pthread_t       m_readyread_thread;
    bool            m_readyread_run;
    QWaitCondition  m_readyread_sleep;

    static const uint kSocketBufferSize;
};

#endif

