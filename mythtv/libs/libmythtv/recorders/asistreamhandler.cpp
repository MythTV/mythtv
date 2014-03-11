// -*- Mode: c++ -*-

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/ioctl.h>
#endif

// Qt headers
#include <QString>
#include <QFile>

// MythTV headers
#include "asistreamhandler.h"
#include "asichannel.h"
#include "dtvsignalmonitor.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cardutil.h"

// DVEO ASI headers
#include <dveo/asi.h>
#include <dveo/master.h>

#define LOC      QString("ASISH(%1): ").arg(_device)

QMap<QString,ASIStreamHandler*> ASIStreamHandler::_handlers;
QMap<QString,uint>              ASIStreamHandler::_handlers_refcnt;
QMutex                          ASIStreamHandler::_handlers_lock;

ASIStreamHandler *ASIStreamHandler::Get(const QString &devname)
{
    QMutexLocker locker(&_handlers_lock);

    QString devkey = devname;

    QMap<QString,ASIStreamHandler*>::iterator it = _handlers.find(devkey);

    if (it == _handlers.end())
    {
        ASIStreamHandler *newhandler = new ASIStreamHandler(devname);
        newhandler->Open();
        _handlers[devkey] = newhandler;
        _handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("ASISH: Creating new stream handler %1 for %2")
                .arg(devkey).arg(devname));
    }
    else
    {
        _handlers_refcnt[devkey]++;
        uint rcount = _handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("ASISH: Using existing stream handler %1 for %2")
                .arg(devkey)
                .arg(devname) + QString(" (%1 in use)").arg(rcount));
    }

    return _handlers[devkey];
}

void ASIStreamHandler::Return(ASIStreamHandler * & ref)
{
    QMutexLocker locker(&_handlers_lock);

    QString devname = ref->_device;

    QMap<QString,uint>::iterator rit = _handlers_refcnt.find(devname);
    if (rit == _handlers_refcnt.end())
        return;

    if (*rit > 1)
    {
        ref = NULL;
        (*rit)--;
        return;
    }

    QMap<QString,ASIStreamHandler*>::iterator it = _handlers.find(devname);
    if ((it != _handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("ASISH: Closing handler for %1")
                           .arg(devname));
        ref->Close();
        delete *it;
        _handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ASISH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    _handlers_refcnt.erase(rit);
    ref = NULL;
}

ASIStreamHandler::ASIStreamHandler(const QString &device) :
    StreamHandler(device),
    _device_num(-1), _buf_size(-1), _fd(-1),
    _packet_size(TSPacket::kSize), _clock_source(kASIInternalClock),
    _rx_mode(kASIRXSyncOnActualConvertTo188), _drb(NULL)
{
    setObjectName("ASISH");
}

void ASIStreamHandler::SetClockSource(ASIClockSource cs)
{
    _clock_source = cs;
    // TODO we should make it possible to set this immediately
    // not wait for the next open
}

void ASIStreamHandler::SetRXMode(ASIRXMode m)
{
    _rx_mode = m;
    // TODO we should make it possible to set this immediately
    // not wait for the next open
}

void ASIStreamHandler::SetRunningDesired(bool desired)
{
    if (_drb && _running_desired && !desired)
        _drb->Stop();
    StreamHandler::SetRunningDesired(desired);
}

void ASIStreamHandler::run(void)
{
    RunProlog();

    LOG(VB_RECORD, LOG_INFO, LOC + "run(): begin");

    if (!Open())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open device %1 : %2")
                .arg(_device).arg(strerror(errno)));
        _error = true;
        return;
    }

    DeviceReadBuffer *drb = new DeviceReadBuffer(this, true, false);
    bool ok = drb->Setup(_device, _fd, _packet_size, _buf_size,
                         _num_buffers / 4);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate DRB buffer");
        delete drb;
        drb = NULL;
        Close();
        _error = true;
        RunEpilog();
        return;
    }

    uint buffer_size = _packet_size * 15000;
    unsigned char *buffer = new unsigned char[buffer_size];
    if (!buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate buffer");
        delete drb;
        drb = NULL;
        Close();
        _error = true;
        RunEpilog();
        return;
    }
    memset(buffer, 0, buffer_size);

    SetRunning(true, true, false);

    drb->Start();

    {
        QMutexLocker locker(&_start_stop_lock);
        _drb = drb;
    }

    int remainder = 0;
    while (_running_desired && !_error)
    {
        UpdateFiltersFromStreamData();

        ssize_t len = 0;

        len = drb->Read(
            &(buffer[remainder]), buffer_size - remainder);

        if (!_running_desired)
            break;

        // Check for DRB errors
        if (drb->IsErrored())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Device error detected");
            _error = true;
        }

        if (drb->IsEOF())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Device EOF detected");
            _error = true;
        }

        len += remainder;

        if (len < 10) // 10 bytes = 4 bytes TS header + 6 bytes PES header
        {
            remainder = len;
            continue;
        }

        if (!_listener_lock.tryLock())
        {
            remainder = len;
            continue;
        }

        if (_stream_data_list.empty())
        {
            _listener_lock.unlock();
            continue;
        }

        StreamDataList::const_iterator sit = _stream_data_list.begin();
        for (; sit != _stream_data_list.end(); ++sit)
            remainder = sit.key()->ProcessData(buffer, len);

        WriteMPTS(buffer, len - remainder);

        _listener_lock.unlock();

        if (remainder > 0 && (len > remainder)) // leftover bytes
            memmove(buffer, &(buffer[len - remainder]), remainder);
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "run(): " + "shutdown");

    RemoveAllPIDFilters();

    {
        QMutexLocker locker(&_start_stop_lock);
        _drb = NULL;
    }

    if (drb->IsRunning())
        drb->Stop();

    delete drb;
    delete[] buffer;
    Close();

    LOG(VB_RECORD, LOG_INFO, LOC + "run(): " + "end");

    SetRunning(false, true, false);
    RunEpilog();
}

