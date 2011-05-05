// -*- Mode: c++ -*-

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>

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

#define LOC      QString("DVBSH(%1): ").arg(_dvb_dev)
#define LOC_WARN QString("DVBSH(%1) Warning: ").arg(_dvb_dev)
#define LOC_ERR  QString("DVBSH(%1) Error: ").arg(_dvb_dev)

QMap<QString,bool> DVBStreamHandler::_rec_supports_ts_monitoring;
QMutex             DVBStreamHandler::_rec_supports_ts_monitoring_lock;

QMap<QString,DVBStreamHandler*> DVBStreamHandler::_handlers;
QMap<QString,uint>              DVBStreamHandler::_handlers_refcnt;
QMutex                          DVBStreamHandler::_handlers_lock;

//#define DEBUG_PID_FILTERS

DVBStreamHandler *DVBStreamHandler::Get(const QString &devname)
{
    QMutexLocker locker(&_handlers_lock);

    QMap<QString,DVBStreamHandler*>::iterator it =
        _handlers.find(devname);

    if (it == _handlers.end())
    {
        _handlers[devname] = new DVBStreamHandler(devname);
        _handlers_refcnt[devname] = 1;
    }
    else
    {
        _handlers_refcnt[devname]++;
    }

    return _handlers[devname];
}

void DVBStreamHandler::Return(DVBStreamHandler * & ref)
{
    QMutexLocker locker(&_handlers_lock);

    QString devname = ref->_dvb_dev;

    QMap<QString,uint>::iterator rit = _handlers_refcnt.find(devname);
    if (rit == _handlers_refcnt.end())
        return;

    if (*rit > 1)
    {
        ref = NULL;
        (*rit)--;
        return;
    }

    QMap<QString,DVBStreamHandler*>::iterator it = _handlers.find(devname);
    if ((it != _handlers.end()) && (*it == ref))
    {
        delete *it;
        _handlers.erase(it);
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("DVBSH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    _handlers_refcnt.erase(rit);
    ref = NULL;
}

DVBStreamHandler::DVBStreamHandler(const QString &dvb_device) :
    _dvb_dev(dvb_device),
    _dvr_dev_path(CardUtil::GetDeviceName(DVB_DEV_DVR, _dvb_dev)),
    _allow_section_reader(false),
    _needs_buffering(false),
    _allow_retune(false),

    _start_stop_lock(QMutex::Recursive),
    _using_section_reader(false),

    _device_read_buffer(NULL),
    _sigmon(NULL),
    _dvbchannel(NULL),

    _pid_lock(QMutex::Recursive),
    _open_pid_filters(0),
    _listener_lock(QMutex::Recursive),
    _run(false)
{
}

DVBStreamHandler::~DVBStreamHandler()
{
    if (!_stream_data_list.empty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "dtor & _stream_data_list not empty");
    }
}

void DVBStreamHandler::AddListener(MPEGStreamData *data,
                                   bool allow_section_reader,
                                   bool needs_buffering)
{
    VERBOSE(VB_RECORD, LOC + QString("AddListener(0x%1) -- begin")
                       .arg((uint64_t)data,0,16));
    if (!data)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("AddListener(0x%1) -- null data")
                .arg((uint64_t)data,0,16));
        return;
    }

    _listener_lock.lock();

    VERBOSE(VB_RECORD, LOC + QString("AddListener(0x%1) -- locked")
                       .arg((uint64_t)data,0,16));

    if (_stream_data_list.empty())
    {
        _allow_section_reader = allow_section_reader;
        _needs_buffering      = needs_buffering;
    }
    else
    {
        _allow_section_reader &= allow_section_reader;
        _needs_buffering      |= needs_buffering;
    }

    _stream_data_list.push_back(data);

    _listener_lock.unlock();

    Start();

    VERBOSE(VB_RECORD, LOC + QString("AddListener(0x%1) -- end")
                       .arg((uint64_t)data,0,16));
}

