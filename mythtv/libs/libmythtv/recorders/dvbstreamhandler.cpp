// -*- Mode: c++ -*-

// POSIX headers
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt headers
#include <QString>

// MythTV headers
#include "dvbstreamhandler.h"
#include "dvbchannel.h"
#include "dtvsignalmonitor.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cardutil.h"
#include "dvbtypes.h" // for pid filtering
#include "diseqc.h" // for rotor retune
#include "mythlogging.h"

#define LOC      QString("DVBSH[%1](%2): ").arg(m_inputid).arg(m_device)

QMap<QString,bool> DVBStreamHandler::s_rec_supports_ts_monitoring;
QMutex             DVBStreamHandler::s_rec_supports_ts_monitoring_lock;

QMap<QString,DVBStreamHandler*> DVBStreamHandler::s_handlers;
QMap<QString,uint>              DVBStreamHandler::s_handlers_refcnt;
QMutex                          DVBStreamHandler::s_handlers_lock;

DVBStreamHandler *DVBStreamHandler::Get(const QString &devname,
                                        int inputid)
{
    QMutexLocker locker(&s_handlers_lock);

    QMap<QString,DVBStreamHandler*>::iterator it =
        s_handlers.find(devname);

    if (it == s_handlers.end())
    {
        s_handlers[devname] = new DVBStreamHandler(devname, inputid);
        s_handlers_refcnt[devname] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("DVBSH[%1]: Creating new stream handler %2")
            .arg(inputid).arg(devname));
    }
    else
    {
        s_handlers_refcnt[devname]++;
        uint rcount = s_handlers_refcnt[devname];
        LOG(VB_RECORD, LOG_INFO,
            QString("DVBSH[%1]: Using existing stream handler for %2")
            .arg(inputid)
            .arg(devname) + QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[devname];
}

void DVBStreamHandler::Return(DVBStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devname = ref->m_device;

    QMap<QString,uint>::iterator rit = s_handlers_refcnt.find(devname);
    if (rit == s_handlers_refcnt.end())
        return;

    QMap<QString,DVBStreamHandler*>::iterator it = s_handlers.find(devname);

    if (*rit > 1)
    {
        ref = nullptr;
        (*rit)--;
        return;
    }

    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("DVBSH[%1]: Closing handler for %2")
            .arg(inputid).arg(devname));
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("DVBSH[%1] Error: Couldn't find handler for %2")
            .arg(inputid).arg(devname));
    }

    s_handlers_refcnt.erase(rit);
    ref = nullptr;
}

DVBStreamHandler::DVBStreamHandler(const QString &dvb_device, int inputid)
    : StreamHandler(dvb_device, inputid)
    , _dvr_dev_path(CardUtil::GetDeviceName(DVB_DEV_DVR, m_device))
    , _allow_retune(false)
    , _sigmon(nullptr)
    , _dvbchannel(nullptr)
    , _drb(nullptr)
{
    setObjectName("DVBRead");
}

void DVBStreamHandler::run(void)
{
    RunProlog();
    LOG(VB_RECORD, LOG_DEBUG, LOC + "run(): begin");

    if (!SupportsTSMonitoring() && m_allow_section_reader)
        RunSR();
    else
        RunTS();

    LOG(VB_RECORD, LOG_DEBUG, LOC + "run(): end");
    RunEpilog();
}

/** \fn DVBStreamHandler::RunTS(void)
 *  \brief Uses TS filtering devices to read a DVB device for tables & data
 *
 *  This supports all types of MPEG based stream data, but is extremely
 *  slow with DVB over USB 1.0 devices which for efficiency reasons buffer
 *  a stream until a full block transfer buffer full of the requested
 *  tables is available. This takes a very long time when you are just
 *  waiting for a PAT or PMT table, and the buffer is hundreds of packets
 *  in size.
 */
