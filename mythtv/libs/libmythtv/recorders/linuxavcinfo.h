#ifndef _LINUX_AVC_INFO_H_
#define _LINUX_AVC_INFO_H_

#ifdef USING_LINUX_FIREWIRE

// Linux headers
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <libiec61883/iec61883.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/rom1394.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QMap>

// MythTV headers
#include "avcinfo.h"

class LinuxAVCInfo : public AVCInfo
{
  public:
    LinuxAVCInfo() : fw_handle(NULL) { }

    bool Update(uint64_t _guid, raw1394handle_t handle,
                uint _port, uint _node);

    bool OpenPort(void);
    bool ClosePort(void);

    virtual bool SendAVCCommand(
        const vector<uint8_t> &cmd,
        vector<uint8_t>       &result,
        int                    retry_cnt);

    bool IsPortOpen(void) const { return fw_handle; }

    /// Returns remote node
    int GetNode(void) const { return node; }

  public:
    raw1394handle_t fw_handle;
};
typedef QMap<uint64_t,LinuxAVCInfo*> avcinfo_list_t;

#endif // USING_LINUX_FIREWIRE

#endif // _LINUX_AVC_INFO_H_
