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

#ifdef USING_V4L
#include "channel.h"
#include "pchdtvsignalmonitor.h"
#include "analogscan.h"
#endif

#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbsignalmonitor.h"
#include "dvbconfparser.h"
#endif

#ifdef USING_HDHOMERUN
#include "hdhrchannel.h"
#include "hdhrsignalmonitor.h"
#endif

#include "freeboxchannelfetcher.h"

#define LOC QString("SWizScan: ")
#define LOC_ERR QString("SWizScan, Error: ")

/// Percentage to set to after the transports have been scanned
#define TRANSPORT_PCT 6
/// Percentage to set to after the first tune
#define TUNED_PCT     3

const QString ScanWizardScanner::strTitle(QObject::tr("Scanning"));

void post_event(QObject *dest, ScannerEvent::TYPE type, int val)
{
    ScannerEvent* e = new ScannerEvent(type);
    e->intValue(val);
    QApplication::postEvent(dest, e);
}

DVBChannel *ScanWizardScanner::GetDVBChannel(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBChannel*>(channel);
#else
    return NULL;
#endif
}

Channel *ScanWizardScanner::GetChannel(void)
{
#ifdef USING_V4L
    return dynamic_cast<Channel*>(channel);
#else
    return NULL;
#endif
}

ScanWizardScanner::ScanWizardScanner(ScanWizard *_parent)
    : ConfigurationGroup(false, true, false, false),
      VerticalConfigurationGroup(false, true, false, false),
      parent(_parent),
      log(new LogList()),
      channel(NULL),                popupProgress(NULL),
      scanner(NULL),                analogScanner(NULL),
      freeboxScanner(NULL),
      nScanType(-1),
      nMultiplexToTuneTo(0),        nVideoSource(0),
      frequency(0),                 modulation("8vsb")
{
    setLabel(strTitle);
    addChild(log);
}

void ScanWizardScanner::finish()
{
    // Join the thread and close the channel
    if (scanner)
    {
        delete scanner;
        scanner = NULL;
    }

    if (channel)
    {
        delete channel;
        channel = NULL;
    }

#ifdef USING_V4L
    if (analogScanner)
    {
        analogScanner->stop();
        delete analogScanner;
        analogScanner = NULL;
    }
#endif

#ifdef USING_FREEBOX
    if (freeboxScanner)
    {
        freeboxScanner->Stop();
        delete freeboxScanner;
        freeboxScanner = NULL;
    }
#endif
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
            cancelScan();
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
        case ScannerEvent::TuneComplete:
        {
            if (scanEvent->intValue() == ScannerEvent::OK)
            {
                HandleTuneComplete();
            }
            else
            {
                MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                          tr("ScanWizard"),
                                          tr("Error tuning to transport"));
                cancelScan();
            }
        }
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

void ScanWizardScanner::cancelScan()
{
    finish();
    delete popupProgress;
    popupProgress = NULL;
}

static bool get_diseqc(uint cardid, uint sourceid,
                       QMap<QString,QString> &startChan)
{
    // SQL code to get the disqec paramters HERE
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT dvb_diseqc_type, diseqc_port,  diseqc_pos, "
        "       lnb_lof_switch,  lnb_lof_hi,   lnb_lof_lo "
        "FROM cardinput, capturecard "
        "WHERE cardinput.cardid   = capturecard.cardid AND "
        "      cardinput.cardid   = :CARDID            AND "
        "      cardinput.sourceid = :SOURCEID ");
    query.bindValue(":CARDID",   cardid);
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("ScanWizardScanner::scan()", query);
        return false;
    }

    if (query.next())
    {
        startChan["diseqc_type"]    = query.value(0).toString();
        startChan["diseqc_port"]    = query.value(1).toString();
        startChan["diseqc_pos"]     = query.value(2).toString();
        startChan["lnb_lof_switch"] = query.value(3).toString();
        startChan["lnb_lof_hi"]     = query.value(4).toString();
        startChan["lnb_lof_lo"]     = query.value(5).toString();
        return true;
    }

    return false;
}

