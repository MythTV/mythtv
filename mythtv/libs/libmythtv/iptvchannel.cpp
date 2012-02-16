/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV headers
#include "iptvstreamhandler.h"
#include "iptvchannel.h"
#include "mythlogging.h"
#include "mythdb.h"

#define LOC QString("IPTVChan(%1): ").arg(GetCardID())

IPTVChannel::IPTVChannel(TVRec *rec) :
    DTVChannel(rec), m_open(false)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
}

IPTVChannel::~IPTVChannel()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "dtor");
}

bool IPTVChannel::Open(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Open()");

    if (IsOpen())
        return true;

    QMutexLocker locker(&m_lock);

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    m_open = true;
    if (!m_last_channel_id.isEmpty())
        m_stream_handler = IPTVStreamHandler::Get(m_last_channel_id);

    return m_open;
}

void IPTVChannel::Close(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Close()");
    QMutexLocker locker(&m_lock);
    if (m_stream_handler)
        IPTVStreamHandler::Return(m_stream_handler);
}

bool IPTVChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_lock);
    return m_open;
}

bool IPTVChannel::Tune(const QString &freqid, int finetune)
{
    (void) finetune;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Tune(%1) TO BE IMPLEMENTED")
        .arg(freqid));

    // TODO IMPLEMENT query from DB

    QHostAddress addr(QHostAddress::Any);

    int ports[3];
    ports[0] = 5555;
    ports[1] = -1;
    ports[2] = -1;

    QString channel_id = QString("%1!%2!%3!%4")
        .arg(addr.toString()).arg(ports[0]).arg(ports[1]).arg(ports[2]);

    if (m_stream_handler)
        IPTVStreamHandler::Return(m_stream_handler);

    m_stream_handler = IPTVStreamHandler::Get(channel_id);

    m_last_channel_id = channel_id;

    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
