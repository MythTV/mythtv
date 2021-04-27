// -*- Mode: c++ -*-

// C++ headers
#include <chrono>
#include <thread>

// Qt headers
#include <QString>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>

// MythTV headers
#include "dtvsignalmonitor.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cardutil.h"
#include "mythlogging.h"
#include "satiputils.h"
#include "satipchannel.h"
#include "satipstreamhandler.h"

#define LOC QString("SatIPSH[%1]: ").arg(m_inputId)

// For implementing Get & Return
QMap<QString, SatIPStreamHandler*> SatIPStreamHandler::s_handlers;
QMap<QString, uint>                SatIPStreamHandler::s_handlersRefCnt;
QMutex                             SatIPStreamHandler::s_handlersLock;

SatIPStreamHandler *SatIPStreamHandler::Get(const QString &devname, int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    QMap<QString, SatIPStreamHandler*>::iterator it = s_handlers.find(devname);

    if (it == s_handlers.end())
    {
        auto *newhandler = new SatIPStreamHandler(devname, inputid);
        newhandler->Open();
        s_handlers[devname] = newhandler;
        s_handlersRefCnt[devname] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("SatIPSH[%1]: Creating new stream handler for %2")
            .arg(inputid).arg(devname));
    }
    else
    {
        s_handlersRefCnt[devname]++;
        uint rcount = s_handlersRefCnt[devname];
        (*it)->m_inputId = inputid;

        LOG(VB_RECORD, LOG_INFO,
            QString("SatIPSH[%1]: Using existing stream handler for %2").arg(inputid).arg(devname) +
            QString(" (%1 users)").arg(rcount));
    }

    return s_handlers[devname];
}

void SatIPStreamHandler::Return(SatIPStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    QString devname = ref->m_device;

    QMap<QString, uint>::iterator rit = s_handlersRefCnt.find(devname);
    if (rit == s_handlersRefCnt.end())
    {
        LOG(VB_RECORD, LOG_ERR, QString("SatIPSH[%1]: Return(%2) not found")
            .arg(inputid).arg(devname));
        return;
    }

    LOG(VB_RECORD, LOG_INFO, QString("SatIPSH[%1]: Return stream handler for %2 (%3 users)")
        .arg(inputid).arg(devname).arg(*rit));

    if (*rit > 1)
    {
        ref = nullptr;
        (*rit)--;
        return;
    }

    QMap<QString, SatIPStreamHandler*>::iterator it = s_handlers.find(devname);
    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("SatIPSH[%1]: Closing handler for %2")
            .arg(inputid).arg(devname));
        (*it)->Stop();
        (*it)->Close();
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("SatIPSH[%1] Error: Couldn't find handler for %2")
            .arg(inputid).arg(devname));
    }

    s_handlersRefCnt.erase(rit);
    ref = nullptr;
}

SatIPStreamHandler::SatIPStreamHandler(const QString &device, int inputid)
    : StreamHandler(device, inputid)
    , m_inputId(inputid)
    , m_device(device)
    , m_rtsp(new SatIPRTSP(this))
{
    setObjectName("SatIPStreamHandler");

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("ctor for %2").arg(device));
}

bool SatIPStreamHandler::UpdateFilters(void)
{
#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + "UpdateFilters()");
#endif // DEBUG_PID_FILTERS
    QMutexLocker locker(&m_pidLock);

    QStringList pids;

    if (m_pidInfo.contains(0x2000))
    {
        pids.append("all");
    }
    else
    {
        for (auto it = m_pidInfo.cbegin(); it != m_pidInfo.cend(); ++it)
            pids.append(QString("%1").arg(it.key()));
    }
#ifdef DEBUG_PID_FILTERS
    QString msg = QString("PIDS: '%1'").arg(pids.join(","));
    LOG(VB_RECORD, LOG_DEBUG, LOC + msg);
#endif // DEBUG_PID_FILTERS

    bool rval = true;
    if (m_rtsp && m_oldpids != pids)
    {
        rval = m_rtsp->Play(pids);
        m_oldpids = pids;
    }

    return rval;
}

void SatIPStreamHandler::run(void)
{
    RunProlog();

    SetRunning(true, false, false);

    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): begin");

    QElapsedTimer last_update;

    while (m_runningDesired && !m_bError)
    {
        {
            QMutexLocker locker(&m_tunelock);

            if (m_oldtuningurl != m_tuningurl)
            {
                if (m_setupinvoked)
                {
                    m_rtsp->Teardown();
                    m_setupinvoked = false;
                }

                m_rtsp->Setup(m_tuningurl);
                m_setupinvoked = true;
                m_oldtuningurl = m_tuningurl;

                last_update.restart();
            }
        }

        // Update the PID filters every 100 milliseconds
        auto elapsed = !last_update.isValid()
            ? -1ms : std::chrono::milliseconds(last_update.elapsed());
        elapsed = (elapsed < 0ms) ? 1s : elapsed;
        if (elapsed > 100ms)
        {
            UpdateFiltersFromStreamData();
            UpdateFilters();
            last_update.restart();
        }

        // Delay to avoid busy wait loop
        std::this_thread::sleep_for(20ms);

    }
    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): " + "shutdown");

    RemoveAllPIDFilters();

    // TEARDOWN command
    if (m_setupinvoked)
    {
        QMutexLocker locker(&m_tunelock);
        m_rtsp->Teardown();
        m_setupinvoked = false;
        m_oldtuningurl = "";
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): end");
    SetRunning(false, false, false);
    RunEpilog();
}

