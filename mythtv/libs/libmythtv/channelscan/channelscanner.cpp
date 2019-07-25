/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 *
 * Description:
 *     Collection of classes to provide channel scanning functionallity
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

#include <algorithm>
using namespace std;

#include "analogsignalmonitor.h"
#include "iptvchannelfetcher.h"
#include "dvbsignalmonitor.h"
#include "scanwizardconfig.h"
#include "channelscan_sm.h"
#include "channelscanner.h"
#include "hdhrchannel.h"
#include "scanmonitor.h"
#include "asichannel.h"
#ifdef USING_DVB        // for bug in gcc 8.3
#include "dvbchannel.h"
#endif
#include "v4lchannel.h"
#include "iptvchannel.h"
#include "ExternalChannel.h"
#include "cardutil.h"

#define LOC QString("ChScan: ")

ChannelScanner::~ChannelScanner()
{
    ChannelScanner::Teardown();

    if (m_scanMonitor)
    {
        m_scanMonitor->deleteLater();
        m_scanMonitor = nullptr;
    }
}

void ChannelScanner::Teardown(void)
{
    if (m_sigmonScanner)
    {
        delete m_sigmonScanner;
        m_sigmonScanner = nullptr;
    }

    if (m_channel)
    {
        delete m_channel;
        m_channel = nullptr;
    }

    if (m_iptvScanner)
    {
        m_iptvScanner->Stop();
        delete m_iptvScanner;
        m_iptvScanner = nullptr;
    }

#ifdef USING_VBOX
    if (m_vboxScanner)
    {
        m_vboxScanner->Stop();
        delete m_vboxScanner;
        m_vboxScanner = nullptr;
    }
#endif

#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    if (m_ExternRecScanner)
    {
        m_ExternRecScanner->Stop();
        delete m_ExternRecScanner;
        m_ExternRecScanner = nullptr;
    }
#endif

    if (m_scanMonitor)
    {
        m_scanMonitor->deleteLater();
        m_scanMonitor = nullptr;
    }
}

