/**
 *  DarwinFirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  SA3250HD support Copyright (c) 2005 by Matt Porter
 *  Copyright (c) 2006 by Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX headers
#include <pthread.h>

// OS X headers
#undef always_inline
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/firewire/IOFireWireLib.h>
#include <IOKit/firewire/IOFireWireLibIsoch.h>
#include <IOKit/firewire/IOFireWireFamilyCommon.h>
#include <IOKit/avc/IOFireWireAVCLib.h>
#include <CoreServices/CoreServices.h>   // for EndianU32_BtoN() etc.

// Std C++ headers
#include <algorithm>
#include <vector>

// MythTV headers
#include "libmythbase/mthread.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimer.h"
#include "darwinavcinfo.h"
#include "darwinfirewiredevice.h"

// Apple Firewire example headers
#include <AVCVideoServices/StringLogger.h>
#include <AVCVideoServices/AVSShared.h>
#include <AVCVideoServices/MPEG2Receiver.h>

#if !HAVE_IOMAINPORT
#define IOMainPort IOMasterPort
#endif

// header not used because it also requires MPEG2Transmitter.h
//#include <AVCVideoServices/FireWireMPEG.h>
namespace AVS
{
    IOReturn CreateMPEG2Receiver(
        MPEG2Receiver           **ppReceiver,
        DataPushProc              dataPushProcHandler,
        void                     *pDataPushProcRefCon = nil,
        MPEG2ReceiverMessageProc  messageProcHandler  = nil,
        void                     *pMessageProcRefCon  = nil,
        StringLogger             *stringLogger        = nil,
        IOFireWireLibNubRef       nubInterface        = nil,
        unsigned int              cyclesPerSegment    =
            kCyclesPerReceiveSegment,
        unsigned int              numSegments         =
            kNumReceiveSegments,
        bool                      doIRMAllocations    = false);
    IOReturn DestroyMPEG2Receiver(MPEG2Receiver *pReceiver);
}

#define LOC      QString("DFireDev(%1): ").arg(guid_to_string(m_guid))

#define kAnyAvailableIsochChannel 0xFFFFFFFF
static constexpr std::chrono::milliseconds kNoDataTimeout {  300ms };
static constexpr std::chrono::milliseconds kResetTimeout  { 1500ms };

static IOReturn dfd_tspacket_handler_thunk(
    UInt32 tsPacketCount, UInt32 **ppBuf, void *callback_data);
static void dfd_update_device_list(void *dfd, io_iterator_t deviter);
static void dfd_streaming_log_message(char *msg);
void *dfd_controller_thunk(void *callback_data);
void dfd_stream_msg(UInt32 msg, UInt32 param1,
                   UInt32 param2, void *callback_data);
int dfd_no_data_notification(void *callback_data);

class DFDPriv
{
private:
    DFDPriv(const DFDPriv &) = delete;            // not copyable
    DFDPriv &operator=(const DFDPriv &) = delete; // not copyable

  public:
    DFDPriv()
    {
        m_logger = new AVS::StringLogger(dfd_streaming_log_message);
    }

    ~DFDPriv()
    {
        for (auto dev : std::as_const(m_devices))
            delete dev;
        m_devices.clear();

        if (m_logger)
        {
            delete m_logger;
            m_logger = nullptr;
        }
    }

    pthread_t              m_controller_thread         {nullptr};
    CFRunLoopRef           m_controller_thread_cf_ref  {nullptr};
    bool                   m_controller_thread_running {false};

    IONotificationPortRef  m_notify_port               {nullptr};
    CFRunLoopSourceRef     m_notify_source             {nullptr};
    io_iterator_t          m_deviter                   {0};

    int                    m_actual_fwchan             {-1};
    bool                   m_is_streaming              {false};
    AVS::MPEG2Receiver    *m_avstream                  {nullptr};
    AVS::StringLogger     *m_logger                    {nullptr};
    uint                   m_no_data_cnt               {0};
    bool                   m_no_data_timer_set         {false};
    MythTimer              m_no_data_timer;

    avcinfo_list_t         m_devices;
};

DarwinFirewireDevice::DarwinFirewireDevice(
    uint64_t guid, uint subunitid, uint speed) :
    FirewireDevice(guid, subunitid, speed),
    m_priv(new DFDPriv())
{
}

DarwinFirewireDevice::~DarwinFirewireDevice()
{
    if (IsPortOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "dtor called with open port");
        while (IsPortOpen())
            ClosePort();
    }

    if (m_priv)
    {
        delete m_priv;
        m_priv = nullptr;
    }
}

void DarwinFirewireDevice::RunController(void)
{
    m_priv->m_controller_thread_cf_ref = CFRunLoopGetCurrent();

    // Set up IEEE-1394 bus change notification
    mach_port_t master_port;
    int ret = IOMainPort(bootstrap_port, &master_port);
    if (kIOReturnSuccess == ret)
    {
        m_priv->m_notify_port   = IONotificationPortCreate(master_port);
        m_priv->m_notify_source = IONotificationPortGetRunLoopSource(
            m_priv->m_notify_port);

        CFRunLoopAddSource(m_priv->m_controller_thread_cf_ref,
                           m_priv->m_notify_source,
                           kCFRunLoopDefaultMode);

        ret = IOServiceAddMatchingNotification(
            m_priv->m_notify_port, kIOMatchedNotification,
            IOServiceMatching("IOFireWireAVCUnit"),
            dfd_update_device_list, this, &m_priv->m_deviter);
    }

    if (kIOReturnSuccess == ret)
        dfd_update_device_list(this, m_priv->m_deviter);

    m_priv->m_controller_thread_running = true;

    if (kIOReturnSuccess == ret)
        CFRunLoopRun();

    QMutexLocker locker(&m_lock); // ensure that controller_thread_running seen

    m_priv->m_controller_thread_running = false;
}

void DarwinFirewireDevice::StartController(void)
{
    m_lock.unlock();

    pthread_create(&m_priv->m_controller_thread, nullptr,
                   dfd_controller_thunk, this);

    m_lock.lock();
    while (!m_priv->m_controller_thread_running)
    {
        m_lock.unlock();
        usleep(5000);
        m_lock.lock();
    }
}

void DarwinFirewireDevice::StopController(void)
{
    if (!m_priv->m_controller_thread_running)
        return;

    if (m_priv->m_deviter)
    {
        IOObjectRelease(m_priv->m_deviter);
        m_priv->m_deviter = 0;
    }

    if (m_priv->m_notify_source)
    {
        CFRunLoopSourceInvalidate(m_priv->m_notify_source);
        m_priv->m_notify_source = nullptr;
    }

    if (m_priv->m_notify_port)
    {
        IONotificationPortDestroy(m_priv->m_notify_port);
        m_priv->m_notify_port = nullptr;
    }

    CFRunLoopStop(m_priv->m_controller_thread_cf_ref);

    while (m_priv->m_controller_thread_running)
    {
        m_lock.unlock();
        usleep(100 * 1000);
        m_lock.lock();
    }
}

bool DarwinFirewireDevice::OpenPort(void)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + "OpenPort()");

    if (GetInfoPtr() && GetInfoPtr()->IsPortOpen())
    {
        m_openPortCnt++;
        return true;
    }

    StartController();

    if (!m_priv->m_controller_thread_running)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to start firewire thread.");
        return false;
    }

    if (!GetInfoPtr())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No IEEE-1394 device with our GUID");

        StopController();
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Opening AVC Device");
    LOG(VB_RECORD, LOG_INFO, LOC + GetInfoPtr()->GetSubunitInfoString());

    if (!GetInfoPtr()->IsSubunitType(kAVCSubunitTypeTuner) ||
        !GetInfoPtr()->IsSubunitType(kAVCSubunitTypePanel))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("No STB at guid: 0x%1")
                .arg(m_guid,0,16));

        StopController();
        return false;
    }

    bool ok = GetInfoPtr()->OpenPort(m_priv->m_controller_thread_cf_ref);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to get handle for port");

        return false;
    }

    // TODO FIXME -- these can change after a reset... (at least local)
    if (!GetInfoPtr()->GetDeviceNodes(m_local_node, m_remote_node))
    {
        if (m_local_node < 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to query local node");
            m_local_node = 0;
        }

        if (m_remote_node < 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to query remote node");
            m_remote_node = 0;
        }
    }

    m_openPortCnt++;

    return true;
}

bool DarwinFirewireDevice::ClosePort(void)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + "ClosePort()");

    if (m_openPortCnt < 1)
        return false;

    m_openPortCnt--;

    if (m_openPortCnt != 0)
        return true;

    if (GetInfoPtr() && GetInfoPtr()->IsPortOpen())
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Closing AVC Device");

        GetInfoPtr()->ClosePort();
    }

    StopController();
    m_local_node  = -1;
    m_remote_node = -1;

    return true;
}

bool DarwinFirewireDevice::OpenAVStream(void)
{
    if (IsAVStreamOpen())
        return true;

    int max_speed = GetMaxSpeed();
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max Speed: %1, Our speed: %2")
                          .arg(max_speed).arg(m_speed));
    m_speed = std::min((uint)max_speed, m_speed);

    uint fwchan = 0;
    bool streaming = IsSTBStreaming(&fwchan);
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("STB is %1already streaming on fwchan: %2")
            .arg(streaming?"":"not ").arg(fwchan));

    // TODO we should use the stream if it already exists,
    //      this is especially true if it is a broadcast stream...

    int ret = AVS::CreateMPEG2Receiver(
        &m_priv->m_avstream,
        dfd_tspacket_handler_thunk, this,
        dfd_stream_msg, this,
        m_priv->m_logger /* StringLogger */,
        GetInfoPtr()->fw_handle,
        AVS::kCyclesPerReceiveSegment,
        AVS::kNumReceiveSegments,
        true /* p2p */);

    if (kIOReturnSuccess != ret)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't create A/V stream object");
        return false;
    }

    m_priv->m_avstream->registerNoDataNotificationCallback(
        dfd_no_data_notification, this, kNoDataTimeout.count());

    return true;
}

