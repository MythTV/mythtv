/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV headers
#include "iptvstreamhandler.h"
#include "iptvrecorder.h"
#include "iptvchannel.h"
#include "mythlogging.h"
#include "mythdb.h"

#define LOC QString("IPTVChan(%1): ").arg(GetCardID())

IPTVChannel::IPTVChannel(TVRec *rec, const QString&) :
    DTVChannel(rec), m_open(false), m_recorder(NULL)
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

void IPTVChannel::SetRecorder(IPTVRecorder *rec)
{
    QMutexLocker locker(&m_lock);
    if (m_recorder && m_stream_handler && m_recorder->GetStreamData())
        m_stream_handler->RemoveListener(m_recorder->GetStreamData());
    m_recorder = rec;
    if (m_recorder && m_stream_handler && m_recorder->GetStreamData())
        m_stream_handler->AddListener(m_recorder->GetStreamData());
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

    int ports[3] = { 5555, -1, -1, };
    int bitrate = 5000000;

    QString channel_id = QString("%1!%2!%3!%4!%5")
        .arg(addr.toString()).arg(ports[0]).arg(ports[1]).arg(ports[2])
        .arg(bitrate);

    if (m_stream_handler)
        IPTVStreamHandler::Return(m_stream_handler);

    m_stream_handler = IPTVStreamHandler::Get(channel_id);

    m_last_channel_id = channel_id;

    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