void DVBStreamHandler::RemoveListener(MPEGStreamData *data)
{
    VERBOSE(VB_RECORD, LOC + QString("RemoveListener(0x%1) -- begin")
                       .arg((uint64_t)data,0,16));
    if (!data)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("RemoveListener(0x%1) -- null data")
                .arg((uint64_t)data,0,16));
        return;
    }

    _listener_lock.lock();

    VERBOSE(VB_RECORD, LOC + QString("RemoveListener(0x%1) -- locked")
                       .arg((uint64_t)data,0,16));

    vector<MPEGStreamData*>::iterator it =
        find(_stream_data_list.begin(), _stream_data_list.end(), data);

    if (it != _stream_data_list.end())
        _stream_data_list.erase(it);

    if (_stream_data_list.empty())
    {
        _allow_section_reader = false;

        _listener_lock.unlock();
        Stop();
    }
    else
    {
        _listener_lock.unlock();
    }

    VERBOSE(VB_RECORD, LOC + QString("RemoveListener(0x%1) -- end")
                       .arg((uint64_t)data,0,16));
}

void DVBReadThread::run(void)
{
    if (!m_parent)
        return;

    threadRegister("DVBRead");
    m_parent->Run();
    threadDeregister();
}

void DVBStreamHandler::Start(void)
{
    QMutexLocker locker(&_start_stop_lock);

    _eit_pids.clear();

    if (IsRunning() && _using_section_reader && !_allow_section_reader)
        Stop();

    if (IsRunning() && _needs_buffering && !_device_read_buffer)
        Stop();

    if (!IsRunning())
    {
        _reader_thread.SetParent(this);
        _reader_thread.start();

        if (!_reader_thread.isRunning())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Start: Failed to create thread.");
            return;
        }
    }
}

void DVBStreamHandler::Stop(void)
{
    QMutexLocker locker(&_start_stop_lock);

    if (IsRunning())
    {
        if (_device_read_buffer)
            _device_read_buffer->Stop();
        _run = false;
        _reader_thread.wait();
    }
}

void DVBStreamHandler::Run(void)
{
    _using_section_reader = !SupportsTSMonitoring() && _allow_section_reader;
    _run = true;

    if (_using_section_reader)
        RunSR();
    else
        RunTS();
}

/** \fn DVBStreamHandler::RunTS(void)
 *  \brief Uses TS filtering devices to read a DVB device for tables & data
 *
 *  This supports all types of MPEG based stream data, but is extreemely
 *  slow with DVB over USB 1.0 devices which for efficiency reasons buffer
 *  a stream until a full block transfer buffer full of the requested
 *  tables is available. This takes a very long time when you are just
 *  waiting for a PAT or PMT table, and the buffer is hundreds of packets
 *  in size.
 */
