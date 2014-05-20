// -*- Mode: c++ -*-

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/ioctl.h>
#endif

// MythTV headers
#include "hdhrstreamhandler.h"
#include "hdhrchannel.h"
#include "dtvsignalmonitor.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cardutil.h"
#include "mythlogging.h"

#define LOC      QString("HDHRSH(%1): ").arg(_device)

QMap<QString,HDHRStreamHandler*> HDHRStreamHandler::_handlers;
QMap<QString,uint>               HDHRStreamHandler::_handlers_refcnt;
QMutex                           HDHRStreamHandler::_handlers_lock;

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

        LOG(VB_RECORD, LOG_INFO,
            QString("HDHRSH: Creating new stream handler %1 for %2")
                .arg(devkey).arg(devname));
    }
    else
    {
        _handlers_refcnt[devkey]++;
        uint rcount = _handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("HDHRSH: Using existing stream handler %1 for %2")
                .arg(devkey)
                .arg(devname) + QString(" (%1 in use)").arg(rcount));
    }

    return _handlers[devkey];
}

void HDHRStreamHandler::Return(HDHRStreamHandler * & ref)
{
    QMutexLocker locker(&_handlers_lock);

    QString devname = ref->_device;

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
        LOG(VB_RECORD, LOG_INFO, QString("HDHRSH: Closing handler for %1")
                           .arg(devname));
        ref->Close();
        delete *it;
        _handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HDHRSH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    _handlers_refcnt.erase(rit);
    ref = NULL;
}

HDHRStreamHandler::HDHRStreamHandler(const QString &device) :
    StreamHandler(device),
    _hdhomerun_device(NULL),
    _tuner(-1),
    _tune_mode(hdhrTuneModeNone),
    _hdhr_lock(QMutex::Recursive)
{
    setObjectName("HDHRStreamHandler");
}

/** \fn HDHRStreamHandler::run(void)
 *  \brief Reads HDHomeRun socket for tables & data
 */
void HDHRStreamHandler::run(void)
{
    RunProlog();
    /* Create TS socket. */
    if (!hdhomerun_device_stream_start(_hdhomerun_device))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Starting recording (set target failed). Aborting.");
        _error = true;
        RunEpilog();
        return;
    }
    hdhomerun_device_stream_flush(_hdhomerun_device);

    SetRunning(true, false, false);

    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): begin");

    int remainder = 0;
    QTime last_update;
    while (_running_desired && !_error)
    {
        int elapsed = !last_update.isValid() ? -1 : last_update.elapsed();
        elapsed = (elapsed < 0) ? 1000 : elapsed;
        if (elapsed > 100)
        {
            UpdateFiltersFromStreamData();
            if (_tune_mode != hdhrTuneModeVChannel)
                UpdateFilters();
            last_update.restart();
        }

        size_t read_size = 64 * 1024; // read about 64KB
        read_size /= VIDEO_DATA_PACKET_SIZE;
        read_size *= VIDEO_DATA_PACKET_SIZE;

        size_t data_length;
        unsigned char *data_buffer = hdhomerun_device_stream_recv(
            _hdhomerun_device, read_size, &data_length);

        if (!data_buffer)
        {
            usleep(20000);
            continue;
        }

        // Assume data_length is a multiple of 188 (packet size)

        _listener_lock.lock();

        if (_stream_data_list.empty())
        {
            _listener_lock.unlock();
            continue;
        }

        StreamDataList::const_iterator sit = _stream_data_list.begin();
        for (; sit != _stream_data_list.end(); ++sit)
            remainder = sit.key()->ProcessData(data_buffer, data_length);

        WriteMPTS(data_buffer, data_length - remainder);

        _listener_lock.unlock();
        if (remainder != 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("RunTS(): data_length = %1 remainder = %2")
                    .arg(data_length).arg(remainder));
        }
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): " + "shutdown");

    RemoveAllPIDFilters();

    hdhomerun_device_stream_stop(_hdhomerun_device);
    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): " + "end");

    SetRunning(false, false, false);
    RunEpilog();
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

bool HDHRStreamHandler::UpdateFilters(void)
{
    if (_tune_mode == hdhrTuneModeFrequency)
        _tune_mode = hdhrTuneModeFrequencyPid;

    if (_tune_mode != hdhrTuneModeFrequencyPid)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "UpdateFilters called in wrong tune mode");
        return false;
    }