void DVBStreamHandler::RunTS(void)
{
    QByteArray dvr_dev_path = _dvr_dev_path.toLatin1();
    int dvr_fd;
    for (int tries = 1; ; ++tries)
    {
        dvr_fd = open(dvr_dev_path.constData(), O_RDONLY | O_NONBLOCK);
        if (dvr_fd >= 0)
            break;

        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Opening DVR device %1 failed : %2")
                .arg(_dvr_dev_path).arg(strerror(errno)));

        if (tries >= 20 || (errno != EBUSY && errno != EAGAIN))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to open DVR device %1 : %2")
                    .arg(_dvr_dev_path).arg(strerror(errno)));
            m_bError = true;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    int remainder = 0;
    int buffer_size = TSPacket::kSize * 15000;
    unsigned char *buffer = new unsigned char[buffer_size];
    if (!buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate memory");
        close(dvr_fd);
        m_bError = true;
        return;
    }
    memset(buffer, 0, buffer_size);

    DeviceReadBuffer *drb = nullptr;
    if (m_needs_buffering)
    {
        drb = new DeviceReadBuffer(this, true, false);
        if (!drb->Setup(m_device, dvr_fd))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate DRB buffer");
            delete drb;
            delete[] buffer;
            close(dvr_fd);
            m_bError = true;
            return;
        }

        drb->Start();
    }

    {
        // SetRunning() + set _drb
        QMutexLocker locker(&m_start_stop_lock);
        m_running = true;
        m_using_buffering = m_needs_buffering;
        m_using_section_reader = false;
        _drb = drb;
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC + "RunTS(): begin");

    fd_set fd_select_set;
    FD_ZERO(        &fd_select_set);
    FD_SET (dvr_fd, &fd_select_set);
    while (m_running_desired && !m_bError)
    {
        RetuneMonitor();
        UpdateFiltersFromStreamData();

        ssize_t len = 0;

        if (drb)
        {
            len = drb->Read(&(buffer[remainder]), buffer_size - remainder);

            // Check for DRB errors
            if (drb->IsErrored())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Device error detected");
                m_bError = true;
            }

            if (drb->IsEOF() && m_running_desired)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Device EOF detected");
                m_bError = true;
            }
        }
        else
        {
            // timeout gets reset by select, so we need to create new one
            struct timeval timeout = { 0, 50 /* ms */ * 1000 /* -> usec */ };
            int ret = select(dvr_fd+1, &fd_select_set, nullptr, nullptr, &timeout);
            if (ret == -1 && errno != EINTR)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "select() failed" + ENO);
            }
            else
            {
                len = read(dvr_fd, &(buffer[remainder]),
                           buffer_size - remainder);
            }

            if ((0 == len) || (-1 == len))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
        }

        len += remainder;

        if (len < 10) // 10 bytes = 4 bytes TS header + 6 bytes PES header
        {
            remainder = len;
            continue;
        }

        m_listener_lock.lock();

        if (m_stream_data_list.empty())
        {
            m_listener_lock.unlock();
            continue;
        }

        StreamDataList::const_iterator sit = m_stream_data_list.begin();
        for (; sit != m_stream_data_list.end(); ++sit)
            remainder = sit.key()->ProcessData(buffer, len);

        WriteMPTS(buffer, len - remainder);

        m_listener_lock.unlock();

        if (remainder > 0 && (len > remainder)) // leftover bytes
            memmove(buffer, &(buffer[len - remainder]), remainder);
    }
    LOG(VB_RECORD, LOG_DEBUG, LOC + "RunTS(): " + "shutdown");

    RemoveAllPIDFilters();

    {
        QMutexLocker locker(&m_start_stop_lock);
        _drb = nullptr;
    }

    delete drb;
    close(dvr_fd);
    delete[] buffer;

    LOG(VB_RECORD, LOG_DEBUG, LOC + "RunTS(): " + "end");

    SetRunning(false, m_needs_buffering, false);
}

/** \fn DVBStreamHandler::RunSR(void)
 *  \brief Uses "Section" reader to read a DVB device for tables
 *
 *  This currently only supports DVB streams, ATSC and the raw MPEG
 *  streams used by some cable and satelite providers is not supported.
 */
