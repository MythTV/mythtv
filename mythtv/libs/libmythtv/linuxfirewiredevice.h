/**
 *  LinuxFirewireDevice
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _LINUX_FIREWIRE_DEVICE_H_
#define _LINUX_FIREWIRE_DEVICE_H_

#include <QRunnable>

#include "firewiredevice.h"
#include "mthread.h"

class LFDPriv;
class LinuxAVCInfo;
class LinuxFirewireDevice;

class LinuxFirewireDevice : public FirewireDevice, public QRunnable
{
    friend int linux_firewire_device_tspacket_handler(
        unsigned char *tspacket, int len, uint dropped, void *callback_data);

  public:
    LinuxFirewireDevice(uint64_t guid, uint subunitid,
                        uint speed, bool use_p2p,
                        uint av_buffer_size_in_bytes = 0);
    ~LinuxFirewireDevice();

    // Commands
    virtual bool OpenPort(void);
    virtual bool ClosePort(void);
    virtual bool ResetBus(void);

    virtual void AddListener(TSDataListener*);
    virtual void RemoveListener(TSDataListener*);

    // Gets
    virtual bool IsPortOpen(void) const;

    // Signal from driver
    void SignalReset(uint generation);

    // Statics
    static vector<AVCInfo> GetSTBList(void);

    // Constants
    static const uint kBroadcastChannel;
    static const uint kConnectionP2P;
    static const uint kConnectionBroadcast;
    static const uint kMaxBufferedPackets;

  private:
    bool OpenNode(void);
    bool CloseNode(void);

    bool OpenAVStream(void);
    bool CloseAVStream(void);

    bool OpenP2PNode(void);
    bool CloseP2PNode(void);

    bool OpenBroadcastNode(void);
    bool CloseBroadcastNode(void);

    bool StartStreaming(void);
    bool StopStreaming(void);

    void run(void); // QRunnable
    void PrintDropped(uint dropped_packets);

    bool SetAVStreamBufferSize(uint size_in_bytes);
    bool SetAVStreamSpeed(uint speed);

    bool IsNodeOpen(void) const;
    bool IsAVStreamOpen(void) const;

    bool UpdateDeviceList(void);
    void UpdateDeviceListItem(uint64_t guid, void *pitem);
    vector<AVCInfo> GetSTBListPrivate(void);

    virtual bool SendAVCCommand(const vector<uint8_t> &cmd,
                                vector<uint8_t>       &result,
                                int                    retry_cnt);

    LinuxAVCInfo *GetInfoPtr(void);
    const LinuxAVCInfo *GetInfoPtr(void) const;

    void HandleBusReset(void);

  private:
    uint                     m_bufsz;
    bool                     m_db_reset_disabled;
    bool                     m_use_p2p;
    LFDPriv                 *m_priv;
};

#endif // _LINUX_FIREWIRE_DEVICE_H_
