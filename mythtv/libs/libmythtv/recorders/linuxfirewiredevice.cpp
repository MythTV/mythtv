/**
 *  LinuxFirewireDevice
 *  Copyright (c) 2005 by Jim Westfall
 *  Copyright (c) 2006 by Daniel Kristjansson
 *  SA3250HD support Copyright (c) 2005 by Matt Porter
 *  SA4200HD/Alternate 3250 support Copyright (c) 2006 by Chris Ingrassia
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX headers
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>

// Linux headers
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <libiec61883/iec61883.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/rom1394.h>

#include <netinet/in.h>

// C++ headers
#include <algorithm>
#include <chrono> // for milliseconds
#include <map>
#include <thread> // for sleep_for

// Qt headers
#include <QDateTime>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "firewirerecorder.h"
#include "linuxavcinfo.h"
#include "linuxfirewiredevice.h"

#define LOC      QString("LFireDev(%1): ").arg(guid_to_string(m_guid))

static constexpr std::chrono::milliseconds kNoDataTimeout { 50ms };
static constexpr std::chrono::milliseconds kResetTimeout  {  1s };

using handle_to_lfd_t = QHash<raw1394handle_t,LinuxFirewireDevice*>;

class LFDPriv
{
  public:
    LFDPriv() = default;

    ~LFDPriv()
    {
        for (const auto & device : std::as_const(m_devices))
            delete device;
        m_devices.clear();

        if (m_portHandlerThread)
        {
            m_portHandlerThread->wait();
            delete m_portHandlerThread;
        }
    }

    uint             m_generation              {0};
    bool             m_resetTimerOn            {false};
    MythTimer        m_resetTimer;

    bool             m_runPortHandler          {false};
    bool             m_isPortHandlerRunning    {false};
    QWaitCondition   m_portHandlerWait;
    QMutex           m_startStopPortHandlerLock;

    iec61883_mpeg2_t m_avstream                {nullptr};
    int              m_channel                 {-1};
    int              m_outputPlug              {-1};
    int              m_inputPlug               {-1};
    int              m_bandwidth               {0};
    uint             m_noDataCnt               {0};

    bool             m_isP2pNodeOpen           {false};
    bool             m_isBcastNodeOpen         {false};
    bool             m_isStreaming             {false};

    MThread         *m_portHandlerThread       {nullptr};

    avcinfo_list_t   m_devices;

    static QMutex          s_lock;
    static handle_to_lfd_t s_handle_info;
};
QMutex          LFDPriv::s_lock;
handle_to_lfd_t LFDPriv::s_handle_info;

static void add_handle(raw1394handle_t handle, LinuxFirewireDevice *dev)
{
    QMutexLocker slocker(&LFDPriv::s_lock);
    LFDPriv::s_handle_info[handle] = dev;
}

static void remove_handle(raw1394handle_t handle)
{
    QMutexLocker slocker(&LFDPriv::s_lock);
    LFDPriv::s_handle_info.remove(handle);
}

const uint LinuxFirewireDevice::kBroadcastChannel    = 63;
const uint LinuxFirewireDevice::kConnectionP2P       = 0;
const uint LinuxFirewireDevice::kConnectionBroadcast = 1;
const uint LinuxFirewireDevice::kMaxBufferedPackets  = 4 * 1024 * 1024 / 188;

// callback function for libiec61883
int linux_firewire_device_tspacket_handler(
    unsigned char *tspacket, int len, uint dropped, void *callback_data);
void *linux_firewire_device_port_handler_thunk(void *param);
static bool has_data(int fd, std::chrono::milliseconds msec);
static QString speed_to_string(uint speed);
static int linux_firewire_device_bus_reset_handler(
    raw1394handle_t handle, uint generation);

LinuxFirewireDevice::LinuxFirewireDevice(
    uint64_t guid, uint subunitid,
    uint speed, bool use_p2p, uint av_buffer_size_in_bytes) :
    FirewireDevice(guid, subunitid, speed),
    m_bufsz(av_buffer_size_in_bytes),
    m_useP2P(use_p2p), m_priv(new LFDPriv())
{
    if (!m_bufsz)
        m_bufsz = gCoreContext->GetNumSetting("HDRingbufferSize");

    m_dbResetDisabled = gCoreContext->GetBoolSetting("DisableFirewireReset", false);

    UpdateDeviceList();
}

LinuxFirewireDevice::~LinuxFirewireDevice()
{
    if (LinuxFirewireDevice::IsPortOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ctor called with open port");
        while (LinuxFirewireDevice::IsPortOpen())
            LinuxFirewireDevice::ClosePort();
    }

    if (m_priv)
    {
        delete m_priv;
        m_priv = nullptr;
    }
}

void LinuxFirewireDevice::SignalReset(uint generation)
{
    const QString loc = LOC + QString("SignalReset(%1->%2)")
        .arg(m_priv->m_generation).arg(generation);

    LOG(VB_GENERAL, LOG_INFO, loc);

    if (GetInfoPtr())
        raw1394_update_generation(GetInfoPtr()->m_fwHandle, generation);

    m_priv->m_generation = generation;

    LOG(VB_GENERAL, LOG_INFO, loc + ": Updating device list -- begin");
    UpdateDeviceList();
    LOG(VB_GENERAL, LOG_INFO, loc + ": Updating device list -- end");

    m_priv->m_resetTimerOn = true;
    m_priv->m_resetTimer.start();
}

void LinuxFirewireDevice::HandleBusReset(void)
{
    const QString loc = LOC + "HandleBusReset";

    if (!GetInfoPtr() || !GetInfoPtr()->m_fwHandle)
        return;

    if (m_priv->m_isP2pNodeOpen)
    {
        LOG(VB_GENERAL, LOG_INFO, loc + ": Reconnecting P2P connection");
        nodeid_t output = GetInfoPtr()->GetNode() | 0xffc0;
        nodeid_t input  = raw1394_get_local_id(GetInfoPtr()->m_fwHandle);

        int fwchan = iec61883_cmp_reconnect(
            GetInfoPtr()->m_fwHandle,
            output, &m_priv->m_outputPlug,
            input,  &m_priv->m_inputPlug,
            &m_priv->m_bandwidth, m_priv->m_channel);

        if (fwchan < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bus Reset: Failed to reconnect");
        }
        else if (fwchan != m_priv->m_channel)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("FWChan changed %1->%2")
                    .arg(m_priv->m_channel).arg(fwchan));
        }
        m_priv->m_channel = fwchan;

        LOG(VB_GENERAL, LOG_INFO,
            loc + QString(": Reconnected fwchan: %1\n\t\t\toutput: 0x%2 "
                          "input: 0x%3")
                .arg(fwchan).arg(output,0,16).arg(input,0,16));
    }

    if (m_priv->m_isBcastNodeOpen)
    {
        nodeid_t output = GetInfoPtr()->GetNode() | 0xffc0;

        LOG(VB_RECORD, LOG_INFO, loc + ": Restarting broadcast connection on " +
            QString("node %1, channel %2")
                .arg(GetInfoPtr()->GetNode()).arg(m_priv->m_channel));

        int err = iec61883_cmp_create_bcast_output(
            GetInfoPtr()->m_fwHandle,
            output, m_priv->m_outputPlug,
            m_priv->m_channel, m_speed);

        if (err < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bus Reset : Failed to reconnect");
        }
    }
}

bool LinuxFirewireDevice::OpenPort(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Starting Port Handler Thread");
    QMutexLocker locker(&m_priv->m_startStopPortHandlerLock);
    LOG(VB_RECORD, LOG_INFO, LOC + "Starting Port Handler Thread -- locked");

    LOG(VB_RECORD, LOG_INFO, LOC + "OpenPort()");

    QMutexLocker mlocker(&m_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + "OpenPort() -- got lock");

    if (!GetInfoPtr())
        return false;

    if (GetInfoPtr()->IsPortOpen())
    {
        m_openPortCnt++;
        return true;
    }

    if (!GetInfoPtr()->OpenPort())
        return false;

    add_handle(GetInfoPtr()->m_fwHandle, this);

    m_priv->m_generation = raw1394_get_generation(GetInfoPtr()->m_fwHandle);
    raw1394_set_bus_reset_handler(
        GetInfoPtr()->m_fwHandle, linux_firewire_device_bus_reset_handler);

    GetInfoPtr()->GetSubunitInfo();
    LOG(VB_RECORD, LOG_INFO, LOC + GetInfoPtr()->GetSubunitInfoString());

    if (!GetInfoPtr()->IsSubunitType(kAVCSubunitTypeTuner) ||
        !GetInfoPtr()->IsSubunitType(kAVCSubunitTypePanel))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Not an STB"));

        mlocker.unlock();
        ClosePort();

        return false;
    }

    m_priv->m_runPortHandler = true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Starting port handler thread");
    m_priv->m_portHandlerThread = new MThread("LinuxController", this);
    m_priv->m_portHandlerThread->start();

    while (!m_priv->m_isPortHandlerRunning)
        m_priv->m_portHandlerWait.wait(mlocker.mutex(), 100);

    LOG(VB_RECORD, LOG_INFO, LOC + "Port handler thread started");

    m_openPortCnt++;

    return true;
}

bool LinuxFirewireDevice::ClosePort(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Stopping Port Handler Thread");
    QMutexLocker locker(&m_priv->m_startStopPortHandlerLock);
    LOG(VB_RECORD, LOG_INFO, LOC + "Stopping Port Handler Thread -- locked");

    QMutexLocker mlocker(&m_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + "ClosePort()");

    if (m_openPortCnt < 1)
        return false;

    m_openPortCnt--;

    if (m_openPortCnt != 0)
        return true;

    if (!GetInfoPtr())
        return false;

    if (GetInfoPtr()->IsPortOpen())
    {
        if (IsNodeOpen())
            CloseNode();

        LOG(VB_RECORD, LOG_INFO,
            LOC + "Waiting for port handler thread to stop");
        m_priv->m_runPortHandler = false;
        m_priv->m_portHandlerWait.wakeAll();

        mlocker.unlock();
        m_priv->m_portHandlerThread->wait();
        mlocker.relock();

        delete m_priv->m_portHandlerThread;
        m_priv->m_portHandlerThread = nullptr;

        LOG(VB_RECORD, LOG_INFO, LOC + "Joined port handler thread");

        remove_handle(GetInfoPtr()->m_fwHandle);

        if (!GetInfoPtr()->ClosePort())
            return false;
    }

    return true;
}

void LinuxFirewireDevice::AddListener(TSDataListener *listener)
{
    QMutexLocker locker(&m_lock);

    FirewireDevice::AddListener(listener);

    if (!m_listeners.empty())
    {
        OpenNode();
        OpenAVStream();
        StartStreaming();
    }
}

void LinuxFirewireDevice::RemoveListener(TSDataListener *listener)
{
    QMutexLocker locker(&m_lock);

    FirewireDevice::RemoveListener(listener);

    if (m_listeners.empty())
    {
        StopStreaming();
        CloseAVStream();
        CloseNode();
    }
}

bool LinuxFirewireDevice::SendAVCCommand(
    const std::vector<uint8_t>  &cmd,
    std::vector<uint8_t>        &result,
    int                     retry_cnt)
{
    return GetInfoPtr()->SendAVCCommand(cmd, result, retry_cnt);
}

bool LinuxFirewireDevice::IsPortOpen(void) const
{
    QMutexLocker locker(&m_lock);

    if (!GetInfoPtr())
        return false;

    return GetInfoPtr()->IsPortOpen();
}

///////////////////////////////////////////////////////////////////////////////
// Private methods

bool LinuxFirewireDevice::OpenNode(void)
{
    if (m_useP2P)
        return OpenP2PNode();
    return OpenBroadcastNode();
}

bool LinuxFirewireDevice::CloseNode(void)
{
    if (m_priv->m_isP2pNodeOpen)
        return CloseP2PNode();

    if (m_priv->m_isBcastNodeOpen)
        return CloseBroadcastNode();

    return true;
}

// This may in fact open a broadcast connection, but it tries to open
// a P2P connection first.
bool LinuxFirewireDevice::OpenP2PNode(void)
{
    if (m_priv->m_isBcastNodeOpen)
        return false;

    if (m_priv->m_isP2pNodeOpen)
        return true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Opening P2P connection");

    m_priv->m_bandwidth   = +1; // +1 == allocate bandwidth
    m_priv->m_outputPlug  = -1; // -1 == find first online plug
    m_priv->m_inputPlug   = -1; // -1 == find first online plug
    nodeid_t output     = GetInfoPtr()->GetNode() | 0xffc0;
    nodeid_t input      = raw1394_get_local_id(GetInfoPtr()->m_fwHandle);
    m_priv->m_channel     = iec61883_cmp_connect(GetInfoPtr()->m_fwHandle,
                                                 output, &m_priv->m_outputPlug,
                                                 input,  &m_priv->m_inputPlug,
                                                 &m_priv->m_bandwidth);

    if (m_priv->m_channel < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create P2P connection");

        m_priv->m_bandwidth = 0;

        return false;
    }

    m_priv->m_isP2pNodeOpen = true;

    return true;
}

bool LinuxFirewireDevice::CloseP2PNode(void)
{
    if (m_priv->m_isP2pNodeOpen && (m_priv->m_channel >= 0))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Closing P2P connection");

        if (m_priv->m_avstream)
            CloseAVStream();

        nodeid_t output     = GetInfoPtr()->GetNode() | 0xffc0;
        nodeid_t input      = raw1394_get_local_id(GetInfoPtr()->m_fwHandle);

        iec61883_cmp_disconnect(GetInfoPtr()->m_fwHandle,
                                output, m_priv->m_outputPlug,
                                input,  m_priv->m_inputPlug,
                                m_priv->m_channel, m_priv->m_bandwidth);

        m_priv->m_channel     = -1;
        m_priv->m_outputPlug  = -1;
        m_priv->m_inputPlug   = -1;
        m_priv->m_isP2pNodeOpen = false;
    }

    return true;
}

bool LinuxFirewireDevice::OpenBroadcastNode(void)
{
    if (m_priv->m_isP2pNodeOpen)
        return false;

    if (m_priv->m_isBcastNodeOpen)
        return true;

    if (m_priv->m_avstream)
        CloseAVStream();

    m_priv->m_channel     = kBroadcastChannel - GetInfoPtr()->GetNode();
    m_priv->m_outputPlug  = 0;
    m_priv->m_inputPlug   = 0;
    nodeid_t output     = GetInfoPtr()->GetNode() | 0xffc0;

    LOG(VB_RECORD, LOG_INFO, LOC + "Opening broadcast connection on " +
            QString("node %1, channel %2")
            .arg(GetInfoPtr()->GetNode()).arg(m_priv->m_channel));

    int err = iec61883_cmp_create_bcast_output(
        GetInfoPtr()->m_fwHandle,
        output, m_priv->m_outputPlug,
        m_priv->m_channel, m_speed);

    if (err != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Broadcast connection");

        m_priv->m_channel     = -1;
        m_priv->m_outputPlug  = -1;
        m_priv->m_inputPlug   = -1;

        return false;
    }

    m_priv->m_isBcastNodeOpen = true;

    return true;
}

bool LinuxFirewireDevice::CloseBroadcastNode(void)
{
    if (m_priv->m_isBcastNodeOpen)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Closing broadcast connection");

        m_priv->m_channel     = -1;
        m_priv->m_outputPlug  = -1;
        m_priv->m_inputPlug   = -1;
        m_priv->m_isBcastNodeOpen = false;
    }
    return true;
}

bool LinuxFirewireDevice::OpenAVStream(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "OpenAVStream");

    if (!GetInfoPtr() || !GetInfoPtr()->IsPortOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Cannot open AVStream without open IEEE 1394 port");

        return false;
    }

    if (!IsNodeOpen() && !OpenNode())
        return false;

    if (m_priv->m_avstream)
        return true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Opening A/V stream object");

    m_priv->m_avstream = iec61883_mpeg2_recv_init(
        GetInfoPtr()->m_fwHandle, linux_firewire_device_tspacket_handler, this);

    if (!m_priv->m_avstream)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to open AVStream" + ENO);

        return false;
    }

    iec61883_mpeg2_set_synch(m_priv->m_avstream, 1 /* sync on close */);

    if (m_bufsz)
        SetAVStreamBufferSize(m_bufsz);

    return true;
}

