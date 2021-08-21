// -*- Mode: c++ -*-

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/ioctl.h>
#endif
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// MythTV headers
#include "hdhrstreamhandler.h"
#include "hdhrchannel.h"
#include "dtvsignalmonitor.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cardutil.h"
#include "mythlogging.h"

#ifdef NEED_HDHOMERUN_DEVICE_SELECTOR_LOAD_FROM_STR
static int hdhomerun_device_selector_load_from_str(struct hdhomerun_device_selector_t *hds, char *device_str);
#endif

#define LOC      QString("HDHRSH[%1](%2): ").arg(m_inputId).arg(m_device)

QMap<int,HDHRStreamHandler*>     HDHRStreamHandler::s_handlers;
QMap<int,uint>                   HDHRStreamHandler::s_handlersRefCnt;
QMutex                           HDHRStreamHandler::s_handlersLock;

HDHRStreamHandler *HDHRStreamHandler::Get(const QString &devname,
                                          int inputid, int majorid)
{
    QMutexLocker locker(&s_handlersLock);

    QMap<int,HDHRStreamHandler*>::iterator it = s_handlers.find(majorid);

    if (it == s_handlers.end())
    {
        auto *newhandler = new HDHRStreamHandler(devname, inputid, majorid);
        newhandler->Open();
        s_handlers[majorid] = newhandler;
        s_handlersRefCnt[majorid] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("HDHRSH[%1]: Creating new stream handler %2 for %3")
            .arg(inputid).arg(majorid).arg(devname));
    }
    else
    {
        s_handlersRefCnt[majorid]++;
        uint rcount = s_handlersRefCnt[majorid];
        LOG(VB_RECORD, LOG_INFO,
            QString("HDHRSH[%1]: Using existing stream handler %2 for %3")
            .arg(inputid).arg(majorid)
            .arg(devname) + QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[majorid];
}

void HDHRStreamHandler::Return(HDHRStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    int majorid = ref->m_majorId;

    QMap<int,uint>::iterator rit = s_handlersRefCnt.find(majorid);
    if (rit == s_handlersRefCnt.end())
        return;

    QMap<int,HDHRStreamHandler*>::iterator it = s_handlers.find(majorid);
    if (*rit > 1)
    {
        ref = nullptr;
        (*rit)--;
        return;
    }

    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("HDHRSH[%1]: Closing handler for %2")
            .arg(inputid).arg(majorid));
        ref->Close();
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HDHRSH[%1] Error: Couldn't find handler for %2")
            .arg(inputid).arg(majorid));
    }

    s_handlersRefCnt.erase(rit);
    ref = nullptr;
}

HDHRStreamHandler::HDHRStreamHandler(const QString &device, int inputid,
                                     int majorid)
    : StreamHandler(device, inputid)
    , m_majorId(majorid)
{
    setObjectName("HDHRStreamHandler");
}

/** \fn HDHRStreamHandler::run(void)
 *  \brief Reads HDHomeRun socket for tables & data
 */
