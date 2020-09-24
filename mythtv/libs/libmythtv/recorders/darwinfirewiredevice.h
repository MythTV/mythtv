#ifndef _DARWIN_FIREWIRE_DEVICE_H_
#define _DARWIN_FIREWIRE_DEVICE_H_

#include "firewiredevice.h"
#include <IOKit/IOKitLib.h>

class DFDPriv;
class DarwinAVCInfo;

class DarwinFirewireDevice : public FirewireDevice
{
    friend void *dfd_controller_thunk(void *callback_data);
    friend void dfd_update_device_list_item(DarwinFirewireDevice *dev,
                                       uint64_t guid, void *item);
    friend int dfd_no_data_notification(void *callback_data);
    friend void dfd_stream_msg(
        UInt32 msg, UInt32 param1,
        UInt32 param2, void *callback_data);
    friend int dfd_tspacket_handler(
        uint tsPacketCount, uint32_t **ppBuf, void *callback_data);


  public:
    DarwinFirewireDevice(uint64_t guid, uint subunitid, uint speed);
    ~DarwinFirewireDevice();

    bool OpenPort(void) override; // FirewireDevice
    bool ClosePort(void) override; // FirewireDevice
    bool ResetBus(void) override; // FirewireDevice

    void HandleDeviceChange(uint messageType);

    void AddListener(TSDataListener* /*listener*/) override; // FirewireDevice
    void RemoveListener(TSDataListener* /*listener*/) override; // FirewireDevice

    // Gets
    bool IsPortOpen(void) const override; // FirewireDevice

    // Statics
    static std::vector<AVCInfo> GetSTBList(void);

  private:
    DarwinFirewireDevice(const DarwinFirewireDevice &) = delete;            // not copyable
    DarwinFirewireDevice &operator=(const DarwinFirewireDevice &) = delete; // not copyable

    void StartController(void);
    void StopController(void);

    bool OpenAVStream(void);
    bool CloseAVStream(void);
    bool IsAVStreamOpen(void) const;

    bool StartStreaming(void);
    bool StopStreaming(void);

    bool SendAVCCommand(
        const std::vector<uint8_t> &cmd,
        std::vector<uint8_t>       &result,
        int                   /*retry_cnt*/) override; // FirewireDevice

    void HandleBusReset(void);
    bool UpdatePlugRegisterPrivate(
        uint plug_number, int fw_chan, int new_speed,
        bool add_plug, bool remove_plug);
    bool UpdatePlugRegister(
        uint plug_number, int fw_chan, int speed,
        bool add_plug, bool remove_plug, uint retry_cnt = 4);

    void RunController(void);
    void BroadcastToListeners(const unsigned char *data, uint dataSize) override; // FirewireDevice
    void UpdateDeviceListItem(uint64_t guid, void *item);
    void ProcessNoDataMessage(void);
    void ProcessStreamingMessage(
        uint32_t msg, uint32_t param1, uint32_t param2);

    DarwinAVCInfo *GetInfoPtr(void);
    const DarwinAVCInfo *GetInfoPtr(void) const;

    int GetMaxSpeed(void);
    bool IsSTBStreaming(uint *fw_channel = nullptr);

    std::vector<AVCInfo> GetSTBListPrivate(void);

  private:
    int      m_local_node  {-1};
    int      m_remote_node {-1};
    DFDPriv *m_priv        {nullptr};
};

#endif // _DARWIN_FIREWIRE_DEVICE_H_