// full scan of existing transports broken
// existing transport scan broken
void ScanWizardScanner::scan()
{
    int  ccardid = parent->captureCard();
    int  pcardid = CardUtil::GetParentCardID(ccardid);
    int  cardid  = (pcardid) ? pcardid : ccardid;
    nScanType    = parent->scanType();
    nVideoSource = parent->videoSource();
    bool do_scan = true;
    bool nit_scan_parse_failed = false;

    VERBOSE(VB_SIPARSER, LOC + "scan(): " +
            QString("type(%1) src(%2) cardid(%3)")
            .arg(nScanType).arg(nVideoSource).arg(cardid));

    if (nScanType == ScanTypeSetting::FullScan_Analog)
    {
        do_scan = false;
        ScanAnalog(cardid, nVideoSource);
    }
    else if (nScanType == ScanTypeSetting::Import)
    {
        do_scan = false;
        Import(nVideoSource, parent->nCardType, parent->filename());
    }
    else if ((nScanType == ScanTypeSetting::FullScan_ATSC)     ||
             (nScanType == ScanTypeSetting::FullTransportScan) ||
             (nScanType == ScanTypeSetting::TransportScan)     ||
             (nScanType == ScanTypeSetting::FullScan_OFDM))
    {
        ;
    }
    else if (nScanType == ScanTypeSetting::NITAddScan_OFDM)
    {
        OFDMPane *pane = parent->paneOFDM;
        startChan.clear();
        startChan["std"]            = "dvb";
        startChan["frequency"]      = pane->frequency();
        startChan["inversion"]      = pane->inversion();
        startChan["bandwidth"]      = pane->bandwidth();
        startChan["modulation"]     = "ofdm";
        startChan["coderate_hp"]    = pane->coderate_hp();
        startChan["coderate_lp"]    = pane->coderate_lp();
        startChan["constellation"]  = pane->constellation();
        startChan["trans_mode"]     = pane->trans_mode();
        startChan["guard_interval"] = pane->guard_interval();
        startChan["hierarchy"]      = pane->hierarchy();

#ifdef USING_DVB
        DVBTuning tuning;
        nit_scan_parse_failed = !tuning.parseOFDM(
            startChan["frequency"],   startChan["inversion"],
            startChan["bandwidth"],   startChan["coderate_hp"],
            startChan["coderate_lp"], startChan["constellation"],
            startChan["trans_mode"],  startChan["guard_interval"],
            startChan["hierarchy"]);
#endif // USING_DVB
    }
    else if (nScanType == ScanTypeSetting::NITAddScan_QPSK)
    {
        QPSKPane *pane = parent->paneQPSK;
        startChan.clear();
        startChan["std"]        = "dvb";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = "qpsk";
        startChan["polarity"]   = pane->polarity();

        nit_scan_parse_failed = !get_diseqc(cardid, nVideoSource, startChan);

#ifdef USING_DVB
        if (!nit_scan_parse_failed)
        {
            DVBTuning tuning;
            nit_scan_parse_failed = !tuning.parseQPSK(
                startChan["frequency"],   startChan["inversion"],
                startChan["symbolrate"],  startChan["fec"],
                startChan["polarity"],
                startChan["diseqc_type"], startChan["diseqc_port"],
                startChan["diseqc_pos"],  startChan["lnb_lof_switch"],
                startChan["lnb_lof_hi"],  startChan["lnb_lof_lo"]);
        }
#endif // USING_DVB
    }
    else if (nScanType == ScanTypeSetting::NITAddScan_QAM)
    {
        QAMPane *pane = parent->paneQAM;
        startChan.clear();
        startChan["std"]        = "dvb";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = pane->modulation();

#ifdef USING_DVB
        DVBTuning tuning;
        nit_scan_parse_failed = !tuning.parseQAM(
            startChan["frequency"],   startChan["inversion"],
            startChan["symbolrate"],  startChan["fec"],
            startChan["modulation"]);
#endif // USING_DVB
    }
    else if (nScanType == ScanTypeSetting::FreeBoxImport)
    {
        do_scan = false;
        ScanFreeBox(cardid, nVideoSource);
    }
    else
    {
        do_scan = false;
        VERBOSE(VB_SIPARSER, LOC_ERR + "scan(): " +
                QString("type(%1) src(%2) cardid(%3) not handled")
                .arg(nScanType).arg(nVideoSource).arg(cardid));

        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), tr("ScanWizard"),
            "Programmer Error, see console");
    }

    if (nit_scan_parse_failed)
    {
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), tr("ScanWizard"),
            tr("Error parsing parameters"));
        return;
    }

    if (do_scan)
    {
        PreScanCommon(cardid, nVideoSource);
        ScannerEvent* e = new ScannerEvent(ScannerEvent::TuneComplete);
        e->intValue(ScannerEvent::OK);
        QApplication::postEvent(this, e);
    }
}