void DVBStreamHandler::RunSR(void)
{
    int buffer_size = 4192;  // maximum size of Section we handle
    unsigned char *buffer = pes_alloc(buffer_size);
    if (!buffer)
    {
        m_bError = true;
        return;
    }

    SetRunning(true, m_needs_buffering, true);

    LOG(VB_RECORD, LOG_DEBUG, LOC + "RunSR(): begin");

    while (m_running_desired && !m_bError)
    {
        RetuneMonitor();
        UpdateFiltersFromStreamData();

        QMutexLocker read_locker(&m_pid_lock);

        bool readSomething = false;
        PIDInfoMap::const_iterator fit = m_pid_info.begin();
        for (; fit != m_pid_info.end(); ++fit)
        {
            int len = read((*fit)->filter_fd, buffer, buffer_size);
            if (len <= 0)
                continue;

            readSomething = true;

            const PSIPTable psip(buffer);

            if (psip.SectionSyntaxIndicator())
            {
                m_listener_lock.lock();
                StreamDataList::const_iterator sit = m_stream_data_list.begin();
                for (; sit != m_stream_data_list.end(); ++sit)
                    sit.key()->HandleTables(fit.key() /* pid */, psip);
                m_listener_lock.unlock();
            }
        }

        if (!readSomething)
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    LOG(VB_RECORD, LOG_DEBUG, LOC + "RunSR(): " + "shutdown");

    RemoveAllPIDFilters();

    pes_free(buffer);

    SetRunning(false, m_needs_buffering, true);

    LOG(VB_RECORD, LOG_DEBUG, LOC + "RunSR(): " + "end");
}

typedef vector<uint> pid_list_t;

static pid_list_t::iterator find(
    const PIDInfoMap &map,
    pid_list_t &list,
    pid_list_t::iterator begin,
    pid_list_t::iterator end, bool find_open)
{
    pid_list_t::iterator it;
    for (it = begin; it != end; ++it)
    {
        PIDInfoMap::const_iterator mit = map.find(*it);
        if ((mit != map.end()) && ((*mit)->IsOpen() == find_open))
            return it;
    }

    for (it = list.begin(); it != begin; ++it)
    {
        PIDInfoMap::const_iterator mit = map.find(*it);
        if ((mit != map.end()) && ((*mit)->IsOpen() == find_open))
            return it;
    }

    return list.end();
}

void DVBStreamHandler::CycleFiltersByPriority(void)
{
    QMutexLocker writing_locker(&m_pid_lock);
    QMap<PIDPriority, pid_list_t> priority_queue;
    QMap<PIDPriority, uint> priority_open_cnt;

    PIDInfoMap::const_iterator cit = m_pid_info.begin();
    for (; cit != m_pid_info.end(); ++cit)
    {
        PIDPriority priority = GetPIDPriority((*cit)->_pid);
        priority_queue[priority].push_back(cit.key());
        if ((*cit)->IsOpen())
            priority_open_cnt[priority]++;
    }

    QMap<PIDPriority, pid_list_t>::iterator it = priority_queue.begin();
    for (; it != priority_queue.end(); ++it)
        sort((*it).begin(), (*it).end());

    for (PIDPriority i = kPIDPriorityHigh; i > kPIDPriorityNone;
         i = (PIDPriority)((int)i-1))
    {
        while (priority_open_cnt[i] < priority_queue[i].size())
        {
            // if we can open a filter, just do it

            // find first closed filter after first open an filter "k"
            pid_list_t::iterator open = find(
                m_pid_info, priority_queue[i],
                priority_queue[i].begin(), priority_queue[i].end(), true);
            if (open == priority_queue[i].end())
                open = priority_queue[i].begin();

            pid_list_t::iterator closed = find(
                m_pid_info, priority_queue[i],
                open, priority_queue[i].end(), false);

            if (closed == priority_queue[i].end())
                break; // something is broken

            if (m_pid_info[*closed]->Open(m_device, m_using_section_reader))
            {
                m_open_pid_filters++;
                priority_open_cnt[i]++;
                continue;
            }

            // if we can't open a filter, try to close a lower priority one
            bool freed = false;
            for (PIDPriority j = (PIDPriority)((int)i - 1);
                 (j > kPIDPriorityNone) && !freed;
                 j = (PIDPriority)((int)j-1))
            {
                if (!priority_open_cnt[j])
                    continue;

                for (uint k = 0; (k < priority_queue[j].size()) && !freed; k++)
                {
                    PIDInfo *info = m_pid_info[priority_queue[j][k]];
                    if (!info->IsOpen())
                        continue;

                    if (info->Close(m_device))
                        freed = true;

                    m_open_pid_filters--;
                    priority_open_cnt[j]--;
                }
            }

            if (freed)
            {
                // if we can open a filter, just do it
                if (m_pid_info[*closed]->Open(
                        m_device, m_using_section_reader))
                {
                    m_open_pid_filters++;
                    priority_open_cnt[i]++;
                    continue;
                }
            }

            // we have to cycle within our priority level

            if (m_cycle_timer.elapsed() < 1000)
                break; // we don't want to cycle too often

            if (!m_pid_info[*open]->IsOpen())
                break; // nothing to close..

            // close "open"
            bool ok = m_pid_info[*open]->Close(m_device);
            m_open_pid_filters--;
            priority_open_cnt[i]--;

            // open "closed"
            if (ok && m_pid_info[*closed]->
                Open(m_device, m_using_section_reader))
            {
                m_open_pid_filters++;
                priority_open_cnt[i]++;
            }

            break; // we only want to cycle once per priority per run
        }
    }

    m_cycle_timer.start();
}

void DVBStreamHandler::SetRetuneAllowed(
    bool              allow,
    DTVSignalMonitor *sigmon,
    DVBChannel       *dvbchan)
{
    if (allow && sigmon && dvbchan)
    {
        _allow_retune = true;
        _sigmon       = sigmon;
        _dvbchannel   = dvbchan;
    }
    else
    {
        _allow_retune = false;
        _sigmon       = nullptr;
        _dvbchannel   = nullptr;
    }
}

void DVBStreamHandler::RetuneMonitor(void)
{
    if (!_allow_retune)
        return;

    // Rotor position
    if (_sigmon->HasFlags(SignalMonitor::kDVBSigMon_WaitForPos))
    {
        const DiSEqCDevRotor *rotor = _dvbchannel->GetRotor();
        if (rotor)
        {
            bool was_moving, is_moving;
            _sigmon->GetRotorStatus(was_moving, is_moving);

            // Retune if move completes normally
            if (was_moving && !is_moving)
            {
                LOG(VB_CHANNEL, LOG_INFO,
                    LOC + "Retuning for rotor completion");
                _dvbchannel->Retune();

                // (optionally) No need to wait for SDT anymore...
                // RemoveFlags(kDTVSigMon_WaitForSDT);
            }
        }
        else
        {
            // If no rotor is present, pretend the movement is completed
            _sigmon->SetRotorValue(100);
        }
    }
}

/** \fn DVBStreamHandler::SupportsTSMonitoring(void)
 *  \brief Returns true if TS monitoring is supported.
 *
 *   NOTE: If you are using a DEC2000-t device you need to
 *   apply the patches provided by Peter Beutner for it, see
 *   http://www.gossamer-threads.com/lists/mythtv/dev/166172
 *   These patches should make it in to Linux 2.6.15 or 2.6.16.
 */
bool DVBStreamHandler::SupportsTSMonitoring(void)
{
    const uint pat_pid = 0x0;

    {
        QMutexLocker locker(&s_rec_supports_ts_monitoring_lock);
        QMap<QString,bool>::const_iterator it;
        it = s_rec_supports_ts_monitoring.find(m_device);
        if (it != s_rec_supports_ts_monitoring.end())
            return *it;
    }

    QByteArray dvr_dev_path = _dvr_dev_path.toLatin1();
    int dvr_fd = open(dvr_dev_path.constData(), O_RDONLY | O_NONBLOCK);
    if (dvr_fd < 0)
    {
        QMutexLocker locker(&s_rec_supports_ts_monitoring_lock);
        s_rec_supports_ts_monitoring[m_device] = false;
        return false;
    }

    bool supports_ts = false;
    if (AddPIDFilter(new DVBPIDInfo(pat_pid)))
    {
        supports_ts = true;
        RemovePIDFilter(pat_pid);
    }

    close(dvr_fd);

    QMutexLocker locker(&s_rec_supports_ts_monitoring_lock);
    s_rec_supports_ts_monitoring[m_device] = supports_ts;

    return supports_ts;
}

#undef LOC

#define LOC      QString("PIDInfo(%1): ").arg(dvb_dev)

bool DVBPIDInfo::Open(const QString &dvb_dev, bool use_section_reader)
{
    if (filter_fd >= 0)
    {
        close(filter_fd);
        filter_fd = -1;
    }

    QString demux_fn = CardUtil::GetDeviceName(DVB_DEV_DEMUX, dvb_dev);
    QByteArray demux_ba = demux_fn.toLatin1();

    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Opening filter for pid 0x%1")
            .arg(_pid, 0, 16));

    int mux_fd = open(demux_ba.constData(), O_RDWR | O_NONBLOCK);
    if (mux_fd == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open demux device %1 "
                                               "for filter on pid 0x%2")
                .arg(demux_fn).arg(_pid, 0, 16));
        return false;
    }

    if (!use_section_reader)
    {
        struct dmx_pes_filter_params pesFilterParams {};
        pesFilterParams.pid      = (uint16_t) _pid;
        pesFilterParams.input    = DMX_IN_FRONTEND;
        pesFilterParams.output   = DMX_OUT_TS_TAP;
        pesFilterParams.flags    = DMX_IMMEDIATE_START;
        pesFilterParams.pes_type = DMX_PES_OTHER;

        if (ioctl(mux_fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to set TS filter (pid 0x%1)")
                    .arg(_pid, 0, 16));

            close(mux_fd);
            return false;
        }
    }
    else
    {
        struct dmx_sct_filter_params sctFilterParams {};
        switch ( _pid )
        {
            case 0x0: // PAT
                sctFilterParams.filter.filter[0] = 0;
                sctFilterParams.filter.mask[0]   = 0xff;
                break;
            case 0x0010: // assume this is for an NIT, NITo, PMT
                // This filter will give us table ids 0x00-0x03, 0x40-0x43
                // we expect to see table ids 0x02, 0x40 and 0x41 on this PID
                // NOTE: In theory, this will break with ATSC when PID 0x10
                //       is used for ATSC/MPEG tables. This is frowned upon,
                //       but PMTs have been seen on in the wild.
                sctFilterParams.filter.filter[0] = 0x00;
                sctFilterParams.filter.mask[0]   = 0xbc;
                break;
            case 0x0011: // assume this is for an SDT, SDTo, PMT
                // This filter will give us table ids 0x02, 0x06, 0x42 and 0x46
                // All but 0x06 are ones we want to see.
                // NOTE: In theory this will break with ATSC when pid 0x11
                //       is used for random ATSC tables. In practice only
                //       video data has been seen on 0x11.
                sctFilterParams.filter.filter[0] = 0x02;
                sctFilterParams.filter.mask[0]   = 0xbb;
                break;
            case 0x1ffb: // assume this is for various ATSC tables
                // MGT 0xC7, Terrestrial VCT 0xC8, Cable VCT 0xC9, RRT 0xCA,
                // STT 0xCD, DCCT 0xD3, DCCSCT 0xD4, Caption 0x86
                sctFilterParams.filter.filter[0] = 0x80;
                sctFilterParams.filter.mask[0]   = 0xa0;
                break;
            default:
                // otherwise assume it could be any table
                sctFilterParams.filter.filter[0] = 0x00;
                sctFilterParams.filter.mask[0]   = 0x00;
                break;
        }
        sctFilterParams.pid            = (uint16_t) _pid;
        sctFilterParams.timeout        = 0;
        sctFilterParams.flags          = DMX_IMMEDIATE_START;

        if (ioctl(mux_fd, DMX_SET_FILTER, &sctFilterParams) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to set \"section\" filter " +
                QString("(pid 0x%1) (filter %2)").arg(_pid, 0, 16)
                    .arg(sctFilterParams.filter.filter[0]));
            close(mux_fd);
            return false;
        }
    }

    filter_fd = mux_fd;

    return true;
}

