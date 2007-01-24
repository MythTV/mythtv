/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionallity
 *
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

// System headers
#include <unistd.h>

// Qt headers
#include <qapplication.h>

// MythTV headers
#include "mythcontext.h"
#include "scanwizard.h"
#include "scanwizardhelpers.h"
#include "scanwizardscanner.h"
#include "channelbase.h"
#include "dtvsignalmonitor.h"
#include "siscan.h"
#include "dvbconfparser.h"

#ifdef USING_V4L
#include "channel.h"
#include "pchdtvsignalmonitor.h"
#include "analogsignalmonitor.h"
#endif

#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbsignalmonitor.h"
#endif

#ifdef USING_HDHOMERUN
#include "hdhrchannel.h"
#include "hdhrsignalmonitor.h"
#endif

#include "iptvchannelfetcher.h"

#define LOC QString("SWizScan: ")
#define LOC_ERR QString("SWizScan, Error: ")

/// Percentage to set to after the transports have been scanned
#define TRANSPORT_PCT 6
/// Percentage to set to after the first tune
#define TUNED_PCT     3

QString ScanWizardScanner::kTitle = QString::null;

// kTitel must be initialized after the Qt translation system is initialized...
static void init_statics(void)
{
    static QMutex lock;
    static bool do_init = true;
    QMutexLocker locker(&lock);
    if (do_init)
    {
        ScanWizardScanner::kTitle = ScanWizardScanner::tr("Scanning");
        do_init = false;
    }
}

void post_event(QObject *dest, ScannerEvent::TYPE type, int val)
{
    ScannerEvent* e = new ScannerEvent(type);
    e->intValue(val);
    QApplication::postEvent(dest, e);
}

ScanWizardScanner::ScanWizardScanner(void)
    : VerticalConfigurationGroup(false, true, false, false),
      log(new LogList()),
      channel(NULL),                popupProgress(NULL),
      scanner(NULL),                analogScanner(NULL),
      freeboxScanner(NULL),
      frequency(0),                 modulation("8vsb")
{
    init_statics();

    setLabel(kTitle);
    addChild(log);
}

void ScanWizardScanner::Teardown()
{
    // Join the thread and close the channel
    if (scanner)
    {
        delete scanner; // TODO we should use deleteLater...
        scanner = NULL;
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
        freeboxScanner->deleteLater();
        freeboxScanner = NULL;
    }
#endif

    if (popupProgress)
    {
        delete popupProgress; // TODO we should use deleteLater...
        popupProgress = NULL;
    }
}

void ScanWizardScanner::customEvent(QCustomEvent *e)
{
    ScannerEvent *scanEvent = (ScannerEvent*) e;
    if ((popupProgress == NULL) &&
        (scanEvent->eventType() != ScannerEvent::Update))
    {
        return;
    }

    switch (scanEvent->eventType())
    {
        case ScannerEvent::ServiceScanComplete:
            popupProgress->progress(PROGRESS_MAX);
            Teardown();
            break;
        case ScannerEvent::Update:
            log->updateText(scanEvent->strValue());
            break;
        case ScannerEvent::TableLoaded:
            popupProgress->incrementProgress();
            break;
        case ScannerEvent::ServicePct:
            popupProgress->progress(
                (scanEvent->intValue() * PROGRESS_MAX) / 100);
            break;
        case ScannerEvent::DVBLock:
            popupProgress->dvbLock(scanEvent->intValue());
            break;
        case ScannerEvent::DVBSNR:
            popupProgress->signalToNoise(scanEvent->intValue());
            break;
        case ScannerEvent::DVBSignalStrength:
            popupProgress->signalStrength(scanEvent->intValue());
            break;
    }
}

void ScanWizardScanner::scanComplete()
{
    ScannerEvent::TYPE se = ScannerEvent::ServiceScanComplete;
    QApplication::postEvent(this, new ScannerEvent(se));
}

void ScanWizardScanner::transportScanComplete()
{
    scanner->ScanServicesSourceID(nVideoSource);
    ScannerEvent* e = new ScannerEvent(ScannerEvent::ServicePct);
    e->intValue(TRANSPORT_PCT);
    QApplication::postEvent(this, e);
}