int DarwinFirewireDevice::GetMaxSpeed(void)
{
    IOFireWireLibDeviceRef fw_handle = GetInfoPtr()->fw_handle;

    if ((*fw_handle)->version < 4)
    {
        // Just get the STB's info & assume we can handle it
        io_object_t dev = (*fw_handle)->GetDevice(fw_handle);

        FWAddress addr(0xffff, 0xf0000900, m_remote_node);
        uint32_t val;
        int ret = (*fw_handle)->ReadQuadlet(
            fw_handle, dev, &addr, (UInt32*) &val, false, 0);
        val = EndianU32_BtoN(val);

        return (ret == kIOReturnSuccess) ? (int)((val>>30) & 0x3) : -1;
    }

    uint32_t generation = 0;
    IOFWSpeed speed;
    int ret = (*fw_handle)->GetBusGeneration(fw_handle, (UInt32*)&generation);
    if (kIOReturnSuccess == ret)
    {
        ret = (*fw_handle)->GetSpeedBetweenNodes(
            fw_handle, generation, m_remote_node, m_local_node, &speed) ;
    }

    return (ret == kIOReturnSuccess) ? (int)speed : -1;
}

bool DarwinFirewireDevice::IsSTBStreaming(uint *fw_channel)
{
    IOFireWireLibDeviceRef fw_handle = GetInfoPtr()->fw_handle;
    io_object_t dev = (*fw_handle)->GetDevice(fw_handle);

    FWAddress addr(0xffff, 0xf0000904, m_remote_node);
    uint32_t val;
    int ret = (*fw_handle)->ReadQuadlet(
        fw_handle, dev, &addr, (UInt32*) &val, false, 0);
    val = EndianU32_BtoN(val);

    if (ret != kIOReturnSuccess)
        return false;

    if (val & (kIOFWPCRBroadcast | kIOFWPCRP2PCount))
    {
        if (fw_channel)
            *fw_channel = (val & kIOFWPCRChannel) >> kIOFWPCRChannelPhase;

        return true;
    }

    return false;
}

