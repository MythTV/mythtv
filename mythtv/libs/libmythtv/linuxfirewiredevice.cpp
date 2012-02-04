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
#include <map>
using namespace std;

// Qt headers
#include <QDateTime>

// MythTV headers
#include "linuxfirewiredevice.h"
#include "firewirerecorder.h"
#include "mythcorecontext.h"
#include "linuxavcinfo.h"
#include "mythlogging.h"

#define LOC      QString("LFireDev(%1): ").arg(guid_to_string(m_guid))

#define kNoDataTimeout            50   /* msec */
#define kResetTimeout             1000 /* msec */

typedef QMap<raw1394handle_t,LinuxFirewireDevice*> handle_to_lfd_t;

class LFDPriv
{
  public:
    LFDPriv() :
        generation(0), reset_timer_on(false),
        run_port_handler(false), is_port_handler_running(false),
        avstream(NULL), channel(-1),
        output_plug(-1), input_plug(-1), bandwidth(0), no_data_cnt(0),
        is_p2p_node_open(false), is_bcast_node_open(false),
        is_streaming(false), port_handler_thread(NULL)
    {
    }

    ~LFDPriv()
    {
        avcinfo_list_t::iterator it = devices.begin();
        for (; it != devices.end(); ++it)
            delete (*it);
        devices.clear();

        if (port_handler_thread)
        {
            port_handler_thread->wait();
            delete port_handler_thread;
        }
    }

    uint             generation;
    bool             reset_timer_on;
    MythTimer        reset_timer;

    bool             run_port_handler;
    bool             is_port_handler_running;
    QWaitCondition   port_handler_wait;
    QMutex           start_stop_port_handler_lock;

    iec61883_mpeg2_t avstream;
    int              channel;
    int              output_plug;
    int              input_plug;
    int              bandwidth;
    uint             no_data_cnt;

    bool             is_p2p_node_open;
    bool             is_bcast_node_open;
    bool             is_streaming;

    QDateTime        stop_streaming_timer;
    MThread         *port_handler_thread;

    avcinfo_list_t   devices;

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
static bool has_data(int fd, uint msec);
static QString speed_to_string(uint speed);
static int linux_firewire_device_bus_reset_handler(
    raw1394handle_t handle, uint generation);

LinuxFirewireDevice::LinuxFirewireDevice(
    uint64_t guid, uint subunitid,
    uint speed, bool use_p2p, uint av_buffer_size_in_bytes) :
    FirewireDevice(guid, subunitid, speed),
    m_bufsz(av_buffer_size_in_bytes),
    m_db_reset_disabled(false),
    m_use_p2p(use_p2p), m_priv(new LFDPriv())
{
    if (!m_bufsz)
        m_bufsz = gCoreContext->GetNumSetting("HDRingbufferSize");

    m_db_reset_disabled = gCoreContext->GetNumSetting("DisableFirewireReset", 0);

    UpdateDeviceList();
}

LinuxFirewireDevice::~LinuxFirewireDevice()
{
    if (IsPortOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ctor called with open port");
        while (IsPortOpen())
            ClosePort();
    }

    if (m_priv)
    {
        delete m_priv;
        m_priv = NULL;
    }
}

void LinuxFirewireDevice::SignalReset(uint generation)
{
    const QString loc = LOC + QString("SignalReset(%1->%2)")
        .arg(m_priv->generation).arg(generation);

    LOG(VB_GENERAL, LOG_INFO, loc);

    if (GetInfoPtr())
        raw1394_update_generation(GetInfoPtr()->fw_handle, generation);

    m_priv->generation = generation;

    LOG(VB_GENERAL, LOG_INFO, loc + ": Updating device list -- begin");
    UpdateDeviceList();
    LOG(VB_GENERAL, LOG_INFO, loc + ": Updating device list -- end");

    m_priv->reset_timer_on = true;
    m_priv->reset_timer.start();
}

void LinuxFirewireDevice::HandleBusReset(void)
{
    const QString loc = LOC + "HandleBusReset";

    if (!GetInfoPtr() || !GetInfoPtr()->fw_handle)
        return;

    if (m_priv->is_p2p_node_open)
    {
        LOG(VB_GENERAL, LOG_INFO, loc + ": Reconnecting P2P connection");
        nodeid_t output = GetInfoPtr()->GetNode() | 0xffc0;
        nodeid_t input  = raw1394_get_local_id(GetInfoPtr()->fw_handle);

        int fwchan = iec61883_cmp_reconnect(
            GetInfoPtr()->fw_handle,
            output, &m_priv->output_plug,
            input,  &m_priv->input_plug,
            &m_priv->bandwidth, m_priv->channel);

        if (fwchan < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bus Reset: Failed to reconnect");
        }
        else if (fwchan != m_priv->channel)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("FWChan changed %1->%2")
                    .arg(m_priv->channel).arg(fwchan));
        }
        m_priv->channel = fwchan;

