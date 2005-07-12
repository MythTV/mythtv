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
#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbsignalmonitor.h"
#include "siscan.h"
#include "dvbconfparser.h"
#endif
#include "videosource.h"
#include "frequencies.h"
#include "analogscan.h"
#include "mythdbcon.h"

#include "scanwizard.h"

/// Percentage to set to after the transports have been scanned
#define TRANSPORT_PCT 6
/// Percentage to set to after the first tune
#define TUNED_PCT     3

const QString ScanWizardScanner::strTitle(QObject::tr("Scanning"));

ScanWizardScanner::ScanWizardScanner(ScanWizard *_parent) :
          parent(_parent) 
{
    tunerthread_running = false;
    scanthread_running = false;
#ifdef USING_DVB
    scanner = NULL;
    dvbchannel = NULL;
    monitor = NULL;
#endif
    analogScan = NULL;
    scanthread_running = false;
    setLabel(strTitle);
    setUseLabel(false);
    addChild(log = new LogList());
}

void ScanWizardScanner::finish()
{
#ifdef USING_DVB
    //Join the thread and close the channel
    if (scanner)
    {
        scanner->StopScanner();
        if (scanthread_running)
            pthread_join(scanner_thread,NULL);
        delete scanner;
        scanner = NULL;
    }

    if (monitor)
    {
        delete monitor;
        monitor = NULL;
    }

    if (dvbchannel)
    {
        dvbchannel->Close();
        delete dvbchannel;
        dvbchannel = NULL;
    }
#endif
    if (analogScan)
    {
        analogScan->stop();
        delete analogScan;
        analogScan=NULL;
    }
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
#ifdef USING_DVB
            if (scanEvent->intValue() == ScannerEvent::OK)
            {
                if (tunerthread_running)
                    pthread_join(tuner_thread,NULL);
                pthread_create(&scanner_thread, NULL, SpawnScanner, scanner);
                scanthread_running = true;
                // Wait for dvbsections to start this is silly,
                // but does the trick
                while (dvbchannel->siparser == NULL)
                    usleep(250);
                popupProgress->status(tr("Scanning"));
                connect(dvbchannel->siparser, SIGNAL(TableLoaded()),
                        this,SLOT(TableLoaded()));
                popupProgress->progress((TUNED_PCT*PROGRESS_MAX)/100);
                if ((nScanType==ScanTypeSetting::FullTunedScan_OFDM) ||
                    (nScanType==ScanTypeSetting::FullTunedScan_QPSK) ||
                    (nScanType==ScanTypeSetting::FullTunedScan_QAM))
                    scanner->ScanTransports();
                else if (nScanType==ScanTypeSetting::FullScan_ATSC)
                    scanner->ATSCScanTransport(nVideoSource,
                                               parent->paneATSC->atscTransport());
                else if (nScanType==ScanTypeSetting::FullScan_OFDM)
                    scanner->DVBTScanTransport(nVideoSource,
                                               parent->country());
                else if (nScanType==ScanTypeSetting::FullTransportScan)
                    transportScanComplete();
                else
                    scanner->ScanServices();
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
#else
            (void)e;
#endif
    }
}

void ScanWizardScanner::scanComplete()
{
    ScannerEvent::TYPE se = ScannerEvent::ServiceScanComplete;
    QApplication::postEvent(this, new ScannerEvent(se));
}

void ScanWizardScanner::transportScanComplete()
{
#ifdef USING_DVB
    scanner->ScanServicesSourceID(nVideoSource);
    ScannerEvent* e = new ScannerEvent(ScannerEvent::ServicePct);
    e->intValue(TRANSPORT_PCT);
    QApplication::postEvent(this, e);
#endif
}

void *ScanWizardScanner::SpawnScanner(void *param)
{
#ifdef USING_DVB
    SIScan *scanner = (SIScan *)param;
    scanner->StartScanner();
#else
    (void)param;
#endif
    return NULL;
}

