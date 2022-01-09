/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall, Dave Abrahams
 *  Copyright (c) 2006 by Daniel Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C/C++ includes
#include <utility>

#include "libmythbase/mythlogging.h"
#include "tv_rec.h"
#include "linuxfirewiredevice.h"
#ifdef USING_OSX_FIREWIRE
#include "darwinfirewiredevice.h"
#endif
#include "firewirechannel.h"

#define LOC QString("FireChan[%1](%2): ").arg(m_inputId).arg(FirewireChannel::GetDevice())

FirewireChannel::FirewireChannel(TVRec *parent, QString _videodevice,
                                 FireWireDBOptions firewire_opts) :
    DTVChannel(parent),
    m_videodevice(std::move(_videodevice)),
    m_fwOpts(std::move(firewire_opts))
{
    uint64_t guid = string_to_guid(m_videodevice);
    uint subunitid = 0; // we only support first tuner on STB...

#ifdef USING_LINUX_FIREWIRE
    m_device = new LinuxFirewireDevice(
        guid, subunitid, m_fwOpts.m_speed,
        LinuxFirewireDevice::kConnectionP2P == (uint) m_fwOpts.m_connection);
#endif // USING_LINUX_FIREWIRE

#ifdef USING_OSX_FIREWIRE
    m_device = new DarwinFirewireDevice(guid, subunitid, m_fwOpts.m_speed);
#endif // USING_OSX_FIREWIRE
}

FirewireChannel::~FirewireChannel()
{
    FirewireChannel::Close();
    delete m_device;
}

bool FirewireChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open()");

    if (!m_device)
        return false;

    if (m_isopen)
        return true;

    if (!InitializeInput())
        return false;

    if (!m_inputId)
        return false;

    if (!FirewireDevice::IsSTBSupported(m_fwOpts.m_model) &&
        !IsExternalChannelChangeInUse())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Model: '%1' is not supported.").arg(m_fwOpts.m_model));

        return false;
    }

    if (!m_device->OpenPort())
        return false;

    m_isopen = true;

    return true;
}

void FirewireChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close()");
    if (m_isopen)
    {
        m_device->ClosePort();
        m_isopen = false;
    }
}

QString FirewireChannel::GetDevice(void) const
{
    return m_videodevice;
}

bool FirewireChannel::SetPowerState(bool on)
{
    if (!m_isopen)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "SetPowerState() called on closed FirewireChannel.");

        return false;
    }

    return m_device->SetPowerState(on);
}

FirewireDevice::PowerState FirewireChannel::GetPowerState(void) const
{
    if (!m_isopen)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "GetPowerState() called on closed FirewireChannel.");

        return FirewireDevice::kAVCPowerQueryFailed;
    }

    return m_device->GetPowerState();
}

bool FirewireChannel::Retune(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Retune()");

    if (FirewireDevice::kAVCPowerOff == GetPowerState())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "STB is turned off, must be on to retune.");

        return false;
    }

    if (m_currentChannel)
    {
        QString freqid = QString::number(m_currentChannel);
        return Tune(freqid, 0);
    }

    return false;
}

bool FirewireChannel::Tune(const QString &freqid, int /*finetune*/)
{
    LOG(VB_CHANNEL, LOG_INFO, QString("Tune(%1)").arg(freqid));

    bool ok = false;
    uint channel = freqid.toUInt(&ok);
    if (!ok)
        return false;

    if (FirewireDevice::kAVCPowerOff == GetPowerState())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
                "STB is turned off, must be on to set channel.");

        return true; // signal monitor will call retune later...
    }

    if (!m_device->SetChannel(m_fwOpts.m_model, 0, channel))
        return false;

    m_currentChannel = channel;

    return true;
}