bool DarwinFirewireDevice::CloseAVStream(void)
{
    if (!m_priv->m_avstream)
        return true;

    StopStreaming();

    LOG(VB_RECORD, LOG_INFO, LOC + "Destroying A/V stream object");
    AVS::DestroyMPEG2Receiver(m_priv->m_avstream);
    m_priv->m_avstream = nullptr;

    return true;
}

bool DarwinFirewireDevice::IsAVStreamOpen(void) const
{
    return m_priv->m_avstream;
}

bool DarwinFirewireDevice::ResetBus(void)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "ResetBus() -- begin");

    if (!GetInfoPtr() || !GetInfoPtr()->fw_handle)
        return false;

    IOFireWireLibDeviceRef fw_handle = GetInfoPtr()->fw_handle;
    bool ok = (*fw_handle)->BusReset(fw_handle) == kIOReturnSuccess;

    if (!ok)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Bus Reset failed" + ENO);

    LOG(VB_GENERAL, LOG_DEBUG, LOC + "ResetBus() -- end");

    return ok;
}

bool DarwinFirewireDevice::StartStreaming(void)
{
    if (m_priv->m_is_streaming)
        return m_priv->m_is_streaming;

    LOG(VB_RECORD, LOG_INFO, LOC + "Starting A/V streaming");

    if (!IsAVStreamOpen() && !OpenAVStream())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Starting A/V streaming: FAILED");
        return false;
    }

    m_priv->m_avstream->setReceiveIsochChannel(kAnyAvailableIsochChannel);
    m_priv->m_avstream->setReceiveIsochSpeed((IOFWSpeed) m_speed);
    int ret = m_priv->m_avstream->startReceive();

    m_priv->m_is_streaming = (kIOReturnSuccess == ret);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Starting A/V streaming: %1")
                          .arg((m_priv->m_is_streaming)?"success":"failure"));

    return m_priv->m_is_streaming;
}