void DVBStreamHandler::RunTS(void)
{
    if (_needs_buffering)
        _device_read_buffer = new DeviceReadBuffer(this);

    int remainder = 0;
    int buffer_size = TSPacket::kSize * 15000;
    unsigned char *buffer = new unsigned char[buffer_size];
    if (!buffer)
        return;
    memset(buffer, 0, buffer_size);

    QByteArray dvr_dev_path = _dvr_dev_path.toAscii();
    int dvr_fd;
    for (int tries = 1; ; ++tries)
    {
        dvr_fd = open(dvr_dev_path.constData(), O_RDONLY | O_NONBLOCK);
        if (dvr_fd >= 0)
            break;
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Opening DVR device %1 failed : %2")
                .arg(_dvr_dev_path).arg(strerror(errno)));
        if (tries >= 20 || (errno != EBUSY && errno != EAGAIN))
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("Failed to open DVR device %1 : %2")
                    .arg(_dvr_dev_path).arg(strerror(errno)));
            delete[] buffer;
            return;
        }
        usleep(50000);
    }

    bool _error = false;
    if (_device_read_buffer)
    {
        bool ok = _device_read_buffer->Setup(_dvb_dev, dvr_fd);

        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate DRB buffer");
            _error = true;
            delete[] buffer;
            return;
        }

        _device_read_buffer->Start();
    }

    VERBOSE(VB_RECORD, LOC + "RunTS(): begin");

    fd_set fd_select_set;
    FD_ZERO(        &fd_select_set);
    FD_SET (dvr_fd, &fd_select_set);
    while (_run && !_error)
    {
        RetuneMonitor();
        UpdateFiltersFromStreamData();

        ssize_t len = 0;

        if (_device_read_buffer)
        {
            len = _device_read_buffer->Read(
                &(buffer[remainder]), buffer_size - remainder);

            // Check for DRB errors
            if (_device_read_buffer->IsErrored())
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Device error detected");
                _error = true;
            }

            if (_device_read_buffer->IsEOF())
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Device EOF detected");
                _error = true;
            }
        }
        else
        {
            // timeout gets reset by select, so we need to create new one
            struct timeval timeout = { 0, 50 /* ms */ * 1000 /* -> usec */ };
            int ret = select(dvr_fd+1, &fd_select_set, NULL, NULL, &timeout);
            if (ret == -1 && errno != EINTR)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "select() failed" + ENO);
            }
            else
            {
                len = read(dvr_fd, &(buffer[remainder]),
                           buffer_size - remainder);
            }
        }

        if ((0 == len) || (-1 == len))
        {
            usleep(100);
            continue;
        }

        len += remainder;

        if (len < 10) // 10 bytes = 4 bytes TS header + 6 bytes PES header
        {
            remainder = len;
            continue;
        }

        _listener_lock.lock();

        if (_stream_data_list.empty())
        {
            _listener_lock.unlock();
            continue;
        }

        for (uint i = 0; i < _stream_data_list.size(); i++)
        {
            remainder = _stream_data_list[i]->ProcessData(buffer, len);
        }

        _listener_lock.unlock();

        if (remainder > 0 && (len > remainder)) // leftover bytes
            memmove(buffer, &(buffer[len - remainder]), remainder);
    }
    VERBOSE(VB_RECORD, LOC + "RunTS(): " + "shutdown");

    RemoveAllPIDFilters();

    if (_device_read_buffer)
    {
        if (_device_read_buffer->IsRunning())
            _device_read_buffer->Stop();

        delete _device_read_buffer;
        _device_read_buffer = NULL;
    }

    close(dvr_fd);
    delete[] buffer;

    VERBOSE(VB_RECORD, LOC + "RunTS(): " + "end");
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
    unsigned char *buffer = new unsigned char[buffer_size];
    if (!buffer)
        return;

    VERBOSE(VB_RECORD, LOC + "RunSR(): begin");

    while (_run)
    {
        RetuneMonitor();
        UpdateFiltersFromStreamData();

        QMutexLocker read_locker(&_pid_lock);

        bool readSomething = false;
        PIDInfoMap::const_iterator fit = _pid_info.begin();
        for (; fit != _pid_info.end(); ++fit)
        {
            int len = read((*fit)->filter_fd, buffer, buffer_size);
            if (len <= 0)
                continue;

            readSomething = true;

            const PESPacket pes = PESPacket::ViewData(buffer);
            const PSIPTable psip(pes);

            if (psip.SectionSyntaxIndicator())
            {
                _listener_lock.lock();
                for (uint i = 0; i < _stream_data_list.size(); i++)
                {
                    _stream_data_list[i]->HandleTables(
                        fit.key() /* pid */, psip);
                }
                _listener_lock.unlock();
            }
        }

        if (!readSomething)
            usleep(3000);
    }
    VERBOSE(VB_RECORD, LOC + "RunSR(): " + "shutdown");

    RemoveAllPIDFilters();

    delete[] buffer;

    VERBOSE(VB_RECORD, LOC + "RunSR(): " + "end");
}

bool DVBStreamHandler::AddPIDFilter(PIDInfo *info)
{
#ifdef DEBUG_PID_FILTERS
    VERBOSE(VB_RECORD, LOC + QString("AddPIDFilter(0x%1) priority %2")
            .arg(info->_pid, 0, 16).arg(GetPIDPriority(info->_pid)));
#endif // DEBUG_PID_FILTERS

    QMutexLocker writing_locker(&_pid_lock);
    _pid_info[info->_pid] = info;

    CycleFiltersByPriority();

    return true;
}

typedef vector<uint> pid_list_t;