bool LinuxFirewireDevice::CloseAVStream(void)
{
    if (!m_priv->m_avstream)
        return true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Closing A/V stream object");

    while (!m_listeners.empty())
        FirewireDevice::RemoveListener(m_listeners[m_listeners.size() - 1]);

    if (m_priv->m_isStreaming)
        StopStreaming();

    iec61883_mpeg2_close(m_priv->m_avstream);
    m_priv->m_avstream = nullptr;

    return true;
}

void LinuxFirewireDevice::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "RunPortHandler -- start");
    m_lock.lock();
    LOG(VB_RECORD, LOG_INFO, LOC + "RunPortHandler -- got first lock");
    m_priv->m_isPortHandlerRunning = true;
    m_priv->m_portHandlerWait.wakeAll();
    // we need to unlock & sleep to allow wakeAll to wake other threads.
    m_lock.unlock();
    std::this_thread::sleep_for(2500us);
    m_lock.lock();

    m_priv->m_noDataCnt = 0;
    while (m_priv->m_runPortHandler)
    {
        LFDPriv::s_lock.lock();
        bool reset_timer_on = m_priv->m_resetTimerOn;
        bool handle_reset = reset_timer_on &&
            (m_priv->m_resetTimer.elapsed() > 100ms);
        if (handle_reset)
            m_priv->m_resetTimerOn = false;
        LFDPriv::s_lock.unlock();

        if (handle_reset)
            HandleBusReset();

        if (!reset_timer_on && m_priv->m_isStreaming &&
            (m_priv->m_noDataCnt > (kResetTimeout / kNoDataTimeout)))
        {
            m_priv->m_noDataCnt = 0;
            ResetBus();
        }

        int fwfd = raw1394_get_fd(GetInfoPtr()->m_fwHandle);
        if (fwfd < 0)
        {
            // We unlock here because this can take a long time
            // and we don't want to block other actions.
            m_priv->m_portHandlerWait.wait(&m_lock, kNoDataTimeout.count());

            m_priv->m_noDataCnt += (m_priv->m_isStreaming) ? 1 : 0;
            continue;
        }

        // We unlock here because this can take a long time and we
        // don't want to block other actions. All reads and writes
        // are done with the lock, so this is safe so long as we
        // check that we really have data once we get the lock.
        m_lock.unlock();
        bool ready = has_data(fwfd, kNoDataTimeout);
        m_lock.lock();

        if (!ready && m_priv->m_isStreaming)
        {
            m_priv->m_noDataCnt++;

            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No Input in %1 msec...")
                .arg(m_priv->m_noDataCnt * kNoDataTimeout.count()));
        }

        // Confirm that we won't block, now that we have the lock...
        if (ready && has_data(fwfd, 1ms))
        {
            // Performs blocking read of next 4 bytes off bus and handles
            // them. Most firewire commands do their own loop_iterate
            // internally to check for results, but some things like
            // streaming data and FireWire bus resets must be handled
            // as well, which we do here...
            int ret = raw1394_loop_iterate(GetInfoPtr()->m_fwHandle);
            if (-1 == ret)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "raw1394_loop_iterate" + ENO);
            }
        }
    }

    m_priv->m_isPortHandlerRunning = false;
    m_priv->m_portHandlerWait.wakeAll();
    m_lock.unlock();
    LOG(VB_RECORD, LOG_INFO, LOC + "RunPortHandler -- end");
}

