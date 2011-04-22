// -*- Mode: c++ -*-

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#ifndef USING_MINGW
#include <sys/select.h>
#include <sys/ioctl.h>
#endif

// Qt headers
#include <QString>

// MythTV headers
#include "hdhrstreamhandler.h"
#include "hdhrchannel.h"
#include "dtvsignalmonitor.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cardutil.h"

#define LOC      QString("HDHRSH(%1): ").arg(_devicename)
#define LOC_WARN QString("HDHRSH(%1) Warning: ").arg(_devicename)
#define LOC_ERR  QString("HDHRSH(%1) Error: ").arg(_devicename)

QMap<uint,bool> HDHRStreamHandler::_rec_supports_ts_monitoring;
QMutex          HDHRStreamHandler::_rec_supports_ts_monitoring_lock;

QMap<QString,HDHRStreamHandler*> HDHRStreamHandler::_handlers;
QMap<QString,uint>               HDHRStreamHandler::_handlers_refcnt;
QMutex                           HDHRStreamHandler::_handlers_lock;

//#define DEBUG_PID_FILTERS

HDHRStreamHandler *HDHRStreamHandler::Get(const QString &devname)
{
    QMutexLocker locker(&_handlers_lock);

    QString devkey = devname.toUpper();

    QMap<QString,HDHRStreamHandler*>::iterator it = _handlers.find(devkey);

    if (it == _handlers.end())
    {
        HDHRStreamHandler *newhandler = new HDHRStreamHandler(devkey);
        newhandler->Open();
        _handlers[devkey] = newhandler;
        _handlers_refcnt[devkey] = 1;

        VERBOSE(VB_RECORD,
                QString("HDHRSH: Creating new stream handler %1 for %2")
                .arg(devkey).arg(devname));
    }
    else
    {
        _handlers_refcnt[devkey]++;
        uint rcount = _handlers_refcnt[devkey];
        VERBOSE(VB_RECORD,
                QString("HDHRSH: Using existing stream handler %1 for %2")
                .arg(devkey)
                .arg(devname) + QString(" (%1 in use)").arg(rcount));
    }

    return _handlers[devkey];
}

void HDHRStreamHandler::Return(HDHRStreamHandler * & ref)
{
    QMutexLocker locker(&_handlers_lock);

    QString devname = ref->_devicename;

    QMap<QString,uint>::iterator rit = _handlers_refcnt.find(devname);
    if (rit == _handlers_refcnt.end())
        return;

    if (*rit > 1)
    {
        ref = NULL;
        (*rit)--;
        return;
    }

    QMap<QString,HDHRStreamHandler*>::iterator it = _handlers.find(devname);
    if ((it != _handlers.end()) && (*it == ref))
    {
        VERBOSE(VB_RECORD, QString("HDHRSH: Closing handler for %1")
                           .arg(devname));
        ref->Close();
        delete *it;
        _handlers.erase(it);
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("HDHRSH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    _handlers_refcnt.erase(rit);
    ref = NULL;
}

HDHRStreamHandler::HDHRStreamHandler(const QString &devicename) :
    _hdhomerun_device(NULL),
    _tuner(-1),
    _devicename(devicename),

    _start_stop_lock(QMutex::Recursive),
    _run(false),

    _pid_lock(QMutex::Recursive),
    _open_pid_filters(0),
    _listener_lock(QMutex::Recursive),
    _hdhr_lock(QMutex::Recursive)
{
}

HDHRStreamHandler::~HDHRStreamHandler()
{
    if (!_stream_data_list.empty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "dtor & _stream_data_list not empty");
    }
}

void HDHRStreamHandler::AddListener(MPEGStreamData *data)
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

    _stream_data_list.push_back(data);

    _listener_lock.unlock();

    Start();

    VERBOSE(VB_RECORD, LOC + QString("AddListener(0x%1) -- end")
                       .arg((uint64_t)data,0,16));
}

void HDHRStreamHandler::RemoveListener(MPEGStreamData *data)
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

void HDHRReadThread::run(void)
{
    if (!m_parent)
        return;

    m_parent->Run();
}

void HDHRStreamHandler::Start(void)
{
    QMutexLocker locker(&_start_stop_lock);

    _eit_pids.clear();

    if (!IsRunning())
    {
        _run = true;
        _reader_thread.SetParent(this);
        _reader_thread.start();

        if (!_reader_thread.isRunning())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Start: Failed to create thread.");
            return;
        }
    }
}

void HDHRStreamHandler::Stop(void)
{
    QMutexLocker locker(&_start_stop_lock);

    if (IsRunning())
    {
        _run = false;
        _reader_thread.wait();
    }
}

void HDHRStreamHandler::Run(void)
{
    RunTS();
}