static pid_list_t::iterator find(
    const PIDInfoMap &map,
    pid_list_t &list,
    pid_list_t::iterator begin,
    pid_list_t::iterator end, bool find_open)
{
    pid_list_t::iterator it;
    for (it = begin; it != end; it++)
    {
        PIDInfoMap::const_iterator mit = map.find(*it);
        if ((mit != map.end()) && ((*mit)->IsOpen() == find_open))
            return it;
    }

    for (it = list.begin(); it != begin; it++)
    {
        PIDInfoMap::const_iterator mit = map.find(*it);
        if ((mit != map.end()) && ((*mit)->IsOpen() == find_open))
            return it;
    }

    return list.end();
}

void DVBStreamHandler::CycleFiltersByPriority(void)
{
    QMutexLocker writing_locker(&_pid_lock);
    QMap<PIDPriority, pid_list_t> priority_queue;
    QMap<PIDPriority, uint> priority_open_cnt;

    PIDInfoMap::const_iterator cit = _pid_info.begin();
    for (; cit != _pid_info.end(); ++cit)
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
                _pid_info, priority_queue[i],
                priority_queue[i].begin(), priority_queue[i].end(), true);
            if (open == priority_queue[i].end())
                open = priority_queue[i].begin();

            pid_list_t::iterator closed = find(
                _pid_info, priority_queue[i],
                open, priority_queue[i].end(), false);

            if (closed == priority_queue[i].end())
                break; // something is broken

            if (_pid_info[*closed]->Open(_dvb_dev, _using_section_reader))
            {
                _open_pid_filters++;
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
                    PIDInfo *info = _pid_info[priority_queue[j][k]];
                    if (!info->IsOpen())
                        continue;

                    if (info->Close(_dvb_dev))
                        freed = true;

                    _open_pid_filters--;
                    priority_open_cnt[j]--;
                }
            }

            if (freed)
            {
                // if we can open a filter, just do it
                if (_pid_info[*closed]->Open(
                        _dvb_dev, _using_section_reader))
                {
                    _open_pid_filters++;
                    priority_open_cnt[i]++;
                    continue;
                }
            }

            // we have to cycle within our priority level

            if (_cycle_timer.elapsed() < 1000)
                break; // we don't want to cycle too often

            if (!_pid_info[*open]->IsOpen())
                break; // nothing to close..

            // close "open"
            bool ok = _pid_info[*open]->Close(_dvb_dev);
            _open_pid_filters--;
            priority_open_cnt[i]--;

            // open "closed"
            if (ok && _pid_info[*closed]->
                Open(_dvb_dev, _using_section_reader))
            {
                _open_pid_filters++;
                priority_open_cnt[i]++;
            }

            break; // we only want to cycle once per priority per run
        }
    }

    _cycle_timer.start();
}

