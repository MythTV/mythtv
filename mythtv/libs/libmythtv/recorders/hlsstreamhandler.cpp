/** -*- Mode: c++ -*-
 *  HLSStreamHandler
 *  Copyright (c) 2013 Bubblestuff Pty Ltd
 *  based on IPTVStreamHandler
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV headers
#include "hlsstreamhandler.h"
#include "mythlogging.h"
#include "recorders/HLS/HLSReader.h"

#define LOC QString("HLSSH(%1): ").arg(_device)

// BUFFER_SIZE is a multiple of TS_SIZE
#define TS_SIZE     188
#define BUFFER_SIZE (512 * TS_SIZE)

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
    m_hls       = new HLSReader();
    m_buffer    = new uint8_t[BUFFER_SIZE];
    m_throttle  = true;
}

HLSStreamHandler::~HLSStreamHandler(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    Stop();
    delete m_hls;
    delete[] m_buffer;
}

void HLSStreamHandler::run(void)
{
    RunProlog();

    QString url = m_tuning.GetURL(0).toString();
    int err_cnt = 0;
    int nil_cnt = 0;
    int open_sleep = 500000;

    LOG(VB_GENERAL, LOG_INFO, LOC + "run() -- begin");

    SetRunning(true, false, false);

    if (!m_hls)
        return;
    m_hls->Throttle(false);

    while (_running_desired)
    {
        if (!m_hls->IsOpen(url))
        {
            if (!m_hls->Open(url, m_tuning.GetBitrate(0)))
            {
                if (m_hls->FatalError())
                    break;
                usleep(open_sleep);
                if (open_sleep < 20000000)
                    open_sleep += 500000;
                continue;
            }
            open_sleep = 500000;
            m_hls->Throttle(m_throttle);
            m_throttle = false;
        }

        int size = m_hls->Read(m_buffer, BUFFER_SIZE);

        if (size < 0)
        {
            // error
            if (++err_cnt > 10)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + "HLSReader failed");
                Stop();
                break;
            }
            continue;
        }
        err_cnt = 0;

        if (size == 0)
        {
            if (nil_cnt < 4)
                ++nil_cnt;
            usleep(250000 * nil_cnt - 1);  // range .25 to 1 second, minus 1
            continue;
        }
        nil_cnt = 0;

        if (m_buffer[0] != 0x47)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Packet not starting with SYNC Byte (got 0x%1)")
                .arg((char)m_buffer[0], 2, QLatin1Char('0')));
            continue;
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

        if (m_hls->IsThrottled())
            usleep(999999);
        else if (size < BUFFER_SIZE)
            usleep(100000); // tenth of a second.
        else
            usleep(1000);
    }

    m_hls->Throttle(false);

    SetRunning(false, false, false);
    RunEpilog();

    LOG(VB_GENERAL, LOG_INFO, LOC + "run() -- done");
}
