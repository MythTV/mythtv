/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall, Dave Abrahams
 *  Copyright (c) 2006 by Daniel Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "mythverbose.h"
#include "tv_rec.h"
#include "linuxfirewiredevice.h"
#include "darwinfirewiredevice.h"
#include "firewirechannel.h"

#define LOC QString("FireChan(%1): ").arg(GetDevice())
#define LOC_WARN QString("FireChan(%1), Warning: ").arg(GetDevice())
#define LOC_ERR QString("FireChan(%1), Error: ").arg(GetDevice())

FirewireChannel::FirewireChannel(TVRec *parent, const QString &_videodevice,
                                 const FireWireDBOptions &firewire_opts) :
    DTVChannel(parent),
    videodevice(_videodevice),
    fw_opts(firewire_opts),
    device(NULL),
    current_channel(0),
    isopen(false)
{
    uint64_t guid = string_to_guid(videodevice);
    uint subunitid = 0; // we only support first tuner on STB...

#ifdef USING_LINUX_FIREWIRE
    device = new LinuxFirewireDevice(
        guid, subunitid, fw_opts.speed,
        LinuxFirewireDevice::kConnectionP2P == (uint) fw_opts.connection);
#endif // USING_LINUX_FIREWIRE

#ifdef USING_OSX_FIREWIRE
    device = new DarwinFirewireDevice(guid, subunitid, fw_opts.speed);
#endif // USING_OSX_FIREWIRE
}

bool FirewireChannel::Open(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Open()");

    if (!device)
        return false;

    if (isopen)
        return true;

    if (!InitializeInputs())
        return false;

    if (m_inputs.find(m_currentInputID) == m_inputs.end())
        return false;

    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    if (!FirewireDevice::IsSTBSupported(fw_opts.model) &&
        (*it)->externalChanger.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Model: '%1' is not supported.").arg(fw_opts.model));

        return false;
    }

    if (!device->OpenPort())
        return false;

    isopen = true;

    return true;
}

void FirewireChannel::Close(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Close()");
    if (isopen)
    {
        device->ClosePort();
        isopen = false;
    }
}

QString FirewireChannel::GetDevice(void) const
{
    return videodevice;
}

bool FirewireChannel::SetPowerState(bool on)
{
    if (!isopen)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "SetPowerState() called on closed FirewireChannel.");

        return false;
    }

    return device->SetPowerState(on);
}

FirewireDevice::PowerState FirewireChannel::GetPowerState(void) const
{
    if (!isopen)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "GetPowerState() called on closed FirewireChannel.");

        return FirewireDevice::kAVCPowerQueryFailed;
    }

    return device->GetPowerState();
}

bool FirewireChannel::Retune(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Retune()");

    if (FirewireDevice::kAVCPowerOff == GetPowerState())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "STB is turned off, must be on to retune.");

        return false;
    }

    if (current_channel)
    {
        QString freqid = QString::number(current_channel);
        return Tune(freqid, 0);
    }

    return false;
}

bool FirewireChannel::Tune(const QString &freqid, int /*finetune*/)
{
    VERBOSE(VB_CHANNEL, QString("Tune(%1)").arg(freqid));

    bool ok;
    uint channel = freqid.toUInt(&ok);
    if (!ok)
        return false;

    if (FirewireDevice::kAVCPowerOff == GetPowerState())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "STB is turned off, must be on to set channel.");

        return true; // signal monitor will call retune later...
    }

    if (!device->SetChannel(fw_opts.model, 0, channel))
        return false;

    current_channel = channel;

    return true;
}
