#ifndef _DARWIN_AVC_INFO_H_
#define _DARWIN_AVC_INFO_H_

#ifdef USING_OSX_FIREWIRE

// C++ headers
#include <vector>

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
    DarwinAVCInfo() = default;

    void Update(uint64_t _guid, DarwinFirewireDevice *dev,
                IONotificationPortRef notify_port,
                CFRunLoopRef &thread_cf_ref, io_object_t obj);

    bool OpenPort(CFRunLoopRef &thread_cf_ref);
    bool ClosePort(void);

    bool OpenAVCInterface(CFRunLoopRef &thread_cf_ref);
    void CloseAVCInterface(void);

    bool OpenDeviceInterface(CFRunLoopRef &thread_cf_ref);
    void CloseDeviceInterface(void);

    bool SendAVCCommand(
        const std::vector<uint8_t> &cmd,
        std::vector<uint8_t>       &result,
        int                   retry_cnt) override; // AVCInfo

    bool GetDeviceNodes(int &local_node, int &remote_node);

    bool IsAVCInterfaceOpen(void) const { return avc_handle; }
    bool IsPortOpen(void)         const { return fw_handle;  }

  public:
    io_service_t fw_node_ref            {0};  // parent of fw_device_ref
    io_service_t fw_device_ref          {0};  // parent of fw_service_ref
    io_service_t fw_service_ref         {0};  // parent of avc_service_ref
    io_service_t avc_service_ref        {0};

    io_object_t  fw_device_notifier_ref {0};

    IOFireWireAVCLibUnitInterface **avc_handle {nullptr};
    IOFireWireLibDeviceRef          fw_handle  {nullptr};
};
using avcinfo_list_t = QMap<uint64_t,DarwinAVCInfo*>;

#endif // USING_OSX_FIREWIRE

#endif // _DARWIN_AVC_INFO_H_
