/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2008 Daniel Kristjansson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// Qt headers
#include <QCoreApplication>
#include <iostream>

// MythTv headers
#include "channelscanner_web.h"
#include "channelscan_sm.h"
#include "channelimporter.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelscan/scanwizardconfig.h"
#ifdef USING_SATIP
#include "recorders/satiputils.h"
#endif // USING_SATIP

#define LOC QString("ChScanWeb: ")

ChannelScannerWeb *ChannelScannerWeb::s_Instance = nullptr;

ChannelScannerWeb *ChannelScannerWeb::getInstance()
{
    if (s_Instance == nullptr)
        s_Instance = new ChannelScannerWeb();
    return s_Instance;
}

// Called from Web Service API
// Run in http thread
bool  ChannelScannerWeb::StartScan (uint cardid,
                            const QString &DesiredServices,
                            bool freeToAirOnly,
                            bool ChannelNumbersOnly,
                            bool CompleteChannelsOnly,
                            bool FullChannelSearch,
                            bool RemoveDuplicates,
                            bool AddFullTS,
                            bool TestDecryptable,
                            const QString &ScanType,
                            const QString &FreqTable,
                            QString modulation,
                            const QString &FirstChan,
                            const QString &LastChan,
                            uint ScanId,
                            bool IgnoreSignalTimeout,
                            bool FollowNITSetting,
                            uint MplexId,
                            const QString &frequency,
                            const QString &bandwidth,
                            const QString &polarity,
                            const QString &symbolrate,
                            const QString &inversion,
                            const QString &constellation,
                            const QString &modsys,
                            const QString &coderate_lp,
                            const QString &coderate_hp,
                            const QString &fec,
                            const QString &trans_mode,
                            const QString &guard_interval,
                            const QString &hierarchy,
                            const QString &rolloff)
{

    ResetStatus();
    m_cardid = cardid;
    m_scanId = ScanId;

    QString subType = CardUtil::ProbeSubTypeName(cardid);
    CardUtil::INPUT_TYPES inputType = CardUtil::toInputType(subType);

#ifdef USING_SATIP
    if (inputType == CardUtil::INPUT_TYPES::SATIP)
    {
        inputType = SatIP::toDVBInputType(CardUtil::GetVideoDevice(cardid));
    }
#endif // USING_SATIP

    int nScanType = -99;
    if (ScanType == "FULL")
    {
        switch(inputType) {
            case CardUtil::INPUT_TYPES::ATSC:
                nScanType = ScanTypeSetting::FullScan_ATSC;
                break;
            case CardUtil::INPUT_TYPES::DVBT:
                nScanType = ScanTypeSetting::FullScan_DVBT;
                break;
            case CardUtil::INPUT_TYPES::V4L:
            case CardUtil::INPUT_TYPES::MPEG:
                nScanType = ScanTypeSetting::FullScan_Analog;
                break;
            case CardUtil::INPUT_TYPES::DVBT2:
                nScanType = ScanTypeSetting::FullScan_DVBT2;
                break;
            case CardUtil::INPUT_TYPES::DVBC:
                nScanType = ScanTypeSetting::FullScan_DVBC;
                break;
            case  CardUtil::INPUT_TYPES::HDHOMERUN:
                if (CardUtil::HDHRdoesDVBC(CardUtil::GetVideoDevice(cardid)))
                    nScanType = ScanTypeSetting::FullScan_DVBC;
                else if (CardUtil::HDHRdoesDVB(CardUtil::GetVideoDevice(cardid)))
                    nScanType = ScanTypeSetting::FullScan_DVBT;
                else
                    nScanType = ScanTypeSetting::FullScan_ATSC;
                break;
            default:
                break;
        }
    }
    else if (ScanType == "FULLTUNED")
    {
        switch(inputType) {
            case CardUtil::INPUT_TYPES::DVBT:
                nScanType = ScanTypeSetting::NITAddScan_DVBT;
                break;
            case CardUtil::INPUT_TYPES::DVBT2:
                nScanType = ScanTypeSetting::NITAddScan_DVBT2;
                break;
            case CardUtil::INPUT_TYPES::DVBS:
                nScanType = ScanTypeSetting::NITAddScan_DVBS;
                break;
            case CardUtil::INPUT_TYPES::DVBS2:
                nScanType = ScanTypeSetting::NITAddScan_DVBS2;
                break;
            case CardUtil::INPUT_TYPES::DVBC:
                nScanType = ScanTypeSetting::NITAddScan_DVBC;
                break;
            case CardUtil::INPUT_TYPES::HDHOMERUN:
                if (CardUtil::HDHRdoesDVBC(CardUtil::GetVideoDevice(cardid)))
                    nScanType = ScanTypeSetting::NITAddScan_DVBC;
                else if (CardUtil::HDHRdoesDVB(CardUtil::GetVideoDevice(cardid)))
                    nScanType = ScanTypeSetting::NITAddScan_DVBT;
                break;
            default:
                break;
        }
    }
    else if (ScanType == "VBOXIMPORT")
    {
        nScanType = ScanTypeSetting::VBoxImport;
    }
    else if (ScanType == "HDHRIMPORT")
    {
        nScanType = ScanTypeSetting::HDHRImport;
    }
    else if (ScanType == "MPTSIMPORT")
    {
        nScanType = ScanTypeSetting::IPTVImportMPTS;
    }
    else if (ScanType == "M3UIMPORT")
    {
        nScanType = ScanTypeSetting::IPTVImport;
    }
    else if (ScanType == "ASI" || ScanType == "MPTS")
    {
        nScanType = ScanTypeSetting::CurrentTransportScan;
    }
    else if (ScanType == "EXTIMPORT")
    {
        nScanType = ScanTypeSetting::ExternRecImport;
    }
    else if (ScanType == "IMPORT")
    {
        nScanType = ScanTypeSetting::ExistingScanImport;
    }
    else if (ScanType == "ALLTRANSPORT")
    {
        nScanType = ScanTypeSetting::FullTransportScan;
    }
    else if (ScanType == "ONETRANSPORT")
    {
        nScanType = ScanTypeSetting::TransportScan;
    }

    if (nScanType == -99)
    {
        m_dlgMsg = QObject::tr("This scan type is not supported");
        return false;
    }
    ServiceRequirements service_requirements
        {static_cast<ServiceRequirements> (kRequireVideo | kRequireAudio)};
    if (DesiredServices == "tv")
        service_requirements = static_cast<ServiceRequirements> (kRequireVideo | kRequireAudio);
    else if (DesiredServices == "audio")
        service_requirements = kRequireAudio;
    else if (DesiredServices == "all")
        service_requirements = kRequireNothing;
    QString freq_std;
    switch(nScanType)
    {
        case ScanTypeSetting::FullScan_ATSC:
            freq_std = "atsc";
            break;
        case ScanTypeSetting::FullScan_DVBC:
            freq_std = "dvbc";
            break;
        case ScanTypeSetting::FullScan_DVBT:
        case ScanTypeSetting::FullScan_DVBT2:
            freq_std = "dvbt";
            break;
        case ScanTypeSetting::FullScan_Analog:
            freq_std = "analog";
            break;
    }

    switch (nScanType)
    {
        case ScanTypeSetting::FullScan_ATSC:
            // in this case the modulation is supplied in the
            // scan call so it is not changed here
            break;
        case ScanTypeSetting::FullScan_DVBC:
            modulation = "qam";
            break;
        case ScanTypeSetting::FullScan_DVBT:
        case ScanTypeSetting::FullScan_DVBT2:
            modulation = "ofdm";
            break;
        case ScanTypeSetting::FullScan_Analog:
            modulation = "analog";
            break;
    }

    m_scantype = nScanType;

    // Get startChan. Logic copied from ScanOptionalConfig::GetStartChan(void)
    QMap<QString,QString> startChan;
    if (ScanTypeSetting::NITAddScan_DVBT == nScanType)
    {
        startChan["std"]            = "dvb";
        startChan["type"]           = "OFDM";
        startChan["frequency"]      = frequency;
        startChan["inversion"]      = inversion;
        startChan["bandwidth"]      = bandwidth;
        startChan["coderate_hp"]    = coderate_hp;
        startChan["coderate_lp"]    = coderate_lp;
        startChan["constellation"]  = constellation;
        startChan["trans_mode"]     = trans_mode;
        startChan["guard_interval"] = guard_interval;
        startChan["hierarchy"]      = hierarchy;
    }
    else if (ScanTypeSetting::NITAddScan_DVBT2 == nScanType)
    {
        startChan["std"]            = "dvb";
        startChan["type"]           = "DVB_T2";
        startChan["frequency"]      = frequency;
        startChan["inversion"]      = inversion;
        startChan["bandwidth"]      = bandwidth;
        startChan["coderate_hp"]    = coderate_hp;
        startChan["coderate_lp"]    = coderate_lp;
        startChan["constellation"]  = constellation;
        startChan["trans_mode"]     = trans_mode;
        startChan["guard_interval"] = guard_interval;
        startChan["hierarchy"]      = hierarchy;
        startChan["mod_sys"]        = modsys;
    }
    else if (ScanTypeSetting::NITAddScan_DVBS == nScanType)
    {
        startChan["std"]        = "dvb";
        startChan["type"]       = "QPSK";
        startChan["modulation"] = "qpsk";
        startChan["frequency"]  = frequency;
        startChan["inversion"]  = inversion;
        startChan["symbolrate"] = symbolrate;
        startChan["fec"]        = fec;
        startChan["polarity"]   = polarity;
    }
    else if (ScanTypeSetting::NITAddScan_DVBC == nScanType)
    {
        startChan["std"]        = "dvb";
        startChan["type"]       = "QAM";
        startChan["frequency"]  = frequency;
        startChan["symbolrate"] = symbolrate;
        startChan["modulation"] = modulation;
        startChan["mod_sys"]    = modsys;
        startChan["inversion"]  = inversion;
        startChan["fec"]        = fec;
    }
    else if (ScanTypeSetting::NITAddScan_DVBS2 == nScanType)
    {
        startChan["std"]        = "dvb";
        startChan["type"]       = "DVB_S2";
        startChan["frequency"]  = frequency;
        startChan["inversion"]  = inversion;
        startChan["symbolrate"] = symbolrate;
        startChan["fec"]        = fec;
        startChan["modulation"] = modulation;
        startChan["polarity"]   = polarity;
        startChan["mod_sys"]    = modsys;
        startChan["rolloff"]    = rolloff;
    }

    setupScan(cardid);

    QString inputname = get_on_input("inputname", cardid);
    int sourceid = get_on_input("sourceid", cardid).toUInt();

    if (ScanTypeSetting::ExistingScanImport == nScanType)
    {
        m_freeToAirOnly       = freeToAirOnly;
        m_channelNumbersOnly  = ChannelNumbersOnly;
        m_completeOnly        = CompleteChannelsOnly;
        m_fullSearch          = FullChannelSearch;
        m_removeDuplicates    = RemoveDuplicates;
        m_addFullTS           = AddFullTS;
        m_serviceRequirements = service_requirements;
        m_sourceid            = sourceid;
        // The import is handled by the monitor thread after the complete event
        post_event(m_scanMonitor, ScannerEvent::kScanComplete, 0);
    }
    else if (nScanType == ScanTypeSetting::IPTVImport)
    {
        ImportM3U(cardid, inputname, sourceid, false);
    }
    else if (nScanType == ScanTypeSetting::VBoxImport)
    {
        ImportVBox(cardid, inputname, sourceid,
                                freeToAirOnly,
                                service_requirements);
    }
    else if (nScanType == ScanTypeSetting::HDHRImport)
    {
        ImportHDHR(cardid, inputname, sourceid,
                                service_requirements);
    }
    else if (nScanType == ScanTypeSetting::ExternRecImport)
    {
        ImportExternRecorder(cardid, inputname, sourceid);
    }
    else if (nScanType == ScanTypeSetting::IPTVImportMPTS)
    {
        ImportM3U(cardid, inputname, sourceid, true);
    }
    else
    {
        Scan(
        nScanType,
        cardid,
        inputname,
        sourceid,
        IgnoreSignalTimeout,      // do_ignore_signal_timeout,
        FollowNITSetting,       //  do_follow_nit,
        TestDecryptable,
        freeToAirOnly,
        ChannelNumbersOnly,
        CompleteChannelsOnly,
        FullChannelSearch,
        RemoveDuplicates,
        AddFullTS,
        service_requirements,
        MplexId,
        startChan,
        freq_std,
        modulation,
        FreqTable,
        FirstChan,
        LastChan);
    }

    return true;
}

