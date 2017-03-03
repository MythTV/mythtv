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
#include "httptsstreamhandler.h"
#include "hlsstreamhandler.h"
#include "iptvrecorder.h"
#include "iptvchannel.h"
#include "mythlogging.h"
#include "mythdb.h"

#define LOC QString("IPTVChan[%1]: ").arg(m_inputid)

IPTVChannel::IPTVChannel(TVRec *rec, const QString &videodev) :
    DTVChannel(rec), m_firsttune(true), m_stream_handler(NULL),
    m_stream_data(NULL), m_videodev(videodev)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
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

    QMutexLocker locker(&m_tune_lock);

    if (!InitializeInput())
    {
        Close();
        return false;
    }

    if (m_stream_data)
        SetStreamData(m_stream_data);

    return true;
}

void IPTVChannel::SetStreamData(MPEGStreamData *sd)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("SetStreamData(0x%1) StreamHandler(0x%2)")
        .arg((intptr_t)sd,0,16).arg((intptr_t)m_stream_handler,0,16));

    QMutexLocker locker(&m_stream_lock);

    if (m_stream_data == sd && m_stream_handler)
        return;

    if (m_stream_handler)
    {
        if (sd)
            m_stream_handler->AddListener(sd);

        if (m_stream_data)
            m_stream_handler->RemoveListener(m_stream_data);
    }
    else if (sd)
    {
        OpenStreamHandler();
        m_stream_handler->AddListener(sd);
    }

    m_stream_data = sd;
}

void IPTVChannel::Close(void)
{
    if (m_stream_handler)
        CloseStreamHandler();
}

bool IPTVChannel::EnterPowerSavingMode(void)
{
    CloseStreamHandler();
    return true;
}

void IPTVChannel::OpenStreamHandler(void)
{
    if (m_last_tuning.IsHLS())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Creating HLSStreamHandler");
        m_stream_handler = HLSStreamHandler::Get(m_last_tuning);
    }
    else if (m_last_tuning.IsHTTPTS())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Creating HTTPTSStreamHandler");
        m_stream_handler = HTTPTSStreamHandler::Get(m_last_tuning);
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Creating IPTVStreamHandler");
        m_stream_handler = IPTVStreamHandler::Get(m_last_tuning);
    }
}

void IPTVChannel::CloseStreamHandler(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "CloseStreamHandler()");

    QMutexLocker locker(&m_stream_lock);

    if (m_stream_handler)
    {
        if (m_stream_data)
        {
            m_stream_handler->RemoveListener(m_stream_data);
            m_stream_data = NULL; //see trac ticket #12773
        }

        HLSStreamHandler* hsh = dynamic_cast<HLSStreamHandler*>(m_stream_handler);
        HTTPTSStreamHandler* httpsh = dynamic_cast<HTTPTSStreamHandler*>(m_stream_handler);

        if (hsh)
        {
            HLSStreamHandler::Return(hsh);
            m_stream_handler = hsh;
        }
        else if (httpsh)
        {
            HTTPTSStreamHandler::Return(httpsh);
            m_stream_handler = httpsh;
        }
        else
        {
            IPTVStreamHandler::Return(m_stream_handler);
        }
    }
}

bool IPTVChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_stream_lock);
    bool ret = (m_stream_handler && !m_stream_handler->HasError() &&
                m_stream_handler->IsRunning());
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("IsOpen(%1) %2")
        .arg(m_last_tuning.GetDeviceName())
        .arg(ret ? "true" : "false"));
    return ret;
}

bool IPTVChannel::Tune(const IPTVTuningData &tuning, bool scanning)
{
    QMutexLocker locker(&m_tune_lock);

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

    if (!m_firsttune || scanning)
        // for historical reason, an initial tune is requested at
        // startup so don't open the stream handler just yet it will
        // be opened after the next Tune or SetStreamData)
    {
        MPEGStreamData *tmp = m_stream_data;

        CloseStreamHandler();
        if (tmp)
            SetStreamData(tmp);
        else
            OpenStreamHandler();
    }

    m_firsttune = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tuned to (%1)")
        .arg(tuning.GetDeviceName()));

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
