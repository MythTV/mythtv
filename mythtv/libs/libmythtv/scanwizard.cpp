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
#include "mythcontext.h"
#include "dvbchannel.h"
#include "videosource.h"
#include "frequencies.h"
#include "siscan.h"
#include "dvbsignalmonitor.h"
#include "mythdbcon.h"

#include "scanwizard.h"

//Max range of the progress bar
#define PROGRESS_MAX  1000
//Percentage to set to after the transports have been scanned
#define TRANSPORT_PCT 10
//Percentage to set to after the fist tune
#define TUNED_PCT     5

class ScanFrequency: public LineEditSetting, public TransientStorage {
public:
    ScanFrequency(): LineEditSetting()
    {
        setLabel(QObject::tr("Frequency"));
        setHelpText(QObject::tr("Frequency (Option has no default)\n"
                    "The frequency for this channel in Hz."));
    };
};

class ScanSymbolRate: public LineEditSetting, public TransientStorage {
public:
    ScanSymbolRate():
        LineEditSetting()
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(QObject::tr("Symbol Rate (Option has no default)"));
    };
};

class ScanPolarity: public ComboBoxSetting, public TransientStorage {
public:
    ScanPolarity():
        ComboBoxSetting()
    {
        setLabel(QObject::tr("Polarity"));
        setHelpText(QObject::tr("Polarity (Option has no default)"));
        addSelection(QObject::tr("Horizontal"), "h",true);
        addSelection(QObject::tr("Vertical"), "v");
        addSelection(QObject::tr("Right Circular"), "r");
        addSelection(QObject::tr("Left Circular"), "l");
    };
};

class ScanInversion: public ComboBoxSetting, public TransientStorage {
public:
    ScanInversion():
        ComboBoxSetting()
    {
        setLabel(QObject::tr("Inversion"));
        setHelpText(QObject::tr("Inversion (Default: Auto):\n"
                    "Most cards can autodetect this now, so leave it at Auto"
                    " unless it won't work."));
        addSelection(QObject::tr("Auto"), "a",true);
        addSelection(QObject::tr("On"), "1");
        addSelection(QObject::tr("Off"), "0");
    };
};

class ScanBandwidth: public ComboBoxSetting, public TransientStorage {
public:
    ScanBandwidth():
        ComboBoxSetting()
    {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("Bandwidth (Default: Auto)\n"));
        addSelection(QObject::tr("Auto"),"a",true);
        addSelection(QObject::tr("6 MHz"),"6");
        addSelection(QObject::tr("7 MHz"),"7");
        addSelection(QObject::tr("8 MHz"),"8");
    };
};

class ScanModulationSetting: public ComboBoxSetting {
public:
    ScanModulationSetting() {
        addSelection(QObject::tr("Auto"),"auto",true);
        addSelection("QPSK","qpsk");
        addSelection("QAM 16","qam_16");
        addSelection("QAM 32","qam_32");
        addSelection("QAM 64","qam_64");
        addSelection("QAM 128","qam_128");
        addSelection("QAM 256","qam_256");
    };
};

class ScanModulation: public ScanModulationSetting, public TransientStorage {
public:
    ScanModulation():
        ScanModulationSetting()
    {
        setLabel(QObject::tr("Modulation"));
        setHelpText(QObject::tr("Modulation (Default: Auto)"));
    };
};

class ScanConstellation: public ScanModulationSetting, public TransientStorage {
public:
    ScanConstellation():
        ScanModulationSetting()
    {
        setLabel(QObject::tr("Constellation"));
        setHelpText(QObject::tr("Constellation (Default: Auto)"));
    };
};

class ScanFecSetting: public ComboBoxSetting {
public:
    ScanFecSetting()
    {
        addSelection(QObject::tr("Auto"),"auto",true);
        addSelection(QObject::tr("None"),"none");
        addSelection("1/2");
        addSelection("2/3");
        addSelection("3/4");
        addSelection("4/5");
        addSelection("5/6");
        addSelection("6/7");
        addSelection("7/8");
        addSelection("8/9");
    }
};

class ScanFec: public ScanFecSetting, public TransientStorage {
public:
    ScanFec():
        ScanFecSetting()
    {
        setLabel(QObject::tr("FEC"));
        setHelpText(QObject::tr("Forward Error Correction (Default: Auto)"));
    }
};