void *ScanWizardScanner::SpawnTune(void *param)
{
#ifdef USING_DVB
    ScanWizardScanner *scanner = (ScanWizardScanner*)param;

    ScannerEvent* e = new ScannerEvent(ScannerEvent::TuneComplete);
    if (scanner->nScanType == ScanTypeSetting::TransportScan)
    {
        if (!scanner->dvbchannel->TuneMultiplex(scanner->transportToTuneTo()))
        {
            e->intValue(ScannerEvent::ERROR_TUNE);
            QApplication::postEvent(scanner, e);
            return NULL;
        }
    }
    else if ((scanner->nScanType == ScanTypeSetting::FullTunedScan_OFDM) ||
             (scanner->nScanType == ScanTypeSetting::FullTunedScan_QPSK) ||
             (scanner->nScanType == ScanTypeSetting::FullTunedScan_QAM)) 
    {
        if (!scanner->dvbchannel->TuneTransport(scanner->chan_opts,true))
        {
            e->intValue(ScannerEvent::ERROR_TUNE);
            QApplication::postEvent(scanner, e);
            return NULL;
        }
    }
    e->intValue(ScannerEvent::OK);
    QApplication::postEvent(scanner,e);
#else
    (void)param;
#endif
    return NULL;
}

void ScanWizardScanner::TableLoaded()
{
    QApplication::postEvent(this,new ScannerEvent(ScannerEvent::TableLoaded));
}

