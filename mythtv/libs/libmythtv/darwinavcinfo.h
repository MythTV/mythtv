#ifndef _DARWIN_AVC_INFO_H_
#define _DARWIN_AVC_INFO_H_

#ifdef USING_OSX_FIREWIRE

// C++ headers
#include <vector>
using namespace std;

// OS X headers
#undef always_inline
#include <IOKit/IOKitLib.h>
#include <IOKit/firewire/IOFireWireLib.h>
#include <IOKit/firewire/IOFireWireLibIsoch.h>
#include <IOKit/firewire/IOFireWireFamilyCommon.h>
#include <IOKit/avc/IOFireWireAVCLib.h>

// Qt headers
#include <QMap>

// MythTV headers
#include "avcinfo.h"

class DarwinFirewireDevice;

class DarwinAVCInfo : public AVCInfo
{
  public:
    DarwinAVCInfo() :
        fw_node_ref(NULL), fw_device_ref(NULL),
        fw_service_ref(NULL), avc_service_ref(NULL),
        fw_device_notifier_ref(NULL),
        avc_handle(NULL), fw_handle(NULL) { }

    void Update(uint64_t _guid, DarwinFirewireDevice *dev,
                IONotificationPortRef notify_port,
                CFRunLoopRef &thread_cf_ref, io_object_t obj);

    bool OpenPort(CFRunLoopRef &thread_cf_ref);
    bool ClosePort(void);

    bool OpenAVCInterface(CFRunLoopRef &thread_cf_ref);
    void CloseAVCInterface(void);

    bool OpenDeviceInterface(CFRunLoopRef &thread_cf_ref);
    void CloseDeviceInterface(void);

    virtual bool SendAVCCommand(
        const vector<uint8_t> &cmd,
        vector<uint8_t>       &result,
        int                   retry_cnt);

    bool GetDeviceNodes(int &local_node, int &remote_node);

    bool IsAVCInterfaceOpen(void) const { return avc_handle; }
    bool IsPortOpen(void)         const { return fw_handle;  }

  public:
    io_service_t fw_node_ref;     // parent of fw_device_ref
    io_service_t fw_device_ref;   // parent of fw_service_ref
    io_service_t fw_service_ref;  // parent of avc_service_ref
    io_service_t avc_service_ref;

    io_object_t  fw_device_notifier_ref;

    IOFireWireAVCLibUnitInterface **avc_handle;
    IOFireWireLibDeviceRef          fw_handle;
};
typedef QMap<uint64_t,DarwinAVCInfo*> avcinfo_list_t;

#endif // USING_OSX_FIREWIRE

#endif // _DARWIN_AVC_INFO_H_