void HDHRStreamHandler::run(void)
{
    RunProlog();

    {
        QMutexLocker locker(&m_hdhrLock);

        /* Create TS socket. */
        if (!hdhomerun_device_stream_start(m_hdhomerunDevice))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Starting recording (set target failed). Aborting.");
            m_bError = true;
            RunEpilog();
            return;
        }
        hdhomerun_device_stream_flush(m_hdhomerunDevice);
    }

    SetRunning(true, false, false);

    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): begin");

    int remainder = 0;
    QElapsedTimer last_update;
    while (m_runningDesired && !m_bError)
    {
        auto elapsed = !last_update.isValid()
            ? -1ms : std::chrono::milliseconds(last_update.elapsed());
        elapsed = (elapsed < 0ms) ? 1s : elapsed;
        if (elapsed > 100ms)
        {
            UpdateFiltersFromStreamData();
            if (m_tuneMode != hdhrTuneModeVChannel)
                UpdateFilters();
            last_update.restart();
        }

        size_t read_size = VIDEO_DATA_BUFFER_SIZE_1S / 8; // read up to 1/8s
        read_size /= VIDEO_DATA_PACKET_SIZE;
        read_size *= VIDEO_DATA_PACKET_SIZE;

        size_t data_length = 0;
        unsigned char *data_buffer = hdhomerun_device_stream_recv(
            m_hdhomerunDevice, read_size, &data_length);

        if (!data_buffer)
        {
            std::this_thread::sleep_for(20ms);
            continue;
        }

        // Assume data_length is a multiple of 188 (packet size)

        m_listenerLock.lock();

        if (m_streamDataList.empty())
        {
            m_listenerLock.unlock();
            continue;
        }

        for (auto sit = m_streamDataList.cbegin(); sit != m_streamDataList.cend(); ++sit)
            remainder = sit.key()->ProcessData(data_buffer, data_length);

        WriteMPTS(data_buffer, data_length - remainder);

        m_listenerLock.unlock();
        if (remainder != 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("RunTS(): data_length = %1 remainder = %2")
                    .arg(data_length).arg(remainder));
        }
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): " + "shutdown");

    RemoveAllPIDFilters();

    {
        QMutexLocker locker(&m_hdhrLock);
        hdhomerun_device_stream_stop(m_hdhomerunDevice);
    }

    if (VERBOSE_LEVEL_CHECK(VB_RECORD, LOG_INFO))
    {
        struct hdhomerun_video_sock_t* vs = nullptr;
        struct hdhomerun_video_stats_t stats {};
        vs = hdhomerun_device_get_video_sock(m_hdhomerunDevice);
        if (vs)
        {
            hdhomerun_video_get_stats(vs, &stats);
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("stream stats: packet_count=%1 "
                        "network_errors=%2 "
                        "transport_errors=%3 "
                        "sequence_errors=%4 "
                        "overflow_errors=%5")
                        .arg(stats.packet_count)
                        .arg(stats.network_error_count)
                        .arg(stats.transport_error_count)
                        .arg(stats.sequence_error_count)
                        .arg(stats.overflow_error_count));
        }
    }

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
    if (m_tuneMode == hdhrTuneModeFrequency)
        m_tuneMode = hdhrTuneModeFrequencyPid;

    if (m_tuneMode != hdhrTuneModeFrequencyPid)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "UpdateFilters called in wrong tune mode");
        return false;
    }

#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + "UpdateFilters()");
#endif // DEBUG_PID_FILTERS
    QMutexLocker locker(&m_pidLock);

    if (!m_filtersChanged)
    {
        return true;
    }

    QString filter = "";

    std::vector<uint> range_min;
    std::vector<uint> range_max;

    for (auto it = m_pidInfo.cbegin(); it != m_pidInfo.cend(); ++it)
    {
        range_min.push_back(it.key());
        PIDInfoMap::const_iterator eit = it;
        for (++eit;
             (eit != m_pidInfo.cend()) && (it.key() + 1 == eit.key());
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

    for (size_t i = 0; i < range_min.size(); i++)
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

    LOG(VB_RECORD, LOG_DEBUG, LOC + msg);
#endif // DEBUG_PID_FILTERS

    m_filtersChanged = false;

    return filter == new_filter;
}

bool HDHRStreamHandler::Open(void)
{
    if (Connect())
    {
        const char *model = hdhomerun_device_get_model_str(m_hdhomerunDevice);
        m_tunerTypes.clear();
        if (QString(model).contains("cablecard", Qt::CaseInsensitive))
        {
            QString status_channel = "none";
            hdhomerun_tuner_status_t t_status {};

            if (hdhomerun_device_get_oob_status(
                    m_hdhomerunDevice, nullptr, &t_status) < 0)
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
                m_tunerTypes.emplace_back(DTVTunerType::kTunerTypeATSC);
            }
            else
            {
                LOG(VB_RECORD, LOG_INFO, LOC + "Cable card is present");
                m_tunerTypes.emplace_back(DTVTunerType::kTunerTypeOCUR);
            }
        }
        else if (QString(model).endsWith("dvbt", Qt::CaseInsensitive))
        {
            m_tunerTypes.emplace_back(DTVTunerType::kTunerTypeDVBT);
        }
        else if (QString(model).endsWith("dvbc", Qt::CaseInsensitive))
        {
            m_tunerTypes.emplace_back(DTVTunerType::kTunerTypeDVBC);
        }
        else if (QString(model).endsWith("dvbtc", Qt::CaseInsensitive))
        {
            m_tunerTypes.emplace_back(DTVTunerType::kTunerTypeDVBT);
            m_tunerTypes.emplace_back(DTVTunerType::kTunerTypeDVBC);
        }
        else
        {
            m_tunerTypes.emplace_back(DTVTunerType::kTunerTypeATSC);
        }

        return true;
    }
    return false;
}

