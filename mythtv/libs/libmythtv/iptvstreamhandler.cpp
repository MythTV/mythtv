// -*- Mode: c++ -*-

// Qt headers
#include <QUdpSocket>

// MythTV headers
#include "iptvstreamhandler.h"
#include "mythlogging.h"

#define LOC      QString("IPTVSH(%1): ").arg(_device)

QMap<QString,IPTVStreamHandler*> IPTVStreamHandler::_handlers;
QMap<QString,uint>               IPTVStreamHandler::_handlers_refcnt;
QMutex                           IPTVStreamHandler::_handlers_lock;

IPTVStreamHandler *IPTVStreamHandler::Get(const QString &devname)
{
    QMutexLocker locker(&_handlers_lock);

    QString devkey = devname.toUpper();

    QMap<QString,IPTVStreamHandler*>::iterator it = _handlers.find(devkey);

    if (it == _handlers.end())
    {
        IPTVStreamHandler *newhandler = new IPTVStreamHandler(devkey);
        newhandler->Open();
        _handlers[devkey] = newhandler;
        _handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("IPTVSH: Creating new stream handler %1 for %2")
                .arg(devkey).arg(devname));
    }
    else
    {
        _handlers_refcnt[devkey]++;
        uint rcount = _handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("IPTVSH: Using existing stream handler %1 for %2")
                .arg(devkey)
                .arg(devname) + QString(" (%1 in use)").arg(rcount));
    }

    return _handlers[devkey];
}

void IPTVStreamHandler::Return(IPTVStreamHandler * & ref)
{
    QMutexLocker locker(&_handlers_lock);

    QString devname = ref->_device;

    QMap<QString,uint>::iterator rit = _handlers_refcnt.find(devname);
    if (rit == _handlers_refcnt.end())
        return;

    if (*rit > 1)
    {
        ref = NULL;
        (*rit)--;
        return;
    }

    QMap<QString,IPTVStreamHandler*>::iterator it = _handlers.find(devname);
    if ((it != _handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("IPTVSH: Closing handler for %1")
                           .arg(devname));
        ref->Close();
        delete *it;
        _handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("IPTVSH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    _handlers_refcnt.erase(rit);
    ref = NULL;
}

IPTVStreamHandler::IPTVStreamHandler(const QString &device) :
    StreamHandler(device)
{
    QStringList parts = device.split("!");
    if (parts.size() >= 4)
    {
        m_addr = QHostAddress(parts[0]);
        m_ports[0] = parts[1].toInt();
        m_ports[1] = parts[2].toInt();
        m_ports[2] = parts[3].toInt();
    }
    else
    {
        m_ports[0] = -1;
        m_ports[1] = -1;
        m_ports[2] = -1;
    }
}

void IPTVStreamHandler::run(void)
{
    RunProlog();

    // Open our ports...
    // TODO Error handling..
    for (uint i = 0; i < sizeof(m_ports)/sizeof(int); i++)
    {
        if (m_ports[i] >= 0)
        {
            m_sockets[i] = new QUdpSocket();
            m_helpers[i] = new IPTVStreamHandlerHelper(this, m_sockets[i]);
            m_sockets[i]->bind(m_addr, m_ports[i]);
        }
    }

    exec();

    // Clean up
    for (uint i = 0; i < sizeof(m_ports)/sizeof(int); i++)
    {
        if (m_sockets[i])
        {
            delete m_sockets[i];
            m_sockets[i] = NULL;
            delete m_helpers[i];
            m_helpers[i] = NULL;
        }
    }

    RunEpilog();
}

void IPTVStreamHandlerHelper::ReadPending(void)
{
    QByteArray datagram;
    QHostAddress sender;
    quint16 senderPort;
            
    while (m_socket->hasPendingDatagrams())
    {
        datagram.resize(m_socket->pendingDatagramSize());
        m_socket->readDatagram(datagram.data(), datagram.size(),
                               &sender, &senderPort);
        // TODO actually do something with the data..
    }
}
