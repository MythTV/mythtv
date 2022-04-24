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
#include "asichannel.h"
#include "asistreamhandler.h"
#include "cardutil.h"
#include "dtvsignalmonitor.h"
#include "mpeg/mpegstreamdata.h"
#include "mpeg/streamlisteners.h"

// DVEO ASI headers
#include <dveo/asi.h>
#include <dveo/master.h>

#define LOC      QString("ASISH[%1](%2): ").arg(m_inputId).arg(m_device)

QMap<QString,ASIStreamHandler*> ASIStreamHandler::s_handlers;
QMap<QString,uint>              ASIStreamHandler::s_handlersRefCnt;
QMutex                          ASIStreamHandler::s_handlersLock;

ASIStreamHandler *ASIStreamHandler::Get(const QString &devname,
                                        int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    const QString& devkey = devname;

    QMap<QString,ASIStreamHandler*>::iterator it = s_handlers.find(devkey);

    if (it == s_handlers.end())
    {
        auto *newhandler = new ASIStreamHandler(devname, inputid);
        newhandler->Open();
        s_handlers[devkey] = newhandler;
        s_handlersRefCnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("ASISH[%1]: Creating new stream handler %2 for %3")
            .arg(QString::number(inputid), devkey, devname));
    }
    else
    {
        s_handlersRefCnt[devkey]++;
        uint rcount = s_handlersRefCnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("ASISH[%1]: Using existing stream handler %2 for %3")
                .arg(QString::number(inputid), devkey, devname) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[devkey];
}

void ASIStreamHandler::Return(ASIStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    QString devname = ref->m_device;

    QMap<QString,uint>::iterator rit = s_handlersRefCnt.find(devname);
    if (rit == s_handlersRefCnt.end())
        return;

    QMap<QString,ASIStreamHandler*>::iterator it = s_handlers.find(devname);

    if (*rit > 1)
    {
        ref = nullptr;
        (*rit)--;
        return;
    }

    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("ASISH[%1]: Closing handler for %2")
            .arg(inputid).arg(devname));
        ref->Close();
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ASISH[%1] Error: Couldn't find handler for %2")
            .arg(inputid).arg(devname));
    }

    s_handlersRefCnt.erase(rit);
    ref = nullptr;
}

ASIStreamHandler::ASIStreamHandler(const QString &device, int inputid)
    : StreamHandler(device, inputid)
{
    setObjectName("ASISH");
}

void ASIStreamHandler::SetClockSource(ASIClockSource cs)
{
    m_clockSource = cs;
    // TODO we should make it possible to set this immediately
    // not wait for the next open
}

void ASIStreamHandler::SetRXMode(ASIRXMode m)
{
    m_rxMode = m;
    // TODO we should make it possible to set this immediately
    // not wait for the next open
}

void ASIStreamHandler::SetRunningDesired(bool desired)
{
    if (m_drb && m_runningDesired && !desired)
        m_drb->Stop();
    StreamHandler::SetRunningDesired(desired);
}

