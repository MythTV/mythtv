#ifndef _DARWIN_FIREWIRE_DEVICE_H_
#define _DARWIN_FIREWIRE_DEVICE_H_

#include "firewiredevice.h"
#include <IOKit/IOKitLib.h>

class DFDPriv;
class DarwinAVCInfo;

class DarwinFirewireDevice : public FirewireDevice
{
    friend void *dfd_controller_thunk(void *param);
    friend void dfd_update_device_list_item(DarwinFirewireDevice *dev,
                                       uint64_t guid, void *item);
    friend int dfd_no_data_notification(void *cb_data);
    friend void dfd_stream_msg(
        UInt32 msg, UInt32 param1,
        UInt32 param2, void *callback_data);
    friend int dfd_tspacket_handler(
        uint tsPacketCount, uint32_t **ppBuf, void *callback_data);


  public:
    DarwinFirewireDevice(uint64_t guid, uint subunitid, uint speed);
    ~DarwinFirewireDevice();

    virtual bool OpenPort(void);
    virtual bool ClosePort(void);
    virtual bool ResetBus(void);

    void HandleDeviceChange(uint messageType);

    virtual void AddListener(TSDataListener*);
    virtual void RemoveListener(TSDataListener*);

    // Gets
    virtual bool IsPortOpen(void) const;

    // Statics
    static vector<AVCInfo> GetSTBList(void);

  private:
    void StartController(void);
    void StopController(void);

    bool OpenAVStream(void);
    bool CloseAVStream(void);
    bool IsAVStreamOpen(void) const;

    bool StartStreaming(void);
    bool StopStreaming(void);

    virtual bool SendAVCCommand(
        const vector<uint8_t> &cmd,
        vector<uint8_t>       &result,
        int                   /*retry_cnt*/);

    void HandleBusReset(void);
    bool UpdatePlugRegisterPrivate(
        uint plug_number, int fw_chan, int new_speed,
        bool add_plug, bool remove_plug);
    bool UpdatePlugRegister(
        uint plug_number, int fw_chan, int speed,
        bool add_plug, bool remove_plug, uint retry_cnt = 4);

    void RunController(void);
    void BroadcastToListeners(const unsigned char *data, uint dataSize);
    void UpdateDeviceListItem(uint64_t guid, void *item);
    void ProcessNoDataMessage(void);
    void ProcessStreamingMessage(
        uint32_t msg, uint32_t param1, uint32_t param2);

    DarwinAVCInfo *GetInfoPtr(void);
    const DarwinAVCInfo *GetInfoPtr(void) const;

    int GetMaxSpeed(void);
    bool IsSTBStreaming(uint *fw_channel = NULL);

    vector<AVCInfo> GetSTBListPrivate(void);

  private:
    int      m_local_node;
    int      m_remote_node;
    DFDPriv *m_priv;
};

#endif // _DARWIN_FIREWIRE_DEVICE_H_
