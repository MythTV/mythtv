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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include "dvbchannel.h"
#include "v4lchannel.h"
#include "cardutil.h"

#define LOC QString("ChScan: ")

ChannelScanner::ChannelScanner() :
    scanMonitor(NULL), channel(NULL), sigmonScanner(NULL), iptvScanner(NULL),
    freeToAirOnly(false), serviceRequirements(kRequireAV)
{
}

ChannelScanner::~ChannelScanner()
{
    Teardown();

    if (scanMonitor)
    {
        scanMonitor->deleteLater();
        scanMonitor = NULL;
    }
}

void ChannelScanner::Teardown(void)
{
    if (sigmonScanner)
    {
        delete sigmonScanner;
        sigmonScanner = NULL;
    }

    if (channel)
    {
        delete channel;
        channel = NULL;
    }

    if (iptvScanner)
    {
        iptvScanner->Stop();
        delete iptvScanner;
        iptvScanner = NULL;
    }

    if (scanMonitor)
    {
        scanMonitor->deleteLater();
        scanMonitor = NULL;
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
    freeToAirOnly = do_fta_only;
    serviceRequirements = service_requirements;

    PreScanCommon(scantype, cardid, inputname,
                  sourceid, do_ignore_signal_timeout, do_test_decryption);

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Scan()");

    if (!sigmonScanner)
    {
        LOG(VB_CHANSCAN, LOG_ERR, LOC + "Scan(): scanner does not exist...");
        return;
    }

    sigmonScanner->StartScanner();
    scanMonitor->ScanUpdateStatusText("");

    bool ok = false;

    if ((ScanTypeSetting::FullScan_ATSC   == scantype) ||
        (ScanTypeSetting::FullScan_DVBC   == scantype) ||
        (ScanTypeSetting::FullScan_DVBT   == scantype) ||
        (ScanTypeSetting::FullScan_Analog == scantype))
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("ScanTransports(%1, %2, %3)")
                .arg(freq_std).arg(mod).arg(tbl));

        // HACK HACK HACK -- begin
        // if using QAM we may need additional time... (at least with HD-3000)
        if ((mod.left(3).toLower() == "qam") &&
            (sigmonScanner->GetSignalTimeout() < 1000))
        {
            sigmonScanner->SetSignalTimeout(1000);
        }
        // HACK HACK HACK -- end

        sigmonScanner->SetAnalog(ScanTypeSetting::FullScan_Analog == scantype);

        ok = sigmonScanner->ScanTransports(
            sourceid, freq_std, mod, tbl, tbl_start, tbl_end);
    }
    else if ((ScanTypeSetting::NITAddScan_DVBT  == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBS  == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBS2 == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBC  == scantype))
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + "ScanTransports()");

        ok = sigmonScanner->ScanTransportsStartingOn(sourceid, startChan);
    }
    else if (ScanTypeSetting::FullTransportScan == scantype)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("ScanExistingTransports(%1)")
                .arg(sourceid));

        ok = sigmonScanner->ScanExistingTransports(sourceid, do_follow_nit);
        if (ok)
        {
            scanMonitor->ScanPercentComplete(0);
        }
        else
        {
            InformUser(tr("Error tuning to transport"));
            Teardown();
        }
    }
    else if ((ScanTypeSetting::DVBUtilsImport == scantype) && channels.size())
    {
        ok = true;

        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("ScanForChannels(%1)").arg(sourceid));

        QString card_type = CardUtil::GetRawCardType(cardid);
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
            ok = sigmonScanner->ScanForChannels(sourceid, freq_std,
                                          sub_type, channels);
        }
        if (ok)
        {
            scanMonitor->ScanPercentComplete(0);
        }
        else
        {
            InformUser(tr("Error tuning to transport"));
            Teardown();
        }
    }
    else if (ScanTypeSetting::TransportScan == scantype)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("ScanTransport(%1)").arg(mplexid));

        ok = sigmonScanner->ScanTransport(mplexid, do_follow_nit);
    }
    else if (ScanTypeSetting::CurrentTransportScan == scantype)
    {
        QString sistandard = "mpeg";
        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            "ScanCurrentTransport(" + sistandard + ")");
        ok = sigmonScanner->ScanCurrentTransport(sistandard);
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
    channels.clear();

    DTVConfParser::cardtype_t type = DTVConfParser::UNKNOWN;
    type = (CardUtil::DVBT == cardtype) ? DTVConfParser::OFDM : type;
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
            channels = parser.GetChannels();
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