void ChannelScannerWeb::ResetStatus()
{
  m_runType = 0;
  m_onlysavescan = false;
  m_interactive = false;
  m_cardid = 0;
  m_status = "IDLE";
  m_statusLock = false;
  m_statusProgress = 0;
  m_statusSnr = 0;
  m_statusText = "";
  m_statusLog = "";
  m_statusTitleText = "";
  m_statusRotorPosition  = 0;
  m_statusSignalStrength  = 0;
  m_showSignalLock = false;
  m_showSignalStrength = false;
  m_showSignalNoise = false;
  m_showRotorPos = false;
  m_dlgMsg = "";
  m_dlgButtons.clear();
  m_dlgInputReq = false;
  m_dlgButton = -1;
  m_dlgString = "";
}

void ChannelScannerWeb::setupScan(int cardId)
{
    stopMon();
    ResetStatus();
    m_cardid = cardId;
    m_status = "RUNNING";
    m_monitorThread = new MThread("ScanMonitor");
    m_monitorThread->start();
    m_scanMonitor = new ScanMonitor(this);
    m_scanMonitor->moveToThread(m_monitorThread->qthread());
}

// run in an http thread to stop the scan
void ChannelScannerWeb::stopScan()
{
    if (m_scanMonitor)
    {
        post_event(m_scanMonitor, ScannerEvent::kScanShutdown, 0);
    }
}