        LOG(VB_GENERAL, LOG_INFO,
            loc + QString(": Reconnected fwchan: %1\n\t\t\toutput: 0x%2 "
                          "input: 0x%3")
                .arg(fwchan).arg(output,0,16).arg(input,0,16));
    }

    if (m_priv->is_bcast_node_open)
    {
        nodeid_t output = GetInfoPtr()->GetNode() | 0xffc0;

        LOG(VB_RECORD, LOG_INFO, loc + ": Restarting broadcast connection on " +
            QString("node %1, channel %2")
                .arg(GetInfoPtr()->GetNode()).arg(m_priv->channel));

        int err = iec61883_cmp_create_bcast_output(
            GetInfoPtr()->fw_handle,
            output, m_priv->output_plug,
            m_priv->channel, m_speed);

        if (err < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bus Reset : Failed to reconnect");
        }
    }
}

bool LinuxFirewireDevice::OpenPort(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Starting Port Handler Thread");
    QMutexLocker locker(&m_priv->start_stop_port_handler_lock);
    LOG(VB_RECORD, LOG_INFO, LOC + "Starting Port Handler Thread -- locked");

    LOG(VB_RECORD, LOG_INFO, LOC + "OpenPort()");

    QMutexLocker mlocker(&m_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + "OpenPort() -- got lock");

    if (!GetInfoPtr())
        return false;

    if (GetInfoPtr()->IsPortOpen())
    {
        m_open_port_cnt++;
        return true;
    }

    if (!GetInfoPtr()->OpenPort())
        return false;

    add_handle(GetInfoPtr()->fw_handle, this);

    m_priv->generation = raw1394_get_generation(GetInfoPtr()->fw_handle);
    raw1394_set_bus_reset_handler(
        GetInfoPtr()->fw_handle, linux_firewire_device_bus_reset_handler);

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

    m_priv->run_port_handler = true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Starting port handler thread");
    m_priv->port_handler_thread = new MThread("LinuxController", this);
    m_priv->port_handler_thread->start();

    while (!m_priv->is_port_handler_running)
        m_priv->port_handler_wait.wait(mlocker.mutex(), 100);

    LOG(VB_RECORD, LOG_INFO, LOC + "Port handler thread started");

    m_open_port_cnt++;

    return true;
}

