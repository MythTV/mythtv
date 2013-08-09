/** -*- Mode: c++ -*-
 *  HLSStreamHandler
 *  Copyright (c) 2013 Bubblestuff Pty Ltd
 *  based on IPTVStreamHandler
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV headers
#include "hlsstreamhandler.h"
#include "mythlogging.h"
#include "HLS/httplivestreambuffer.h"

#define LOC QString("HLSSH(%1): ").arg(_device)

// BUFFER_SIZE is a multiple of TS_SIZE
#define TS_SIZE        188
#define BUFFER_SIZE (128 * TS_SIZE)

QMap<QString,HLSStreamHandler*>  HLSStreamHandler::s_handlers;
QMap<QString,uint>               HLSStreamHandler::s_handlers_refcnt;
QMutex                           HLSStreamHandler::s_handlers_lock;

HLSStreamHandler* HLSStreamHandler::Get(const IPTVTuningData& tuning)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devkey = tuning.GetDeviceKey();

    QMap<QString,HLSStreamHandler*>::iterator it = s_handlers.find(devkey);

    if (it == s_handlers.end())
    {
        HLSStreamHandler* newhandler = new HLSStreamHandler(tuning);
        newhandler->Start();
        s_handlers[devkey] = newhandler;
        s_handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("HLSSH: Creating new stream handler %1 for %2")
            .arg(devkey).arg(tuning.GetDeviceName()));
    }
    else
    {
        s_handlers_refcnt[devkey]++;
        uint rcount = s_handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("HLSSH: Using existing stream handler %1 for %2")
            .arg(devkey).arg(tuning.GetDeviceName()) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[devkey];
}

void HLSStreamHandler::Return(HLSStreamHandler* & ref)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devname = ref->_device;

    QMap<QString,uint>::iterator rit = s_handlers_refcnt.find(devname);
    if (rit == s_handlers_refcnt.end())
        return;

    LOG(VB_RECORD, LOG_INFO, QString("HLSSH: Return(%1) has %2 handlers")
        .arg(devname).arg(*rit));

    if (*rit > 1)
    {
        ref = NULL;
        (*rit)--;
        return;
    }

    QMap<QString,HLSStreamHandler*>::iterator it = s_handlers.find(devname);
    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("HLSSH: Closing handler for %1")
                           .arg(devname));
        ref->Stop();
        LOG(VB_RECORD, LOG_DEBUG, QString("HLSSH: handler for %1 stopped")
            .arg(devname));
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HLSSH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    s_handlers_refcnt.erase(rit);
    ref = NULL;
}

HLSStreamHandler::HLSStreamHandler(const IPTVTuningData& tuning) :
    IPTVStreamHandler(tuning),
    m_tuning(tuning)
{
    m_hls       = new HLSRingBuffer(m_tuning.GetURL(0).toString(), false);
    m_buffer    = new uint8_t[BUFFER_SIZE];
}

HLSStreamHandler::~HLSStreamHandler(void)
{
    Stop();
    m_hls->Interrupt();
    delete m_hls;
    delete[] m_buffer;
}

void HLSStreamHandler::run(void)
{
    RunProlog();

    LOG(VB_GENERAL, LOG_INFO, LOC + "run()");

    SetRunning(true, false, false);

    // TODO Error handling..

    if (!m_hls->IsOpen())
    {
        m_hls->OpenFile(m_tuning.GetURL(0).toString());
    }

    uint64_t startup    = MythDate::current().toMSecsSinceEpoch();
    uint64_t expected   = 0;

    while (m_hls->IsOpen() && _running_desired)
    {
        int size = m_hls->Read((void*)m_buffer, BUFFER_SIZE);

        if (size < 0)
        {
            break; // error
        }
        if (m_buffer[0] != 0x47)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Packet not starting with SYNC Byte (got 0x%1)")
                .arg((char)m_buffer[0], 2, QLatin1Char('0')));
        }

        int remainder = 0;
        {
            QMutexLocker locker(&_listener_lock);
            HLSStreamHandler::StreamDataList::const_iterator sit;
            sit = _stream_data_list.begin();
            for (; sit != _stream_data_list.end(); ++sit)
            {
                remainder = sit.key()->ProcessData(m_buffer, size);
            }
        }
        
        if (remainder != 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("data_length = %1 remainder = %2")
                .arg(size).arg(remainder));
        }

        expected        += m_hls->DurationForBytes(size);
        uint64_t actual  = MythDate::current().toMSecsSinceEpoch() - startup;
        uint64_t waiting = 0;
        if (expected > actual)
        {
            waiting = expected-actual;
        }
        // The HLS Stream Handler feeds data to the MPEGStreamData, however it feeds
        // data as fast as the MPEGStream can accept it, which quickly exhausts the HLS buffer.
        // The data fed however is lost by the time the recorder starts,
        // forcing the HLS ringbuffer to wait for new data to arrive.
        // The frontend will usually timeout by then.
        // So we simulate a live mechanism by pausing before feeding new data
        // to the MPEGStreamData
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString("waiting %1ms (actual:%2, expected:%3)")
            .arg(waiting).arg(actual).arg(expected));
        usleep(waiting * 1000);
    }

    SetRunning(false, false, false);
    RunEpilog();
}