bool DarwinFirewireDevice::StopStreaming(void)
{
    if (!m_priv->m_is_streaming)
        return true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Stopping A/V streaming");

    bool ok = (kIOReturnSuccess == m_priv->m_avstream->stopReceive());
    m_priv->m_is_streaming = !ok;

    if (!ok)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to stop A/V streaming");
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Stopped A/V streaming");
    return true;
}

bool DarwinFirewireDevice::SendAVCCommand(const std::vector<uint8_t> &cmd,
                                          std::vector<uint8_t>       &result,
                                          int                   retry_cnt)
{
    return GetInfoPtr()->SendAVCCommand(cmd, result, retry_cnt);
}

bool DarwinFirewireDevice::IsPortOpen(void) const
{
    QMutexLocker locker(&m_lock);

    if (!GetInfoPtr())
        return false;

    return GetInfoPtr()->IsPortOpen();
}

void DarwinFirewireDevice::AddListener(TSDataListener *listener)
{
    QMutexLocker locker(&m_lock);

    FirewireDevice::AddListener(listener);

    if (!m_listeners.empty())
        StartStreaming();
}

void DarwinFirewireDevice::RemoveListener(TSDataListener *listener)
{
    QMutexLocker locker(&m_lock);

    FirewireDevice::RemoveListener(listener);

    if (m_priv->m_is_streaming && m_listeners.empty())
    {
        StopStreaming();
        CloseAVStream();
    }
}

void DarwinFirewireDevice::BroadcastToListeners(
    const unsigned char *data, uint dataSize)
{
    QMutexLocker locker(&m_lock);
    FirewireDevice::BroadcastToListeners(data, dataSize);
}

void DarwinFirewireDevice::ProcessNoDataMessage(void)
{
    if (m_priv->m_no_data_timer_set)
    {
        std::chrono::milliseconds short_interval = kNoDataTimeout + (kNoDataTimeout/2);
        bool recent = m_priv->m_no_data_timer.elapsed() <= short_interval;
        m_priv->m_no_data_cnt = (recent) ? m_priv->m_no_data_cnt + 1 : 1;
    }
    m_priv->m_no_data_timer_set = true;
    m_priv->m_no_data_timer.start();

    LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No Input in %1 msecs")
            .arg(m_priv->m_no_data_cnt * kNoDataTimeout.count()));

    if (m_priv->m_no_data_cnt > (kResetTimeout / kNoDataTimeout))
    {
        m_priv->m_no_data_timer_set = false;
        m_priv->m_no_data_cnt = 0;
        ResetBus();
    }
}