bool ASIStreamHandler::Open(void)
{
    if (_fd >= 0)
        return true;

    QString error;
    _device_num = CardUtil::GetASIDeviceNumber(_device, &error);
    if (_device_num < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + error);
        return false;
    }

    _buf_size = CardUtil::GetASIBufferSize(_device_num, &error);
    if (_buf_size <= 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + error);
        return false;
    }

    _num_buffers = CardUtil::GetASINumBuffers(_device_num, &error);
    if (_num_buffers <= 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + error);
        return false;
    }

    if (!CardUtil::SetASIMode(_device_num, (uint)_rx_mode, &error))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set RX Mode: " + error);
        return false;
    }

    // actually open the device
    _fd = open(_device.toLocal8Bit().constData(), O_RDONLY, 0);
    if (_fd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open '%1'").arg(_device) + ENO);
        return false;
    }

    // get the rx capabilities
    unsigned int cap;
    if (ioctl(_fd, ASI_IOC_RXGETCAP, &cap) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to query capabilities '%1'").arg(_device) + ENO);
        Close();
        return false;
    }
    // TODO? do stuff with capabilities..

    // we need to handle 188 & 204 byte packets..
    switch (_rx_mode)
    {
        case kASIRXRawMode:
        case kASIRXSyncOnActualSize:
            _packet_size = TSPacket::kDVBEmissionSize *  TSPacket::kSize;
            break;
        case kASIRXSyncOn204:
            _packet_size = TSPacket::kDVBEmissionSize;
            break;
        case kASIRXSyncOn188:
        case kASIRXSyncOnActualConvertTo188:
        case kASIRXSyncOn204ConvertTo188:
            _packet_size = TSPacket::kSize;
            break;
    }

    // pid counter?

    return _fd >= 0;
}

void ASIStreamHandler::Close(void)
{
    if (_fd >= 0)
    {
        close(_fd);
        _fd = -1;
    }
}

void ASIStreamHandler::PriorityEvent(int fd)
{
    // TODO report on buffer overruns, etc.
}
