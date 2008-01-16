/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall, Dave Abrahams
 *  Copyright (c) 2006 by Daniel Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "mythcontext.h"
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

    InitializeInputs();
}

bool FirewireChannel::SetChannelByString(const QString &channum)
{
    QString loc = LOC + QString("SetChannelByString(%1)").arg(channum);
    VERBOSE(VB_CHANNEL, loc);

    InputMap::const_iterator it = inputs.find(currentInputID);
    if (it == inputs.end())
        return false;

    uint mplexid_restriction;
    if (!IsInputAvailable(currentInputID, mplexid_restriction))
    {
        VERBOSE(VB_IMPORTANT, loc + " " + QString(
                    "Requested channel '%1' is on input '%2' "
                    "which is in a busy input group")
                .arg(channum).arg(currentInputID));

        return false;
    }

    // Fetch tuning data from the database.
    QString tvformat, modulation, freqtable, freqid, dtv_si_std;
    int finetune;
    uint64_t frequency;
    int mpeg_prog_num;
    uint atsc_major, atsc_minor, mplexid, tsid, netid;

    if (!ChannelUtil::GetChannelData(
        (*it)->sourceid, channum,
        tvformat, modulation, freqtable, freqid,
        finetune, frequency,
        dtv_si_std, mpeg_prog_num, atsc_major, atsc_minor, tsid, netid,
        mplexid, commfree))
    {
        VERBOSE(VB_IMPORTANT, loc + " " + QString(
                    "Requested channel '%1' is on input '%2' "
                    "which is in a busy input group")
                .arg(channum).arg(currentInputID));

        return false;
    }

    if (mplexid_restriction && (mplexid != mplexid_restriction))
    {
        VERBOSE(VB_IMPORTANT, loc + " " + QString(
                    "Requested channel '%1' is not available because "
                    "the tuner is currently in use on another transport.")
                .arg(channum));

        return false;
    }

    bool ok = false;
    if (!(*it)->externalChanger.isEmpty())
    {
        ok = ChangeExternalChannel(freqid);
        SetSIStandard("mpeg");
        SetDTVInfo(0,0,0,0,1);
    }
    else
    {
        uint ichan = freqid.toUInt(&ok);
        ok = ok && isopen && SetChannelByNumber(ichan);
    }

    if (ok)
    {
        // Set the current channum to the new channel's channum
        curchannelname = QDeepCopy<QString>(channum);
        (*it)->startChanNum = QDeepCopy<QString>(channum);
    }

    VERBOSE(VB_CHANNEL, loc + " " + ((ok) ? "success" : "failure"));

    return ok;
}

bool FirewireChannel::Open(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Open()");

    if (inputs.find(currentInputID) == inputs.end())
        return false;

    if (!device)
        return false;

    if (isopen)
        return true;

    InputMap::const_iterator it = inputs.find(currentInputID);
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
        return SetChannelByNumber(current_channel);

    return false;
}

bool FirewireChannel::SetChannelByNumber(int channel)
{
    VERBOSE(VB_CHANNEL, QString("SetChannelByNumber(%1)").arg(channel));
    current_channel = channel;

    if (FirewireDevice::kAVCPowerOff == GetPowerState())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "STB is turned off, must be on to set channel.");

        SetSIStandard("mpeg");
        SetDTVInfo(0,0,0,0,1);

        return true; // signal monitor will call retune later...
    }

    if (!device->SetChannel(fw_opts.model, 0, channel))
        return false;

    SetSIStandard("mpeg");
    SetDTVInfo(0,0,0,0,1);

    return true;
}