bool DVBStreamHandler::RemovePIDFilter(uint pid)
{
#ifdef DEBUG_PID_FILTERS
    VERBOSE(VB_RECORD, LOC +
            QString("RemovePIDFilter(0x%1)").arg(pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker write_locker(&_pid_lock);

    PIDInfoMap::iterator it = _pid_info.find(pid);
    if (it == _pid_info.end())
        return false;

    PIDInfo *tmp = *it;
    _pid_info.erase(it);

    bool ok = true;
    if (tmp->IsOpen())
    {
        ok = tmp->Close(_dvb_dev);
        _open_pid_filters--;

        CycleFiltersByPriority();
    }

    delete tmp;

    return ok;
}

bool DVBStreamHandler::RemoveAllPIDFilters(void)
{
    QMutexLocker write_locker(&_pid_lock);

#ifdef DEBUG_PID_FILTERS
    VERBOSE(VB_RECORD, LOC + "RemoveAllPIDFilters()");
#endif // DEBUG_PID_FILTERS

    vector<int> del_pids;
    PIDInfoMap::iterator it = _pid_info.begin();
    for (; it != _pid_info.end(); ++it)
        del_pids.push_back(it.key());

    bool ok = true;
    vector<int>::iterator dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
        ok &= RemovePIDFilter(*dit);

    return ok;
}

void DVBStreamHandler::UpdateListeningForEIT(void)
{
    vector<uint> add_eit, del_eit;

    QMutexLocker read_locker(&_listener_lock);

    for (uint i = 0; i < _stream_data_list.size(); i++)
    {
        MPEGStreamData *sd = _stream_data_list[i];
        if (sd->HasEITPIDChanges(_eit_pids) &&
            sd->GetEITPIDChanges(_eit_pids, add_eit, del_eit))
        {
            for (uint i = 0; i < del_eit.size(); i++)
            {
                uint_vec_t::iterator it;
                it = find(_eit_pids.begin(), _eit_pids.end(), del_eit[i]);
                if (it != _eit_pids.end())
                    _eit_pids.erase(it);
                sd->RemoveListeningPID(del_eit[i]);
            }

            for (uint i = 0; i < add_eit.size(); i++)
            {
                _eit_pids.push_back(add_eit[i]);
                sd->AddListeningPID(add_eit[i]);
            }
        }
    }
}

bool DVBStreamHandler::UpdateFiltersFromStreamData(void)
{
    UpdateListeningForEIT();

    pid_map_t pids;

    {
        QMutexLocker read_locker(&_listener_lock);

        for (uint i = 0; i < _stream_data_list.size(); i++)
            _stream_data_list[i]->GetPIDs(pids);
    }

    QMap<uint, PIDInfo*> add_pids;
    vector<uint>         del_pids;

    {
        QMutexLocker read_locker(&_pid_lock);

        // PIDs that need to be added..
        pid_map_t::const_iterator lit = pids.constBegin();
        for (; lit != pids.constEnd(); ++lit)
        {
            if (*lit && (_pid_info.find(lit.key()) == _pid_info.end()))
            {
                add_pids[lit.key()] = new PIDInfo(
                    lit.key(), StreamID::PrivSec,  DMX_PES_OTHER);
            }
        }

        // PIDs that need to be removed..
        PIDInfoMap::const_iterator fit = _pid_info.begin();
        for (; fit != _pid_info.end(); ++fit)
        {
            bool in_pids = pids.find(fit.key()) != pids.end();
            if (!in_pids)
                del_pids.push_back(fit.key());
        }
    }

    // Remove PIDs
    bool ok = true;
    vector<uint>::iterator dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
        ok &= RemovePIDFilter(*dit);

    // Add PIDs
    QMap<uint, PIDInfo*>::iterator ait = add_pids.begin();
    for (; ait != add_pids.end(); ++ait)
        ok &= AddPIDFilter(*ait);

    // Cycle filters if it's been a while
    if (_cycle_timer.elapsed() > 1000)
        CycleFiltersByPriority();

    return ok;
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
        _sigmon       = NULL;
        _dvbchannel   = NULL;
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
                VERBOSE(VB_CHANNEL, LOC + "Retuning for rotor completion");
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
        QMutexLocker locker(&_rec_supports_ts_monitoring_lock);
        QMap<QString,bool>::const_iterator it;
        it = _rec_supports_ts_monitoring.find(_dvb_dev);
        if (it != _rec_supports_ts_monitoring.end())
            return *it;
    }

    QByteArray dvr_dev_path = _dvr_dev_path.toAscii();
    int dvr_fd = open(dvr_dev_path.constData(), O_RDONLY | O_NONBLOCK);
    if (dvr_fd < 0)
    {
        QMutexLocker locker(&_rec_supports_ts_monitoring_lock);
        _rec_supports_ts_monitoring[_dvb_dev] = false;
        return false;
    }

    bool supports_ts = false;
    if (AddPIDFilter(new PIDInfo(pat_pid)))
    {
        supports_ts = true;
        RemovePIDFilter(pat_pid);
    }

    close(dvr_fd);

    QMutexLocker locker(&_rec_supports_ts_monitoring_lock);
    _rec_supports_ts_monitoring[_dvb_dev] = supports_ts;

    return supports_ts;
}

#undef LOC
#undef LOC_WARN
#undef LOC_ERR

#define LOC      QString("PIDInfo(%1): ").arg(dvb_dev)
#define LOC_WARN QString("PIDInfo(%1) Warning: ").arg(dvb_dev)
#define LOC_ERR  QString("PIDInfo(%1) Error: ").arg(dvb_dev)

bool PIDInfo::Open(const QString &dvb_dev, bool use_section_reader)
{
    if (filter_fd >= 0)
    {
        close(filter_fd);
        filter_fd = -1;
    }

    QString demux_fn = CardUtil::GetDeviceName(DVB_DEV_DEMUX, dvb_dev);
    QByteArray demux_ba = demux_fn.toAscii();

    VERBOSE(VB_RECORD, LOC + QString("Opening filter for pid 0x%1")
            .arg(_pid, 0, 16));

    int mux_fd = open(demux_ba.constData(), O_RDWR | O_NONBLOCK);
    if (mux_fd == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Failed to open demux device %1 "
                        "for filter on pid 0x%2")
                .arg(demux_fn).arg(_pid, 0, 16));
        return false;
    }

    if (!use_section_reader)
    {
        struct dmx_pes_filter_params pesFilterParams;
        memset(&pesFilterParams, 0, sizeof(struct dmx_pes_filter_params));
        pesFilterParams.pid      = (__u16) _pid;
        pesFilterParams.input    = DMX_IN_FRONTEND;
        pesFilterParams.output   = DMX_OUT_TS_TAP;
        pesFilterParams.flags    = DMX_IMMEDIATE_START;
        pesFilterParams.pes_type = DMX_PES_OTHER;

        if (ioctl(mux_fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Failed to set TS filter (pid 0x%1)")
                    .arg(_pid, 0, 16));

            close(mux_fd);
            return false;
        }
    }
    else
    {
        struct dmx_sct_filter_params sctFilterParams;
        memset(&sctFilterParams, 0, sizeof(struct dmx_sct_filter_params));
        switch ( (__u16) _pid )
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
        sctFilterParams.pid            = (__u16) _pid;
        sctFilterParams.timeout        = 0;
        sctFilterParams.flags          = DMX_IMMEDIATE_START;

        if (ioctl(mux_fd, DMX_SET_FILTER, &sctFilterParams) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
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

bool PIDInfo::Close(const QString &dvb_dev)
{
    VERBOSE(VB_RECORD, LOC +
            QString("Closing filter for pid 0x%1").arg(_pid, 0, 16));

    if (!IsOpen())
        return false;

    int tmp = filter_fd;
    filter_fd = -1;

    int err = close(tmp);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("Failed to close mux (pid 0x%1)")
                .arg(_pid, 0, 16) + ENO);

        return false;
    }

    return true;
}

