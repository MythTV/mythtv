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
    friend class FirewireSignalMonitor;
    friend class FirewireRecorder;

  public:
    FirewireChannel(TVRec *parent, const QString &videodevice,
                    const FireWireDBOptions &firewire_opts);
    virtual ~FirewireChannel();

    // Commands
     bool Open(void) override; // ChannelBase
    void Close(void) override; // ChannelBase

    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex&) override // DTVChannel
        { return false; }
    bool Tune(const QString &freqid, int finetune) override; // DTVChannel
    bool Retune(void) override; // ChannelBase

    // Sets
    virtual bool SetPowerState(bool on);

    // Gets
    bool IsOpen(void) const override // ChannelBase
        { return isopen; }
    QString GetDevice(void) const override; // ChannelBase

  protected:
    bool IsExternalChannelChangeSupported(void) override // ChannelBase
        { return true; }

  private:
    virtual FirewireDevice::PowerState GetPowerState(void) const;
    virtual FirewireDevice *GetFirewireDevice(void) { return device; }

  protected:
    QString            videodevice;
    FireWireDBOptions  fw_opts;
    FirewireDevice    *device;
    uint               current_channel;
    bool               isopen;
};

#endif // _FIREWIRECHANNEL_H_