void HDHRStreamHandler::Close(void)
{
    if (m_hdhomerunDevice)
    {
        TuneChannel("none");
        hdhomerun_device_tuner_lockkey_release(m_hdhomerunDevice);
        m_hdhomerunDevice = nullptr;
    }
    if (m_deviceSelector)
    {
        hdhomerun_device_selector_destroy(m_deviceSelector, true);
        m_deviceSelector = nullptr;
    }
}

bool HDHRStreamHandler::Connect(void)
{
    m_deviceSelector = hdhomerun_device_selector_create(nullptr);
    if (!m_deviceSelector)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to create device selector");
        return false;
    }

    QStringList devices = m_device.split(",");
    for (const QString& device : qAsConst(devices))
    {
        QByteArray ba = device.toUtf8();
        int n = hdhomerun_device_selector_load_from_str(
            m_deviceSelector, ba.data());
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Added %1 devices from %3")
            .arg(n).arg(device));
    }

    m_hdhomerunDevice = hdhomerun_device_selector_choose_and_lock(
        m_deviceSelector, nullptr);
    if (!m_hdhomerunDevice)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to find a free device"));
        hdhomerun_device_selector_destroy(m_deviceSelector, true);
        m_deviceSelector = nullptr;
        return false;
    }

    m_tuner = hdhomerun_device_get_tuner(m_hdhomerunDevice);

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Connected to device(%1)")
        .arg(hdhomerun_device_get_name(m_hdhomerunDevice)));

    return true;
}

QString HDHRStreamHandler::TunerGet(
    const QString &name, bool report_error_return, bool print_error) const
{
    QMutexLocker locker(&m_hdhrLock);

    if (!m_hdhomerunDevice)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Get request failed (not connected)");
        return QString();
    }

    QString valname = QString("/tuner%1/%2").arg(m_tuner).arg(name);
    char *value = nullptr;
    char *error = nullptr;
    if (hdhomerun_device_get_var(
            m_hdhomerunDevice, valname.toLocal8Bit().constData(),
            &value, &error) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Get %1 request failed").arg(valname) + ENO);
        return QString();
    }

    if (report_error_return && error)
    {
        if (print_error)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("DeviceGet(%1): %2")
                    .arg(name, error));
        }

        return QString();
    }

    return QString(value);
}

QString HDHRStreamHandler::TunerSet(
    const QString &name, const QString &val,
    bool report_error_return, bool print_error)
{
    QMutexLocker locker(&m_hdhrLock);

    if (!m_hdhomerunDevice)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Set request failed (not connected)");
        return QString();
    }


    QString valname = QString("/tuner%1/%2").arg(m_tuner).arg(name);
    char *value = nullptr;
    char *error = nullptr;

#if 0
    LOG(VB_CHANSCAN, LOG_DEBUG, LOC + valname + " " + val);
#endif
    if (hdhomerun_device_set_var(
            m_hdhomerunDevice, valname.toLocal8Bit().constData(),
            val.toLocal8Bit().constData(), &value, &error) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Set %1 to '%2' request failed").arg(valname, val) +
            ENO);

        return QString();
    }

    if (report_error_return && error)
    {
        if (print_error)
        {
            // Skip error messages from MPTS recordings
            if (!(val.contains("0x2000") && strstr(error, "ERROR: invalid pid filter")))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("DeviceSet(%1 %2): %3")
                        .arg(name, val, error));
            }
        }

        return QString();
    }

    return QString(value);
}

void HDHRStreamHandler::GetTunerStatus(struct hdhomerun_tuner_status_t *status)
{
    QMutexLocker locker(&m_hdhrLock);

    hdhomerun_device_get_tuner_status(m_hdhomerunDevice, nullptr, status);
}

bool HDHRStreamHandler::IsConnected(void) const
{
    return (m_hdhomerunDevice != nullptr);
}

