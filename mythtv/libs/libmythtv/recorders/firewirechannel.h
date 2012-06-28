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
    virtual bool Open(void);
    virtual void Close(void);

    using DTVChannel::Tune;
    virtual bool Tune(const DTVMultiplex&, QString) { return false; }
    virtual bool Tune(const QString &freqid, int finetune);
    virtual bool Retune(void);

    // Sets
    virtual bool SetPowerState(bool on);

    // Gets
    virtual bool IsOpen(void) const { return isopen; }
    virtual QString GetDevice(void) const;
    virtual bool IsExternalChannelChangeSupported(void) { return true; }

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