void DarwinFirewireDevice::ProcessStreamingMessage(
    uint32_t msg, uint32_t param1, uint32_t param2)
{
    int plug_number = 0;

    if (AVS::kMpeg2ReceiverAllocateIsochPort == msg)
    {
        int speed = param1;
        int fw_channel = param2;

        bool ok = UpdatePlugRegister(
            plug_number, fw_channel, speed, true, false);

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("AllocateIsochPort(%1,%2) %3")
                .arg(fw_channel).arg(speed).arg(((ok)?"ok":"error")));
    }
    else if (AVS::kMpeg2ReceiverReleaseIsochPort == msg)
    {
        int ret = UpdatePlugRegister(plug_number, -1, -1, false, true);

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("ReleaseIsochPort %1")
                              .arg((kIOReturnSuccess == ret)?"ok":"error"));
    }
    else if (AVS::kMpeg2ReceiverDCLOverrun == msg)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "DCL Overrun");
    }
    else if (AVS::kMpeg2ReceiverReceivedBadPacket == msg)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Received Bad Packet");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Streaming Message: %1").arg(msg));
    }
}

std::vector<AVCInfo> DarwinFirewireDevice::GetSTBList(void)
{
    std::vector<AVCInfo> list;

    {
        DarwinFirewireDevice dev(0,0,0);

        dev.m_lock.lock();
        dev.StartController();
        dev.m_lock.unlock();

        list = dev.GetSTBListPrivate();

        dev.m_lock.lock();
        dev.StopController();
        dev.m_lock.unlock();
    }

    return list;
}

std::vector<AVCInfo> DarwinFirewireDevice::GetSTBListPrivate(void)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "GetSTBListPrivate -- begin");
#endif
    QMutexLocker locker(&m_lock);
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "GetSTBListPrivate -- got lock");
#endif

    std::vector<AVCInfo> list;

    for (auto dev : std::as_const(m_priv->m_devices))
    {
        if (dev->IsSubunitType(kAVCSubunitTypeTuner) &&
            dev->IsSubunitType(kAVCSubunitTypePanel))
        {
            list.push_back(*dev);
        }
    }

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "GetSTBListPrivate -- end");
#endif
    return list;
}

void DarwinFirewireDevice::UpdateDeviceListItem(uint64_t guid, void *pitem)
{
    QMutexLocker locker(&m_lock);

    avcinfo_list_t::iterator it = m_priv->m_devices.find(guid);

    if (it == m_priv->m_devices.end())
    {
        auto *ptr = new DarwinAVCInfo();

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Adding   0x%1").arg(guid, 0, 16));

        m_priv->m_devices[guid] = ptr;
        it = m_priv->m_devices.find(guid);
    }

    if (it != m_priv->m_devices.end())
    {
        io_object_t &item = *((io_object_t*) pitem);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Updating 0x%1").arg(guid, 0, 16));
        (*it)->Update(guid, this, m_priv->m_notify_port,
                      m_priv->m_controller_thread_cf_ref, item);
    }
}

DarwinAVCInfo *DarwinFirewireDevice::GetInfoPtr(void)
{
    avcinfo_list_t::iterator it = m_priv->m_devices.find(m_guid);
    return (it == m_priv->m_devices.end()) ? nullptr : *it;
}

const DarwinAVCInfo *DarwinFirewireDevice::GetInfoPtr(void) const
{
    avcinfo_list_t::iterator it = m_priv->m_devices.find(m_guid);
    return (it == m_priv->m_devices.end()) ? nullptr : *it;
}


