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
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    m_stream_data = NULL;
}

bool IPTVChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

    if (IsOpen())
        return true;

    QMutexLocker locker(&m_lock);

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    if (m_stream_data)
        SetStreamDataInternal(m_stream_data);

    return true;
}

void IPTVChannel::SetStreamData(MPEGStreamData *sd)
{
    QMutexLocker locker(&m_lock);
    SetStreamDataInternal(sd);
}

void IPTVChannel::SetStreamDataInternal(MPEGStreamData *sd)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("SetStreamData(0x%1)")
        .arg((intptr_t)sd,0,16));

    if (m_stream_data == sd)
        return;

    if (m_stream_data)
    {
        if (sd)
        {
            m_stream_handler->RemoveListener(m_stream_data);
            m_stream_handler->AddListener(sd);
        }
        else
        {
            m_stream_handler->RemoveListener(m_stream_data);
            IPTVStreamHandler::Return(m_stream_handler);
        }
    }
    else
    {
        assert(m_stream_handler == NULL);
        if (m_stream_handler)
            IPTVStreamHandler::Return(m_stream_handler);

        if (sd)
        {
            if (m_last_tuning.IsValid())
                m_stream_handler = IPTVStreamHandler::Get(m_last_tuning);
            if (m_stream_handler)
                m_stream_handler->AddListener(sd);
        }
    }

    m_stream_data = sd;
}

void IPTVChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");
    QMutexLocker locker(&m_lock);
    if (m_stream_handler)
    {
        if (m_stream_data)
            m_stream_handler->RemoveListener(m_stream_data);
        IPTVStreamHandler::Return(m_stream_handler);
    }
}

bool IPTVChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_lock);
    return m_open;
}

bool IPTVChannel::Tune(const IPTVTuningData &tuning)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(%1)")
        .arg(tuning.GetDeviceName()));

    if (tuning.GetDataURL().scheme().toUpper() == "RTSP")
    {
        // TODO get RTP info using RTSP
    }

    if (!tuning.IsValid())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + QString("Invalid tuning info %1")
            .arg(tuning.GetDeviceName()));
        return false;
    }

    if (m_last_tuning == tuning)
        return true;

    m_last_tuning = tuning;

    if (m_stream_data)
    {
        MPEGStreamData *tmp = m_stream_data;
        SetStreamDataInternal(NULL);
        SetStreamDataInternal(tmp);
    }

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
