/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall and Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef LIBMYTHTV_DARWINFIREWIRERECORDER_H_
#define LIBMYTHTV_DARWINFIREWIRERECORDER_H_

#include "firewirerecorderbase.h"
#include "darwinfirewirechannel.h"

//#include <IOKit/IOReturn.h>
//#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

typedef unsigned long UInt32;
typedef int IOReturn;

namespace AVS
{
  class AVCDeviceController;
  class AVCDevice;
  class StringLogger;
  class AVCDeviceStream;
}

/** \class DarwinFirewireRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle DVB and ATSC streams from a firewire input.
 *
 *  \sa DTVRecorder
 */
class DarwinFirewireRecorder : public FirewireRecorderBase
{
  public:
    DarwinFirewireRecorder(TVRec *rec, ChannelBase* tuner);
    ~DarwinFirewireRecorder();

    bool Open(void); 

    void SetOption(const QString &name, const QString &value);
    void SetOption(const QString &name, int value);

  private:
    void Close();

    void start();
    void stop();
    void no_data();
    bool grab_frames();

    static IOReturn MPEGNoData(void* pRefCon);
    static IOReturn tspacket_callback(UInt32 tsPacketCount, UInt32 **ppBuf, void *pRefCon);

    AVS::AVCDevice* capture_device;
    AVS::StringLogger* message_log;
    AVS::AVCDeviceStream* video_stream;

    bool isopen;
};

#endif