// full scan of existing transports broken
// existing transport scan broken
void ChannelScanner::Scan(
    int            scantype,
    uint           cardid,
    const QString &inputname,
    uint           sourceid,
    bool           do_ignore_signal_timeout,
    bool           do_follow_nit,
    bool           do_test_decryption,
    bool           do_fta_only,
    bool           do_lcn_only,
    bool           do_complete_only,
    bool           do_add_full_ts,
    ServiceRequirements service_requirements,
    // stuff needed for particular scans
    uint           mplexid /* TransportScan */,
    const QMap<QString,QString> &startChan /* NITAddScan */,
    const QString &freq_std /* FullScan */,
    const QString &mod /* FullScan */,
    const QString &tbl /* FullScan */,
    const QString &tbl_start /* FullScan optional */,
    const QString &tbl_end   /* FullScan optional */)
{
    m_freeToAirOnly = do_fta_only;
    m_channelNumbersOnly = do_lcn_only;
    m_completeOnly = do_complete_only;
    m_addFullTS = do_add_full_ts;
    m_serviceRequirements = service_requirements;
    m_sourceid = sourceid;

    PreScanCommon(scantype, cardid, inputname,
                  sourceid, do_ignore_signal_timeout, do_test_decryption);

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Scan()");

    if (!m_sigmonScanner)
    {
        LOG(VB_CHANSCAN, LOG_ERR, LOC + "Scan(): scanner does not exist...");
        return;
    }

    m_sigmonScanner->StartScanner();
    m_scanMonitor->ScanUpdateStatusText("");

    bool ok = false;

    if ((ScanTypeSetting::FullScan_ATSC   == scantype) ||
        (ScanTypeSetting::FullScan_DVBC   == scantype) ||
        (ScanTypeSetting::FullScan_DVBT   == scantype) ||
        (ScanTypeSetting::FullScan_DVBT2  == scantype) ||
        (ScanTypeSetting::FullScan_Analog == scantype))
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("ScanTransports(%1, %2, %3)")
                .arg(freq_std).arg(mod).arg(tbl));

        // HACK HACK HACK -- begin
        // if using QAM we may need additional time... (at least with HD-3000)
        if ((mod.startsWith("qam", Qt::CaseInsensitive)) &&
            (m_sigmonScanner->GetSignalTimeout() < 1000))
        {
            m_sigmonScanner->SetSignalTimeout(1000);
        }
        // HACK HACK HACK -- end

        m_sigmonScanner->SetAnalog(ScanTypeSetting::FullScan_Analog == scantype);

        ok = m_sigmonScanner->ScanTransports(
            sourceid, freq_std, mod, tbl, tbl_start, tbl_end);
    }
    else if ((ScanTypeSetting::NITAddScan_DVBT  == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBT2 == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBS  == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBS2 == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBC  == scantype))
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + "ScanTransports()");

        ok = m_sigmonScanner->ScanTransportsStartingOn(sourceid, startChan);
    }
    else if (ScanTypeSetting::FullTransportScan == scantype)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("ScanExistingTransports(%1)")
                .arg(sourceid));

        ok = m_sigmonScanner->ScanExistingTransports(sourceid, do_follow_nit);
        if (ok)
        {
            m_scanMonitor->ScanPercentComplete(0);
        }
        else
        {
            InformUser(tr("Error tuning to transport"));
            Teardown();
        }
    }
    else if ((ScanTypeSetting::DVBUtilsImport == scantype) && !m_channels.empty())
    {
        ok = true;

        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("ScanForChannels(%1)").arg(sourceid));

        QString card_type = CardUtil::GetRawInputType(cardid);
        QString sub_type  = card_type;
        if (card_type == "DVB")
        {
            QString device = CardUtil::GetVideoDevice(cardid);

            ok = !device.isEmpty();
            if (ok)
                sub_type = CardUtil::ProbeDVBType(device).toUpper();
        }

        if (ok)
        {
            ok = m_sigmonScanner->ScanForChannels(sourceid, freq_std,
                                                sub_type, m_channels);
        }
        if (ok)
        {
            m_scanMonitor->ScanPercentComplete(0);
        }
        else
        {
            InformUser(tr("Error tuning to transport"));
            Teardown();
        }
    }
    else if (ScanTypeSetting::IPTVImportMPTS == scantype)
    {
        if (m_iptv_channels.empty())
        {
            LOG(VB_CHANSCAN, LOG_INFO, LOC + "IPTVImportMPTS: no channels");
        }
        else
        {
            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("ScanIPTVChannels(%1) IPTV MPTS").arg(sourceid));

            if ((ok = m_sigmonScanner->ScanIPTVChannels(sourceid, m_iptv_channels)))
                m_scanMonitor->ScanPercentComplete(0);
            else
            {
                InformUser(tr("Error scanning MPTS in IPTV"));
                Teardown();
            }
        }
    }
    else if (ScanTypeSetting::TransportScan == scantype)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("ScanTransport(%1)").arg(mplexid));

        ok = m_sigmonScanner->ScanTransport(mplexid, do_follow_nit);
    }
    else if (ScanTypeSetting::CurrentTransportScan == scantype)
    {
        QString sistandard = "mpeg";
        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            "ScanCurrentTransport(" + sistandard + ")");
        ok = m_sigmonScanner->ScanCurrentTransport(sistandard);
    }
    else if (ScanTypeSetting::ExternRecImport == scantype)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            "Importing channels from External Recorder");
        ok = ImportExternRecorder(cardid, inputname, sourceid);
    }

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to handle tune complete.");
        InformUser(tr("Programmer Error: "
                      "Failed to handle tune complete."));
    }
}

