/**
 *  DarwinDarwinFirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall and Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV includes
#include "darwinfirewirerecorder.h"
#include "tspacket.h"

#undef always_inline
#include <AVCVideoServices/AVCVideoServices.h>

DarwinFirewireRecorder::DarwinFirewireRecorder(TVRec *rec, ChannelBase* tuner)
 : FirewireRecorderBase(rec, "DarwinFirewireRecorder"),
   capture_device(
       dynamic_cast<DarwinFirewireChannel*>(tuner)->GetAVCDevice()
   ),
   message_log(NULL),
   video_stream(NULL),
   isopen(false)
{;}

DarwinFirewireRecorder::~DarwinFirewireRecorder()
{
    this->Close();
}

// Various message callbacks.
IOReturn DarwinFirewireRecorder::MPEGNoData(void *pRefCon)
{
    
    DarwinFirewireRecorder* self = static_cast<DarwinFirewireRecorder*>(pRefCon);
    self->no_data();
    return 0;
}

void DarwinFirewireRecorder::no_data()
{
    VERBOSE(
        VB_IMPORTANT, 
        QString("Firewire: No Input in %1 seconds").arg(FIREWIRE_TIMEOUT));
}

namespace
{
  void avs_log_message(char *pString)
  {
      // I don't know what QString does with plain char*, but surely it
      // treats char const* as an NTBS.
      char const* s = pString;

      VERBOSE(VB_GENERAL,QString("Firewire MPEG2Receiver log: %1")
              .arg(s));
  }

  void avs_message_received(
      UInt32 msg, UInt32 param1, UInt32 param2, void *pRefCon)
  {
      (void)pRefCon;

      VERBOSE(VB_RECORD,QString("Firewire MPEG2Receiver message: %1")
              .arg(msg));

      switch (msg)
      {
      case AVS::kMpeg2ReceiverAllocateIsochPort:
          VERBOSE(
              VB_RECORD,
              QString("Firewire MPEG2Receiver allocated channel: %1, speed %2")
                  .arg(param2).arg(param1)
          );
          break;

      case AVS::kMpeg2ReceiverDCLOverrun:
          VERBOSE(
              VB_IMPORTANT,
              QString("Firewire MPEG2Receiver DCL Overrun")
          );
          break;

      case AVS::kMpeg2ReceiverReceivedBadPacket:
          VERBOSE(
              VB_IMPORTANT,
              QString("Firewire MPEG2Receiver Received Bad Packet ")
          );
          break;

      default:
          break;
      }
  }

  bool find_capture_device(AVS::AVCDevice* d)
  {
      // We'd check isMPEGDevice, but it turns out that for the
      // DCT-6200, Apple doesn't set that flag.  So instead we rule
      // out DV devices.
      // A more general OSX AVCRecorder class that also handles DV
      // devices might not check either flag.
      return d->isAttached && !d->isDVDevice 
//          && (d->hasTapeSubunit || d->hasMonitorOrTunerSubunit)
          ;
  }

  IOReturn device_controller_notification(AVS::AVCDeviceController *, void *, AVS::AVCDevice*)
  {
      return 0;
  }	
}

IOReturn DarwinFirewireRecorder::tspacket_callback(UInt32 tsPacketCount, UInt32 **ppBuf,void *pRefCon)
{
    for (UInt32 i = 0; i < tsPacketCount; ++i)
    {
        void* packet = ppBuf[i];
        int ok = FirewireRecorderBase::read_tspacket( 
            static_cast<unsigned char *>(packet), 
              AVS::kMPEG2TSPacketSize,
              0, // dropped
              pRefCon);

        // This is based on knowledge of what read_tspacket does --
        // the only way it fails is with a NULL callback_data
        // argument.
        if (!ok)
            return kIOReturnBadArgument;
    }
    
    return 0;
}


bool DarwinFirewireRecorder::Open()
{
     if (isopen)
         return true;
    
     VERBOSE(VB_GENERAL,QString("Firewire: Creating logger object"));

     this->message_log = new AVS::StringLogger(avs_log_message);
     if (!this->message_log)
     {
         VERBOSE(VB_IMPORTANT, QString("Firewire: Couldn't create logger") );
         return false;
     }

     // If we don't set this immediately, Close() will refuse to clean
     // up after whatever mess we make here
     this->isopen = true;

     VERBOSE(VB_GENERAL,QString("Firewire: Creating MPEG-2 device stream"));
     
     // This not only builds an MPEG2Receiver object but also starts dedicated real-time threads.
     this->video_stream = capture_device->CreateMPEGReceiverForDevicePlug(
         0,                // Plug number.  Why is zero always OK?  I
                           // don't know, but that's what Apple's
                           // examples do.
         tspacket_callback,
         this,
         avs_message_received,
         this,
         this->message_log,
         AVS::kCyclesPerReceiveSegment,
         // Why multiply by 2 instead of using the default,
         // kNumReceiveSegments?  Because it's what Apple's only
         // example of the use of this function does.
         AVS::kNumReceiveSegments*2);
         
     if (!this->video_stream)
     {
         VERBOSE(VB_IMPORTANT, QString("Firewire: Couldn't create MPEG-2 device stream") );
         this->Close();
         return false;
     }

	// We could set the channel to receive on, but it doesn't seem
	// like we need to, and if the device is already transmitting it
	// could lead to inefficiency because the device stream is smart
	// enough to avoid allocating new bandwidth.

	// Register a no-data notification callback
	video_stream->pMPEGReceiver->registerNoDataNotificationCallback(
        MPEGNoData, this, FIREWIRE_TIMEOUT * 1000);
	
     return true;
}

void DarwinFirewireRecorder::Close()
{
    if (!isopen)
        return;
    
    isopen = false;

    if (this->video_stream)
    {
        this->stop();
        VERBOSE(VB_RECORD, "Firewire: Destroying device stream");
        this->capture_device->DestroyAVCDeviceStream(this->video_stream);
        this->video_stream = 0;
    }

    delete this->message_log;
    this->message_log = 0;
}

void DarwinFirewireRecorder::start()
{
    VERBOSE(VB_RECORD, "Firewire: Starting video stream");
    this->capture_device->StartAVCDeviceStream(this->video_stream);
}

void DarwinFirewireRecorder::stop()
{
    VERBOSE(VB_RECORD, "Firewire: Stopping video stream");
    this->capture_device->StopAVCDeviceStream(this->video_stream);
}

bool DarwinFirewireRecorder::grab_frames()
{
    usleep(1000000 / 2);  // 2 times a second
    return true;
}        

void DarwinFirewireRecorder::SetOption(const QString &name, const QString &value) 
{
    (void)name;
    (void)value;
}

void DarwinFirewireRecorder::SetOption(const QString &name, int value) 
{
    (void)name;
    (void)value;
}