/** \fn HDHRStreamHandler::RunTS(void)
 *  \brief Uses TS filtering devices to read a DVB device for tables & data
 *
 *  This supports all types of MPEG based stream data, but is extreemely
 *  slow with DVB over USB 1.0 devices which for efficiency reasons buffer
 *  a stream until a full block transfer buffer full of the requested
 *  tables is available. This takes a very long time when you are just
 *  waiting for a PAT or PMT table, and the buffer is hundreds of packets
 *  in size.
 */
void HDHRStreamHandler::RunTS(void)
{
    int remainder = 0;

    /* Calculate buffer size */
    uint buffersize = gCoreContext->GetNumSetting(
        "HDRingbufferSize", 50 * TSPacket::SIZE) * 1024;
    buffersize /= VIDEO_DATA_PACKET_SIZE;
    buffersize *= VIDEO_DATA_PACKET_SIZE;

    // Buffer should be at least about 1MB..
    buffersize = max(49 * TSPacket::SIZE * 128, buffersize);

    /* Create TS socket. */
    if (!hdhomerun_device_stream_start(_hdhomerun_device))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Starting recording (set target failed). Aborting.");
        return;
    }
    hdhomerun_device_stream_flush(_hdhomerun_device);

    bool _error = false;

    VERBOSE(VB_RECORD, LOC + "RunTS(): begin");

    while (_run && !_error)
    {
        UpdateFiltersFromStreamData();

        size_t read_size = 64 * 1024; // read about 64KB
        read_size /= VIDEO_DATA_PACKET_SIZE;
        read_size *= VIDEO_DATA_PACKET_SIZE;

        size_t data_length;
        unsigned char *data_buffer = hdhomerun_device_stream_recv(
            _hdhomerun_device, read_size, &data_length);

        if (!data_buffer)
        {
            usleep(5000);
            continue;
        }

        // Assume data_length is a multiple of 188 (packet size)

        _listener_lock.lock();

        if (_stream_data_list.empty())
        {
            _listener_lock.unlock();
            continue;
        }

        for (uint i = 0; i < _stream_data_list.size(); i++)
        {
            remainder = _stream_data_list[i]->ProcessData(
                data_buffer, data_length);
        }

        _listener_lock.unlock();
        if (remainder != 0)
        {
            VERBOSE(VB_RECORD, LOC +
                    QString("RunTS(): data_length = %1 remainder = %2")
                    .arg(data_length).arg(remainder));
        }
    }
    VERBOSE(VB_RECORD, LOC + "RunTS(): " + "shutdown");

    RemoveAllPIDFilters();

    hdhomerun_device_stream_stop(_hdhomerun_device);
    VERBOSE(VB_RECORD, LOC + "RunTS(): " + "end");
}

