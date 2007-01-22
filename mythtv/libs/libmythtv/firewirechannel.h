/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall and Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _FIREWIRECHANNEL_H_
#define _FIREWIRECHANNEL_H_

#include "tv_rec.h"
#include "dtvchannel.h"
#include "firewiredevice.h"

class FirewireChannel : public DTVChannel
{
  public:
    FirewireChannel(TVRec *parent, const QString &videodevice,
                    const FireWireDBOptions &firewire_opts);
    ~FirewireChannel() { Close(); }

    // Commands
    virtual bool Open(void);
    virtual void Close(void);
    virtual bool SwitchToInput(const QString &inputname, const QString &chan);
    virtual bool SwitchToInput(int newcapchannel, bool setstarting);

    virtual bool TuneMultiplex(uint /*mplexid*/, QString /*inputname*/)
        { return false; }
    virtual bool Tune(const DTVMultiplex &/*tuning*/, QString /*inputname*/)
        { return false; }
    virtual bool Retune(void);

    // Sets
    virtual bool SetChannelByString(const QString &chan);
    virtual bool SetChannelByNumber(int channel);
    virtual bool SetPowerState(bool on);

    // Gets
    virtual bool IsOpen(void) const { return isopen; }
    virtual FirewireDevice::PowerState GetPowerState(void) const;
    virtual QString GetDevice(void) const;
    virtual FirewireDevice *GetFirewireDevice(void) { return device; }

  protected:
    QString            videodevice;
    FireWireDBOptions  fw_opts;
    FirewireDevice    *device;
    uint               current_channel;
    bool               isopen;
};

#endif // _FIREWIRECHANNEL_H_
