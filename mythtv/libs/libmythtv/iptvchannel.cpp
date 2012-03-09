/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// Qt headers
#include <QUrl>

// MythTV headers
#include "iptvstreamhandler.h"
#include "iptvrecorder.h"
#include "iptvchannel.h"
#include "mythlogging.h"
#include "mythdb.h"

#define LOC QString("IPTVChan(%1): ").arg(GetCardID())

IPTVChannel::IPTVChannel(TVRec *rec, const QString&) :
    DTVChannel(rec), m_open(false),
    m_stream_handler(NULL), m_stream_data(NULL)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
}

IPTVChannel::~IPTVChannel()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "dtor");
    m_stream_data = NULL;
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
    if (m_last_tuning.IsValid())
        m_stream_handler = IPTVStreamHandler::Get(m_last_tuning);

    return m_open;
}

void IPTVChannel::SetStreamData(MPEGStreamData *sd)
{
    QMutexLocker locker(&m_lock);
    if (m_stream_data && m_stream_handler)
        m_stream_handler->RemoveListener(m_stream_data);
    m_stream_data = sd;
    if (m_stream_data && m_stream_handler)
        m_stream_handler->AddListener(m_stream_data);
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

bool IPTVChannel::Tune(const IPTVTuningData &tuning)
{
    QMutexLocker locker(&m_lock);

    if (tuning.GetDataURL().scheme().toUpper() == "RTSP")
    {
        // TODO get RTP info using RTSP
    }

    if (!tuning.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid tuning info %1")
            .arg(tuning.GetDeviceName()));
        return false;
    }

    if (m_stream_handler)
    {
        if (m_stream_data)
            m_stream_handler->RemoveListener(m_stream_data);
        IPTVStreamHandler::Return(m_stream_handler);
    }

    m_stream_handler = IPTVStreamHandler::Get(tuning);
    if (m_stream_data)
        m_stream_handler->AddListener(m_stream_data);

    m_last_tuning = tuning;

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