bool LinuxFirewireDevice::ClosePort(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Stopping Port Handler Thread");
    QMutexLocker locker(&m_priv->start_stop_port_handler_lock);
    LOG(VB_RECORD, LOG_INFO, LOC + "Stopping Port Handler Thread -- locked");

    QMutexLocker mlocker(&m_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + "ClosePort()");

    if (m_open_port_cnt < 1)
        return false;

    m_open_port_cnt--;

    if (m_open_port_cnt != 0)
        return true;

    if (!GetInfoPtr())
        return false;

    if (GetInfoPtr()->IsPortOpen())
    {
        if (IsNodeOpen())
            CloseNode();

        LOG(VB_RECORD, LOG_INFO,
            LOC + "Waiting for port handler thread to stop");
        m_priv->run_port_handler = false;
        m_priv->port_handler_wait.wakeAll();

        mlocker.unlock();
        m_priv->port_handler_thread->wait();
        mlocker.relock();

        delete m_priv->port_handler_thread;
        m_priv->port_handler_thread = NULL;

        LOG(VB_RECORD, LOG_INFO, LOC + "Joined port handler thread");

        remove_handle(GetInfoPtr()->fw_handle);

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
    const vector<uint8_t>  &cmd,
    vector<uint8_t>        &result,
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
    if (m_use_p2p)
        return OpenP2PNode();
    else
        return OpenBroadcastNode();
}

bool LinuxFirewireDevice::CloseNode(void)
{
    if (m_priv->is_p2p_node_open)
        return CloseP2PNode();

    if (m_priv->is_bcast_node_open)
        return CloseBroadcastNode();

    return true;
}

// This may in fact open a broadcast connection, but it tries to open
// a P2P connection first.
bool LinuxFirewireDevice::OpenP2PNode(void)
{
    if (m_priv->is_bcast_node_open)
        return false;

    if (m_priv->is_p2p_node_open)
        return true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Opening P2P connection");

    m_priv->bandwidth   = +1; // +1 == allocate bandwidth
    m_priv->output_plug = -1; // -1 == find first online plug
    m_priv->input_plug  = -1; // -1 == find first online plug
    nodeid_t output     = GetInfoPtr()->GetNode() | 0xffc0;
    nodeid_t input      = raw1394_get_local_id(GetInfoPtr()->fw_handle);
    m_priv->channel     = iec61883_cmp_connect(GetInfoPtr()->fw_handle,
                                               output, &m_priv->output_plug,
                                               input,  &m_priv->input_plug,
                                               &m_priv->bandwidth);

    if (m_priv->channel < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create P2P connection");

        m_priv->bandwidth = 0;

        return false;
    }

    m_priv->is_p2p_node_open = true;

    return true;
}

bool LinuxFirewireDevice::CloseP2PNode(void)
{
    if (m_priv->is_p2p_node_open && (m_priv->channel >= 0))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Closing P2P connection");

        if (m_priv->avstream)
            CloseAVStream();

        nodeid_t output     = GetInfoPtr()->GetNode() | 0xffc0;
        nodeid_t input      = raw1394_get_local_id(GetInfoPtr()->fw_handle);

        iec61883_cmp_disconnect(GetInfoPtr()->fw_handle,
                                output, m_priv->output_plug,
                                input,  m_priv->input_plug,
                                m_priv->channel, m_priv->bandwidth);

        m_priv->channel     = -1;
        m_priv->output_plug = -1;
        m_priv->input_plug  = -1;
        m_priv->is_p2p_node_open = false;
    }

    return true;
}

bool LinuxFirewireDevice::OpenBroadcastNode(void)
{
    if (m_priv->is_p2p_node_open)
        return false;

    if (m_priv->is_bcast_node_open)
        return true;

    if (m_priv->avstream)
        CloseAVStream();

    m_priv->channel     = kBroadcastChannel - GetInfoPtr()->GetNode();
    m_priv->output_plug = 0;
    m_priv->input_plug  = 0;
    nodeid_t output     = GetInfoPtr()->GetNode() | 0xffc0;

    LOG(VB_RECORD, LOG_INFO, LOC + "Opening broadcast connection on " +
            QString("node %1, channel %2")
            .arg(GetInfoPtr()->GetNode()).arg(m_priv->channel));

    int err = iec61883_cmp_create_bcast_output(
        GetInfoPtr()->fw_handle,
        output, m_priv->output_plug,
        m_priv->channel, m_speed);

    if (err != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Broadcast connection");

        m_priv->channel     = -1;
        m_priv->output_plug = -1;
        m_priv->input_plug  = -1;

        return false;
    }

    m_priv->is_bcast_node_open = true;

    return true;
}