void ScanWizardScanner::serviceScanPctComplete(int pct)
{
    ScannerEvent* e = new ScannerEvent(ScannerEvent::ServicePct);
    int tmp = TRANSPORT_PCT + ((100 - TRANSPORT_PCT) * pct)/100;
    e->intValue(tmp);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::updateText(const QString& str)
{
    if (str.isEmpty())
        return;
    ScannerEvent* e = new ScannerEvent(ScannerEvent::Update);
    e->strValue(str);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::dvbLock(int locked)
{
    ScannerEvent* e = new ScannerEvent(ScannerEvent::DVBLock);
    e->intValue(locked);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::dvbSNR(int i)
{
    ScannerEvent* e = new ScannerEvent(ScannerEvent::DVBSNR);
    e->intValue(i);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::dvbSignalStrength(int i)
{
    ScannerEvent* e = new ScannerEvent(ScannerEvent::DVBSignalStrength);
    e->intValue(i);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::cancelScan()
{
    //cerr << "ScanWizardScanner::cancelScan"<<endl;
    finish();
    delete popupProgress;
    popupProgress = NULL;
}

void ScanWizardScanner::scan()
{
    //cerr << "ScanWizardScanner::scan " << parent->scanType() <<" "<< parent->videoSource() << " " << parent->transport() << " " << parent->captureCard() << "\n";
    tunerthread_running = false;

    nScanType = parent->scanType();
    nVideoSource = parent->videoSource();
    if (nScanType == ScanTypeSetting::FullScan_Analog)
    {
        //Create an analog scan object
        analogScan = new AnalogScan(nVideoSource, parent->captureCard());

        popupProgress = new ScanProgressPopup(this,false);
        connect(analogScan,SIGNAL(serviceScanComplete(void)),
                this,SLOT(scanComplete(void)));
        connect(analogScan,SIGNAL(serviceScanUpdateText(const QString&)),
                this,SLOT(updateText(const QString&)));
        connect(analogScan,SIGNAL(serviceScanPCTComplete(int)),
                this,SLOT(serviceScanPctComplete(int)));
        popupProgress->progress(0);
        popupProgress->exec(this);

        if (!analogScan->scan())
        {
             MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      tr("ScanWizard"),
                                      tr("Error starting scan"));
             cancelScan();
        }
    }
#ifdef USING_DVB
    else if (nScanType == ScanTypeSetting::Import)
    {
        //cerr << "cardtype = " << parent->nCardType << " filename = " << parent->filename() << endl;
        DVBConfParser *parser = NULL;
        switch  (parent->nCardType)
        {
        case CardUtil::OFDM:
            parser=new DVBConfParser(DVBConfParser::OFDM,nVideoSource,parent->filename());
            break;
        case CardUtil::QPSK:
            parser=new DVBConfParser(DVBConfParser::QPSK,nVideoSource,parent->filename());
            break;
        case CardUtil::QAM:
            parser=new DVBConfParser(DVBConfParser::QAM,nVideoSource,parent->filename());
            break;
        case CardUtil::ATSC:
            parser=new DVBConfParser(DVBConfParser::ATSC,nVideoSource,parent->filename());
            break;
        }
        if (parser != NULL)
        {
            connect(parser,SIGNAL(updateText(const QString&)),
                this,SLOT(updateText(const QString&)));
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
    }
    else //DVB
    {
        nTransportToTuneTo = parent->transport();
        QString device;
        if (!CardUtil::GetVideoDevice(parent->captureCard(),device))
            return;
        dvbchannel = new DVBChannel(device.toInt());

        // These locks and objects might already exist in videosource need to check
        if (!dvbchannel->Open())
            return;

        scanner = new SIScan(dvbchannel, nVideoSource);
    
        scanner->SetForceUpdate(true);

        MSqlQuery query(MSqlQuery::InitCon());

        QString thequery = QString("SELECT freetoaironly "
                                   "FROM cardinput "
                                   "WHERE cardinput.cardid=%1 AND "
                                   "cardinput.sourceid=%2")
            .arg(parent->captureCard())
            .arg(nVideoSource);

        query.prepare(thequery);

        bool freetoair = true;
        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            freetoair=query.value(0).toBool();
        }
        scanner->SetFTAOnly(freetoair);

        connect(scanner,SIGNAL(ServiceScanComplete(void)),
                this,SLOT(scanComplete(void)));
        connect(scanner,SIGNAL(TransportScanComplete(void)),
                this,SLOT(transportScanComplete(void)));
        connect(scanner,SIGNAL(ServiceScanUpdateText(const QString&)),
                this,SLOT(updateText(const QString&)));
        connect(scanner,SIGNAL(TransportScanUpdateText(const QString&)),
                this,SLOT(updateText(const QString&)));
    
        connect(scanner,SIGNAL(PctServiceScanComplete(int)),
                this,SLOT(serviceScanPctComplete(int)));
    
        monitor = new DVBSignalMonitor(-1, dvbchannel);

        // Signal Meters Need connecting here
        connect(monitor,SIGNAL(StatusSignalLock(int)),this,SLOT(dvbLock(int)));
        connect(monitor,SIGNAL(StatusSignalToNoise(int)),this,SLOT(dvbSNR(int)));
        connect(monitor,SIGNAL(StatusSignalStrength(int)),
            this,SLOT(dvbSignalStrength(int)));

        monitor->Start();

        popupProgress = new ScanProgressPopup(this);
        popupProgress->progress(0);
        popupProgress->exec(this);

        memset(&chan_opts.tuning,0,sizeof(chan_opts.tuning));

        bool fParseError = false;
        if (nScanType == ScanTypeSetting::FullTunedScan_OFDM)
        {
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
        }
        else if (nScanType == ScanTypeSetting::FullTunedScan_QPSK)
        {
           //SQL code to get the disqec paramters HERE
            thequery = QString("SELECT dvb_diseqc_type, "
                       "diseqc_port, diseqc_pos, lnb_lof_switch, lnb_lof_hi, "
                       "lnb_lof_lo FROM cardinput,capturecard "
                       "WHERE capturecard.cardid=%1 and cardinput.sourceid=%2")
                       .arg(parent->captureCard())
                       .arg(nVideoSource);
    
            query.prepare(thequery);
    
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
        }
        else if (nScanType == ScanTypeSetting::FullTunedScan_QAM)
        {
            QAMPane *pane = parent->paneQAM;
            if (!chan_opts.tuning.parseQAM(pane->frequency(),
                                pane->inversion(),
                                pane->symbolrate(),
                                pane->fec(),
                                pane->modulation()))
                 fParseError = true;
        }
        else if ((nScanType == ScanTypeSetting::FullScan_OFDM) ||
                  (nScanType == ScanTypeSetting::FullScan_ATSC))
        {
            ScannerEvent* e=new ScannerEvent(ScannerEvent::TuneComplete);
            e->intValue(ScannerEvent::OK);
            QApplication::postEvent(this,e);
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
#endif
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

    connect(this,SIGNAL(scan()), page2 ,SLOT(scan()));

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
        emit cardTypeChanged(QString::number(nCardType));
    }
}

MythDialog* ScanWizard::dialogWidget(MythMainWindow *parent,
                                     const char *widgetName)
{
    MythWizard* wizard = (MythWizard*)
        ConfigurationWizard::dialogWidget(parent,widgetName);

    connect(wizard, SIGNAL(selected(const QString&)),
            this, SLOT(pageSelected(const QString&)));
    return wizard;
}

void ScanWizard::pageSelected(const QString& strTitle)
{
    if (strTitle == ScanWizardScanner::strTitle)
       emit scan();
}