// run in either a monitor thread when scan completes
// or an http thread when starting the next scan
void ChannelScannerWeb::stopMon(void)
{
    if (m_monitorThread)
    {
        if (m_scanMonitor)
            m_scanMonitor->deleteLater();
        m_scanMonitor = nullptr;
        m_monitorThread->exit();
        if (QThread::currentThread() != m_monitorThread->qthread())
        {
            m_monitorThread->wait();
            delete m_monitorThread;
            m_monitorThread = nullptr;
        }
    }
}


// run in the monitor thread
void ChannelScannerWeb::HandleEvent(const ScannerEvent *scanEvent)
{
    auto type = scanEvent->type();
    if ((type == ScannerEvent::kScanComplete) ||
        (type == ScannerEvent::kScanShutdown) ||
        (type == ScannerEvent::kScanErrored))
    {
        if (type == ScannerEvent::kScanComplete)
        {
            m_statusText = tr("Scan Complete");
        }
        else if (type == ScannerEvent::kScanShutdown)
        {
            m_statusText = tr("Scan Shut Down");
        }
        else if (type == ScannerEvent::kScanErrored)
        {
            m_statusText = tr("Scan Error") + " " + scanEvent->strValue();
        }
        if (m_sigmonScanner)
        {
            m_sigmonScanner->StopScanner();
            m_transports = m_sigmonScanner->GetChannelList(m_addFullTS);
            QString msg = tr("Found %1 Transports").arg(m_transports.size());
            m_statusTitleText = msg;
        }

        bool success = (m_iptvScanner != nullptr);
#ifdef USING_VBOX
        success |= (m_vboxScanner != nullptr);
#endif
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
        success |= (m_externRecScanner != nullptr);
#endif
#ifdef USING_HDHOMERUN
        success |= (m_hdhrScanner != nullptr);
#endif
        Teardown();

        if (m_scantype == ScanTypeSetting::ExistingScanImport)
        {
            ScanDTVTransportList transports = LoadScan(m_scanId);
            ChannelImporter ci(true, true, true, true, false,
                            m_freeToAirOnly,
                            m_channelNumbersOnly,
                            m_completeOnly,
                            m_fullSearch,
                            m_removeDuplicates,
                            m_serviceRequirements);
            ci.Process(transports, get_on_input("sourceid", m_cardid).toUInt());
        }
        else if (type != ScannerEvent::kScanErrored)
        {
            Process(m_transports, success);
        }
        stopMon();
        m_status = "IDLE";

    }
    else if (type == ScannerEvent::kAppendTextToLog)
    {
        log(scanEvent->strValue());
    }
    else if (type == ScannerEvent::kSetStatusText)
    {
        m_statusText = scanEvent->strValue();
    }
    else if (type == ScannerEvent::kSetPercentComplete)
    {
        m_statusProgress = scanEvent->intValue();
    }
    else if (type == ScannerEvent::kSetStatusSignalLock)
    {
        m_statusLock = scanEvent->boolValue();
    }
    else if (type == ScannerEvent::kSetStatusSignalToNoise)
    {
        m_statusSnr = scanEvent->intValue() * 100 / 65535;
    }
    else if (type == ScannerEvent::kSetStatusTitleText)
    {
        m_statusTitleText = scanEvent->strValue();
    }
    else if (type == ScannerEvent::kSetStatusRotorPosition)
    {
        m_statusRotorPosition = scanEvent->intValue();
    }
    else if (type == ScannerEvent::kSetStatusSignalStrength)
    {
        m_statusSignalStrength = scanEvent->intValue() * 100 / 65535;
    }

    QString msg;
    if (VERBOSE_LEVEL_NONE() || VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_INFO))
    {
        msg = QString("%1% S/N %2 %3 : %4 (%5) %6")
                  .arg(m_statusProgress, 3)
                  .arg(m_statusSnr)
                  .arg((m_statusLock) ? "l" : "L",
                       qPrintable(m_statusText),
                       qPrintable(scanEvent->strValue()))
                  .arg("", 20);
    }

    if (VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_INFO))
    {
        static QString s_oldMsg;
        if (msg != s_oldMsg)
        {
            LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);
            s_oldMsg = msg;
        }
    }
    else if (VERBOSE_LEVEL_NONE())
    {
        if (msg.length() > 80)
            msg = msg.left(77) + "...";
        std::cout << "\r" << msg.toLatin1().constData() << "\r";
        std::cout << std::flush;
    }
}

void ChannelScannerWeb::InformUser(const QString &error)
{
    LOG(VB_GENERAL, LOG_ERR, LOC + error);
    // post_event(m_scanMonitor, ScannerEvent::ScanErrored, 0);
    m_statusLog.append(error).append('\n');
    m_dlgMsg = error;
}

void ChannelScannerWeb::log(const QString &msg)
{
    m_statusLog.append(msg).append("\n");
}

// run in the monitor thread
void ChannelScannerWeb::Process(const ScanDTVTransportList &_transports,
                                bool success)
{
    ChannelImporter ci(true, true, true, true, true,
                       m_freeToAirOnly, m_channelNumbersOnly, m_completeOnly,
                       m_fullSearch, m_removeDuplicates, m_serviceRequirements, success);
    ci.Process(_transports, m_sourceid);
}

void ChannelScannerWeb::MonitorProgress(
    bool lock, bool strength, bool snr, bool rotorpos)
{
    m_showSignalLock = lock;
    m_showSignalStrength = strength;
    m_showSignalNoise = snr;
    m_showRotorPos = rotorpos;
}