class ScanCodeRateLP: public ScanFecSetting, public TransientStorage {
public:
    ScanCodeRateLP(): ScanFecSetting()
    {
        setLabel(QObject::tr("LP Coderate"));
        setHelpText(QObject::tr("Low Priority Code Rate (Default: Auto)"));
    }
};

class ScanCodeRateHP: public ScanFecSetting, public TransientStorage {
public:
    ScanCodeRateHP(): ScanFecSetting()
    {
        setLabel(QObject::tr("HP Coderate"));
        setHelpText(QObject::tr("High Priority Code Rate (Default: Auto)"));
    };
};

class ScanGuardInterval: public ComboBoxSetting, public TransientStorage {
public:
    ScanGuardInterval():
        ComboBoxSetting()
    {
        setLabel(QObject::tr("Guard Interval"));
        setHelpText(QObject::tr("Guard Interval (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"auto");
        addSelection("1/4");
        addSelection("1/8");
        addSelection("1/16");
        addSelection("1/32");
    };
};

class ScanTransmissionMode: public ComboBoxSetting, public TransientStorage {
public:
    ScanTransmissionMode():
        ComboBoxSetting()
    {
        setLabel(QObject::tr("Trans. Mode"));
        setHelpText(QObject::tr("Transmission Mode (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection("2K","2");
        addSelection("8K","8");
    };
};

class ScanHierarchy: public ComboBoxSetting, public TransientStorage {
public:
    ScanHierarchy():
        ComboBoxSetting()
    {
        setLabel(QObject::tr("Hierarchy"));
        setHelpText(QObject::tr("Hierarchy (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection(QObject::tr("None"), "n");
        addSelection("1");
        addSelection("2");
        addSelection("4");
    };
};

VideoSourceSetting::VideoSourceSetting()
{
    setLabel(QObject::tr("Video Source"));
}

void VideoSourceSetting::load()
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    QString querystr = QString(
        "SELECT DISTINCT videosource.name, videosource.sourceid "
        "FROM cardinput, videosource, capturecard "
        "WHERE cardinput.sourceid=videosource.sourceid "
        "AND cardinput.cardid=capturecard.cardid "
        "AND capturecard.cardtype in (\"DVB\") "
        "AND capturecard.hostname=\"%1\"").arg(gContext->GetHostName());
    
    query.prepare(querystr);

    if (query.exec() && query.isActive() && query.size() > 0)
        while(query.next())
            addSelection(query.value(0).toString(),
                         query.value(1).toString());
}

TransportSetting::TransportSetting() : nSourceID(0)
{
    setLabel(QObject::tr("Transport"));
}

void TransportSetting::load()
{
    refresh();
}

void TransportSetting::refresh()
{
    clearSelections();
    
    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = QString(
               "SELECT mplexid, networkid, transportid, "
               " frequency, symbolrate, modulation FROM dtv_multiplex channel "
               " WHERE sourceid=%1 ORDER by networkid, transportid ")
               .arg(nSourceID);

    query.prepare(querystr);

    if (query.exec() && query.isActive() && query.size() > 0)
        while(query.next())
        {
            QString DisplayText;
            if (query.value(5).toString() == "8vsb")
            {
                QString ChannelNumber;
                struct CHANLIST* curList = chanlists[0].list;
                int totalChannels = chanlists[0].count;
                int findFrequency = (query.value(3).toInt() / 1000) - 1750;

                for (int x = 0 ; x < totalChannels ; x++)
                {
                    if (curList[x].freq == findFrequency)
                        ChannelNumber = QString("%1").arg(curList[x].name);
                }

                DisplayText = QString("ATSC Channel %1").arg(ChannelNumber);
            }
            else
            {
                DisplayText = QString("%1 Hz (%2) (%3) (%4)")
                                  .arg(query.value(3).toString())
                                  .arg(query.value(4).toString())
                                  .arg(query.value(1).toInt())
                                  .arg(query.value(2).toInt());
            }
            addSelection(DisplayText, query.value(0).toString());
        }
}

void TransportSetting::sourceID(const QString& str)
{
    nSourceID = str.toInt();
    refresh();
}

CaptureCardSetting::CaptureCardSetting() : nSourceID(0)
{
    setLabel(QObject::tr("Capture Card"));
}

void CaptureCardSetting::load()
{
    refresh();
}

void CaptureCardSetting::refresh()
{
    clearSelections();

    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = QString("SELECT DISTINCT cardtype,videodevice,capturecard.cardid "
                               "FROM capturecard, videosource, cardinput "
                               "WHERE videosource.sourceid=%1 "
                               "AND cardinput.sourceid=videosource.sourceid "
                               "AND cardinput.cardid=capturecard.cardid "
                               "AND capturecard.cardtype=\"DVB\" "
                               "AND capturecard.hostname=\"%2\";")
                              .arg(nSourceID).arg(gContext->GetHostName());
    query.prepare(thequery);

    if (query.exec() && query.isActive() && query.size() > 0)
        while(query.next())
            addSelection("[ " + query.value(0).toString() + " : " +
                                query.value(1).toString() + " ]",
                                query.value(2).toString());
}

void CaptureCardSetting::sourceID(const QString& str)
{
    nSourceID = str.toInt();
    refresh();
}

ScanWizardScanType::ScanWizardScanType(ScanWizard *parent)
{
    setLabel(tr("Scan Type"));
    setUseLabel(false);
    VideoSourceSetting *videoSource = new VideoSourceSetting();
    ScanTypeSetting *scanType = new ScanTypeSetting();
    transport = new TransportSetting();
    capturecard = new CaptureCardSetting();

    addChild(videoSource);
    addChild(capturecard);
    addChild(scanType);
    addChild(transport);

    connect(videoSource, SIGNAL(valueChanged(const QString&)),
        transport, SLOT(sourceID(const QString&)));
    connect(videoSource, SIGNAL(valueChanged(const QString&)),
        capturecard, SLOT(sourceID(const QString&)));

    connect(videoSource, SIGNAL(valueChanged(const QString&)),
        parent, SLOT(videoSource(const QString&)));

    connect(transport, SIGNAL(valueChanged(const QString&)),
        parent, SLOT(transport(const QString&)));
    connect(capturecard, SIGNAL(valueChanged(const QString&)),
        parent, SLOT(captureCard(const QString&)));

    connect(scanType, SIGNAL(valueChanged(const QString&)),
        this, SLOT(scanTypeChanged(const QString&)));
    scanTypeChanged(scanType->getValue());
}

void ScanWizardScanType::scanTypeChanged(const QString& str)
{
    unsigned nType = str.toInt();
    if ((nType == ScanTypeSetting::FullScan) ||
        (nType == ScanTypeSetting::FullTransportScan))
       transport->setEnabled(false);
    if (nType == ScanTypeSetting::TransportScan)
       transport->setEnabled(true);
    emit scanTypeChanged((ScanTypeSetting::Type)nType);
}

class BasePane : public HorizontalConfigurationGroup
{
public:
     void enable(bool fEnable)
     {
         Configurable *c;
         for (c = configurables.first(); c; c = configurables.next())
            c->setEnabled(fEnable); 
     } 

protected:
     void addSetting(Configurable *c) { configurables.append(c);}
     QPtrList<Configurable> configurables;
};

class OFDMPane : public BasePane
{
public:
    OFDMPane(ScanWizard *parent)
    {
        setUseFrame(false);
        ScanFrequency *frequency = new ScanFrequency();
        ScanBandwidth *bandwidth = new ScanBandwidth();
        ScanInversion *inversion = new ScanInversion();
        ScanConstellation* constellation = new ScanConstellation();

        ScanCodeRateLP *coderate_lp = new ScanCodeRateLP();
        ScanCodeRateHP *coderate_hp = new ScanCodeRateHP();
        ScanTransmissionMode *trans_mode = new ScanTransmissionMode();
        ScanGuardInterval *guard_interval = new ScanGuardInterval();
        ScanHierarchy *hierarchy = new ScanHierarchy();

        connect(frequency, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(frequency(const QString&)));
        parent->frequency(frequency->getValue());
        connect(inversion, SIGNAL(valueChanged(const QString&)),
             parent, SLOT(inversion(const QString&)));
        parent->inversion(inversion->getValue());
        connect(bandwidth, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(bandwidth(const QString&)));
        parent->bandwidth(bandwidth->getValue());
        connect(constellation, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(constellation(const QString&)));
        parent->constellation(constellation->getValue());
        connect(coderate_lp, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(codeRateLP(const QString&)));
        parent->codeRateLP(coderate_lp->getValue());
        connect(coderate_hp, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(codeRateHP(const QString&)));
        parent->codeRateHP(coderate_hp->getValue());
        connect(trans_mode, SIGNAL(valueChanged(const QString&)),
             parent, SLOT(transmissionMode(const QString&)));
        parent->transmissionMode(trans_mode->getValue());
        connect(guard_interval, SIGNAL(valueChanged(const QString&)),
             parent, SLOT(guardInterval(const QString&)));
        parent->guardInterval(guard_interval->getValue());
        connect(hierarchy, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(hierarchy(const QString&)));
        parent->hierarchy(hierarchy->getValue());
 
        VerticalConfigurationGroup *left=new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right=new VerticalConfigurationGroup(false,true);

        left->addChild(frequency);
        left->addChild(bandwidth);
        left->addChild(inversion);
        left->addChild(constellation);
        right->addChild(coderate_lp);
        right->addChild(coderate_hp);
        right->addChild(trans_mode);
        right->addChild(guard_interval);
        right->addChild(hierarchy);
        addChild(left);
        addChild(right);

        addSetting(frequency);
        addSetting(bandwidth);
        addSetting(inversion);
        addSetting(constellation);
        addSetting(coderate_lp);
        addSetting(coderate_hp);
        addSetting(trans_mode);
        addSetting(guard_interval);
        addSetting(hierarchy);
     }
};

class QPSKPane : public BasePane
{
public:
    QPSKPane(ScanWizard *parent)
    {
        setUseFrame(false);
        ScanFrequency *frequency = new ScanFrequency();
        ScanInversion *inversion = new ScanInversion();
        ScanSymbolRate *symbolrate = new ScanSymbolRate();
        ScanPolarity *polarity = new ScanPolarity();
        ScanFec *fec = new ScanFec();

        connect(frequency, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(frequency(const QString&)));
        parent->frequency(frequency->getValue());
        connect(inversion, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(inversion(const QString&)));
        parent->inversion(inversion->getValue());
        connect(symbolrate, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(symbolRate(const QString&)));
        parent->symbolRate(symbolrate->getValue());
        connect(polarity, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(polarity(const QString&)));
        parent->polarity(polarity->getValue());
        connect(fec, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(fec(const QString&)));
        parent->fec(fec->getValue());
  
        VerticalConfigurationGroup *left=new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right=new VerticalConfigurationGroup(false,true);

        left->addChild(frequency);
        left->addChild(symbolrate);
        left->addChild(inversion);
        right->addChild(fec);
        right->addChild(polarity);
        addChild(left);
        addChild(right);     

        addSetting(frequency);
        addSetting(symbolrate);
        addSetting(inversion);
        addSetting(fec);
        addSetting(polarity);
    }
};

class QAMPane : public BasePane
{
public:
    QAMPane(ScanWizard *parent)
    {
        setUseFrame(false);
        ScanFrequency *frequency = new ScanFrequency();
        ScanModulation *modulation = new ScanModulation();
        ScanInversion *inversion = new ScanInversion();
        ScanSymbolRate *symbolrate = new ScanSymbolRate();
        ScanFec *fec = new ScanFec();

        connect(frequency, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(frequency(const QString&)));
        parent->frequency(frequency->getValue());
        connect(inversion, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(inversion(const QString&)));
        parent->inversion(inversion->getValue());
        connect(symbolrate, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(symbolRate(const QString&)));
        parent->symbolRate(symbolrate->getValue());
        connect(fec, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(fec(const QString&)));
        parent->fec(fec->getValue());
        connect(modulation, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(modulation(const QString&)));
        parent->modulation(modulation->getValue());
  
        VerticalConfigurationGroup *left=new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right=new VerticalConfigurationGroup(false,true);

        left->addChild(frequency);
        left->addChild(symbolrate);
        left->addChild(inversion);
        right->addChild(modulation);
        right->addChild(fec);
        addChild(left);
        addChild(right);     

        addSetting(frequency);
        addSetting(symbolrate);
        addSetting(inversion);
        addSetting(modulation);
        addSetting(fec);
    }
};

class ATSCPane : public BasePane
{
public:
    ATSCPane(ScanWizard *parent)
    {
        ScanATSCTransport *atscTransport = new ScanATSCTransport();
        connect(atscTransport, SIGNAL(valueChanged(const QString&)),
            parent, SLOT(atscTransport(const QString&)));
        addChild(atscTransport);
        addSetting(atscTransport);
    }
};

class ErrorPane : public HorizontalConfigurationGroup
{
public:
    ErrorPane(const QString& error)
    {
        TransLabelSetting* label = new TransLabelSetting();
        label->setValue(error);
        addChild(label);
    }
};

class CardTypeSetting : public ComboBoxSetting, public TransientStorage
{
public:
    CardTypeSetting()
    {
        addSelection("ERROR_OPEN","ERROR_OPEN");
        addSelection("ERROR_PROBE","ERROR_PROBE");
        addSelection("QPSK","QPSK");
        addSelection("QAM","QAM");
        addSelection("OFDM","OFDM");
        addSelection("ATSC","ATSC");
    }
};

ScanWizardTuningPage::ScanWizardTuningPage(ScanWizard *parent)
{
    setLabel(tr("Tuning"));
    setUseLabel(false);

    CardTypeSetting *card = new CardTypeSetting();
    card->setVisible(false);
    addChild(card);
     
    setTrigger(card);
    setSaveAll(false);

    addTarget("ERROR_OPEN", new ErrorPane(tr("Failed to open the card")));
    addTarget("ERROR_PROBE", new ErrorPane(tr("Failed to probe the card")));
    addTarget("QPSK", qpsk =  new QPSKPane(parent));
    addTarget("QAM",  qam = new QAMPane(parent));
    addTarget("OFDM", ofdm = new OFDMPane(parent));
    addTarget("ATSC", atsc = new ATSCPane(parent));
}

void ScanWizardTuningPage::triggerChanged(const QString& value)
{
    TriggeredConfigurationGroup::triggerChanged(value);
}

void ScanWizardTuningPage::cardTypeChanged(unsigned nType)
{
    char *table[] = { "ERROR_OPEN", 
                      "ERROR_PROBE", 
                      "QPSK", 
                      "QAM", 
                      "OFDM", 
                      "ATSC"};
    if (nType <= CardUtil::ATSC)
        TriggeredConfigurationGroup::triggerChanged(table[nType]);
}

void  ScanWizardTuningPage::scanType(ScanTypeSetting::Type _type)
{
    bool fEnable;
    if ((_type == ScanTypeSetting::TransportScan) ||
        (_type == ScanTypeSetting::FullTransportScan))
        fEnable = false;
    else
        fEnable = true;

    ofdm->enable(fEnable);
    qpsk->enable(fEnable);
    qam->enable(fEnable);
    atsc->enable(fEnable);
}

LogList::LogList() : n(0)
{
   setSelectionMode(MythListBox::NoSelection);
}

void LogList::updateText(const QString& status)
{
    addSelection(status,QString::number(n));
    setCurrentItem(n);
    n++;
}

const QString ScanWizardScanner::strTitle(QObject::tr("Scanning"));

ScanWizardScanner::ScanWizardScanner(ScanWizard *_parent) :
          parent(_parent) 
{
    scanner = NULL;
    dvbchannel = NULL;
    scanthread_running = false;
    setLabel(strTitle);
    setUseLabel(false);
    addChild(log = new LogList());
}

ScanWizardScanner::~ScanWizardScanner()
{
    finish();
}

void ScanWizardScanner::finish()
{
    //Join the thread and close the channel
    if (scanner)
    {
        scanner->StopScanner();
        if (scanthread_running)
            pthread_join(scanner_thread,NULL);
        delete scanner;
        scanner = NULL;
    }

    if (dvbchannel)
    {
        dvbchannel->Close();
        delete dvbchannel;
        dvbchannel = NULL;
    }
}

void ScanWizardScanner::customEvent( QCustomEvent * e )
{
     ScannerEvent *scanEvent = (ScannerEvent*)e;
     if ((popupProgress == NULL) &&
          (scanEvent->eventType() != ScanWizardScanner::ScannerEvent::Update))
           return;

     switch (scanEvent->eventType())
     {
     case ScanWizardScanner::ScannerEvent::ServiceScanComplete:
         popupProgress->progress(PROGRESS_MAX);
         cancelScan();
         break;
     case ScanWizardScanner::ScannerEvent::Update:
         log->updateText(scanEvent->strValue());
         break;
     case ScanWizardScanner::ScannerEvent::TableLoaded:
         popupProgress->incrementProgress();
         break;
     case ScanWizardScanner::ScannerEvent::ServicePct:
         popupProgress->progress(scanEvent->intValue()*PROGRESS_MAX/100);
         break;
     case ScanWizardScanner::ScannerEvent::DVBStatus:
         popupProgress->dvbStatus(scanEvent->strValue());
         break;
     case ScanWizardScanner::ScannerEvent::DVBSNR:
         popupProgress->signalToNoise(scanEvent->intValue());
         break;
     case ScanWizardScanner::ScannerEvent::DVBSignalStrength:
         popupProgress->signalStrength(scanEvent->intValue());
         break;
     case ScanWizardScanner::ScannerEvent::TuneComplete:
         switch (scanEvent->intValue())
         {
            case ScanWizardScanner::ScannerEvent::OK:
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
                if (parent->scanType()==ScanTypeSetting::FullScan)
                {
                     if (parent->cardType() == CardUtil::ATSC)
                        scanner->ATSCScanTransport(parent->videoSource(),parent->atscTransport());
                     else
                        scanner->ScanTransports();
                }
                else if (parent->scanType()==ScanTypeSetting::FullTransportScan)
                    transportScanComplete();
                else
                    scanner->ScanServices();
                break;
            default:
                MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      tr("ScanWizard"),
                                      tr("Error tuning to transport"));
                pthread_join(tuner_thread,NULL);
                cancelScan();
          }
     }
}

void ScanWizardScanner::scanComplete()
{
    QApplication::postEvent(this,new ScannerEvent(ScannerEvent::ServiceScanComplete));
}

void ScanWizardScanner::transportScanComplete()
{
    scanner->ScanServicesSourceID(parent->videoSource());
    ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::ServicePct);
    e->intValue(TRANSPORT_PCT);
    QApplication::postEvent(this,e);
}

void *ScanWizardScanner::SpawnScanner(void *param)
{
    SIScan *scanner = (SIScan *)param;
    scanner->StartScanner();
    return NULL;
}

void *ScanWizardScanner::SpawnTune(void *param)
{
    ScanWizardScanner *scanner = (ScanWizardScanner*)param;
    ScanWizard *parent = scanner->parent;

    ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::TuneComplete);
    if (parent->scanType() == ScanTypeSetting::TransportScan)
    {
        if (!scanner->dvbchannel->SetTransportByInt(scanner->transportToTuneTo()))
        {
            e->intValue(ScanWizardScanner::ScannerEvent::ERROR_TUNE);
            QApplication::postEvent(scanner,e);
            return NULL;
        }
    }
    else if (parent->scanType() == ScanTypeSetting::FullScan)
    {
        if (parent->cardType() != CardUtil::ATSC)
        {
            if (!scanner->dvbchannel->TuneTransport(scanner->chan_opts,true))
            {
                e->intValue(ScanWizardScanner::ScannerEvent::ERROR_TUNE);
                QApplication::postEvent(scanner,e);
                return NULL;
            }
        }
    }
    e->intValue(ScanWizardScanner::ScannerEvent::OK);
    QApplication::postEvent(scanner,e);
    return NULL;
}

void ScanWizardScanner::TableLoaded()
{
    QApplication::postEvent(this,new ScannerEvent(ScannerEvent::TableLoaded));
}

void ScanWizardScanner::serviceScanPctComplete(int pct)
{
    ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::ServicePct);
    int tmp = TRANSPORT_PCT + ((100 - TRANSPORT_PCT) * pct)/100;
    e->intValue(tmp);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::updateText(const QString& str)
{
    ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::Update);
    e->strValue(str);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::dvbStatus(QString str)
{
    ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::DVBStatus);
    e->strValue(str);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::dvbSNR(int i)
{
    ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::DVBSNR);
    e->intValue(i);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::dvbSignalStrength(int i)
{
    ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::DVBSignalStrength);
    e->intValue(i);
    QApplication::postEvent(this,e);
}

ScanProgressPopup::ScanProgressPopup(ScanWizardScanner *parent) : VerticalConfigurationGroup(false,false)
{
    setLabel(tr("Scan Progress"));

    HorizontalConfigurationGroup *box = new HorizontalConfigurationGroup();
    box->addChild(sta = new TransLabelSetting());
    box->addChild(sl = new TransLabelSetting());
    sta->setLabel(QObject::tr("Status"));
    sta->setValue(tr("Tuning"));
    sl->setValue("                           ");
    box->setUseFrame(false);
    addChild(box);

    addChild(progressBar = new ScanSignalMeter(PROGRESS_MAX));
    progressBar->setValue(0);
    addChild(ss = new ScanSignalMeter(65535));
    addChild(sn = new ScanSignalMeter(65535));
    progressBar->setLabel(QObject::tr("Scan Activity"));
    ss->setLabel(QObject::tr("Signal Strength"));
    sn->setLabel(QObject::tr("Signal/Noise"));

    TransButtonSetting *cancel = new TransButtonSetting();
    cancel->setLabel(tr("Cancel"));
    addChild(cancel);

    connect(cancel,SIGNAL(pressed(void)),parent,SLOT(cancelScan(void)));

    //Seem to need to do this as the constructor doesn't seem enough
    setUseLabel(false);
    setUseFrame(false);
}

ScanProgressPopup::~ScanProgressPopup()
{
}

void ScanProgressPopup::signalToNoise(int value)
{
    sn->setValue(value);
}

void ScanProgressPopup::signalStrength(int value)
{
    ss->setValue(value);
}

void ScanProgressPopup::dvbStatus(const QString& value)
{
    sl->setValue(value);
}

void ScanProgressPopup::status(const QString& value)
{
    sta->setValue(value);
}

void ScanProgressPopup::exec(ScanWizardScanner *parent)
{
    dialog = (ConfigPopupDialogWidget*)dialogWidget(gContext->GetMainWindow());
    connect(dialog,SIGNAL(popupDone(void)),parent,SLOT(cancelScan(void)));
    dialog->ShowPopup(this);
}

void ScanWizardScanner::cancelScan()
{
//cerr << "ScanWizardScanner::cancelScan\n";
    finish();
    delete popupProgress;
    popupProgress = NULL;
}

void ScanWizardScanner::scan()
{
    //cerr << "ScanWizardScanner::scan " << parent->scanType() <<" "<< parent->videoSource() << " " << parent->transport() << " " << parent->captureCard() << "\n";

    nTransportToTuneTo = parent->transport();
    dvbchannel = new DVBChannel(parent->videoDev());

    // These locks and objects might already exist in videosource need to check

    if(!dvbchannel->Open())
       return;

    scanner = new SIScan(dvbchannel, parent->videoSource());
    
    scanner->SetForceUpdate(true);
    
    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = QString("SELECT freetoaironly FROM cardinput "
                               "WHERE cardinput.cardid=%1 AND "
                               "cardinput.sourceid=%2")
                               .arg(parent->captureCard())
                               .arg(parent->videoSource());

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

    // Signal Meters Need connecting here
    connect(dvbchannel->monitor,SIGNAL(Status(QString)),this,SLOT(dvbStatus(QString)));
    connect(dvbchannel->monitor,SIGNAL(StatusSignalToNoise(int)),this,SLOT(dvbSNR(int)));
    connect(dvbchannel->monitor,SIGNAL(StatusSignalStrength(int)),this,SLOT(dvbSignalStrength(int)));

    popupProgress = new ScanProgressPopup(this);
    popupProgress->progress(0);
    popupProgress->exec(this);

    memset(&chan_opts.tuning,0,sizeof(chan_opts.tuning));
    if (parent->scanType() == ScanTypeSetting::FullScan)
    {
        bool fParseError = false;
        switch (parent->cardType())
        {
           case CardUtil::OFDM:
               if (!dvbchannel->ParseOFDM(parent->frequency(),
                              parent->inversion(),
                              parent->bandwidth(),
                              parent->codeRateHP(),
                              parent->codeRateLP(),
                              parent->constellation(),
                              parent->transmissionMode(),
                              parent->guardInterval(),
                              parent->hierarchy(),chan_opts.tuning))
               {
                    fParseError = true;
                    break;
               }
               break;
           case CardUtil::QPSK:
               //SQL code to get the disqec paramters HERE
               thequery = QString("SELECT dvb_diseqc_type, "
                        "diseqc_port, diseqc_pos, lnb_lof_switch, lnb_lof_hi, "
                        "lnb_lof_lo FROM cardinput,capturecard "
                        "WHERE capturecard.cardid=%1 and cardinput.sourceid=%2")
                        .arg(parent->captureCard())
                        .arg(parent->videoSource());

               query.prepare(thequery);

               if (query.exec() && query.isActive() && query.size() > 0)
               {
                   query.next();
                   if (!dvbchannel->ParseQPSK(parent->frequency(),
                                  parent->inversion(),
                                  parent->symbolRate(),
                                  parent->fec(),
                                  parent->polarity(),
                                  query.value(0).toString(), // diseqc_type
                                  query.value(1).toString(), // diseqc_port
                                  query.value(2).toString(), // diseqc_pos
                                  query.value(3).toString(), // lnb_lof_switch
                                  query.value(4).toString(), // lnb_lof_hi
                                  query.value(5).toString(), // lnb_lof_lo
                                  chan_opts.tuning))
                        fParseError = true;
                   }
                   else
                       fParseError = true;
               break;
           case CardUtil::QAM:
               if (!dvbchannel->ParseQAM(parent->frequency(),
                                         parent->inversion(),
                                         parent->symbolRate(),
                                         parent->fec(),
                                         parent->modulation(),
                                         chan_opts.tuning))
               {
                    fParseError = true;
                    break;
               }
               break;
           case CardUtil::ATSC:
               break;
           default:
               MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      tr("ScanWizard"),
                                      tr("Error detecting card type"));
               cancelScan();
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
    }
    pthread_create(&tuner_thread, NULL, SpawnTune, this);
}

ScanWizard::ScanWizard() :
    nScanType(ScanTypeSetting::FullScan), nVideoDev(0),
    nATSCTransport(0)
{
    ScanWizardScanner *page3 = new ScanWizardScanner(this);
    ScanWizardScanType *page1 = new ScanWizardScanType(this);
    ScanWizardTuningPage *page2 = new ScanWizardTuningPage(this);

    connect(this,SIGNAL(cardTypeChanged(unsigned)),
             page2,SLOT(cardTypeChanged(unsigned)));

    connect(page1, SIGNAL(scanTypeChanged(ScanTypeSetting::Type)),
        this, SLOT(scanType(ScanTypeSetting::Type)));
    connect(page1, SIGNAL(scanTypeChanged(ScanTypeSetting::Type)),
        page2, SLOT(scanType(ScanTypeSetting::Type)));

    connect(this,SIGNAL(scan()), page3 ,SLOT(scan()));

    addChild(page1);
    addChild(page2);
    addChild(page3);

}

void ScanWizard::videoSource(const QString& str)
{
    nVideoSource = str.toInt();
}

void ScanWizard::transport(const QString& str)
{
    nTransport = str.toInt();
}

void ScanWizard::captureCard(const QString& str)
{
    int nNewVideoDev = -1;
    unsigned nNewCaptureCard = str.toInt();
    //Work out what the new card type is
    if ((nNewCaptureCard != nCaptureCard) || (nCardType == CardUtil::ERROR_PROBE))
    {
        nCaptureCard = nNewCaptureCard;

        //Work out what kind of card we've got
        nNewVideoDev = CardUtil::videoDeviceFromCardID(nCaptureCard);
        CardUtil::DVB_TYPES nNewCardType = CardUtil::ERROR_PROBE;
        if (nNewVideoDev >= 0)
        {
            nVideoDev = nNewVideoDev;
            nNewCardType = CardUtil::cardDVBType(nNewVideoDev);
        }
        if (nCardType != (unsigned)nNewCardType)
        {
           nCardType = nNewCardType;
           emit cardTypeChanged(nCardType);
        }
     }
}

void ScanWizard::atscTransport(const QString& str)
{
    nATSCTransport = str.toInt();
}

MythDialog* ScanWizard::dialogWidget(MythMainWindow *parent,
                                     const char *widgetName)
{
    MythWizard* wizard = (MythWizard*)ConfigurationWizard::dialogWidget(parent,widgetName);
    connect(wizard, SIGNAL(selected(const QString&)),
            this, SLOT(pageSelected(const QString&)));
    return wizard;
}

void ScanWizard::pageSelected(const QString& strTitle)
{
    if (strTitle == ScanWizardScanner::strTitle)
       emit scan();
}