#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_INFO, LOC + "UpdateFilters()");
#endif // DEBUG_PID_FILTERS
    QMutexLocker locker(&_pid_lock);

    QString filter = "";

    vector<uint> range_min;
    vector<uint> range_max;

    PIDInfoMap::const_iterator it = _pid_info.begin();
    for (; it != _pid_info.end(); ++it)
    {
        range_min.push_back(it.key());
        PIDInfoMap::const_iterator eit = it;
        for (++eit;
             (eit != _pid_info.end()) && (it.key() + 1 == eit.key());
             ++it, ++eit);
        range_max.push_back(it.key());
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

    LOG(VB_RECORD, LOG_INFO, LOC + msg);
#endif // DEBUG_PID_FILTERS

    return filter == new_filter;
}

bool HDHRStreamHandler::Open(void)
{
    if (Connect())
    {
        const char *model = hdhomerun_device_get_model_str(_hdhomerun_device);
        _tuner_types.clear();
        if (QString(model).toLower().contains("cablecard"))
        {
            QString status_channel = "none";
            hdhomerun_tuner_status_t t_status;

            if (hdhomerun_device_get_oob_status(
                    _hdhomerun_device, NULL, &t_status) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to query Cable card OOB channel");
            }
            else
            {
                status_channel = QString(t_status.channel);
                LOG(VB_RECORD, LOG_INFO, LOC +
                    QString("Cable card OOB channel is '%1'")
                    .arg(status_channel));
            }

            if (status_channel ==  "none")
            {
                LOG(VB_RECORD, LOG_INFO, LOC + "Cable card is not present");
                _tuner_types.push_back(DTVTunerType::kTunerTypeATSC);
            }
            else
            {
                LOG(VB_RECORD, LOG_INFO, LOC + "Cable card is present");
                _tuner_types.push_back(DTVTunerType::kTunerTypeOCUR);
            }
        }
        else if (QString(model).toLower().contains("dvb"))
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
        _device.toLocal8Bit().constData(), NULL);

    if (!_hdhomerun_device)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to create hdhomerun object");
        return false;
    }

    _tuner = hdhomerun_device_get_tuner(_hdhomerun_device);

    if (hdhomerun_device_get_local_machine_addr(_hdhomerun_device) == 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to connect to device");
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Successfully connected to device");
    return true;
}

bool HDHRStreamHandler::EnterPowerSavingMode(void)
{
    QMutexLocker locker(&_listener_lock);

    if (!_stream_data_list.empty())
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            "Ignoring request - video streaming active");
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
        LOG(VB_GENERAL, LOG_ERR, LOC + "Get request failed (not connected)");
        return QString::null;
    }

    QString valname = QString("/tuner%1/%2").arg(_tuner).arg(name);
    char *value = NULL;
    char *error = NULL;
    if (hdhomerun_device_get_var(
            _hdhomerun_device, valname.toLocal8Bit().constData(),
            &value, &error) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Get request failed" + ENO);
        return QString::null;
    }

    if (report_error_return && error)
    {
        if (print_error)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("DeviceGet(%1): %2")
                    .arg(name).arg(error));
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
        LOG(VB_GENERAL, LOG_ERR, LOC + "Set request failed (not connected)");
        return QString::null;
    }


    QString valname = QString("/tuner%1/%2").arg(_tuner).arg(name);
    char *value = NULL;
    char *error = NULL;

    if (hdhomerun_device_set_var(
            _hdhomerun_device, valname.toLocal8Bit().constData(),
            val.toLocal8Bit().constData(), &value, &error) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Set request failed" + ENO);

        return QString::null;
    }

    if (report_error_return && error)
    {
        if (print_error)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("DeviceSet(%1 %2): %3")
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
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Not Re-Tuning channel %1")
                .arg(chn));
        return true;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Tuning channel %1 (was %2)")
            .arg(chn).arg(current));
    return !TunerSet("channel", chn).isEmpty();
}

bool HDHRStreamHandler::TuneProgram(uint mpeg_prog_num)
{
    if (_tune_mode == hdhrTuneModeFrequency)
        _tune_mode = hdhrTuneModeFrequencyProgram;

    if (_tune_mode != hdhrTuneModeFrequencyProgram)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "TuneProgram called in wrong tune mode");
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Tuning program %1")
            .arg(mpeg_prog_num));
    return !TunerSet(
        "program", QString::number(mpeg_prog_num), false).isEmpty();
}

bool HDHRStreamHandler::TuneVChannel(const QString &vchn)
{
    _tune_mode = hdhrTuneModeVChannel;

    QString current = TunerGet("vchannel");
    if (current == vchn)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Not Re-Tuning channel %1")
            .arg(vchn));
        return true;
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("TuneVChannel(%1) from (%2)")
            .arg(vchn).arg(current));
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Tuning vchannel %1").arg(vchn));
    return !TunerSet("vchannel", vchn).isEmpty();
}
