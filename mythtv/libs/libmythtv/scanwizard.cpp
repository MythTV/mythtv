/*
 * $Id$
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
#include <qapplication.h>
#include <qptrlist.h>
#if (QT_VERSION >= 0x030300)
#include <qlocale.h>
#endif

#include "mythcontext.h"
#include "siscan.h"
#include "channelbase.h"
#include "dtvsignalmonitor.h"
#include "videosource.h"
#include "frequencies.h"
#include "mythdbcon.h"
#include "scanwizard.h"

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

#ifdef USE_SIPARSER
#include "dvbsiparser.h"
#endif // USE_SIPARSER

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

DVBChannel *ScanWizardScanner::GetDVBChannel()
{
#ifdef USING_DVB
    return dynamic_cast<DVBChannel*>(channel);
#else
    return NULL;
#endif
}

Channel *ScanWizardScanner::GetChannel()
{
#ifdef USING_V4L
    return dynamic_cast<Channel*>(channel);
#else
    return NULL;
#endif
}

ScanWizardScanner::ScanWizardScanner(ScanWizard *_parent)
    : parent(_parent), frequency(0), modulation("8vsb")
{
    scanner = NULL;
    channel = NULL;
    tunerthread_running = false;
#ifdef USING_V4L
    analogScan = NULL;
#endif

    setLabel(strTitle);
    setUseLabel(false);
    addChild(log = new LogList());
}

void ScanWizardScanner::finish()
{
    //Join the thread and close the channel
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
    if (analogScan)
    {
        analogScan->stop();
        delete analogScan;
        analogScan = NULL;
    }
#endif
}

void ScanWizardScanner::customEvent(QCustomEvent *e)
{
    ScannerEvent *scanEvent = (ScannerEvent*) e;
    if ((popupProgress == NULL) &&
        (scanEvent->eventType() != ScannerEvent::Update))
        return;

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
                if (tunerthread_running)
                    pthread_join(tuner_thread, NULL);
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

void *ScanWizardScanner::SpawnTune(void *param)
{
    bool ok = false;
    ScanWizardScanner *scanner = (ScanWizardScanner*)param;

    ScannerEvent* e = new ScannerEvent(ScannerEvent::TuneComplete);
    if (scanner->nScanType == ScanTypeSetting::TransportScan)
    {
        int mplexid = scanner->transportToTuneTo();
        (void) mplexid;
#ifdef USING_DVB
        if (scanner->GetDVBChannel())
            ok = scanner->GetDVBChannel()->TuneMultiplex(mplexid);
#endif
#ifdef USING_V4L
        if (scanner->GetChannel())
            ok = scanner->GetChannel()->TuneMultiplex(mplexid);
#endif
    }
    else if ((scanner->nScanType == ScanTypeSetting::FullTunedScan_OFDM) ||
             (scanner->nScanType == ScanTypeSetting::FullTunedScan_QPSK) ||
             (scanner->nScanType == ScanTypeSetting::FullTunedScan_QAM)) 
    {
#ifdef USING_DVB
        if (scanner->GetDVBChannel())
            ok = scanner->GetDVBChannel()->Tune(scanner->chan_opts, true);
#endif
#ifdef USING_V4L
        if (scanner->GetChannel())
            ok = scanner->GetChannel()->Tune(scanner->frequency,
                                             scanner->modulation);
#endif
    }
    else
    {
        ok = true;
    }

    e->intValue((ok) ? ScannerEvent::OK : ScannerEvent::ERROR_TUNE);

    QApplication::postEvent(scanner, e);
    return NULL;
}

void ScanWizardScanner::TableLoaded()
{
#ifdef USE_SIPARSER
    QApplication::postEvent(this, new ScannerEvent(ScannerEvent::TableLoaded));
#endif // USE_SIPARSER
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

void ScanWizardScanner::scan()
{
    SISCAN(QString("ScanWizardScanner::scan(): "
                   "type(%1) src(%2) transport(%3) card(%4)")
           .arg(parent->scanType()).arg(parent->videoSource())
           .arg(parent->transport()).arg(parent->captureCard()));

    tunerthread_running = false;

    nScanType = parent->scanType();
    nVideoSource = parent->videoSource();
    if (nScanType == ScanTypeSetting::FullScan_Analog)
    {
#ifdef USING_V4L
        //Create an analog scan object
        analogScan = new AnalogScan(nVideoSource, parent->captureCard());

        popupProgress = new ScanProgressPopup(this, false);
        connect(analogScan, SIGNAL(serviceScanComplete(void)),
                this, SLOT(scanComplete(void)));
        connect(analogScan, SIGNAL(serviceScanUpdateText(const QString&)),
                this, SLOT(updateText(const QString&)));
        connect(analogScan, SIGNAL(serviceScanPCTComplete(int)),
                this, SLOT(serviceScanPctComplete(int)));
        popupProgress->progress(0);
        popupProgress->exec(this);

        if (!analogScan->scan())
        {
             MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      tr("ScanWizard"),
                                      tr("Error starting scan"));
             cancelScan();
        }
#endif
    }
    else if (nScanType == ScanTypeSetting::Import)
    {
#ifdef USING_DVB
        DVBConfParser *parser = NULL;
        switch (parent->nCardType)
        {
        case CardUtil::OFDM:
            parser = new DVBConfParser(DVBConfParser::OFDM,
                                       nVideoSource,parent->filename());
            break;
        case CardUtil::QPSK:
            parser = new DVBConfParser(DVBConfParser::QPSK,
                                       nVideoSource,parent->filename());
            break;
        case CardUtil::QAM:
            parser = new DVBConfParser(DVBConfParser::QAM,
                                       nVideoSource,parent->filename());
            break;
        case CardUtil::ATSC:
        case CardUtil::HDTV:
            parser = new DVBConfParser(DVBConfParser::ATSC,
                                       nVideoSource,parent->filename());
            break;
        }
        if (parser != NULL)
        {
            connect(parser, SIGNAL(updateText(const QString&)),
                this, SLOT(updateText(const QString&)));
            switch (parser->parse())
            {
            case DVBConfParser::ERROR_OPEN:
                MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                              tr("ScanWizard"),
                              tr("Failed to open : ")+parent->filename());
                break;
            case DVBConfParser::ERROR_PARSE:
                MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                              tr("ScanWizard"),
                              tr("Failed to parse : ")+parent->filename());
                break;
            }
            delete parser;
        }
#endif // USING_DVB
    }
    else // DVB || V4L
    {
        nTransportToTuneTo = parent->transport();
        int cardid = parent->captureCard();

        QString device, cn, card_type;
        if (!CardUtil::GetVideoDevice(cardid, device))
            return;

        int nCardType = CardUtil::GetCardType(cardid, cn, card_type);
        (void) nCardType;
#ifdef USING_DVB
        if (CardUtil::IsDVB(cardid))
            channel = new DVBChannel(device.toInt());
#endif
#ifdef USING_V4L
        if (nCardType == CardUtil::HDTV)
            channel = new Channel(NULL, device);
#endif
        if (!channel)
        {
            VERBOSE(VB_IMPORTANT, "Error, Channel not created");
            return;
        }

        // These locks and objects might already exist in videosource need to check
        if (!channel->Open())
        {
            VERBOSE(VB_IMPORTANT, "Error, Channel could not be opened");
            return;
        }

        scanner = new SIScan(card_type, channel, parent->videoSource());

        scanner->SetForceUpdate(true);

        bool ftao = CardUtil::IgnoreEncrypted(
            parent->captureCard(), channel->GetCurrentInput());
        scanner->SetFTAOnly(ftao);

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
            connect(monitor, SIGNAL(StatusSignalLock(const SignalMonitorValue&)),
                    this, SLOT(dvbLock(const SignalMonitorValue&)));
            connect(monitor, SIGNAL(StatusSignalStrength(const SignalMonitorValue&)),
                    this, SLOT(dvbSignalStrength(const SignalMonitorValue&)));
        }

#ifdef USING_DVB
        DVBSignalMonitor *dvbm = scanner->GetDVBSignalMonitor();
        if (dvbm)
        {
            connect(dvbm, SIGNAL(StatusSignalToNoise(const SignalMonitorValue&)),
                    this, SLOT(  dvbSNR(const SignalMonitorValue&)));
        }
        bzero(&chan_opts.tuning, sizeof(chan_opts.tuning));
#endif // USING_DVB

        popupProgress = new ScanProgressPopup(this);
        popupProgress->progress(0);
        popupProgress->exec(this);

        bool fParseError = false;
        if (nScanType == ScanTypeSetting::FullTunedScan_OFDM)
        {
#ifdef USING_DVB
            OFDMPane *pane = parent->paneOFDM;
            if (!chan_opts.tuning.parseOFDM(
                          pane->frequency(),
                          pane->inversion(),
                          pane->bandwidth(),
                          pane->coderate_hp(),
                          pane->coderate_lp(),
                          pane->constellation(),
                          pane->trans_mode(),
                          pane->guard_interval(),
                          pane->hierarchy()))
                fParseError = true;
#endif // USING_DVB
        }
        else if (nScanType == ScanTypeSetting::FullTunedScan_QPSK)
        {
#ifdef USING_DVB
            // SQL code to get the disqec paramters HERE
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(
                QString(
                    "SELECT dvb_diseqc_type, diseqc_port,  diseqc_pos, "
                    "       lnb_lof_switch,  lnb_lof_hi,   lnb_lof_lo "
                    "FROM cardinput, capturecard "
                    "WHERE capturecard.cardid=%1 and cardinput.sourceid=%2")
                .arg(parent->captureCard()).arg(nVideoSource));

            if (query.exec() && query.isActive() && query.size() > 0)
            {
                QPSKPane *pane = parent->paneQPSK;
                query.next();
                if (!chan_opts.tuning.parseQPSK(
                        pane->frequency(),
                        pane->inversion(),
                        pane->symbolrate(),
                        pane->fec(),
                        pane->polarity(),
                        query.value(0).toString(), // diseqc_type
                        query.value(1).toString(), // diseqc_port
                        query.value(2).toString(), // diseqc_pos
                        query.value(3).toString(), // lnb_lof_switch
                        query.value(4).toString(), // lnb_lof_hi
                        query.value(5).toString()  // lnb_lof_lo
                        ))
                    fParseError = true;
            }
#endif // USING_DVB
        }
        else if (nScanType == ScanTypeSetting::FullTunedScan_QAM)
        {
#ifdef USING_DVB
            QAMPane *pane = parent->paneQAM;
            if (!chan_opts.tuning.parseQAM(pane->frequency(),
                                           pane->inversion(),
                                           pane->symbolrate(),
                                           pane->fec(),
                                           pane->modulation()))
                fParseError = true;
#endif // USING_DVB
        }
        else if (nScanType == ScanTypeSetting::FullScan_OFDM)
        {
#ifdef USING_DVB
            ScannerEvent* e = new ScannerEvent(ScannerEvent::TuneComplete);
            e->intValue(ScannerEvent::OK);
            QApplication::postEvent(this, e);
            return;
#endif // USING_DVB
        }
        else if (nScanType == ScanTypeSetting::FullScan_ATSC)
        {
            ScannerEvent* e = new ScannerEvent(ScannerEvent::TuneComplete);
            e->intValue(ScannerEvent::OK);
            QApplication::postEvent(this, e);
            return;
        }

        if (fParseError)
        {
             MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                       tr("ScanWizard"),
                                       tr("Error parsing parameters"));
             cancelScan();
             return;
        }
        tunerthread_running = true;
        pthread_create(&tuner_thread, NULL, SpawnTune, this);
    }
}

ScanWizard::ScanWizard()
    : nVideoDev(-1), nCardType(CardUtil::ERROR_PROBE), nCaptureCard(-1)
{
    paneQAM = new QAMPane();
    paneOFDM = new OFDMPane();
    paneQPSK = new QPSKPane();
    paneATSC = new ATSCPane();

    page1 = new ScanWizardScanType(this);
    ScanWizardScanner *page2 = new ScanWizardScanner(this);

    connect(this,  SIGNAL(scan()),
            page2, SLOT(scan()));

    addChild(page1);
    addChild(page2);
}

void ScanWizard::captureCard(const QString& str)
{
    int nNewCaptureCard = str.toInt();
    //Work out what kind of card we've got
    //We need to check against the last capture card so that we don't
    //try and probe a card which is already open by scan()
    if ((nCaptureCard != nNewCaptureCard) ||
        (nCardType == CardUtil::ERROR_OPEN))
    {
        nCaptureCard = nNewCaptureCard;
        nCardType = CardUtil::GetCardType(nCaptureCard);
        QString fmt = (nCardType == CardUtil::HDTV) ? "%1_%2" : "%1%2";
        paneATSC->SetDefaultFormat(fmt);
        emit cardTypeChanged(QString::number(nCardType));
    }
}

MythDialog* ScanWizard::dialogWidget(MythMainWindow *parent,
                                     const char *widgetName)
{
    MythWizard* wizard = (MythWizard*)
        ConfigurationWizard::dialogWidget(parent, widgetName);

    connect(wizard, SIGNAL(selected(const QString&)),
            this, SLOT(pageSelected(const QString&)));
    return wizard;
}

void ScanWizard::pageSelected(const QString& strTitle)
{
    if (strTitle == ScanWizardScanner::strTitle)
       emit scan();
}

void ScanWizardScanner::HandleTuneComplete(void)
{
    SISCAN("ScanWizardScanner::HandleTuneComplete()");

    if (!scanner)
    {
        SISCAN("ScanWizardScanner::HandleTuneComplete(): "
               "Waiting for scan to start.");
        QTime t = QTime::currentTime();
        while (!scanner && t.elapsed() < 500)
            usleep(250);
        if (!scanner)
        {
            SISCAN("ScanWizardScanner::HandleTuneComplete(): "
                   "scan() did not start scanner! Aborting.");
            return;
        }
        SISCAN("ScanWizardScanner::HandleTuneComplete(): "
               "scan() has started scanner.");
        usleep(5000);
    }

    scanner->StartScanner();
#ifdef USE_SIPARSER
    if (scanner->siparser)
    {
        connect(scanner->siparser, SIGNAL(TableLoaded()),
                this, SLOT(TableLoaded()));
    }
#endif // USE_SIPARSER

    popupProgress->status(tr("Scanning"));
    popupProgress->progress( (TUNED_PCT * PROGRESS_MAX) / 100 );

    QString std     = "dvbt";
    QString mod     = "ofdm";
    QString country = parent->country();
    if (nScanType == ScanTypeSetting::FullScan_ATSC)
    {
        std     = "atsc";
        mod     = parent->paneATSC->atscTransport();
        country = "us";
    }

    bool ok = false;
    if ((nScanType == ScanTypeSetting::FullScan_ATSC) ||
        (nScanType == ScanTypeSetting::FullScan_OFDM))
    {
        SISCAN("ScanTransports("<<std<<", "<<mod<<", "<<country<<")");
        scanner->SetChannelFormat(parent->paneATSC->atscFormat());
        ok = scanner->ScanTransports(nVideoSource, std, mod, country);
    }
    else if ((nScanType == ScanTypeSetting::FullTunedScan_OFDM) ||
             (nScanType == ScanTypeSetting::FullTunedScan_QPSK) ||
             (nScanType == ScanTypeSetting::FullTunedScan_QAM))
    {
        SISCAN("ScanTransports()");
        ok = scanner->ScanTransports("dvb");
    }
    else if (nScanType == ScanTypeSetting::FullTransportScan)
    {
        SISCAN("ScanServicesSourceID("<<nVideoSource<<")");
        ok = scanner->ScanServicesSourceID(nVideoSource);
        post_event(this,
                   (ok) ? ScannerEvent::ServicePct : ScannerEvent::TuneComplete,
                   (ok) ? TRANSPORT_PCT            : ScannerEvent::ERROR_TUNE);
    }
    else if (nScanType == ScanTypeSetting::TransportScan)
    {
        SISCAN("ScanTransport("<<parent->transport()<<")");
        ok = scanner->ScanTransport(parent->transport());
    }

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "Failed to handle tune complete.");
    }
}