void ScanWizardScanner::Import(uint sourceid, int cardtype,
                               const QString &file)
{
    (void) sourceid;
    (void) cardtype;
    (void) file;

#ifdef USING_DVB
    DVBConfParser *parser = NULL;

    if (CardUtil::OFDM == cardtype)
        parser = new DVBConfParser(DVBConfParser::OFDM, sourceid, file);
    else if (CardUtil::QPSK == cardtype)
        parser = new DVBConfParser(DVBConfParser::QPSK, sourceid, file);
    else if (CardUtil::QAM == cardtype)
        parser = new DVBConfParser(DVBConfParser::QAM, sourceid, file);
    else if ((CardUtil::ATSC == cardtype) ||
             (CardUtil::HDTV == cardtype))
        parser = new DVBConfParser(DVBConfParser::ATSC, sourceid, file);

    if (!parser)
        return;

    connect(parser, SIGNAL(updateText(const QString&)),
            this,   SLOT(  updateText(const QString&)));

    int ret = parser->parse();
    parser->deleteLater();

    if (DVBConfParser::ERROR_OPEN == ret)
    {
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), tr("ScanWizard"),
            tr("Failed to open '%1'").arg(file));
    }

    if (DVBConfParser::ERROR_PARSE == ret)
    {
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), tr("ScanWizard"),
            tr("Failed to parse '%1'").arg(file));
    }
#endif // USING_DVB
}

void ScanWizardScanner::PreScanCommon(uint cardid, uint sourceid)
{
    uint signal_timeout  = 1000;
    uint channel_timeout = 40000;
    CardUtil::GetTimeouts(parent->captureCard(),
                          signal_timeout, channel_timeout);

    nMultiplexToTuneTo = parent->paneSingle->GetMultiplex();

    QString device = CardUtil::GetVideoDevice(cardid, sourceid);
    if (device.isEmpty())
        return;

    QString card_type = CardUtil::GetRawCardType(cardid, sourceid);

    if ("DVB" == card_type)
    {
        QString sub_type = CardUtil::ProbeDVBType(device.toUInt()).upper();
        bool need_nit = (("QAM"  == sub_type) ||
                         ("QPSK" == sub_type) ||
                         ("OFDM" == sub_type));

        // Ugh, Some DVB drivers don't fully support signal monitoring...
        if (ScanTypeSetting::TransportScan == parent->scanType() ||
            ScanTypeSetting::FullTransportScan == parent->scanType())
        {
            signal_timeout = (parent->ignoreSignalTimeout()) ?
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
    if ("HDTV" == card_type)
        channel = new Channel(NULL, device);
#endif

#ifdef USING_HDHOMERUN
    if ("HDHOMERUN" == card_type)
    {
        uint tuner = CardUtil::GetHDHRTuner(cardid, sourceid);
        channel = new HDHRChannel(NULL, device, tuner);
    }
#endif // USING_HDHOMERUN

    if (!channel)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Channel not created");
        return;
    }

    // explicitly set the cardid
    channel->SetCardID(cardid);

    // If the backend is running this may fail...
    if (!channel->Open())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Channel could not be opened");
        return;
    }

    scanner = new SIScan(card_type, channel, parent->videoSource(),
                         signal_timeout, channel_timeout);

    scanner->SetForceUpdate(true);

    bool ftao = CardUtil::IgnoreEncrypted(cardid, channel->GetCurrentInput());
    scanner->SetFTAOnly(ftao);

    bool tvo = CardUtil::TVOnly(cardid, channel->GetCurrentInput());
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

void ScanWizardScanner::ScanAnalog(uint cardid, uint sourceid)
{
#ifdef USING_V4L
    //Create an analog scan object
    analogScanner    = new AnalogScan(sourceid, cardid);
    popupProgress = new ScanProgressPopup(this, false);

    connect(analogScanner, SIGNAL(serviceScanComplete(void)),
            this,       SLOT(  scanComplete(void)));
    connect(analogScanner, SIGNAL(serviceScanUpdateText(const QString&)),
            this,       SLOT(  updateText(const QString&)));
    connect(analogScanner, SIGNAL(serviceScanPCTComplete(int)),
            this,       SLOT(  serviceScanPctComplete(int)));

    popupProgress->progress(0);
    popupProgress->exec(this);

    if (!analogScanner->scan())
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("ScanWizard"),
                                  tr("Error starting scan"));
        cancelScan();
    }
#endif
}

