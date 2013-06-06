/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Copyright (c) 2013 Bubblestuff Pty Ltd
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// Qt headers
#include <QUrl>

// MythTV headers
#include "iptvstreamhandler.h"
#include "hlsstreamhandler.h"
#include "iptvrecorder.h"
#include "iptvchannel.h"
#include "mythlogging.h"
#include "mythdb.h"

#define LOC QString("IPTVChan[%1]: ").arg(GetCardID())

IPTVChannel::IPTVChannel(TVRec *rec, const QString&) :
    DTVChannel(rec), m_open(false), m_firsttune(true),
    m_stream_handler(NULL), m_stream_data(NULL), m_timer(0)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
}

void IPTVChannel::KillTimer(void)
{
    if (m_timer)
    {
        killTimer(m_timer);
        m_timer = 0;
    }
}

IPTVChannel::~IPTVChannel()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    Close();
}

bool IPTVChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

    if (IsOpen())
        return true;

    m_lock.lock();

    if (!InitializeInputs())
    {
        m_lock.unlock();
        Close();
        return false;
    }

    if (m_stream_data)
        SetStreamDataInternal(m_stream_data, true);

    m_open = true;

    m_lock.unlock();

    return true;
}

void IPTVChannel::SetStreamData(MPEGStreamData *sd)
{
    QMutexLocker locker(&m_lock);
    SetStreamDataInternal(sd, false);
}

void IPTVChannel::SetStreamDataInternal(MPEGStreamData *sd, bool closeimmediately)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("SetStreamData(0x%1) StreamHandler(0x%2) Close(%3)")
        .arg((intptr_t)sd,0,16).arg((intptr_t)m_stream_handler,0,16).arg(closeimmediately));

    if (m_stream_data == sd)
        return;

    if (m_stream_handler)
    {
        KillTimer();

        if (m_stream_data)
        {
            m_stream_handler->RemoveListener(m_stream_data);
        }
        if (sd)
        {
            m_stream_handler->AddListener(sd);
        }
        else
        {
            if (!closeimmediately)
            {
                // streamdata is zero
                // mark stream handler for deletion in 5s
                // so we can re-use it shortly if need be
                LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                    QString("Scheduling for deletion: StreamHandler(0x%1)")
                    .arg((intptr_t)m_stream_handler,0,16));
                m_timer = startTimer(5000);
            }
            else
            {
                CloseStreamHandler();
            }
        }
    }
    else
    {
        if (sd)
        {
            OpenStreamHandler();
            m_stream_handler->AddListener(sd);
        }
    }

    m_stream_data = sd;
}

void IPTVChannel::timerEvent(QTimerEvent*)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "timerEvent()");

    CloseStreamHandler();
}

void IPTVChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");
    QMutexLocker locker(&m_lock);

    CloseStreamHandler();

    m_open = false;
}

void IPTVChannel::OpenStreamHandler(void)
{
    if (m_last_tuning.IsHLS())
    {
        m_stream_handler = HLSStreamHandler::Get(m_last_tuning);
    }
    else
    {
        m_stream_handler = IPTVStreamHandler::Get(m_last_tuning);
    }
}

void IPTVChannel::CloseStreamHandler(void)
{
    KillTimer();

    if (m_stream_handler)
    {
        if (m_stream_data)
            m_stream_handler->RemoveListener(m_stream_data);

        HLSStreamHandler* hsh = dynamic_cast<HLSStreamHandler*>(m_stream_handler);
        if (hsh)
        {
            HLSStreamHandler::Return(hsh);
            m_stream_handler = hsh;
        }
        else
        {
            IPTVStreamHandler::Return(m_stream_handler);
        }
    }
}

bool IPTVChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_lock);
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("IsOpen(%1)")
        .arg(m_last_tuning.GetDeviceName()));
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
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Already tuned to %1")
            .arg(tuning.GetDeviceName()));
        return true;
    }

    m_last_tuning = tuning;

    if (!m_firsttune) // for historical reason, an initial tune is requested at startup
                      // so don't open the stream handler just yet
                      // it will be opened after the next Tune or SetStreamData)
    {
        MPEGStreamData *tmp = m_stream_data;

        CloseStreamHandler();

        if (tmp)
        {
            SetStreamDataInternal(tmp, true);
        }
        else
        {
            OpenStreamHandler();
        }
    }

    m_firsttune = false;

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