bool LinuxFirewireDevice::StartStreaming(void)
{
    if (m_priv->m_isStreaming)
        return m_priv->m_isStreaming;

    if (!IsAVStreamOpen() && !OpenAVStream())
        return false;

    if (m_priv->m_channel < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Starting A/V streaming, no channel");
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Starting A/V streaming -- really");

    if (iec61883_mpeg2_recv_start(m_priv->m_avstream, m_priv->m_channel) == 0)
    {
        m_priv->m_isStreaming = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Starting A/V streaming " + ENO);
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Starting A/V streaming -- done");

    return m_priv->m_isStreaming;
}

bool LinuxFirewireDevice::StopStreaming(void)
{
    if (m_priv->m_isStreaming)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Stopping A/V streaming -- really");

        m_priv->m_isStreaming = false;

        iec61883_mpeg2_recv_stop(m_priv->m_avstream);

        raw1394_iso_recv_flush(GetInfoPtr()->m_fwHandle);
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Stopped A/V streaming");

    return true;
}

bool LinuxFirewireDevice::SetAVStreamBufferSize(uint size_in_bytes)
{
    if (!m_priv->m_avstream)
        return false;

    // Set buffered packets size
    uint   buffer_size      = std::max(size_in_bytes, 50 * TSPacket::kSize);
    size_t buffered_packets = std::min(buffer_size / 4, kMaxBufferedPackets);

    iec61883_mpeg2_set_buffers(m_priv->m_avstream, buffered_packets);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Buffered packets %1 (%2 KB)")
            .arg(buffered_packets).arg(buffered_packets * 4));

    return true;
}

bool LinuxFirewireDevice::SetAVStreamSpeed(uint speed)
{
    if (!m_priv->m_avstream)
        return false;

    uint curspeed = iec61883_mpeg2_get_speed(m_priv->m_avstream);

    if (curspeed == speed)
    {
        m_speed = speed;
        return true;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Changing Speed %1 -> %2")
            .arg(speed_to_string(curspeed),
                 speed_to_string(m_speed)));

    iec61883_mpeg2_set_speed(m_priv->m_avstream, speed);

    if (speed == (uint)iec61883_mpeg2_get_speed(m_priv->m_avstream))
    {
        m_speed = speed;
        return true;
    }

    LOG(VB_GENERAL, LOG_WARNING, LOC + "Unable to set firewire speed.");

    return false;
}

bool LinuxFirewireDevice::IsNodeOpen(void) const
{
    return m_priv->m_isP2pNodeOpen || m_priv->m_isBcastNodeOpen;
}

bool LinuxFirewireDevice::IsAVStreamOpen(void) const
{
    return m_priv->m_avstream;
}

bool LinuxFirewireDevice::ResetBus(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "ResetBus() -- begin");

    if (m_dbResetDisabled)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Bus Reset disabled" + ENO);
        LOG(VB_GENERAL, LOG_INFO, LOC + "ResetBus() -- end");
        return true;
    }

    bool ok = (raw1394_reset_bus_new(GetInfoPtr()->m_fwHandle,
                                     RAW1394_LONG_RESET) == 0);
    if (!ok)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Bus Reset failed" + ENO);

    LOG(VB_GENERAL, LOG_INFO, LOC + "ResetBus() -- end");

    return ok;
}