void ScanWizardScanner::ScanFreeBox(uint cardid, uint sourceid)
{
#ifdef USING_FREEBOX
    //Create an analog scan object
    freeboxScanner = new FreeboxChannelFetcher(sourceid, cardid);
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
        cancelScan();
    }
#endif // USING_FREEBOX
}

void ScanWizardScanner::HandleTuneComplete(void)
{
    VERBOSE(VB_SIPARSER, LOC + "HandleTuneComplete()");

    if (!scanner)
    {
        VERBOSE(VB_SIPARSER, LOC + "HandleTuneComplete(): "
                "Waiting for scan to start.");
        MythTimer t;
        t.start();
        while (!scanner && t.elapsed() < 500)
            usleep(250);
        if (!scanner)
        {
            VERBOSE(VB_SIPARSER, LOC +
                    "HandleTuneComplete(): "
                    "scan() did not start scanner! Aborting.");
            return;
        }
        VERBOSE(VB_SIPARSER, LOC + "HandleTuneComplete(): "
                "scan() has started scanner.");
        usleep(5000);
    }

    scanner->StartScanner();

    popupProgress->status(tr("Scanning"));
    popupProgress->progress( (TUNED_PCT * PROGRESS_MAX) / 100 );

    QString std     = "dvbt";
    QString mod     = "ofdm";
    QString country = parent->country();
    if (nScanType == ScanTypeSetting::FullScan_ATSC)
    {
        std     = "atsc";
        mod     = parent->paneATSC->atscModulation();
        country = parent->paneATSC->atscFreqTable();
    }

    bool ok = false;

    if ((nScanType == ScanTypeSetting::FullScan_ATSC) ||
        (nScanType == ScanTypeSetting::FullScan_OFDM))
    {
        VERBOSE(VB_SIPARSER, LOC +
                "ScanTransports("<<std<<", "<<mod<<", "<<country<<")");
        scanner->SetChannelFormat(parent->paneATSC->atscFormat());
        if (parent->paneATSC->DoDeleteChannels())
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("DELETE FROM channel "
                          "WHERE sourceid = :SOURCEID");
            query.bindValue(":SOURCEID", nVideoSource);
            query.exec();
            query.prepare("DELETE FROM dtv_multiplex "
                          "WHERE sourceid = :SOURCEID");
            query.bindValue(":SOURCEID", nVideoSource);
            query.exec();
        }
        scanner->SetRenameChannels(parent->paneATSC->DoRenameChannels());

        // HACK HACK HACK -- begin
        // if using QAM we may need additional time... (at least with HD-3000)
        if ((mod.left(3).lower() == "qam") &&
            (scanner->GetSignalTimeout() < 1000))
        {
            scanner->SetSignalTimeout(1000);
        }
        // HACK HACK HACK -- end

        ok = scanner->ScanTransports(nVideoSource, std, mod, country);
    }
    else if ((nScanType == ScanTypeSetting::NITAddScan_OFDM) ||
             (nScanType == ScanTypeSetting::NITAddScan_QPSK) ||
             (nScanType == ScanTypeSetting::NITAddScan_QAM))
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanTransports()");
        scanner->SetRenameChannels(false);
        ok = scanner->ScanTransportsStartingOn(nVideoSource, startChan);
    }
    else if (nScanType == ScanTypeSetting::FullTransportScan)
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanServicesSourceID("<<nVideoSource<<")");
        scanner->SetRenameChannels(false);
        ok = scanner->ScanServicesSourceID(nVideoSource);
        if (ok)
        {
            post_event(this, ScannerEvent::ServicePct,
                       TRANSPORT_PCT);
        }
        else
        {
            post_event(this, ScannerEvent::TuneComplete,
                       ScannerEvent::ERROR_TUNE);
        }
    }
    else if (nScanType == ScanTypeSetting::TransportScan)
    {
        VERBOSE(VB_SIPARSER, LOC + "ScanTransport("<<nMultiplexToTuneTo<<")");
        scanner->SetChannelFormat(parent->paneSingle->atscFormat());

        if (parent->paneSingle->DoDeleteChannels())
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("DELETE FROM channel "
                          "WHERE sourceid = :SOURCEID AND "
                          "      mplexid  = :MPLEXID");
            query.bindValue(":SOURCEID", nVideoSource);
            query.bindValue(":MPLEXID",  nMultiplexToTuneTo);
            query.exec();
        }

        scanner->SetRenameChannels(parent->paneSingle->DoRenameChannels());
        ok = scanner->ScanTransport(nMultiplexToTuneTo);
    }

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "Failed to handle tune complete.");
    }
}
