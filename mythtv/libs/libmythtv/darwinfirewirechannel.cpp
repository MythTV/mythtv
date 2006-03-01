/**
 *  DarwinFirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  SA3250HD support Copyright (c) 2005 by Matt Porter
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#include <iostream>
#include "mythcontext.h"
#include "darwinfirewirechannel.h"

#include "selectavcdevice.h"

#undef always_inline
#include <AVCVideoServices/AVCVideoServices.h>


namespace
{
  bool find_device(AVS::AVCDevice* d)
  {
      return d->isAttached && d->hasMonitorOrTunerSubunit
          // For the time being, the DarwinFireWireRecorder doesn't
          // handle DVB devices, so there's no point in finding one we
          // can tune to, here.  That saves us from having to search
          // twice for an eligible device.
          && !d->isDVDevice  
          ;
  }
}

DarwinFirewireChannel::DarwinFirewireChannel(FireWireDBOptions const& firewire_opts,TVRec *parent)
  : FirewireChannelBase(parent)
  , device_controller(0)
  , device(0)
{
    (void)firewire_opts;
}

bool DarwinFirewireChannel::OpenFirewire()
{
    IOReturn err = AVS::CreateAVCDeviceController(&this->device_controller);
    if (err)
    {
        VERBOSE(
            VB_IMPORTANT, 
                QString("unable to open device controller: %1").arg(err,0,16));
        return false;
    }

    if ((this->device = SelectAVCDevice(device_controller, find_device)))
    {
        VERBOSE(VB_RECORD, QString("DarwinFirewireChannel: opening device") );
        err = this->device->openDevice();
        if (!err)
            return true;

        VERBOSE(
            VB_IMPORTANT,
            QString("FireWireChannel: couldn't open tuner device: %1").arg(err,0,16));
    }
    else
    {
        VERBOSE(
            VB_IMPORTANT,
            QString(
                "DarwinFireWireChannel: unable to find an attached"
                " MPEG2 device that supports channel changes"));
    }
    AVS::DestroyAVCDeviceController(this->device_controller);
    return false;
}

void DarwinFirewireChannel::CloseFirewire()
{
    this->device->closeDevice();
    AVS::DestroyAVCDeviceController(this->device_controller);
    // Leave the device controller for the destructor
}

AVS::AVCDevice* DarwinFirewireChannel::GetAVCDevice() const
{
    return this->device;
}

bool DarwinFirewireChannel::SetChannelByNumber(int channel)
{
     // If the tuner is off, try to turn it on.
     UInt8 power_state;
     IOReturn err = this->device->GetPowerState(&power_state);
     if (err == kIOReturnSuccess && power_state == kAVCPowerStateOff)
     {
         this->device->SetPowerState(kAVCPowerStateOn);
        
         // Give it time to power up.
         usleep(2000000); // Sleep for two seconds
     }

     AVS::PanelSubunitController panel(this->device);
     err = panel.Tune(channel);
     if (err != kIOReturnSuccess)
     {
         VERBOSE(VB_GENERAL, QString("DarwinFirewireChannel: Tuning failed: %1").arg(err,0,16));
         VERBOSE(VB_GENERAL, QString("Ignoring error per apple example"));
     }
     // Give it time to transition.        
     usleep(1000000); // Sleep for one second
     return true;
}