bool HDHRStreamHandler::AddPIDFilter(uint pid, bool do_update)
{
#ifdef DEBUG_PID_FILTERS
    VERBOSE(VB_RECORD, LOC + QString("AddPIDFilter(0x%1)")
            .arg(pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker writing_locker(&_pid_lock);

    vector<uint>::iterator it;
    it = lower_bound(_pid_info.begin(), _pid_info.end(), pid);
    if (it != _pid_info.end() && *it == pid)
        return true;

    _pid_info.insert(it, pid);

    if (do_update)
        return UpdateFilters();

    return true;
}

bool HDHRStreamHandler::RemovePIDFilter(uint pid, bool do_update)
{
#ifdef DEBUG_PID_FILTERS
    VERBOSE(VB_RECORD, LOC +
            QString("RemovePIDFilter(0x%1)").arg(pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker write_locker(&_pid_lock);

    vector<uint>::iterator it;
    it = lower_bound(_pid_info.begin(), _pid_info.end(), pid);
    if ((it == _pid_info.end()) || (*it != pid))
       return false;

    _pid_info.erase(it);

    if (do_update)
        return UpdateFilters();

    return true;
}

bool HDHRStreamHandler::RemoveAllPIDFilters(void)
{
    QMutexLocker write_locker(&_pid_lock);

#ifdef DEBUG_PID_FILTERS
    VERBOSE(VB_RECORD, LOC + "RemoveAllPIDFilters()");
#endif // DEBUG_PID_FILTERS

    _pid_info.clear();

    return UpdateFilters();
}

static QString filt_str(uint pid)
{
    uint pid0 = (pid / (16*16*16)) % 16;
    uint pid1 = (pid / (16*16))    % 16;
    uint pid2 = (pid / (16))        % 16;
    uint pid3 = pid % 16;
    return QString("0x%1%2%3%4")
        .arg(pid0,0,16).arg(pid1,0,16)
        .arg(pid2,0,16).arg(pid3,0,16);
}

void HDHRStreamHandler::UpdateListeningForEIT(void)
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

bool HDHRStreamHandler::UpdateFiltersFromStreamData(void)
{
    UpdateListeningForEIT();

    pid_map_t pids;

    {
        QMutexLocker read_locker(&_listener_lock);

        for (uint i = 0; i < _stream_data_list.size(); i++)
            _stream_data_list[i]->GetPIDs(pids);
    }

    uint_vec_t           add_pids;
    vector<uint>         del_pids;

    {
        QMutexLocker read_locker(&_pid_lock);

        // PIDs that need to be added..
        pid_map_t::const_iterator lit = pids.constBegin();
        for (; lit != pids.constEnd(); ++lit)
        {
            vector<uint>::iterator it;
            it = lower_bound(_pid_info.begin(), _pid_info.end(), lit.key());
            if (it == _pid_info.end() || *it != lit.key())
                add_pids.push_back(lit.key());
        }

        // PIDs that need to be removed..
        vector<uint>::const_iterator fit = _pid_info.begin();
        for (; fit != _pid_info.end(); ++fit)
        {
            bool in_pids = pids.find(*fit) != pids.end();
            if (!in_pids)
                del_pids.push_back(*fit);
        }
    }

    bool need_update = false;

    // Remove PIDs
    bool ok = true;
    vector<uint>::iterator dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
    {
        need_update = true;
        ok &= RemovePIDFilter(*dit, false);
    }

    // Add PIDs
    vector<uint>::iterator ait = add_pids.begin();
    for (; ait != add_pids.end(); ++ait)
    {
        need_update = true;
        ok &= AddPIDFilter(*ait, false);
    }

    if (need_update)
        return UpdateFilters() && ok;

    return ok;
}

bool HDHRStreamHandler::UpdateFilters(void)
{
    if (_tune_mode == hdhrTuneModeFrequency)
        _tune_mode = hdhrTuneModeFrequencyPid;

    if (_tune_mode != hdhrTuneModeFrequencyPid)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "UpdateFilters called in wrong tune mode");
        return false;
    }

#ifdef DEBUG_PID_FILTERS
    VERBOSE(VB_RECORD, LOC + "UpdateFilters()");
#endif // DEBUG_PID_FILTERS
    QMutexLocker locker(&_pid_lock);

    QString filter = "";

    vector<uint> range_min;
    vector<uint> range_max;

    for (uint i = 0; i < _pid_info.size(); i++)
    {
        uint pid_min = _pid_info[i];
        uint pid_max  = pid_min;
        for (uint j = i + 1; j < _pid_info.size(); j++)
        {
            if (pid_max + 1 != _pid_info[j])
                break;
            pid_max++;
            i++;
        }
        range_min.push_back(pid_min);
        range_max.push_back(pid_max);
    }
    if (range_min.size() > 16)
    {
        range_min.resize(16);
        uint pid_max = range_max.back();
        range_max.resize(15);
        range_max.push_back(pid_max);
    }

    for (uint i = 0; i < range_min.size(); i++)
    {
        filter += filt_str(range_min[i]);
        if (range_min[i] != range_max[i])
            filter += QString("-%1").arg(filt_str(range_max[i]));
        filter += " ";
    }

    filter = filter.trimmed();

    QString new_filter = TunerSet("filter", filter);

#ifdef DEBUG_PID_FILTERS
    QString msg = QString("Filter: '%1'").arg(filter);
    if (filter != new_filter)
        msg += QString("\n\t\t\t\t'%2'").arg(new_filter);

    VERBOSE(VB_RECORD, LOC + msg);
#endif // DEBUG_PID_FILTERS

    return filter == new_filter;
}

PIDPriority HDHRStreamHandler::GetPIDPriority(uint pid) const
{
    QMutexLocker reading_locker(&_listener_lock);

    PIDPriority tmp = kPIDPriorityNone;

    for (uint i = 0; i < _stream_data_list.size(); i++)
        tmp = max(tmp, _stream_data_list[i]->GetPIDPriority(pid));

    return tmp;
}

bool HDHRStreamHandler::Open(void)
{
    if (Connect())
    {
        const char *model = hdhomerun_device_get_model_str(_hdhomerun_device);
        _tuner_types.clear();
        if (QString(model).toLower().contains("dvb"))
        {
            _tuner_types.push_back(DTVTunerType::kTunerTypeDVBT);
            _tuner_types.push_back(DTVTunerType::kTunerTypeDVBC);
        }
        else
        {
            _tuner_types.push_back(DTVTunerType::kTunerTypeATSC);
        }

        return true;
    }
    return false;
}

void HDHRStreamHandler::Close(void)
{
    if (_hdhomerun_device)
    {
        TuneChannel("none");
        hdhomerun_device_destroy(_hdhomerun_device);
        _hdhomerun_device = NULL;
    }
}

bool HDHRStreamHandler::Connect(void)
{
    _hdhomerun_device = hdhomerun_device_create_from_str(
        _devicename.toLocal8Bit().constData(), NULL);

    if (!_hdhomerun_device)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to create hdhomerun object");
        return false;
    }

    _tuner = hdhomerun_device_get_tuner(_hdhomerun_device);

    if (hdhomerun_device_get_local_machine_addr(_hdhomerun_device) == 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to connect to device");
        return false;
    }

    VERBOSE(VB_RECORD, LOC + "Successfully connected to device");
    return true;
}

bool HDHRStreamHandler::EnterPowerSavingMode(void)
{
    QMutexLocker locker(&_listener_lock);

    if (!_stream_data_list.empty())
    {
        VERBOSE(VB_RECORD, LOC + "Ignoring request - video streaming active");
        return false;
    }
    else
    {
        locker.unlock(); // _listener_lock
        return TuneChannel("none");
    }
}

QString HDHRStreamHandler::TunerGet(
    const QString &name, bool report_error_return, bool print_error) const
{
    QMutexLocker locker(&_hdhr_lock);

    if (!_hdhomerun_device)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Get request failed (not connected)");
        return QString::null;
    }

    QString valname = QString("/tuner%1/%2").arg(_tuner).arg(name);
    char *value = NULL;
    char *error = NULL;
    if (hdhomerun_device_get_var(
            _hdhomerun_device, valname.toLocal8Bit().constData(),
            &value, &error) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Get request failed" + ENO);
        return QString::null;
    }

    if (report_error_return && error)
    {
        if (print_error)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("DeviceGet(%1): %2").arg(name).arg(error));
        }

        return QString::null;
    }

    return QString(value);
}