bool LinuxFirewireDevice::CloseBroadcastNode(void)
{
    if (m_priv->is_bcast_node_open)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Closing broadcast connection");

        m_priv->channel     = -1;
        m_priv->output_plug = -1;
        m_priv->input_plug  = -1;
        m_priv->is_bcast_node_open = false;
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

    if (m_priv->avstream)
        return true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Opening A/V stream object");

    m_priv->avstream = iec61883_mpeg2_recv_init(
        GetInfoPtr()->fw_handle, linux_firewire_device_tspacket_handler, this);

    if (!m_priv->avstream)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to open AVStream" + ENO);

        return false;
    }

    iec61883_mpeg2_set_synch(m_priv->avstream, 1 /* sync on close */);

    if (m_bufsz)
        SetAVStreamBufferSize(m_bufsz);

    return true;
}

bool LinuxFirewireDevice::CloseAVStream(void)
{
    if (!m_priv->avstream)
        return true;

    LOG(VB_RECORD, LOG_INFO, LOC + "Closing A/V stream object");

    while (m_listeners.size())
        FirewireDevice::RemoveListener(m_listeners[m_listeners.size() - 1]);

    if (m_priv->is_streaming)
        StopStreaming();

    iec61883_mpeg2_close(m_priv->avstream);
    m_priv->avstream = NULL;

    return true;
}

void LinuxFirewireDevice::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "RunPortHandler -- start");
    m_lock.lock();
    LOG(VB_RECORD, LOG_INFO, LOC + "RunPortHandler -- got first lock");
    m_priv->is_port_handler_running = true;
    m_priv->port_handler_wait.wakeAll();
    // we need to unlock & sleep to allow wakeAll to wake other threads.
    m_lock.unlock();
    usleep(2500);
    m_lock.lock();

    m_priv->no_data_cnt = 0;
    while (m_priv->run_port_handler)
    {
        LFDPriv::s_lock.lock();
        bool reset_timer_on = m_priv->reset_timer_on;
        bool handle_reset = reset_timer_on &&
            (m_priv->reset_timer.elapsed() > 100);
        if (handle_reset)
            m_priv->reset_timer_on = false;
        LFDPriv::s_lock.unlock();

        if (handle_reset)
            HandleBusReset();

        if (!reset_timer_on && m_priv->is_streaming &&
            (m_priv->no_data_cnt > (kResetTimeout / kNoDataTimeout)))
        {
            m_priv->no_data_cnt = 0;
            ResetBus();
        }

        int fwfd = raw1394_get_fd(GetInfoPtr()->fw_handle);
        if (fwfd < 0)
        {
            // We unlock here because this can take a long time
            // and we don't want to block other actions.
            m_priv->port_handler_wait.wait(&m_lock, kNoDataTimeout);

            m_priv->no_data_cnt += (m_priv->is_streaming) ? 1 : 0;
            continue;
        }

        // We unlock here because this can take a long time and we
        // don't want to block other actions. All reads and writes
        // are done with the lock, so this is safe so long as we
        // check that we really have data once we get the lock.
        m_lock.unlock();
        bool ready = has_data(fwfd, kNoDataTimeout);
        m_lock.lock();

        if (!ready && m_priv->is_streaming)
        {
            m_priv->no_data_cnt++;

            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No Input in %1 msec...")
                    .arg(m_priv->no_data_cnt * kNoDataTimeout));
        }

        // Confirm that we won't block, now that we have the lock...
        if (ready && has_data(fwfd, 1 /* msec */))
        {
            // Performs blocking read of next 4 bytes off bus and handles
            // them. Most firewire commands do their own loop_iterate
            // internally to check for results, but some things like
            // streaming data and FireWire bus resets must be handled
            // as well, which we do here...
            int ret = raw1394_loop_iterate(GetInfoPtr()->fw_handle);
            if (-1 == ret)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "raw1394_loop_iterate" + ENO);
            }
        }
    }

    m_priv->is_port_handler_running = false;
    m_priv->port_handler_wait.wakeAll();
    m_lock.unlock();
    LOG(VB_RECORD, LOG_INFO, LOC + "RunPortHandler -- end");
}