void ScanWizardScanner::serviceScanPctComplete(int pct)
{
    ScannerEvent* e = new ScannerEvent(ScannerEvent::ServicePct);
    int tmp = TRANSPORT_PCT + ((100 - TRANSPORT_PCT) * pct)/100;
    e->intValue(tmp);
    QApplication::postEvent(this, e);
}

void ScanWizardScanner::updateText(const QString &str)
{
    if (str.isEmpty())
        return;
    ScannerEvent* e = new ScannerEvent(ScannerEvent::Update);
    e->strValue(str);
    QApplication::postEvent(this, e);
}

void ScanWizardScanner::updateStatusText(const QString &str)
{
    if (str.isEmpty())
        return;
    if (popupProgress)
        popupProgress->status(tr("Scanning")+" "+str);
}

void ScanWizardScanner::dvbLock(const SignalMonitorValue &val)
{
    dvbLock(val.GetValue());
}

void ScanWizardScanner::dvbSNR(const SignalMonitorValue &val)
{
    dvbSNR(val.GetNormalizedValue(0, 65535));
}

void ScanWizardScanner::dvbSignalStrength(const SignalMonitorValue &val)
{
    dvbSignalStrength(val.GetNormalizedValue(0, 65535));
}

void ScanWizardScanner::dvbLock(int locked)
{
    ScannerEvent* e = new ScannerEvent(ScannerEvent::DVBLock);
    e->intValue(locked);
    QApplication::postEvent(this, e);
}

void ScanWizardScanner::dvbSNR(int i)
{
    ScannerEvent* e = new ScannerEvent(ScannerEvent::DVBSNR);
    e->intValue(i);
    QApplication::postEvent(this, e);
}

void ScanWizardScanner::dvbSignalStrength(int i)
{
    ScannerEvent* e = new ScannerEvent(ScannerEvent::DVBSignalStrength);
    e->intValue(i);
    QApplication::postEvent(this, e);
}