PIDPriority DVBStreamHandler::GetPIDPriority(uint pid) const
{
    QMutexLocker reading_locker(&_listener_lock);

    PIDPriority tmp = kPIDPriorityNone;

    for (uint i = 0; i < _stream_data_list.size(); i++)
        tmp = max(tmp, _stream_data_list[i]->GetPIDPriority(pid));

    return tmp;
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

    VERBOSE(VB_RECORD, LOC + QString("Adding pid 0x%1 size(%2)")
            .arg(pid,0,16).arg(pid_buffer_size));

    // Open the demux device
    QString dvbdev = CardUtil::GetDeviceName(
        DVB_DEV_DEMUX, _card_number_option);
    QByteArray dev = dvbdev.toAscii();

    int fd_tmp = open(dev.constData(), O_RDWR);
    if (fd_tmp < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not open demux device." + ENO);
        _max_pid_filters = _open_pid_filters;
        return -1;
    }

    // Try to make the demux buffer large enough to
    // allow for longish disk writes.
    uint sz    = pid_buffer_size;
    uint usecs = msec_of_buffering * 1000;
    while (ioctl(fd_tmp, DMX_SET_BUFFER_SIZE, sz) < 0 && sz > 1024*8)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to set demux buffer size for "+
                QString("pid 0x%1 to %2").arg(pid,0,16).arg(sz) + ENO);

        sz    /= 2;
        sz     = ((sz+4095)/4096)*4096;
        usecs /= 2;
    }
    /*
    VERBOSE(VB_RECORD, LOC + "Set demux buffer size for " +
            QString("pid 0x%1 to %2,\n\t\t\twhich gives us a %3 msec buffer.")
            .arg(pid,0,16).arg(sz).arg(usecs/1000));
    */

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

        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to set demux filter." + ENO);
        _max_pid_filters = _open_pid_filters;
        return -1;
    }

    _open_pid_filters++;
    return fd_tmp;
}
#endif