bool DarwinFirewireDevice::UpdatePlugRegisterPrivate(
    uint plug_number, int new_fw_chan, int new_speed,
    bool add_plug, bool remove_plug)
{
    if (!GetInfoPtr())
        return false;

    IOFireWireLibDeviceRef fw_handle = GetInfoPtr()->fw_handle;
    if (!fw_handle)
        return false;

    io_object_t dev = (*fw_handle)->GetDevice(fw_handle);

    // Read the register
    uint      low_addr = kPCRBaseAddress + 4 + (plug_number << 2);
    FWAddress addr(0xffff, low_addr, m_remote_node);
    uint32_t  old_plug_val;
    if (kIOReturnSuccess != (*fw_handle)->ReadQuadlet(
            fw_handle, dev, &addr, (UInt32*) &old_plug_val, false, 0))
    {
        return false;
    }
    old_plug_val = EndianU32_BtoN(old_plug_val);

    int old_plug_cnt = (old_plug_val >> 24) & 0x3f;
    int old_fw_chan  = (old_plug_val >> 16) & 0x3f;
    int old_speed    = (old_plug_val >> 14) & 0x03;

    int new_plug_cnt = (int) old_plug_cnt;
    new_plug_cnt += ((add_plug) ? 1 : 0) - ((remove_plug) ? 1 : 0);
    if ((new_plug_cnt > 0x3f) || (new_plug_cnt < 0))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid Plug Count %1")
                              .arg(new_plug_cnt));
        return false;
    }

    new_fw_chan = (new_fw_chan >= 0) ? new_fw_chan : old_fw_chan;
    if (old_plug_cnt && (new_fw_chan != old_fw_chan))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Ignoring FWChan change request, plug already open");

        new_fw_chan = old_fw_chan;
    }

    new_speed = (new_speed >= 0) ? new_speed : old_speed;
    if (old_plug_cnt && (new_speed != old_speed))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Ignoring speed change request, plug already open");

        new_speed = old_speed;
    }

    uint32_t new_plug_val = old_plug_val;

    new_plug_val &= ~(0x3f<<24);
    new_plug_val &= (remove_plug) ? ~kIOFWPCRBroadcast : ~0x0;
    new_plug_val |= (new_plug_cnt & 0x3f) << 24;

    new_plug_val &= ~(0x3f<<16);
    new_plug_val |= (new_fw_chan & 0x3F) << 16;

    new_plug_val &= ~(0x03<<14);
    new_plug_val |= (new_speed & 0x03) << 14;

    old_plug_val = EndianU32_NtoB(old_plug_val);
    new_plug_val = EndianU32_NtoB(new_plug_val);

    return (kIOReturnSuccess == (*fw_handle)->CompareSwap(
                fw_handle, dev, &addr, old_plug_val, new_plug_val, false, 0));
}

void DarwinFirewireDevice::HandleBusReset(void)
{
    int plug_number = 0;
    if (!GetInfoPtr())
        return;

    int fw_channel = m_priv->m_actual_fwchan;
    bool ok = UpdatePlugRegister(plug_number, fw_channel,
                                 m_speed, true, false);
    if (!ok)
    {
        ok = UpdatePlugRegister(plug_number, kAnyAvailableIsochChannel,
                                m_speed, true, false);
    }

    if (!ok)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Reset: Failed to reconnect");
    else
        LOG(VB_RECORD, LOG_INFO, LOC + "Reset: Reconnected succesfully");
}

bool DarwinFirewireDevice::UpdatePlugRegister(
    uint plug_number, int fw_chan, int speed,
    bool add_plug, bool remove_plug, uint retry_cnt)
{
    if (!GetInfoPtr() || !GetInfoPtr()->fw_handle)
        return false;

    bool ok = false;

    for (uint i = 0; (i < retry_cnt) && !ok; i++)
    {
        ok = UpdatePlugRegisterPrivate(
            plug_number, fw_chan, speed, add_plug, remove_plug);
    }

    m_priv->m_actual_fwchan = (ok) ? fw_chan : kAnyAvailableIsochChannel;

    return ok;
}

