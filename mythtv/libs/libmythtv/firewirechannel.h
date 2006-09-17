/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  SA3250HD support Copyright (c) 2005 by Matt Porter
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#ifndef FIREWIRECHANNEL_H
#define FIREWIRECHANNEL_H

#include <qstring.h>
#include "tv_rec.h"
#include "firewirechannelbase.h"
#include <libavc1394/avc1394.h>

using namespace std;

class FirewireChannel : public FirewireChannelBase
{
  public:
    enum PowerState {
        On,
        Off,
        Failed
    };

    FirewireChannel(FireWireDBOptions firewire_opts, TVRec *parent);
    ~FirewireChannel(void);

    bool OpenFirewire(void); 
    void CloseFirewire(void); 

    // Sets
    void SetExternalChanger(void);
    bool SetChannelByNumber(int channel);

    // Gets
    bool IsOpen(void) const { return isopen; }
    QString GetDevice(void) const
        { return QString("%1:%2").arg(fw_opts.port).arg(fw_opts.node); }
    PowerState GetPowerState(void);

  private:
    FireWireDBOptions  fw_opts;
    nodeid_t           fwnode;
    raw1394handle_t    fwhandle;
};

#endif