bool HDHRStreamHandler::TuneChannel(const QString &chanid)
{
    m_tuneMode = hdhrTuneModeFrequency;

    QString current = TunerGet("channel");
    if (current == chanid)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Not Re-Tuning channel %1")
                .arg(chanid));
        return true;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Tuning channel %1 (was %2)")
            .arg(chanid, current));
    return !TunerSet("channel", chanid).isEmpty();
}

bool HDHRStreamHandler::TuneProgram(uint mpeg_prog_num)
{
    if (m_tuneMode == hdhrTuneModeFrequency)
        m_tuneMode = hdhrTuneModeFrequencyProgram;

    if (m_tuneMode != hdhrTuneModeFrequencyProgram)
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
    m_tuneMode = hdhrTuneModeVChannel;

    QString current = TunerGet("vchannel");
    if (current == vchn)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Not Re-Tuning channel %1")
            .arg(vchn));
        return true;
    }
    LOG(VB_RECORD, LOG_INFO, LOC + QString("TuneVChannel(%1) from (%2)")
        .arg(vchn, current));

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Tuning vchannel %1").arg(vchn));
    return !TunerSet("vchannel", vchn).isEmpty();
}

#ifdef NEED_HDHOMERUN_DEVICE_SELECTOR_LOAD_FROM_STR

// Provide functions we need that are not included in some versions of
// libhdhomerun.  These were taken
// from version 20180817 and modified as needed.

struct hdhomerun_device_selector_t {
        struct hdhomerun_device_t **hd_list;
        size_t hd_count;
        struct hdhomerun_debug_t *dbg;
};

static int hdhomerun_device_selector_load_from_str_discover(struct hdhomerun_device_selector_t *hds, uint32_t target_ip, uint32_t device_id)
{
	struct hdhomerun_discover_device_t result;
	int discover_count = hdhomerun_discover_find_devices_custom(target_ip, HDHOMERUN_DEVICE_TYPE_TUNER, device_id, &result, 1);
	if (discover_count != 1) {
		return 0;
	}

	int count = 0;
	unsigned int tuner_index;
	for (tuner_index = 0; tuner_index < result.tuner_count; tuner_index++) {
		struct hdhomerun_device_t *hd = hdhomerun_device_create(result.device_id, result.ip_addr, tuner_index, hds->dbg);
		if (!hd) {
			continue;
		}

		hdhomerun_device_selector_add_device(hds, hd);
		count++;
	}

	return count;
}

static int hdhomerun_device_selector_load_from_str(struct hdhomerun_device_selector_t *hds, char *device_str)
{
	/*
	 * IP address based device_str.
	 */
	unsigned int a[4];
	if (sscanf(device_str, "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]) == 4) {
		uint32_t ip_addr = (uint32_t)((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | (a[3] << 0));

		/*
		 * IP address + tuner number.
		 */
		unsigned int tuner;
		if (sscanf(device_str, "%u.%u.%u.%u-%u", &a[0], &a[1], &a[2], &a[3], &tuner) == 5) {
			struct hdhomerun_device_t *hd = hdhomerun_device_create(HDHOMERUN_DEVICE_ID_WILDCARD, ip_addr, tuner, hds->dbg);
			if (!hd) {
				return 0;
			}

			hdhomerun_device_selector_add_device(hds, hd);
			return 1;
		}

		/*
		 * IP address only - discover and add tuners.
		 */
		return hdhomerun_device_selector_load_from_str_discover(hds, ip_addr, HDHOMERUN_DEVICE_ID_WILDCARD);
	}

	/*
	 * Device ID based device_str.
	 */
	char *end;
	uint32_t device_id = (uint32_t)strtoul(device_str, &end, 16);
	if ((end == device_str + 8) && hdhomerun_discover_validate_device_id(device_id)) {
		/*
		 * IP address + tuner number.
		 */
		if (*end == '-') {
			unsigned int tuner = (unsigned int)strtoul(end + 1, NULL, 10);
			struct hdhomerun_device_t *hd = hdhomerun_device_create(device_id, 0, tuner, hds->dbg);
			if (!hd) {
				return 0;
			}

			hdhomerun_device_selector_add_device(hds, hd);
			return 1;
		}

		/*
		 * Device ID only - discover and add tuners.
		 */
		return hdhomerun_device_selector_load_from_str_discover(hds, 0, device_id);
	}

        return 0;
}

#endif