void DarwinFirewireDevice::HandleDeviceChange(uint messageType)
{
    QString loc = LOC + "HandleDeviceChange: ";

    if (kIOMessageServiceIsTerminated == messageType)
    {
        LOG(VB_RECORD, LOG_INFO, loc + "Disconnect");
        // stop printing no data messages.. don't try to open
        return;
    }

    if (kIOMessageServiceIsAttemptingOpen == messageType)
    {
        LOG(VB_RECORD, LOG_INFO, loc + "Attempting open");
        return;
    }

    if (kIOMessageServiceWasClosed == messageType)
    {
        LOG(VB_RECORD, LOG_INFO, loc + "Device Closed");
        // fill unit_table
        return;
    }

    if (kIOMessageServiceIsSuspended == messageType)
    {
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageServiceIsSuspended");
        // start of reset
        return;
    }

    if (kIOMessageServiceIsResumed == messageType)
    {
        // end of reset
        HandleBusReset();
    }

    if (kIOMessageServiceIsTerminated == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageServiceIsTerminated");
    else if (kIOMessageServiceIsRequestingClose == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageServiceIsRequestingClose");
    else if (kIOMessageServiceIsAttemptingOpen == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageServiceIsAttemptingOpen");
    else if (kIOMessageServiceWasClosed == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageServiceWasClosed");
    else if (kIOMessageServiceBusyStateChange == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageServiceBusyStateChange");
    else if (kIOMessageCanDevicePowerOff == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageCanDevicePowerOff");
    else if (kIOMessageDeviceWillPowerOff == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageDeviceWillPowerOff");
    else if (kIOMessageDeviceWillNotPowerOff == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageDeviceWillNotPowerOff");
    else if (kIOMessageDeviceHasPoweredOn == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageDeviceHasPoweredOn");
    else if (kIOMessageCanSystemPowerOff == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageCanSystemPowerOff");
    else if (kIOMessageSystemWillPowerOff == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageSystemWillPowerOff");
    else if (kIOMessageSystemWillNotPowerOff == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageSystemWillNotPowerOff");
    else if (kIOMessageCanSystemSleep == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageCanSystemSleep");
    else if (kIOMessageSystemWillSleep == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageSystemWillSleep");
    else if (kIOMessageSystemWillNotSleep == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageSystemWillNotSleep");
    else if (kIOMessageSystemHasPoweredOn == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageSystemHasPoweredOn");
    else if (kIOMessageSystemWillRestart == messageType)
        LOG(VB_RECORD, LOG_INFO, loc + "kIOMessageSystemWillRestart");
    else
    {
        LOG(VB_RECORD, LOG_ERR, loc + QString("unknown message 0x%1")
                           .arg(messageType, 0, 16));
    }
}

// Various message callbacks.

void *dfd_controller_thunk(void *callback_data)
{
    MThread::ThreadSetup("DarwinController");
    reinterpret_cast<DarwinFirewireDevice*>(callback_data)->RunController();
    MThread::ThreadCleanup();
    return nullptr;
}

void dfd_update_device_list_item(
    DarwinFirewireDevice *dev, uint64_t guid, void *item)
{
    dev->UpdateDeviceListItem(guid, item);
}

int dfd_no_data_notification(void *callback_data)
{
    reinterpret_cast<DarwinFirewireDevice*>(callback_data)->
        ProcessNoDataMessage();

    return kIOReturnSuccess;
}

void dfd_stream_msg(UInt32 msg, UInt32 param1,
                    UInt32 param2, void *callback_data)
{
    reinterpret_cast<DarwinFirewireDevice*>(callback_data)->
        ProcessStreamingMessage(msg, param1, param2);
}

int dfd_tspacket_handler(uint tsPacketCount, uint32_t **ppBuf,
                         void *callback_data)
{
    auto *fw = reinterpret_cast<DarwinFirewireDevice*>(callback_data);
    if (!fw)
        return kIOReturnBadArgument;

    for (uint32_t i = 0; i < tsPacketCount; ++i)
        fw->BroadcastToListeners((const unsigned char*) ppBuf[i], 188);

    return kIOReturnSuccess;
}

static IOReturn dfd_tspacket_handler_thunk(
    UInt32 tsPacketCount, UInt32 **ppBuf, void *callback_data)
{
    return dfd_tspacket_handler(
        tsPacketCount, (uint32_t**)ppBuf, callback_data);
}

static void dfd_update_device_list(void *dfd, io_iterator_t deviter)
{
    auto *dev = reinterpret_cast<DarwinFirewireDevice*>(dfd);

    io_object_t it = 0;
    while ((it = IOIteratorNext(deviter)))
    {
        uint64_t guid = 0;

        CFMutableDictionaryRef props;
        int ret = IORegistryEntryCreateCFProperties(
            it, &props, kCFAllocatorDefault, kNilOptions);

        if (kIOReturnSuccess == ret)
        {
            auto GUIDDesc = (CFNumberRef)
                CFDictionaryGetValue(props, CFSTR("GUID"));
            CFNumberGetValue(GUIDDesc, kCFNumberSInt64Type, &guid);
            CFRelease(props);
            dfd_update_device_list_item(dev, guid, &it);
        }
    }
}

static void dfd_streaming_log_message(char *msg)
{
    LOG(VB_RECORD, LOG_INFO, QString("MPEG2Receiver: %1").arg(msg));
}