void LinuxFirewireDevice::PrintDropped(uint dropped_packets)
{
    if (dropped_packets == 1)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Dropped a TS packet");
    }
    else if (dropped_packets > 1)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("Dropped %1 TS packets")
                .arg(dropped_packets));
    }
}

std::vector<AVCInfo> LinuxFirewireDevice::GetSTBList(void)
{
    std::vector<AVCInfo> list;

    {
        LinuxFirewireDevice dev(0,0,0,false);
        list = dev.GetSTBListPrivate();
    }

    return list;
}

std::vector<AVCInfo> LinuxFirewireDevice::GetSTBListPrivate(void)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "GetSTBListPrivate -- begin");
#endif
    QMutexLocker locker(&m_lock);
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "GetSTBListPrivate -- got lock");
#endif

    std::vector<AVCInfo> list;

    for (const auto & device : std::as_const(m_priv->m_devices))
    {
        if (device->IsSubunitType(kAVCSubunitTypeTuner) &&
            device->IsSubunitType(kAVCSubunitTypePanel))
        {
            list.push_back(*device);
        }
    }

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "GetSTBListPrivate -- end");
#endif
    return list;
}

struct dev_item
{
    raw1394handle_t m_handle;
    int             m_port;
    int             m_node;
};

bool LinuxFirewireDevice::UpdateDeviceList(void)
{
    dev_item item {};

    item.m_handle = raw1394_new_handle();
    if (!item.m_handle)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("LinuxFirewireDevice: ") +
                "Couldn't get handle" + ENO);
        return false;
    }

    std::array<raw1394_portinfo,16> port_info {};
    int numcards = raw1394_get_port_info(item.m_handle, port_info.data(),
                                         port_info.size());
    if (numcards < 1)
    {
        raw1394_destroy_handle(item.m_handle);
        return true;
    }

    std::map<uint64_t,bool> guid_online;
    for (int port = 0; port < numcards; port++)
    {
        if (raw1394_set_port(item.m_handle, port) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("LinuxFirewireDevice: "
                    "Couldn't set port to %1").arg(port));
            continue;
        }

        for (int node = 0; node < raw1394_get_nodecount(item.m_handle); node++)
        {
            uint64_t guid = 0;

            guid = rom1394_get_guid(item.m_handle, node);
            item.m_port = port;
            item.m_node = node;
            UpdateDeviceListItem(guid, &item);
            guid_online[guid] = true;
        }

        raw1394_destroy_handle(item.m_handle);

        item.m_handle = raw1394_new_handle();
        if (!item.m_handle)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("LinuxFirewireDevice: ") +
                    "Couldn't get handle " +
                QString("(after setting port %1").arg(port) + ENO);
            item.m_handle = nullptr;
            break;
        }

        numcards = raw1394_get_port_info(item.m_handle, port_info.data(),
                                         port_info.size());
    }

    if (item.m_handle)
    {
        raw1394_destroy_handle(item.m_handle);
        item.m_handle = nullptr;
    }

    item.m_port = -1;
    item.m_node = -1;
    for (auto it = m_priv->m_devices.begin(); it != m_priv->m_devices.end(); ++it)
    {
        if (!guid_online[it.key()])
            UpdateDeviceListItem(it.key(), &item);
    }

    return true;
}

