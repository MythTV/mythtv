//
//  iptvfeederhls.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 24/05/12.
//  Copyright (c) 2012 Bubblestuff Pty Ltd. All rights reserved.
//

#include "iptvfeederhls.h"

// MythTV headers
#include "mythlogging.h"
#include "streamlisteners.h"

#include "HLS/httplivestreambuffer.h"
#define LOC QString("IPTVHLS: ")

#define TS_SIZE 188

// BUFFER_SIZE must be a multiple of TS_SIZE, otherwise the IPTVRecorder will
// choke on it
#define BUFFER_SIZE (128 * TS_SIZE)

IPTVFeederHLS::IPTVFeederHLS() :
    m_buffer(new uint8_t[BUFFER_SIZE]), m_hls(NULL),
    m_interrupted(false), m_running(false)
{
}

IPTVFeederHLS::~IPTVFeederHLS()
{
    Close();
    delete[] m_buffer;
}

bool IPTVFeederHLS::IsHLS(const QString &url)
{
    QString turl = url;
    return HLSRingBuffer::TestForHTTPLiveStreaming(turl);
}

bool IPTVFeederHLS::IsOpen(void) const
{
    if (m_hls == NULL)
        return false;
    return m_hls->IsOpen();
}

bool IPTVFeederHLS::Open(const QString &url)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Open(%1) -- begin").arg(url));

    QMutexLocker locker(&_lock);

    if (m_hls)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Open() -- end 1");
        return true;
    }

    m_hls = new HLSRingBuffer(url);

    LOG(VB_RECORD, LOG_INFO, LOC + "Open() -- end");

    return true;
}

void IPTVFeederHLS::Run(void)
{
    m_lock.lock();
    m_running = true;
    m_interrupted = false;
    m_lock.unlock();

    while (!m_interrupted)
    {
        if (m_hls == NULL)
            break;
        m_lock.lock();
        uint size = m_hls->Read((void *)m_buffer, BUFFER_SIZE);
        if (m_buffer[0] != 0x47)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Packet not starting with SYNC Byte (got 0x%1)")
                .arg((char)m_buffer[0], 2, QLatin1Char('0')));
        }

        _lock.lock();
        vector<TSDataListener*>::iterator it = _listeners.begin();
        for (; it != _listeners.end(); ++it)
            (*it)->AddData(m_buffer, size);
        _lock.unlock();
        m_lock.unlock();
    }

    m_running = false;
    m_lock.lock();
    m_waitcond.wakeAll();
    m_lock.unlock();
}

void IPTVFeederHLS::Stop(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- begin");
    QMutexLocker locker(&m_lock);
    m_interrupted = true;

    while (m_running)
        m_waitcond.wait(&m_lock, 500);

    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- end");
}

void IPTVFeederHLS::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");
    Stop();

    QMutexLocker lock(&m_lock);
    delete m_hls;
    m_hls = NULL;

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}