bool DVBPIDInfo::Close(const QString &dvb_dev)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Closing filter for pid 0x%1").arg(_pid, 0, 16));

    if (!IsOpen())
        return false;

    int tmp = filter_fd;
    filter_fd = -1;

    int err = close(tmp);
    if (err < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            LOC + QString("Failed to close mux (pid 0x%1)")
                .arg(_pid, 0, 16) + ENO);

        return false;
    }

    return true;
}

#if 0

// We don't yet do kernel buffer allocation in dvbstreamhandler..

int DVBRecorder::OpenFilterFd(uint pid, int pes_type, uint stream_type)
{
    if (_open_pid_filters >= _max_pid_filters)
        return -1;

    // bits per millisecond
    uint bpms = (StreamID::IsVideo(stream_type)) ? 19200 : 500;
    // msec of buffering we want
    uint msec_of_buffering = max(POLL_WARNING_TIMEOUT + 50, 1500);
    // actual size of buffer we need
    uint pid_buffer_size = ((bpms*msec_of_buffering + 7) / 8);
    // rounded up to the nearest page
    pid_buffer_size = ((pid_buffer_size + 4095) / 4096) * 4096;

    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Adding pid 0x%1 size(%2)")
            .arg(pid,0,16).arg(pid_buffer_size));

    // Open the demux device
    QString dvbdev = CardUtil::GetDeviceName(
        DVB_DEV_DEMUX, _card_number_option);
    QByteArray dev = dvbdev.toLatin1();

    int fd_tmp = open(dev.constData(), O_RDWR);
    if (fd_tmp < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not open demux device." + ENO);
        _max_pid_filters = _open_pid_filters;
        return -1;
    }

    // Try to make the demux buffer large enough to
    // allow for longish disk writes.
    uint sz    = pid_buffer_size;
    uint usecs = msec_of_buffering * 1000;
    while (ioctl(fd_tmp, DMX_SET_BUFFER_SIZE, sz) < 0 && sz > 1024*8)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set demux buffer size for "+
                QString("pid 0x%1 to %2").arg(pid,0,16).arg(sz) + ENO);

        sz    /= 2;
        sz     = ((sz+4095)/4096)*4096;
        usecs /= 2;
    }
#if 0
    LOG(VB_RECORD, LOG_DEBUG, LOC + "Set demux buffer size for " +
        QString("pid 0x%1 to %2,\n\t\t\twhich gives us a %3 msec buffer.")
            .arg(pid,0,16).arg(sz).arg(usecs/1000));
#endif

    // Set the filter type
    struct dmx_pes_filter_params params;
    memset(&params, 0, sizeof(params));
    params.input    = DMX_IN_FRONTEND;
    params.output   = DMX_OUT_TS_TAP;
    params.flags    = DMX_IMMEDIATE_START;
    params.pid      = pid;
    params.pes_type = (dmx_pes_type_t) pes_type;
    if (ioctl(fd_tmp, DMX_SET_PES_FILTER, &params) < 0)
    {
        close(fd_tmp);

        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set demux filter." + ENO);
        _max_pid_filters = _open_pid_filters;
        return -1;
    }

    _open_pid_filters++;
    return fd_tmp;
}
#endif