void LinuxFirewireDevice::UpdateDeviceListItem(uint64_t guid, void *pitem)
{
    avcinfo_list_t::iterator it = m_priv->m_devices.find(guid);

    if (it == m_priv->m_devices.end())
    {
        auto *ptr = new LinuxAVCInfo();

        LOG(VB_RECORD, LOG_INFO, LOC + QString("Adding   0x%1").arg(guid,0,16));

        m_priv->m_devices[guid] = ptr;
        it = m_priv->m_devices.find(guid);
    }

    if (it != m_priv->m_devices.end())
    {
        dev_item &item = *((dev_item*) pitem);
        LOG(VB_RECORD, LOG_INFO,
            LOC + QString("Updating 0x%1 port: %2 node: %3")
                .arg(guid,0,16).arg(item.m_port).arg(item.m_node));

        (*it)->Update(guid, item.m_handle, item.m_port, item.m_node);
    }
}

LinuxAVCInfo *LinuxFirewireDevice::GetInfoPtr(void)
{
    if (!m_priv)
        return nullptr;

    avcinfo_list_t::iterator it = m_priv->m_devices.find(m_guid);
    return (it == m_priv->m_devices.end()) ? nullptr : *it;
}

const LinuxAVCInfo *LinuxFirewireDevice::GetInfoPtr(void) const
{
    if (!m_priv)
        return nullptr;

    avcinfo_list_t::iterator it = m_priv->m_devices.find(m_guid);
    return (it == m_priv->m_devices.end()) ? nullptr : *it;
}

