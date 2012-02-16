// -*- Mode: c++ -*-

#ifndef _IPTVSTREAMHANDLER_H_
#define _IPTVSTREAMHANDLER_H_

#include <vector>
using namespace std;

#include <QUdpSocket>
#include <QString>
#include <QMutex>
#include <QMap>

#include "util.h"
#include "DeviceReadBuffer.h"
#include "mpegstreamdata.h"
#include "streamhandler.h"
#include "dtvconfparserhelpers.h"

class IPTVStreamHandler;
class DTVSignalMonitor;
class IPTVChannel;

class IPTVStreamHandlerHelper : QObject
{
  public:
    IPTVStreamHandlerHelper(IPTVStreamHandler *p, QUdpSocket *s) :
        m_parent(p), m_socket(s)
    {
        connect(m_socket, SIGNAL(readyRead()),
                this,     SLOT(ReadPending()));
    }

  public slots:
    void ReadPending(void);

  private:
    IPTVStreamHandler *m_parent;
    QUdpSocket *m_socket;
};

class IPTVStreamHandler : public StreamHandler
{
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

  private:
    IPTVStreamHandler(const QString &);

    bool Open(void) { return true; }
    void Close(void) { MThread::exit(0); }

    virtual void run(void); // MThread

  private:
    mutable QMutex m_lock;
    // TODO should we care about who is broadcasting to us?
    QHostAddress m_addr;
    int m_ports[3];
    QUdpSocket *m_sockets[3];
    IPTVStreamHandlerHelper *m_helpers[3];

    // for implementing Get & Return
    static QMutex                            _handlers_lock;
    static QMap<QString, IPTVStreamHandler*> _handlers;
    static QMap<QString, uint>               _handlers_refcnt;
};

#endif // _IPTVSTREAMHANDLER_H_