bool LinuxFirewireDevice::StartStreaming(void)
{
    if (m_priv->is_streaming)
        return m_priv->is_streaming;

    if (!IsAVStreamOpen() && !OpenAVStream())
        return false;

    if (m_priv->channel < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Starting A/V streaming, no channel");
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Starting A/V streaming -- really");

    if (iec61883_mpeg2_recv_start(m_priv->avstream, m_priv->channel) == 0)
    {
        m_priv->is_streaming = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Starting A/V streaming " + ENO);
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Starting A/V streaming -- done");

    return m_priv->is_streaming;
}

bool LinuxFirewireDevice::StopStreaming(void)
{
    if (m_priv->is_streaming)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Stopping A/V streaming -- really");

        m_priv->is_streaming = false;

        iec61883_mpeg2_recv_stop(m_priv->avstream);

        raw1394_iso_recv_flush(GetInfoPtr()->fw_handle);
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Stopped A/V streaming");

    return true;
}

bool LinuxFirewireDevice::SetAVStreamBufferSize(uint size_in_bytes)
{
    if (!m_priv->avstream)
        return false;

    // Set buffered packets size
    uint   buffer_size      = max(size_in_bytes, 50 * TSPacket::kSize);
    size_t buffered_packets = min(buffer_size / 4, kMaxBufferedPackets);

    iec61883_mpeg2_set_buffers(m_priv->avstream, buffered_packets);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Buffered packets %1 (%2 KB)")
            .arg(buffered_packets).arg(buffered_packets * 4));

    return true;
}

bool LinuxFirewireDevice::SetAVStreamSpeed(uint speed)
{
    if (!m_priv->avstream)
        return false;

    uint curspeed = iec61883_mpeg2_get_speed(m_priv->avstream);

    if (curspeed == speed)
    {
        m_speed = speed;
        return true;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Changing Speed %1 -> %2")
            .arg(speed_to_string(curspeed))
            .arg(speed_to_string(m_speed)));

    iec61883_mpeg2_set_speed(m_priv->avstream, speed);

    if (speed == (uint)iec61883_mpeg2_get_speed(m_priv->avstream))
    {
        m_speed = speed;
        return true;
    }

    LOG(VB_GENERAL, LOG_WARNING, LOC + "Unable to set firewire speed.");

    return false;
}

bool LinuxFirewireDevice::IsNodeOpen(void) const
{
    return m_priv->is_p2p_node_open || m_priv->is_bcast_node_open;
}

bool LinuxFirewireDevice::IsAVStreamOpen(void) const
{
    return m_priv->avstream;
}

bool LinuxFirewireDevice::ResetBus(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "ResetBus() -- begin");

    if (m_db_reset_disabled)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Bus Reset disabled" + ENO);
        LOG(VB_GENERAL, LOG_INFO, LOC + "ResetBus() -- end");
        return true;
    }

    bool ok = (raw1394_reset_bus_new(GetInfoPtr()->fw_handle,
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

vector<AVCInfo> LinuxFirewireDevice::GetSTBList(void)
{
    vector<AVCInfo> list;

    {
        LinuxFirewireDevice dev(0,0,0,false);
        list = dev.GetSTBListPrivate();
    }

    return list;
}

vector<AVCInfo> LinuxFirewireDevice::GetSTBListPrivate(void)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "GetSTBListPrivate -- begin");
#endif
    QMutexLocker locker(&m_lock);
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "GetSTBListPrivate -- got lock");
#endif

    vector<AVCInfo> list;

    avcinfo_list_t::iterator it = m_priv->devices.begin();
    for (; it != m_priv->devices.end(); ++it)
    {
        if ((*it)->IsSubunitType(kAVCSubunitTypeTuner) &&
            (*it)->IsSubunitType(kAVCSubunitTypePanel))
        {
            list.push_back(*(*it));
        }
    }

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "GetSTBListPrivate -- end");
#endif
    return list;
}

typedef struct
{
    raw1394handle_t handle;
    int port;
    int node;
} dev_item;