bool ChannelScanner::ImportM3U(
    uint cardid, const QString &inputname, uint sourceid)
{
    (void) cardid;
    (void) inputname;
    (void) sourceid;

    if (!scanMonitor)
        scanMonitor = new ScanMonitor(this);

    // Create an IPTV scan object
    iptvScanner = new IPTVChannelFetcher(
        cardid, inputname, sourceid, scanMonitor);

    MonitorProgress(false, false, false, false);

    iptvScanner->Scan();

    return true;
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

    if (!scanMonitor)
        scanMonitor = new ScanMonitor(this);

    QString card_type = CardUtil::GetRawCardType(cardid);

    if ("DVB" == card_type)
    {
        QString sub_type = CardUtil::ProbeDVBType(device).toUpper();
        bool need_nit = (("QAM"  == sub_type) ||
                         ("QPSK" == sub_type) ||
                         ("OFDM" == sub_type));

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
        channel = new DVBChannel(device);
#endif

#ifdef USING_V4L2
    if (("V4L" == card_type) || ("MPEG" == card_type))
        channel = new V4LChannel(NULL, device);
#endif

#ifdef USING_HDHOMERUN
    if ("HDHOMERUN" == card_type)
    {
        channel = new HDHRChannel(NULL, device);
    }
#endif // USING_HDHOMERUN

#ifdef USING_ASI
    if ("ASI" == card_type)
    {
        channel = new ASIChannel(NULL, device);
    }
#endif // USING_ASI

    if (!channel)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Channel not created");
        InformUser(tr("Programmer Error: Channel not created"));
        return;
    }

    // explicitly set the cardid
    channel->SetCardID(cardid);

    // If the backend is running this may fail...
    if (!channel->Open())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Channel could not be opened");
        InformUser(tr("Channel could not be opened."));
        return;
    }

    ScanMonitor *lis = scanMonitor;

    sigmonScanner = new ChannelScanSM(
        lis, card_type, channel, sourceid,
        signal_timeout, channel_timeout, inputname,
        do_test_decryption);

    // If we know the channel types we can give the signal montior a hint.
    // Since we unfortunately do not record this info in the DB, we cannot
    // do this for the other scan types and have to guess later on...
    switch (scantype)
    {
        case ScanTypeSetting::FullScan_ATSC:
            sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeATSC);
            break;
        case ScanTypeSetting::FullScan_DVBC:
            sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBC);
            break;
        case ScanTypeSetting::FullScan_DVBT:
            sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBT);
            break;
        case ScanTypeSetting::NITAddScan_DVBT:
            sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBT);
            break;
        case ScanTypeSetting::NITAddScan_DVBS:
            sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBS1);
            break;
        case ScanTypeSetting::NITAddScan_DVBS2:
            sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBS2);
            break;
        case ScanTypeSetting::NITAddScan_DVBC:
            sigmonScanner->SetScanDTVTunerType(DTVTunerType::kTunerTypeDVBC);
            break;
        default:
            break;
    }

    // Signal Meters are connected here
    SignalMonitor *mon = sigmonScanner->GetSignalMonitor();
    if (mon)
        mon->AddListener(lis);

    DVBSignalMonitor *dvbm = NULL;
    bool using_rotor = false;

#ifdef USING_DVB
    dvbm = sigmonScanner->GetDVBSignalMonitor();
    if (dvbm && mon)
        using_rotor = mon->HasFlags(SignalMonitor::kDVBSigMon_WaitForPos);
#endif // USING_DVB

    MonitorProgress(mon, mon, dvbm, using_rotor);
}