void ASIStreamHandler::run(void)
{
    RunProlog();

    LOG(VB_RECORD, LOG_INFO, LOC + "run(): begin");

    if (!Open())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open device %1 : %2")
                .arg(m_device, strerror(errno)));
        m_bError = true;
        return;
    }

    auto *drb = new DeviceReadBuffer(this, true, false);
    bool ok = drb->Setup(m_device, m_fd, m_packetSize, m_bufSize,
                         m_numBuffers / 4);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate DRB buffer");
        delete drb;
        drb = nullptr;
        Close();
        m_bError = true;
        RunEpilog();
        return;
    }

    uint buffer_size = m_packetSize * 15000;
    auto *buffer = new unsigned char[buffer_size];
    if (!buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate buffer");
        delete drb;
        drb = nullptr;
        Close();
        m_bError = true;
        RunEpilog();
        return;
    }
    memset(buffer, 0, buffer_size);

    SetRunning(true, true, false);

    drb->Start();

    {
        QMutexLocker locker(&m_startStopLock);
        m_drb = drb;
    }

    int remainder = 0;
    while (m_runningDesired && !m_bError)
    {
        UpdateFiltersFromStreamData();

        ssize_t len = 0;

        len = drb->Read(
            &(buffer[remainder]), buffer_size - remainder);

        if (!m_runningDesired)
            break;

        // Check for DRB errors
        if (drb->IsErrored())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Device error detected");
            m_bError = true;
        }

        if (drb->IsEOF())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Device EOF detected");
            m_bError = true;
        }

        len += remainder;

        if (len < 10) // 10 bytes = 4 bytes TS header + 6 bytes PES header
        {
            remainder = len;
            continue;
        }

        if (!m_listenerLock.tryLock())
        {
            remainder = len;
            continue;
        }

        if (m_streamDataList.empty())
        {
            m_listenerLock.unlock();
            continue;
        }

        for (auto sit = m_streamDataList.cbegin();
             sit != m_streamDataList.cend(); ++sit)
            remainder = sit.key()->ProcessData(buffer, len);

        WriteMPTS(buffer, len - remainder);

        m_listenerLock.unlock();

        if (remainder > 0 && (len > remainder)) // leftover bytes
            memmove(buffer, &(buffer[len - remainder]), remainder);
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "run(): " + "shutdown");

    RemoveAllPIDFilters();

    {
        QMutexLocker locker(&m_startStopLock);
        m_drb = nullptr;
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
    if (m_fd >= 0)
        return true;

    QString error;
    m_deviceNum = CardUtil::GetASIDeviceNumber(m_device, &error);
    if (m_deviceNum < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + error);
        return false;
    }

    m_bufSize = CardUtil::GetASIBufferSize(m_deviceNum, &error);
    if (m_bufSize <= 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + error);
        return false;
    }

    m_numBuffers = CardUtil::GetASINumBuffers(m_deviceNum, &error);
    if (m_numBuffers <= 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + error);
        return false;
    }

    if (!CardUtil::SetASIMode(m_deviceNum, (uint)m_rxMode, &error))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set RX Mode: " + error);
        return false;
    }

    // actually open the device
    m_fd = open(m_device.toLocal8Bit().constData(), O_RDONLY, 0);
    if (m_fd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open '%1'").arg(m_device) + ENO);
        return false;
    }

    // get the rx capabilities
    unsigned int cap = 0;
    if (ioctl(m_fd, ASI_IOC_RXGETCAP, &cap) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to query capabilities '%1'").arg(m_device) + ENO);
        Close();
        return false;
    }
    // TODO? do stuff with capabilities..

    // we need to handle 188 & 204 byte packets..
    switch (m_rxMode)
    {
        case kASIRXRawMode:
        case kASIRXSyncOnActualSize:
            m_packetSize = TSPacket::kDVBEmissionSize *  TSPacket::kSize;
            break;
        case kASIRXSyncOn204:
            m_packetSize = TSPacket::kDVBEmissionSize;
            break;
        case kASIRXSyncOn188:
        case kASIRXSyncOnActualConvertTo188:
        case kASIRXSyncOn204ConvertTo188:
            m_packetSize = TSPacket::kSize;
            break;
    }

    // pid counter?

    return m_fd >= 0;
}

void ASIStreamHandler::Close(void)
{
    if (m_fd >= 0)
    {
        close(m_fd);
        m_fd = -1;
    }
}

void ASIStreamHandler::PriorityEvent(int fd)
{
    int val = 0;
    if(ioctl(fd, ASI_IOC_RXGETEVENTS, &val) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open device %1: ")
            .arg(m_device) + ENO);
        //TODO: Handle error
        return;
    }
    if(val & ASI_EVENT_RX_BUFFER)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Driver receive buffer queue overrun detected %1")
            .arg(m_device));
    }
    if(val & ASI_EVENT_RX_FIFO)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Driver receive FIFO overrun detected %1")
            .arg(m_device));
    }
    if(val & ASI_EVENT_RX_CARRIER)
    {
        LOG(VB_RECORD, LOG_NOTICE, LOC +
            QString("Carrier Status change detected %1")
            .arg(m_device));
    }
    if(val & ASI_EVENT_RX_LOS)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Loss of Packet Sync detected %1")
            .arg(m_device));
    }
    if(val & ASI_EVENT_RX_AOS)
    {
        LOG(VB_RECORD, LOG_NOTICE, LOC +
            QString("Acquisition of Sync detected %1")
            .arg(m_device));
    }
    if(val & ASI_EVENT_RX_DATA)
    {
        LOG(VB_RECORD, LOG_NOTICE, LOC +
            QString("Receive data status change detected %1")
            .arg(m_device));
    }
}