QString HDHRStreamHandler::TunerSet(
    const QString &name, const QString &val,
    bool report_error_return, bool print_error)
{
    QMutexLocker locker(&_hdhr_lock);

    if (!_hdhomerun_device)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Set request failed (not connected)");
        return QString::null;
    }


    QString valname = QString("/tuner%1/%2").arg(_tuner).arg(name);
    char *value = NULL;
    char *error = NULL;

    if (hdhomerun_device_set_var(
            _hdhomerun_device, valname.toLocal8Bit().constData(),
            val.toLocal8Bit().constData(), &value, &error) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Set request failed" + ENO);

        return QString::null;
    }

    // Database modulation strings and HDHR use different syntax.
    // HACK!! Caller should be doing this. (e.g. auto in HDHRChannel::Tune())
    //
    if (error && name == QString("channel") && val.contains("qam_"))
    {
        QString newval = val;
        newval.replace("qam_256", "qam");
        newval.replace("qam_64", "qam");
        VERBOSE(VB_CHANNEL, "HDHRSH::TunerSet() Failed. Trying " + newval);
        return TunerSet(name, newval, report_error_return, print_error);
    }

    if (report_error_return && error)
    {
        if (print_error)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("DeviceSet(%1 %2): %3")
                    .arg(name).arg(val).arg(error));
        }

        return QString::null;
    }

    return QString(value);
}

void HDHRStreamHandler::GetTunerStatus(struct hdhomerun_tuner_status_t *status)
{
    hdhomerun_device_get_tuner_status(_hdhomerun_device, NULL, status);
}

bool HDHRStreamHandler::IsConnected(void) const
{
    return (_hdhomerun_device != NULL);
}

bool HDHRStreamHandler::TuneChannel(const QString &chn)
{
    _tune_mode = hdhrTuneModeFrequency;

    QString current = TunerGet("channel");
    if (current == chn)
    {
        VERBOSE(VB_RECORD, QString(LOC + "Not Re-Tuning channel %1").arg(chn));
        return true;
    }

    VERBOSE(VB_RECORD, QString(LOC + "Tuning channel %1 (was %2)")
            .arg(chn).arg(current));
    return !TunerSet("channel", chn).isEmpty();
}

bool HDHRStreamHandler::TuneProgram(uint mpeg_prog_num)
{
    if (_tune_mode == hdhrTuneModeFrequency)
        _tune_mode = hdhrTuneModeFrequencyProgram;

    if (_tune_mode != hdhrTuneModeFrequencyProgram)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "TuneProgram called in wrong tune mode");
        return false;
    }

    VERBOSE(VB_RECORD, QString(LOC + "Tuning program %1").arg(mpeg_prog_num));
    return !TunerSet(
        "program", QString::number(mpeg_prog_num), false).isEmpty();
}

bool HDHRStreamHandler::TuneVChannel(const QString &vchn)
{
    _tune_mode = hdhrTuneModeVChannel;

    VERBOSE(VB_RECORD, QString(LOC + "Tuning vchannel %1").arg(vchn));
    return !TunerSet(
        "vchannel", vchn).isEmpty();
}
