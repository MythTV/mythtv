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

#include "channelscanner.h"
#include "cardutil.h"
#include "iptvchannelfetcher.h"
#include "channelscan_sm.h"
#include "scanmonitor.h"
#include "scanwizardconfig.h"

#include "v4lchannel.h"
#include "analogsignalmonitor.h"
#include "dvbchannel.h"
#include "dvbsignalmonitor.h"
#include "hdhrchannel.h"

#define LOC QString("ChScan: ")
#define LOC_ERR QString("ChScan, Error: ")

ChannelScanner::ChannelScanner() :
    scanMonitor(NULL), channel(NULL), sigmonScanner(NULL), freeboxScanner(NULL)
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

#ifdef USING_IPTV
    if (freeboxScanner)
    {
        freeboxScanner->Stop();
        delete freeboxScanner;
        freeboxScanner = NULL;
    }
#endif // USING_IPTV

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
    // stuff needed for particular scans
    uint           mplexid /* TransportScan */,
    const QMap<QString,QString> &startChan /* NITAddScan */,
    const QString &freq_std /* FullScan */,
    const QString &mod /* FullScan */,
    const QString &tbl /* FullScan */)
{
    PreScanCommon(scantype, cardid, inputname,
                  sourceid, do_ignore_signal_timeout);

    VERBOSE(VB_SIPARSER, LOC + "Scan()");

    if (!sigmonScanner)
    {
        VERBOSE(VB_SIPARSER, LOC + "Scan(): "
                "scanner does not exist...");
        return;
    }

    sigmonScanner->StartScanner();
    scanMonitor->ScanUpdateStatusText("");

    bool ok = false;

    if ((ScanTypeSetting::FullScan_ATSC   == scantype) ||
        (ScanTypeSetting::FullScan_DVBT   == scantype) ||
        (ScanTypeSetting::FullScan_Analog == scantype))
    {
        VERBOSE(VB_SIPARSER, LOC +
                "ScanTransports("<<freq_std<<", "<<mod<<", "<<tbl<<")");

        // HACK HACK HACK -- begin
        // if using QAM we may need additional time... (at least with HD-3000)
        if ((mod.left(3).lower() == "qam") &&
            (sigmonScanner->GetSignalTimeout() < 1000))
        {
            sigmonScanner->SetSignalTimeout(1000);
        }
        // HACK HACK HACK -- end

        sigmonScanner->SetAnalog(ScanTypeSetting::FullScan_Analog == scantype);

        ok = sigmonScanner->ScanTransports(sourceid, freq_std, mod, tbl);
    }
    else if ((ScanTypeSetting::NITAddScan_DVBT == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBS == scantype) ||
             (ScanTypeSetting::NITAddScan_DVBC == scantype))
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanTransports()");

        ok = sigmonScanner->ScanTransportsStartingOn(sourceid, startChan);
    }
    else if (ScanTypeSetting::FullTransportScan == scantype)
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanServicesSourceID("<<sourceid<<")");

        ok = sigmonScanner->ScanServicesSourceID(sourceid);
        if (ok)
        {
            scanMonitor->ScanPercentComplete(0);
        }
        else
        {
            InformUser(QObject::tr("Error tuning to transport"));
            Teardown();
        }
    }
    else if ((ScanTypeSetting::DVBUtilsImport == scantype) && channels.size())
    {
        ok = true;

        VERBOSE(VB_SIPARSER, LOC + "ScanForChannels("<<sourceid<<")");

        QString card_type = CardUtil::GetRawCardType(cardid);
        QString sub_type  = card_type;
        if (card_type == "DVB")
        {
            QString device = CardUtil::GetVideoDevice(cardid);

            ok = !device.isEmpty();
            if (ok)
                sub_type = CardUtil::ProbeDVBType(device.toUInt()).upper();
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
            InformUser(QObject::tr("Error tuning to transport"));
            Teardown();
        }
    }
    else if (ScanTypeSetting::TransportScan == scantype)
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanTransport("<<mplexid<<")");

        ok = sigmonScanner->ScanTransport(mplexid);
    }

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to handle tune complete.");
        InformUser(QObject::tr("Programmer Error: "
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
            QObject::tr("Failed to parse '%1'") :
            ((DTVConfParser::ERROR_CARDTYPE == ret) ?
             QString("Programmer Error : incorrect card type") :
             QObject::tr("Failed to open '%1'"));

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
    bool ok = false;

#ifdef USING_IPTV
    // Create an IPTV scan object
    freeboxScanner = new IPTVChannelFetcher(
        cardid, inputname, sourceid, scanMonitor);

    MonitorProgress(false, false, false, false);

    ok = freeboxScanner->Scan();
#endif // USING_IPTV

    if (!ok)
        InformUser(QObject::tr("Error starting scan"));

    return ok;
}

void ChannelScanner::PreScanCommon(
    int scantype,
    uint cardid,
    const QString &inputname,
    uint sourceid,
    bool do_ignore_signal_timeout)
{
    uint signal_timeout  = 1000;
    uint channel_timeout = 40000;
    CardUtil::GetTimeouts(cardid, signal_timeout, channel_timeout);

    QString device = CardUtil::GetVideoDevice(cardid);
    if (device.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No Device");
        InformUser(QObject::tr("Programmer Error: No Device"));
        return;
    }

    if (!scanMonitor)
        scanMonitor = new ScanMonitor(this);

    QString card_type = CardUtil::GetRawCardType(cardid);

    if ("DVB" == card_type)
    {
        QString sub_type = CardUtil::ProbeDVBType(device.toUInt()).upper();
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

        // Since we the NIT is only sent every 10 seconds, we add an
        // extra 20 + 2 seconds to the scan time for DVB countries.
        channel_timeout += (need_nit) ? 22 * 1000 : 0;
    }

#ifdef USING_DVB
    if ("DVB" == card_type)
        channel = new DVBChannel(device.toInt());
#endif

#ifdef USING_V4L
    if (("V4L" == card_type) || ("MPEG" == card_type))
        channel = new V4LChannel(NULL, device);
#endif

#ifdef USING_HDHOMERUN
    if ("HDHOMERUN" == card_type)
    {
        uint tuner = CardUtil::GetHDHRTuner(cardid);
        channel = new HDHRChannel(NULL, device, tuner);
    }
#endif // USING_HDHOMERUN

    if (!channel)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Channel not created");
        InformUser(QObject::tr("Programmer Error: Channel not created"));
        return;
    }

    // explicitly set the cardid
    channel->SetCardID(cardid);

    // If the backend is running this may fail...
    if (!channel->Open())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Channel could not be opened");
        InformUser(QObject::tr("Channel could not be opened."));
        return;
    }

    ScanMonitor *lis = scanMonitor;

    sigmonScanner = new ChannelScanSM(
        lis, card_type, channel, sourceid,
        signal_timeout, channel_timeout, inputname);

    // Signal Meters are connected here
    SignalMonitor *mon = sigmonScanner->GetSignalMonitor();
    if (mon)
        mon->AddListener(lis);

    DVBSignalMonitor *dvbm = NULL;
    bool using_rotor = false;

#ifdef USING_DVB
    dvbm = sigmonScanner->GetDVBSignalMonitor();
    if (dvbm)
        using_rotor = mon->HasFlags(SignalMonitor::kDVBSigMon_WaitForPos);
#endif // USING_DVB

    MonitorProgress(mon, mon, dvbm, using_rotor);
}
