/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Copyright (c) 2013 Bubblestuff Pty Ltd
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C++ headers
#include <utility>

// Qt headers
#include <QUrl>

// MythTV headers
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

#include "hlsstreamhandler.h"
#include "httptsstreamhandler.h"
#include "iptvchannel.h"
#include "iptvrecorder.h"
#include "iptvstreamhandler.h"

#define LOC QString("IPTVChan[%1]: ").arg(m_inputId)

IPTVChannel::IPTVChannel(TVRec *rec, QString videodev) :
    DTVChannel(rec), m_videoDev(std::move(videodev))
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
}

IPTVChannel::~IPTVChannel()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    IPTVChannel::Close();
}

bool IPTVChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

    if (IsOpen())
        return true;

    QMutexLocker locker(&m_tuneLock);

    if (!InitializeInput())
    {
        Close();
        return false;
    }

    if (m_streamData)
        SetStreamData(m_streamData);

    return true;
}

void IPTVChannel::SetStreamData(MPEGStreamData *sd)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("SetStreamData(0x%1) StreamHandler(0x%2)")
        .arg((intptr_t)sd,0,16).arg((intptr_t)m_streamHandler,0,16));

    QMutexLocker locker(&m_streamLock);

    if (m_streamData == sd && m_streamHandler)
        return;

    if (m_streamHandler)
    {
        if (sd)
            m_streamHandler->AddListener(sd);

        if (m_streamData)
            m_streamHandler->RemoveListener(m_streamData);
    }
    else if (sd)
    {
        OpenStreamHandler();
        m_streamHandler->AddListener(sd);
    }

    m_streamData = sd;
}

void IPTVChannel::Close(void)
{
    if (m_streamHandler)
        CloseStreamHandler();
}

bool IPTVChannel::EnterPowerSavingMode(void)
{
    CloseStreamHandler();
    return true;
}

void IPTVChannel::OpenStreamHandler(void)
{
    if (m_lastTuning.IsHLS())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Creating HLSStreamHandler");
        m_streamHandler = HLSStreamHandler::Get(m_lastTuning, GetInputID());
    }
    else if (m_lastTuning.IsHTTPTS())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Creating HTTPTSStreamHandler");
        m_streamHandler = HTTPTSStreamHandler::Get(m_lastTuning, GetInputID());
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Creating IPTVStreamHandler");
        m_streamHandler = IPTVStreamHandler::Get(m_lastTuning, GetInputID());
    }
}

void IPTVChannel::CloseStreamHandler(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "CloseStreamHandler()");

    QMutexLocker locker(&m_streamLock);

    if (m_streamHandler)
    {
        if (m_streamData)
        {
            m_streamHandler->RemoveListener(m_streamData);
            m_streamData = nullptr; //see trac ticket #12773
        }

        auto* hsh = dynamic_cast<HLSStreamHandler*>(m_streamHandler);
        auto* httpsh = dynamic_cast<HTTPTSStreamHandler*>(m_streamHandler);

        if (hsh)
        {
            HLSStreamHandler::Return(hsh, GetInputID());
            m_streamHandler = hsh;
        }
        else if (httpsh)
        {
            HTTPTSStreamHandler::Return(httpsh, GetInputID());
            m_streamHandler = httpsh;
        }
        else
        {
            IPTVStreamHandler::Return(m_streamHandler, GetInputID());
        }
    }
}

bool IPTVChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_streamLock);
    bool ret = (m_streamHandler && !m_streamHandler->HasError() &&
                m_streamHandler->IsRunning());
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("IsOpen(%1) %2")
        .arg(m_lastTuning.GetDeviceName(),
             ret ? "true" : "false"));
    return ret;
}

bool IPTVChannel::Tune(const IPTVTuningData &tuning, bool scanning)
{
    QMutexLocker locker(&m_tuneLock);

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

    if (m_lastTuning == tuning)
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Already tuned to %1")
            .arg(tuning.GetDeviceName()));
        return true;
    }

    m_lastTuning = tuning;

    if (!m_firstTune || scanning)
        // for historical reason, an initial tune is requested at
        // startup so don't open the stream handler just yet it will
        // be opened after the next Tune or SetStreamData)
    {
        MPEGStreamData *tmp = m_streamData;

        CloseStreamHandler();
        if (tmp)
            SetStreamData(tmp);
        else
            OpenStreamHandler();
    }

    m_firstTune = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tuned to (%1)")
        .arg(tuning.GetDeviceName()));

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