bool SatIPStreamHandler::Tune(const DTVMultiplex &tuning)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Tune %1").arg(tuning.m_frequency));

    QMutexLocker locker(&m_tunelock);

    // Build the query string
    QStringList qry;

    if (m_tunerType == DTVTunerType::kTunerTypeDVBC)
    {
        qry.append(QString("fe=%1").arg(m_frontend+1));
        qry.append(QString("freq=%1").arg(SatIP::freq(tuning.m_frequency)));
        qry.append(QString("sr=%1").arg(tuning.m_symbolRate / 1000)); // symbolrate in ksymb/s
        qry.append("msys=dvbc");
        qry.append(QString("mtype=%1").arg(SatIP::mtype(tuning.m_modulation)));
    }
    else if (m_tunerType == DTVTunerType::kTunerTypeDVBT || m_tunerType == DTVTunerType::kTunerTypeDVBT2)
    {
        qry.append(QString("fe=%1").arg(m_frontend+1));
        qry.append(QString("freq=%1").arg(SatIP::freq(tuning.m_frequency)));
        qry.append(QString("bw=%1").arg(SatIP::bw(tuning.m_bandwidth)));
        qry.append(QString("msys=%1").arg(SatIP::msys(tuning.m_modSys)));
        qry.append(QString("tmode=%1").arg(SatIP::tmode(tuning.m_transMode)));
        qry.append(QString("mtype=%1").arg(SatIP::mtype(tuning.m_modulation)));
        qry.append(QString("gi=%1").arg(SatIP::gi(tuning.m_guardInterval)));
        qry.append(QString("fec=%1").arg(SatIP::fec(tuning.m_fec)));
    }
    else if (m_tunerType == DTVTunerType::kTunerTypeDVBS1 || m_tunerType == DTVTunerType::kTunerTypeDVBS2)
    {
        qry.append(QString("fe=%1").arg(m_frontend+1));
        qry.append(QString("freq=%1").arg(SatIP::freq(tuning.m_frequency*1000)));   // frequency in Hz
        qry.append(QString("pol=%1").arg(SatIP::pol(tuning.m_polarity)));
        qry.append(QString("ro=%1").arg(SatIP::ro(tuning.m_rolloff)));
        qry.append(QString("msys=%1").arg(SatIP::msys(tuning.m_modSys)));
        qry.append(QString("mtype=%1").arg(SatIP::mtype(tuning.m_modulation)));
        qry.append(QString("sr=%1").arg(tuning.m_symbolRate / 1000));               // symbolrate in ksymb/s
        qry.append(QString("fec=%1").arg(SatIP::fec(tuning.m_fec)));
        qry.append(QString("plts=auto"));                                           // pilot tones
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("Unhandled m_tunerType %1 %2").arg(m_tunerType).arg(m_tunerType.toString()));
        return false;
    }

    QUrl url = QUrl(m_baseurl);
    url.setQuery(qry.join("&"));

    m_tuningurl = url;

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Tune url:%1").arg(url.toString()));

    if (m_tuningurl == m_oldtuningurl)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Skip tuning, already tuned to this url"));
        return true;
    }

    // Need SETUP and PLAY (with pids=none) to get RTSP packets with tuner lock info
    if (m_rtsp)
    {
        bool rval = true;

        // TEARDOWN command
        if (m_setupinvoked)
        {
            rval = m_rtsp->Teardown();
            m_setupinvoked = false;
        }

        // SETUP command
        if (rval)
        {
            rval = m_rtsp->Setup(m_tuningurl);
            m_oldtuningurl = m_tuningurl;
            m_setupinvoked = true;
        }

        // PLAY command
        if (rval)
        {
            QStringList pids;
            m_rtsp->Play(pids);
            m_oldpids = pids;
        }
        return rval;
    }
    return true;
}

bool SatIPStreamHandler::Open(void)
{
    QUrl url;
    url.setScheme("rtsp");
    url.setPort(554);
    url.setPath("/");

    // Discover the device using SSDP
    QStringList devinfo = m_device.split(":");
    if (devinfo.value(0).toUpper() == "UUID")
    {
        QString deviceId = QString("uuid:%1").arg(devinfo.value(1));
        m_frontend = devinfo.value(3).toUInt();

        QString ip = SatIP::findDeviceIP(deviceId);
        if (ip != nullptr)
        {
            LOG(VB_RECORD, LOG_INFO, LOC + QString("Discovered device %1 at %2").arg(deviceId, ip));
        }
        else
        {
            LOG(VB_RECORD, LOG_ERR, LOC + QString("Failed to discover device %1, no IP found").arg(deviceId));
            return false;
        }

        url.setHost(ip);
    }
    else
    {
        // TODO: Handling of manual IP devices
    }

    m_tunerType = SatIP::toTunerType(m_device);
    m_baseurl = url;

    return true;
}

void SatIPStreamHandler::Close(void)
{
    delete m_rtsp;
    m_rtsp = nullptr;
    m_baseurl = nullptr;
}