DTVConfParser::return_t ChannelScanner::ImportDVBUtils(
    uint sourceid, int cardtype, const QString &file)
{
    m_sourceid = sourceid;
    m_channels.clear();

    DTVConfParser::cardtype_t type = DTVConfParser::UNKNOWN;
    type = ((CardUtil::DVBT == cardtype) ||
            (CardUtil::DVBT2 == cardtype)) ? DTVConfParser::OFDM : type;
    type = (CardUtil::QPSK == cardtype) ? DTVConfParser::QPSK : type;
    type = (CardUtil::DVBC == cardtype) ? DTVConfParser::QAM  : type;
    type = (CardUtil::DVBS2 == cardtype) ? DTVConfParser::DVBS2 : type;
    type = ((CardUtil::ATSC == cardtype) ||
            (CardUtil::HDHOMERUN == cardtype)) ? DTVConfParser::ATSC : type;

    DTVConfParser::return_t ret = DTVConfParser::OK;
    if (type == DTVConfParser::UNKNOWN)
        ret = DTVConfParser::ERROR_CARDTYPE;
    else
    {
        DTVConfParser parser(type, sourceid, file);

        ret = parser.Parse();
        if (DTVConfParser::OK == ret)
            m_channels = parser.GetChannels();
    }

    if (DTVConfParser::OK != ret)
    {
        QString msg = (DTVConfParser::ERROR_PARSE == ret) ?
            tr("Failed to parse '%1'").arg(file) :
            ((DTVConfParser::ERROR_CARDTYPE == ret) ?
             tr("Programmer Error : incorrect card type") :
             tr("Failed to open '%1'").arg(file));

        InformUser(msg);
    }

    return ret;
}

bool ChannelScanner::ImportM3U(uint cardid, const QString &inputname,
                               uint sourceid, bool is_mpts)
{
    (void) cardid;
    (void) inputname;
    m_sourceid = sourceid;

    if (!m_scanMonitor)
        m_scanMonitor = new ScanMonitor(this);

    // Create an IPTV scan object
    m_iptvScanner = new IPTVChannelFetcher(cardid, inputname, sourceid,
                                         is_mpts, m_scanMonitor);

    MonitorProgress(false, false, false, false);

    m_iptvScanner->Scan();

    if (is_mpts)
        m_iptv_channels = m_iptvScanner->GetChannels();

    return true;
}

bool ChannelScanner::ImportVBox(uint cardid, const QString &inputname, uint sourceid,
                                bool ftaOnly, ServiceRequirements serviceType)
{
    m_sourceid = sourceid;
#ifdef USING_VBOX
    if (!m_scanMonitor)
        m_scanMonitor = new ScanMonitor(this);

    // Create a VBox scan object
    m_vboxScanner = new VBoxChannelFetcher(cardid, inputname, sourceid, ftaOnly, serviceType, m_scanMonitor);

    MonitorProgress(false, false, false, false);

    m_vboxScanner->Scan();

    return true;
#else
    (void) cardid;
    (void) inputname;
    return false;
#endif
}

bool ChannelScanner::ImportExternRecorder(uint cardid, const QString &inputname,
                                          uint sourceid)
{
    m_sourceid = sourceid;
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    if (!m_scanMonitor)
        m_scanMonitor = new ScanMonitor(this);

    // Create a External Recorder Channel Fetcher
    m_ExternRecScanner = new ExternRecChannelScanner(cardid,
                                                     inputname,
                                                     sourceid,
                                                     m_scanMonitor);

    MonitorProgress(false, false, false, false);

    m_ExternRecScanner->Scan();

    return true;
#else
    (void) cardid;
    (void) inputname;

    return false;
#endif
}

