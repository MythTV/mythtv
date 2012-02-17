// -*- Mode: c++ -*-

#ifndef _IPTVSTREAMHANDLER_H_
#define _IPTVSTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <QUdpSocket>
#include <QString>
#include <QMutex>
#include <QMap>

#include "streamhandler.h"

class IPTVStreamHandler;
class DTVSignalMonitor;
class MPEGStreamData;
class PacketBuffer;
class IPTVChannel;

class IPTVStreamHandlerReadHelper : QObject
{
    Q_OBJECT

  public:
    IPTVStreamHandlerReadHelper(
        IPTVStreamHandler *p, QUdpSocket *s, uint stream) :
        m_parent(p), m_socket(s), m_stream(stream)
    {
        connect(m_socket, SIGNAL(readyRead()),
                this,     SLOT(ReadPending()));
    }

  public slots:
    void ReadPending(void);

  private:
    IPTVStreamHandler *m_parent;
    QUdpSocket *m_socket;
    uint m_stream;
};

class IPTVStreamHandlerWriteHelper : QObject
{
    Q_OBJECT

  public:
    IPTVStreamHandlerWriteHelper(IPTVStreamHandler *p) : m_parent(p) { }
    ~IPTVStreamHandlerWriteHelper()
    {
        killTimer(m_timer);
        m_timer = 0;
        m_parent = NULL;
    }

    void Start(void)
    {
        m_timer = startTimer(200);
    }

  private:
    void timerEvent(QTimerEvent*);

  private:
    IPTVStreamHandler *m_parent;
    int m_timer;
};

class IPTVStreamHandler : public StreamHandler
{
    friend class IPTVStreamHandlerReadHelper;
    friend class IPTVStreamHandlerWriteHelper;
  public:
    static IPTVStreamHandler *Get(const QString &devicename);
    static void Return(IPTVStreamHandler * & ref);

    virtual void AddListener(MPEGStreamData *data,
                             bool allow_section_reader = false,
                             bool needs_drb            = false,
                             QString output_file       = QString())
    {
        StreamHandler::AddListener(data, false, false, output_file);
    } // StreamHandler

    int GetBitrate(void) const { return m_bitrate; }

  private:
    IPTVStreamHandler(const QString &);

    virtual void run(void); // MThread

    virtual void SetRunningDesired(bool desired); // StreamHandler

  private:
    // TODO should we care about who is broadcasting to us?
    QHostAddress m_addr;
    int m_ports[3];
    int m_bitrate;
    QUdpSocket *m_sockets[3];
    IPTVStreamHandlerReadHelper *m_read_helpers[3];
    IPTVStreamHandlerWriteHelper *m_write_helper;
    PacketBuffer *m_buffer;
    bool m_use_rtp_streaming;

    // for implementing Get & Return
    static QMutex                            s_handlers_lock;
    static QMap<QString, IPTVStreamHandler*> s_handlers;
    static QMap<QString, uint>               s_handlers_refcnt;
};

#endif // _IPTVSTREAMHANDLER_H_
