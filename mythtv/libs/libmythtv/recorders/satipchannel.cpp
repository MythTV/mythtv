// C++ includes
#include <utility>

// Qt includes
#include <QMutexLocker>
#include <QString>

// MythTV includes
#include "mythlogging.h"
#include "mpegtables.h"
#include "tv_rec.h"
#include "satiputils.h"
#include "satipchannel.h"

#define LOC  QString("SatIPChan[%1](%2): ").arg(m_inputId).arg(GetDevice())

SatIPChannel::SatIPChannel(TVRec *parent, QString  device) :
    DTVChannel(parent), m_device(std::move(device))
{
}

SatIPChannel::~SatIPChannel(void)
{
    SatIPChannel::Close();
}

bool SatIPChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

    if (IsOpen())
        return true;

    QMutexLocker locker(&m_tuneLock);

    m_tunerType = SatIP::toTunerType(m_device);

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Open() m_tunerType:%1 %2")
        .arg(m_tunerType).arg(m_tunerType.toString()));

    if (!InitializeInput())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "InitializeInput() failed");
        Close();
        return false;
    }
    OpenStreamHandler();
    return true;
}

void SatIPChannel::Close()
{
    LOG(VB_CHANNEL, LOG_INFO, QString("SatIPChan[%1](%2): Close()")
        .arg(m_inputId).arg(SatIPChannel::GetDevice()));
    CloseStreamHandler();
}

void SatIPChannel::OpenStreamHandler(void)
{
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + "OpenStreamHandler()");
    m_streamHandler = SatIPStreamHandler::Get(m_device, GetInputID());
}

void SatIPChannel::CloseStreamHandler(void)
{
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + "CloseStreamHandler()");
    QMutexLocker locker(&m_streamLock);
    if (m_streamHandler)
    {
        if (m_streamData)
        {
            m_streamHandler->RemoveListener(m_streamData);
        }
        SatIPStreamHandler::Return(m_streamHandler, m_inputId);
    }
}

bool SatIPChannel::Tune(const QString &channum)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(channum=%1) TODO").arg(channum));
    if (!IsOpen())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "Tune failed, not open");
        return false;
    }
    return false;
}

bool SatIPChannel::Tune(const DTVMultiplex &tuning)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(frequency=%1)").arg(tuning.m_frequency));
    m_streamHandler->Tune(tuning);
    return true;
}

bool SatIPChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_streamLock);
    return m_streamHandler;
}