void ChannelScanner::PreScanCommon(
    int scantype,
    uint cardid,
    const QString &inputname,
    uint sourceid,
    bool do_ignore_signal_timeout,
    bool do_test_decryption)
{
    uint signal_timeout  = 1000;
    uint channel_timeout = 40000;
    CardUtil::GetTimeouts(cardid, signal_timeout, channel_timeout);

    QString device = CardUtil::GetVideoDevice(cardid);
    if (device.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No Device");
        InformUser(tr("Programmer Error: No Device"));
        return;
    }

    if (!m_scanMonitor)
        m_scanMonitor = new ScanMonitor(this);

    QString card_type = CardUtil::GetRawInputType(cardid);

    if ("DVB" == card_type)
    {
        QString sub_type = CardUtil::ProbeDVBType(device).toUpper();
        bool need_nit = (("QAM"    == sub_type) ||
                         ("QPSK"   == sub_type) ||
                         ("OFDM"   == sub_type) ||
                         ("DVB_T2" == sub_type));

        // Ugh, Some DVB drivers don't fully support signal monitoring...
        if ((ScanTypeSetting::TransportScan     == scantype) ||
            (ScanTypeSetting::FullTransportScan == scantype))
        {
            signal_timeout = (do_ignore_signal_timeout) ?
                channel_timeout * 10 : signal_timeout;
        }

        // ensure a minimal signal timeout of 1 second
        signal_timeout = max(signal_timeout, 1000U);

        // Make sure that channel_timeout is at least 7 seconds to catch
        // at least one SDT section. kDVBTableTimeout in ChannelScanSM
        // ensures that we catch the NIT then.
        channel_timeout = max(channel_timeout, need_nit * 7 * 1000U);
    }

#ifdef USING_DVB
    if ("DVB" == card_type)
        m_channel = new DVBChannel(device);
#endif

#ifdef USING_V4L2
    if (("V4L" == card_type) || ("MPEG" == card_type))
        m_channel = new V4LChannel(nullptr, device);
#endif

#ifdef USING_HDHOMERUN
    if ("HDHOMERUN" == card_type)
    {
        m_channel = new HDHRChannel(nullptr, device);
    }
#endif // USING_HDHOMERUN

#ifdef USING_ASI
    if ("ASI" == card_type)
    {
        m_channel = new ASIChannel(nullptr, device);
    }
#endif // USING_ASI

#ifdef USING_IPTV
    if ("FREEBOX" == card_type)
    {
        m_channel = new IPTVChannel(nullptr, device);
    }
#endif

#ifdef USING_VBOX
    if ("VBOX" == card_type)
    {
        m_channel = new IPTVChannel(nullptr, device);
    }
#endif

#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    if ("EXTERNAL" == card_type)
    {
        m_channel = new ExternalChannel(nullptr, device);
    }
#endif

    if (!m_channel)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Channel not created");
        InformUser(tr("Programmer Error: Channel not created"));
        return;
    }

    // explicitly set the cardid
    m_channel->SetInputID(cardid);

    // If the backend is running this may fail...
    if (!m_channel->Open())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Channel could not be opened");
        InformUser(tr("Channel could not be opened."));
        return;
    }

    ScanMonitor *lis = m_scanMonitor;

    m_sigmonScanner = new ChannelScanSM(lis, card_type, m_channel,
                                      sourceid, signal_timeout, channel_timeout,
                                      inputname, do_test_decryption);

    // If we know the channel types we can give the signal montior a hint.
    // Since we unfortunately do not record this info in the DB, we cannot
    // do this for the other scan types and have to guess later on...
    switch (scantype)
    {
        case ScanTypeSetting::FullScan_ATSC:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeATSC);
            break;
        case ScanTypeSetting::FullScan_DVBC:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBC);
            break;
        case ScanTypeSetting::FullScan_DVBT:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBT);
            break;
        case ScanTypeSetting::FullScan_DVBT2:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBT2);
            break;
        case ScanTypeSetting::NITAddScan_DVBT:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBT);
            break;
        case ScanTypeSetting::NITAddScan_DVBT2:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBT2);
            break;
        case ScanTypeSetting::NITAddScan_DVBS:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBS1);
            break;
        case ScanTypeSetting::NITAddScan_DVBS2:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBS2);
            break;
        case ScanTypeSetting::NITAddScan_DVBC:
            m_sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBC);
            break;
        default:
            break;
    }

    // Signal Meters are connected here
    SignalMonitor *mon = m_sigmonScanner->GetSignalMonitor();
    if (mon)
        mon->AddListener(lis);

    DVBSignalMonitor *dvbm = nullptr;
    bool using_rotor = false;

#ifdef USING_DVB
    // cppcheck-suppress redundantAssignment
    dvbm = m_sigmonScanner->GetDVBSignalMonitor();
    if (dvbm && mon)
        using_rotor = mon->HasFlags(SignalMonitor::kDVBSigMon_WaitForPos);
#endif // USING_DVB

    MonitorProgress(mon, mon, dvbm, using_rotor);
}