bool LinuxFirewireDevice::UpdateDeviceList(void)
{
    dev_item item;

    item.handle = raw1394_new_handle();
    if (!item.handle)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("LinuxFirewireDevice: ") +
                "Couldn't get handle" + ENO);
        return false;
    }

    struct raw1394_portinfo port_info[16];
    int numcards = raw1394_get_port_info(item.handle, port_info, 16);
    if (numcards < 1)
    {
        raw1394_destroy_handle(item.handle);
        return true;
    }

    map<uint64_t,bool> guid_online;
    for (int port = 0; port < numcards; port++)
    {
        if (raw1394_set_port(item.handle, port) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("LinuxFirewireDevice: "
                    "Couldn't set port to %1").arg(port));
            continue;
        }

        for (int node = 0; node < raw1394_get_nodecount(item.handle); node++)
        {
            uint64_t guid;

            guid = rom1394_get_guid(item.handle, node);
            item.port = port;
            item.node = node;
            UpdateDeviceListItem(guid, &item);
            guid_online[guid] = true;
        }

        raw1394_destroy_handle(item.handle);

        item.handle = raw1394_new_handle();
        if (!item.handle)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("LinuxFirewireDevice: ") +
                    "Couldn't get handle " +
                QString("(after setting port %1").arg(port) + ENO);
            item.handle = NULL;
            break;
        }

        numcards = raw1394_get_port_info(item.handle, port_info, 16);
    }

    if (item.handle)
    {
        raw1394_destroy_handle(item.handle);
        item.handle = NULL;
    }

    item.port = -1;
    item.node = -1;
    avcinfo_list_t::iterator it = m_priv->devices.begin();
    for (; it != m_priv->devices.end(); ++it)
    {
        if (!guid_online[it.key()])
            UpdateDeviceListItem(it.key(), &item);
    }

    return true;
}

void LinuxFirewireDevice::UpdateDeviceListItem(uint64_t guid, void *pitem)
{
    avcinfo_list_t::iterator it = m_priv->devices.find(guid);

    if (it == m_priv->devices.end())
    {
        LinuxAVCInfo *ptr = new LinuxAVCInfo();

        LOG(VB_RECORD, LOG_INFO, LOC + QString("Adding   0x%1").arg(guid,0,16));

        m_priv->devices[guid] = ptr;
        it = m_priv->devices.find(guid);
    }

    if (it != m_priv->devices.end())
    {
        dev_item &item = *((dev_item*) pitem);
        LOG(VB_RECORD, LOG_INFO,
            LOC + QString("Updating 0x%1 port: %2 node: %3")
                .arg(guid,0,16).arg(item.port).arg(item.node));

        (*it)->Update(guid, item.handle, item.port, item.node);
    }
}

LinuxAVCInfo *LinuxFirewireDevice::GetInfoPtr(void)
{
    if (!m_priv)
        return NULL;

    avcinfo_list_t::iterator it = m_priv->devices.find(m_guid);
    return (it == m_priv->devices.end()) ? NULL : *it;
}

const LinuxAVCInfo *LinuxFirewireDevice::GetInfoPtr(void) const
{
    if (!m_priv)
        return NULL;

    avcinfo_list_t::iterator it = m_priv->devices.find(m_guid);
    return (it == m_priv->devices.end()) ? NULL : *it;
}

int linux_firewire_device_tspacket_handler(
    unsigned char *tspacket, int len, uint dropped, void *callback_data)
{
    LinuxFirewireDevice *fw =
        reinterpret_cast<LinuxFirewireDevice*>(callback_data);

    if (!fw)
        return 0;

    if (dropped)
        fw->PrintDropped(dropped);

    if (len > 0)
        fw->BroadcastToListeners(tspacket, len);

    return 1;
}

static bool has_data(int fd, uint msec)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    struct timeval tv;
    tv.tv_sec  = msec / 1000;
    tv.tv_usec = (msec % 1000) * 1000;

    int ready = select(fd + 1, &rfds, NULL, NULL, &tv);

    if (ready < 0)
        LOG(VB_GENERAL, LOG_ERR, "LFireDev: Select Error" + ENO);

    if (ready <= 0)
        return false;

    return true;
}

static QString speed_to_string(uint speed)
{
    if (speed > 3)
        return QString("Invalid Speed (%1)").arg(speed);

    static const uint speeds[] = { 100, 200, 400, 800 };
    return QString("%1Mbps").arg(speeds[speed]);
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