// full scan of existing transports broken
// existing transport scan broken
void ScanWizardScanner::Scan(
    int            scantype,
    uint           parent_cardid,
    uint           child_cardid,
    const QString &inputname,
    uint           sourceid,
    bool           do_delete_channels,
    bool           do_rename_channels,
    bool           do_ignore_signal_timeout,
    // stuff needed for particular scans
    uint           mplexid /* TransportScan */,
    const QMap<QString,QString> &startChan /* NITAddScan */,
    const QString &freq_std /* FullScan */,
    const QString &mod /* FullScan */,
    const QString &tbl /* FullScan */,
    const QString &atsc_format /* any ATSC scan */)
{
    nVideoSource = sourceid;
    PreScanCommon(scantype, parent_cardid, child_cardid, inputname,
                  sourceid, do_ignore_signal_timeout);

    VERBOSE(VB_SIPARSER, LOC + "HandleTuneComplete()");

    if (!scanner)
    {
        VERBOSE(VB_SIPARSER, LOC + "HandleTuneComplete(): "
                "scanner does not exist...");
        return;
    }

    scanner->StartScanner();

    popupProgress->status(tr("Scanning"));
    popupProgress->progress( (TUNED_PCT * PROGRESS_MAX) / 100 );

    bool ok = false;

    if (do_delete_channels && (ScanTypeSetting::TransportScan == scantype))
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM channel "
                      "WHERE sourceid = :SOURCEID AND "
                      "      mplexid  = :MPLEXID");
        query.bindValue(":SOURCEID", sourceid);
        query.bindValue(":MPLEXID",  mplexid);
        query.exec();
    }
    else if (do_delete_channels)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM channel "
                      "WHERE sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", sourceid);
        query.exec();

        if (ScanTypeSetting::TransportScan != scantype)
        {
            query.prepare("DELETE FROM dtv_multiplex "
                          "WHERE sourceid = :SOURCEID");
            query.bindValue(":SOURCEID", sourceid);
            query.exec();
        }
    }

    scanner->SetChannelFormat(atsc_format);
    scanner->SetRenameChannels(do_rename_channels);

    if ((ScanTypeSetting::FullScan_ATSC   == scantype) ||
        (ScanTypeSetting::FullScan_OFDM   == scantype) ||
        (ScanTypeSetting::FullScan_Analog == scantype))
    {
        VERBOSE(VB_SIPARSER, LOC +
                "ScanTransports("<<freq_std<<", "<<mod<<", "<<tbl<<")");

        // HACK HACK HACK -- begin
        // if using QAM we may need additional time... (at least with HD-3000)
        if ((mod.left(3).lower() == "qam") &&
            (scanner->GetSignalTimeout() < 1000))
        {
            scanner->SetSignalTimeout(1000);
        }
        // HACK HACK HACK -- end

        scanner->SetAnalog(ScanTypeSetting::FullScan_Analog == scantype);

        ok = scanner->ScanTransports(sourceid, freq_std, mod, tbl);
    }
    else if ((ScanTypeSetting::NITAddScan_OFDM == scantype) ||
             (ScanTypeSetting::NITAddScan_QPSK == scantype) ||
             (ScanTypeSetting::NITAddScan_QAM  == scantype))
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanTransports()");

        ok = scanner->ScanTransportsStartingOn(sourceid, startChan);
    }
    else if (ScanTypeSetting::FullTransportScan == scantype)
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanServicesSourceID("<<sourceid<<")");

        ok = scanner->ScanServicesSourceID(sourceid);
        if (ok)
        {
            post_event(this, ScannerEvent::ServicePct,
                       TRANSPORT_PCT);
        }
        else
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      tr("ScanWizard"),
                                      tr("Error tuning to transport"));
            Teardown();
        }
    }
    else if ((ScanTypeSetting::DVBUtilsImport == scantype) && channels.size())
    {
        ok = true;

        VERBOSE(VB_SIPARSER, LOC + "ScanForChannels("<<sourceid<<")");

        QString card_type = CardUtil::GetRawCardType(parent_cardid, inputname);
        QString sub_type  = card_type;
        if (card_type == "DVB")
        {
            QString device = CardUtil::GetVideoDevice(
                parent_cardid, inputname);

            ok = !device.isEmpty();
            if (ok)
                sub_type = CardUtil::ProbeDVBType(device.toUInt()).upper();
        }

        if (ok)
        {
            ok = scanner->ScanForChannels(sourceid, freq_std,
                                          sub_type, channels);
        }
        if (ok)
        {
            post_event(this, ScannerEvent::ServicePct,
                       TRANSPORT_PCT);
        }
        else
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      tr("ScanWizard"),
                                      tr("Error tuning to transport"));
            Teardown();
        }
    }
    else if (ScanTypeSetting::TransportScan == scantype)
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanTransport("<<mplexid<<")");

        ok = scanner->ScanTransport(mplexid);
    }

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "Failed to handle tune complete.");
    }
}

void ScanWizardScanner::ImportDVBUtils(uint sourceid, int cardtype,
                                       const QString &file)
{
    channels.clear();

    DTVConfParser::cardtype_t type = DTVConfParser::UNKNOWN;
    type = (CardUtil::OFDM == cardtype) ? DTVConfParser::OFDM : type;
    type = (CardUtil::QPSK == cardtype) ? DTVConfParser::QPSK : type;
    type = (CardUtil::QAM  == cardtype) ? DTVConfParser::QAM  : type;
    type = ((CardUtil::ATSC == cardtype) || (CardUtil::HDTV == cardtype) ||
            (CardUtil::HDHOMERUN == cardtype)) ? DTVConfParser::ATSC : type;

    if (type == DTVConfParser::UNKNOWN)
        return;

    DTVConfParser parser(type, sourceid, file);

    DTVConfParser::return_t ret = parser.Parse();
    if (DTVConfParser::OK != ret)
    {
        QString msg = (DTVConfParser::ERROR_PARSE == ret) ?
            tr("Failed to parse '%1'") : tr("Failed to open '%1'");

        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("ScanWizard"), msg.arg(file));
    }
    else
    {
        channels = parser.GetChannels();
    }
}

void ScanWizardScanner::PreScanCommon(int scantype,
                                      uint pcardid,
                                      uint ccardid,
                                      const QString &inputname,
                                      uint sourceid,
                                      bool do_ignore_signal_timeout)
{
    uint hw_cardid       = (ccardid) ? ccardid : pcardid;
    uint signal_timeout  = 1000;
    uint channel_timeout = 40000;
    CardUtil::GetTimeouts(hw_cardid, signal_timeout, channel_timeout);

    QString device = CardUtil::GetVideoDevice(pcardid, inputname);
    if (device.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "No Device");
        return;
    }

    QString card_type = CardUtil::GetRawCardType(pcardid, inputname);

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
    if (("HDTV" == card_type) || ("V4L" == card_type) || ("MPEG" == card_type))
        channel = new Channel(NULL, device);