int linux_firewire_device_tspacket_handler(
    unsigned char *tspacket, int len, uint dropped, void *callback_data)
{
    auto *fw = reinterpret_cast<LinuxFirewireDevice*>(callback_data);
    if (!fw)
        return 0;

    if (dropped)
        fw->PrintDropped(dropped);

    if (len > 0)
        fw->BroadcastToListeners(tspacket, len);

    return 1;
}

static bool has_data(int fd, std::chrono::milliseconds msec)
{
    fd_set rfds;
    FD_ZERO(&rfds); // NOLINT(readability-isolate-declaration)
    FD_SET(fd, &rfds);

    struct timeval tv {};
    tv.tv_sec  = msec.count() / 1000;
    tv.tv_usec = (msec.count() % 1000) * 1000;

    int ready = select(fd + 1, &rfds, nullptr, nullptr, &tv);

    if (ready < 0)
        LOG(VB_GENERAL, LOG_ERR, "LFireDev: Select Error" + ENO);

    return ready > 0;
}

static QString speed_to_string(uint speed)
{
    if (speed > 3)
        return QString("Invalid Speed (%1)").arg(speed);

    static constexpr std::array<const uint,4> kSpeeds { 100, 200, 400, 800 };
    return QString("%1Mbps").arg(kSpeeds[speed]);
}

static int linux_firewire_device_bus_reset_handler(
    raw1394handle_t handle, unsigned int generation)
{
    QMutexLocker locker(&LFDPriv::s_lock);

    handle_to_lfd_t::iterator it = LFDPriv::s_handle_info.find(handle);

    if (it != LFDPriv::s_handle_info.end())
        (*it)->SignalReset(generation);

    return 0;
}
