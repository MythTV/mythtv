// C++ includes
#include <utility>

// Qt includes
#include <QMutexLocker>
#include <QString>

// MythTV includes
#include "libmythbase/mythlogging.h"

#include "mpeg/mpegtables.h"
#include "satipchannel.h"
#include "satiputils.h"
#include "tv_rec.h"

#define LOC QString("SatIPChan[%1]: ").arg(m_inputId)

SatIPChannel::SatIPChannel(TVRec *parent, QString  device) :
    DTVChannel(parent), m_device(std::move(device))
{
    RegisterForMaster(m_device);
}

SatIPChannel::~SatIPChannel(void)
{
    SatIPChannel::Close();
    DeregisterForMaster(m_device);
}

bool SatIPChannel::IsMaster(void) const
{
    DTVChannel *master = DTVChannel::GetMasterLock(m_device);
    bool is_master = (master == this);
    DTVChannel::ReturnMasterLock(master);
    return is_master;
}

bool SatIPChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Open(%1)").arg(m_device));

    if (IsOpen())
    {
        return true;
    }

    QMutexLocker locker(&m_tuneLock);

    m_tunerType = SatIP::toTunerType(m_device);

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Open(%1) m_tunerType:%2 %3")
        .arg(m_device).arg(m_tunerType).arg(m_tunerType.toString()));

    if (!InitializeInput())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "InitializeInput() failed");
        Close();
        return false;
    }

    m_streamHandler = SatIPStreamHandler::Get(m_device, GetInputID());

    return true;
}

void SatIPChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Close(%1)").arg(m_device));

    QMutexLocker locker(&m_streamLock);
    if (m_streamHandler)
    {
        if (m_streamData)
        {
            m_streamHandler->RemoveListener(m_streamData);
        }
        SatIPStreamHandler::Return(m_streamHandler, GetInputID());
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

bool SatIPChannel::EnterPowerSavingMode(void)
{
    return true;
}

bool SatIPChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_streamLock);
    bool result = m_streamHandler != nullptr;
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("IsOpen:%1").arg(result));
    return result;
}

bool SatIPChannel::Tune(const DTVMultiplex &tuning)
{
    uint satipsrc = CardUtil::GetDiSEqCPosition(GetInputID()).toUInt();
    satipsrc = std::clamp(satipsrc, 1U, 255U);
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune freq=%1,src=%2").arg(tuning.m_frequency).arg(satipsrc));

    m_streamHandler->m_satipsrc = satipsrc;
    if (m_streamHandler->Tune(tuning))
    {
        SetSIStandard(tuning.m_sistandard);
        return true;
    }
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Tune failed"));
    return false;
}