#endif

#ifdef USING_HDHOMERUN
    if ("HDHOMERUN" == card_type)
    {
        uint tuner = CardUtil::GetHDHRTuner(pcardid, inputname);
        channel = new HDHRChannel(NULL, device, tuner);
    }
#endif // USING_HDHOMERUN

    if (!channel)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Channel not created");
        return;
    }

    // explicitly set the cardid
    channel->SetCardID(pcardid);

    // If the backend is running this may fail...
    if (!channel->Open())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Channel could not be opened");
        return;
    }

    scanner = new SIScan(card_type, channel, sourceid, signal_timeout,
                         channel_timeout, inputname);

    scanner->SetForceUpdate(true);

    bool ftao = CardUtil::IgnoreEncrypted(pcardid, inputname);
    scanner->SetFTAOnly(ftao);

    bool tvo = CardUtil::TVOnly(pcardid, inputname);
    scanner->SetTVOnly(tvo);

    connect(scanner, SIGNAL(ServiceScanComplete(void)),
            this,    SLOT(  scanComplete(void)));
    connect(scanner, SIGNAL(TransportScanComplete(void)),
            this,    SLOT(  transportScanComplete(void)));
    connect(scanner, SIGNAL(ServiceScanUpdateStatusText(const QString&)),
            this,    SLOT(  updateStatusText(const QString&)));
    connect(scanner, SIGNAL(ServiceScanUpdateText(const QString&)),
            this,    SLOT(  updateText(const QString&)));
    connect(scanner, SIGNAL(TransportScanUpdateText(const QString&)),
            this,    SLOT(  updateText(const QString&)));
    connect(scanner, SIGNAL(PctServiceScanComplete(int)),
            this,    SLOT(  serviceScanPctComplete(int)));

    // Signal Meters are connected here
    SignalMonitor *monitor = scanner->GetSignalMonitor();
    if (monitor)
    {
        connect(monitor,
                SIGNAL(StatusSignalLock(const SignalMonitorValue&)),
                this,
                SLOT(  dvbLock(         const SignalMonitorValue&)));
        connect(monitor,
                SIGNAL(StatusSignalStrength(const SignalMonitorValue&)),
                this,
                SLOT(  dvbSignalStrength(   const SignalMonitorValue&)));
    }

#ifdef USING_DVB
    DVBSignalMonitor *dvbm = scanner->GetDVBSignalMonitor();
    if (dvbm)
    {
        connect(dvbm,
                SIGNAL(StatusSignalToNoise(const SignalMonitorValue&)),
                this,
                SLOT(  dvbSNR(const SignalMonitorValue&)));
    }
#endif // USING_DVB

    popupProgress = new ScanProgressPopup(this);
    popupProgress->progress(0);
    popupProgress->exec(this);
}

void ScanWizardScanner::ImportM3U(uint cardid, const QString &inputname,
                                  uint sourceid)
{
    (void) cardid;
    (void) inputname;
    (void) sourceid;

#ifdef USING_IPTV
    //Create an analog scan object
    freeboxScanner = new IPTVChannelFetcher(cardid, inputname, sourceid);
    popupProgress  = new ScanProgressPopup(this, false);

    connect(freeboxScanner, SIGNAL(ServiceScanComplete(void)),
            this,           SLOT(  scanComplete(void)));
    connect(freeboxScanner, SIGNAL(ServiceScanUpdateText(const QString&)),
            this,           SLOT(  updateText(const QString&)));
    connect(freeboxScanner, SIGNAL(ServiceScanPercentComplete(int)),
            this,           SLOT(  serviceScanPctComplete(int)));

    popupProgress->progress(0);
    popupProgress->exec(this);

    if (!freeboxScanner->Scan())
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("ScanWizard"),
                                  tr("Error starting scan"));
        Teardown();
    }
#endif // USING_IPTV
}
