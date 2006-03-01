/**
 *  DarwinFirewireChannel
 *  Copyright (c) 2005 by Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#ifndef LIBMYTHTV_DARWINFIREWIRECHANNEL_H
#define LIBMYTHTV_DARWINFIREWIRECHANNEL_H

#include <qstring.h>
#include "tv_rec.h"
#include "firewirechannelbase.h"


namespace AVS
{
  class AVCDeviceController;
  class AVCDevice;
}

class DarwinFirewireChannel : public FirewireChannelBase
{
  public:
    DarwinFirewireChannel(FireWireDBOptions const&, TVRec *parent);

    // Gets
    AVS::AVCDevice* GetAVCDevice() const;

    // Sets
    bool SetChannelByNumber(int channel);

  private:
    bool OpenFirewire();
    void CloseFirewire();

  private:
    AVS::AVCDeviceController* device_controller;
    AVS::AVCDevice* device;
};

#endif
