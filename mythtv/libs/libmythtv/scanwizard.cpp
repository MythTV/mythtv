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
#include <qlocale.h>
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

class ScanATSCTransport: public ComboBoxSetting, public TransientStorage
{
public:
    enum Type {Terrestrial,Cable} ;
    ScanATSCTransport()
    {
        addSelection(QObject::tr("Terrestrial"),QString::number(Terrestrial),true);
        addSelection(QObject::tr("Cable"),QString::number(Cable));

        setLabel(QObject::tr("ATSC Transport"));
        setHelpText(QObject::tr("ATSC transport, cable or terrestrial"));
    }
};

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

class OFDMPane : public HorizontalConfigurationGroup
{
public:
    OFDMPane() : HorizontalConfigurationGroup(false,false,true,true)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left=new VerticalConfigurationGroup(false,true,true,false);
        VerticalConfigurationGroup *right=new VerticalConfigurationGroup(false,true,true,false);
        left->addChild(pfrequency=new ScanFrequency());
        left->addChild(pbandwidth=new ScanBandwidth());
        left->addChild(pinversion=new ScanInversion());
        left->addChild(pconstellation=new ScanConstellation());
        right->addChild(pcoderate_lp=new ScanCodeRateLP());
        right->addChild(pcoderate_hp=new ScanCodeRateHP());
        right->addChild(ptrans_mode=new ScanTransmissionMode());
        right->addChild(pguard_interval=new ScanGuardInterval());
        right->addChild(phierarchy=new ScanHierarchy());
        addChild(left);
        addChild(right);
     }

     QString frequency() { return pfrequency->getValue();}
     QString bandwidth() { return pbandwidth->getValue();}
     QString inversion() { return pinversion->getValue();}
     QString constellation() { return pconstellation->getValue();}
     QString coderate_lp() { return pcoderate_lp->getValue();}
     QString coderate_hp() { return pcoderate_hp->getValue();}
     QString trans_mode() { return ptrans_mode->getValue();}
     QString guard_interval() { return pguard_interval->getValue();}
     QString hierarchy() { return phierarchy->getValue();}

protected:
    ScanFrequency *pfrequency;
    ScanInversion *pinversion;
    ScanBandwidth *pbandwidth;
    ScanConstellation* pconstellation;
    ScanCodeRateLP *pcoderate_lp;
    ScanCodeRateHP *pcoderate_hp;
    ScanTransmissionMode *ptrans_mode;
    ScanGuardInterval *pguard_interval;
    ScanHierarchy *phierarchy;
};

class QPSKPane : public HorizontalConfigurationGroup
{
public:
    QPSKPane() : HorizontalConfigurationGroup(false,false,true,false) 
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left=new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right=new VerticalConfigurationGroup(false,true);
        left->addChild(pfrequency = new ScanFrequency());
        left->addChild(ppolarity = new ScanPolarity());
        left->addChild(psymbolrate=new ScanSymbolRate());
        right->addChild(pfec=new ScanFec());
        right->addChild(pinversion=new ScanInversion());
        addChild(left);
        addChild(right);     
    }

    QString frequency() {return pfrequency->getValue();}
    QString symbolrate() {return psymbolrate->getValue();}
    QString inversion() {return pinversion->getValue();}
    QString fec() {return pfec->getValue();}
    QString polarity() {return ppolarity->getValue();}

protected:
    ScanFrequency *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion *pinversion;
    ScanFec *pfec;
    ScanPolarity *ppolarity;
};

class QAMPane : public HorizontalConfigurationGroup
{
public:
    QAMPane() : HorizontalConfigurationGroup(false,false,true,false) 
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left=new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right=new VerticalConfigurationGroup(false,true);
        left->addChild(pfrequency = new ScanFrequency());
        left->addChild(psymbolrate=new ScanSymbolRate());
        left->addChild(pinversion=new ScanInversion());
        right->addChild(pmodulation = new ScanModulation());
        right->addChild(pfec=new ScanFec());
        addChild(left);
        addChild(right);     
    }

    QString frequency() {return pfrequency->getValue();}
    QString symbolrate() {return psymbolrate->getValue();}
    QString inversion() {return pinversion->getValue();}
    QString fec() {return pfec->getValue();}
    QString modulation() {return pmodulation->getValue();}

protected:
    ScanFrequency *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion *pinversion;
    ScanModulation *pmodulation;
    ScanFec *pfec;
};

class ATSCPane : public HorizontalConfigurationGroup
{
public:
    ATSCPane()  : HorizontalConfigurationGroup(false,false,true,false)
    {
        setUseFrame(false);
        addChild(patscTransport=new ScanATSCTransport());
    }

    int atscTransport() {return patscTransport->getValue().toInt();}
protected:
    ScanATSCTransport *patscTransport;
};

class ErrorPane : public HorizontalConfigurationGroup
{
public:
    ErrorPane(const QString& error) :
            HorizontalConfigurationGroup(false,false,true,false)
    {
        setUseFrame(false);
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
        addSelection("ERROR_OPEN","0");
        addSelection("ERROR_PROBE","1");
        addSelection("QPSK","2");
        addSelection("QAM","3");
        addSelection("OFDM","4");
        addSelection("ATSC","5");
    }
};

class BlankSetting: public TransLabelSetting
{
public:
     BlankSetting() : TransLabelSetting()
     {
         setLabel("");
     }
};

class OptionalTypeSetting : public ComboBoxSetting, public TransientStorage
{
public:
    enum {Error_Open,Error_Probe,QPSK,QAM,OFDM,ATSC,Country,Transport,Blank};
    OptionalTypeSetting()
    {
        addSelection("ERROR_OPEN",QString::number(Error_Open));
        addSelection("ERROR_PROBE",QString::number(Error_Probe));
        addSelection("QPSK",QString::number(QPSK));
        addSelection("QAM",QString::number(QAM));
        addSelection("OFDM",QString::number(OFDM));
        addSelection("ATSC",QString::number(ATSC));
        addSelection("Full-scan:country",QString::number(Country));
        addSelection("Given transport",QString::number(Transport));
        addSelection("Blank",QString::number(Blank));
    }
};

ScanCountry::ScanCountry()
{
    QLocale locale = QLocale::system();
    QLocale::Country country = locale.country();

    setLabel(tr("Country"));
    addSelection(QObject::tr("Australia"),QString::number(AU),
                 country==QLocale::Australia);
    addSelection(QObject::tr("Finland"),QString::number(FI),
                 country==QLocale::Finland);
    addSelection(QObject::tr("Sweden"),QString::number(SE),
                 country==QLocale::Sweden);
    addSelection(QObject::tr("United Kingdom"),QString::number(UK),
                 country==QLocale::UnitedKingdom);
    addSelection(QObject::tr("Germany"),QString::number(DE),
                 country==QLocale::Germany);
}

ScanOptionalConfig::ScanOptionalConfig(ScanWizard *wizard) : 
                  VerticalConfigurationGroup(false,false,true,true)
{
    setUseLabel(false);

    optionalTypeSetting=new OptionalTypeSetting();
    optionalTypeSetting->setVisible(false);
    addChild(optionalTypeSetting);
    setTrigger(optionalTypeSetting);
    // only save settings for the selected grabber
    setSaveAll(false);

    addTarget(QString::number(OptionalTypeSetting::Error_Open),
             new ErrorPane(QObject::tr("Failed to open the card")));
    addTarget(QString::number(OptionalTypeSetting::Error_Probe),
             new ErrorPane(QObject::tr("Failed to probe the card")));
    addTarget(QString::number(OptionalTypeSetting::QAM),wizard->paneQAM);
    addTarget(QString::number(OptionalTypeSetting::QPSK),wizard->paneQPSK);
    addTarget(QString::number(OptionalTypeSetting::OFDM),wizard->paneOFDM);
    addTarget(QString::number(OptionalTypeSetting::ATSC),wizard->paneATSC);
    country = new ScanCountry();
    addTarget(QString::number(OptionalTypeSetting::Country),country);
    transport = new TransportSetting();
    addTarget(QString::number(OptionalTypeSetting::Transport),transport);
    addTarget(QString::number(OptionalTypeSetting::Blank),new BlankSetting());
}

void ScanOptionalConfig::triggerChanged(const QString& value)
{
    TriggeredConfigurationGroup::triggerChanged(value);
}

ScanWizardScanType::ScanWizardScanType(ScanWizard *_parent) : parent(_parent)
{
    setLabel(tr("Scan Type"));
    setUseLabel(false);

    videoSource = new VideoSourceSetting();
    capturecard = new CaptureCardSetting();

    HorizontalConfigurationGroup *h1 = new HorizontalConfigurationGroup(false,false,true,true);
    h1->addChild(videoSource);
    h1->addChild(capturecard);
    addChild(h1);
    scanType = new ScanTypeSetting();
    addChild(scanType);
    scanConfig = new ScanOptionalConfig(_parent);
    addChild(scanConfig);

    connect(videoSource, SIGNAL(valueChanged(const QString&)),
        scanConfig->transport, SLOT(sourceID(const QString&)));
    connect(videoSource, SIGNAL(valueChanged(const QString&)),
        capturecard, SLOT(sourceID(const QString&)));

    connect(capturecard, SIGNAL(valueChanged(const QString&)),
        parent, SLOT(captureCard(const QString&)));

    connect(scanType, SIGNAL(valueChanged(const QString&)),
        this, SLOT(scanTypeChanged(const QString&)));
    connect(capturecard, SIGNAL(valueChanged(const QString&)),
        this, SLOT(scanTypeChanged(const QString&)));

    connect(this,SIGNAL(scanOptionChange(const  QString&)),
        scanConfig,SLOT(triggerChanged(const QString&)));
    scanTypeChanged("");
}

void ScanWizardScanType::scanTypeChanged(const QString& str)
{
    (void)str;
    int nCard = capturecard->getValue().toInt();
    int nType = scanType->getValue().toInt();

    int nVideoDev = CardUtil::videoDeviceFromCardID(nCard);
    int nCardType = CardUtil::ERROR_OPEN;
    if (nVideoDev >= 0)
        nCardType = CardUtil::cardDVBType(nVideoDev);
    int nPane = OptionalTypeSetting::Blank;
    switch (nType)
    {
    case ScanTypeSetting::FullScan:
        if (nCardType == CardUtil::OFDM)
        {
            nPane = OptionalTypeSetting::Country;
            break;
        }
        //Fall through
    case ScanTypeSetting::FullTunedScan:
        switch (nCardType)
        {
        case CardUtil::QPSK:
            nPane = OptionalTypeSetting::QPSK;
            break;
        case CardUtil::QAM:
            nPane = OptionalTypeSetting::QAM;
            break;
        case CardUtil::OFDM:
            nPane = OptionalTypeSetting::OFDM;
            break;
        case CardUtil::ATSC:
            nPane = OptionalTypeSetting::ATSC;
            break;
        case CardUtil::ERROR_OPEN:
            nPane = OptionalTypeSetting::Error_Open;
            break;
        default:
            nPane = OptionalTypeSetting::Error_Probe;
            break;
        }
        break;
    case ScanTypeSetting::TransportScan:
        nPane = OptionalTypeSetting::Transport;
        break;
    }
    emit scanOptionChange(QString::number(nPane));
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
    tunerthread_running = false;
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
                if (parent->scanType()==ScanTypeSetting::FullTunedScan)
                {
                     if (parent->nCardType == CardUtil::ATSC)
                        scanner->ATSCScanTransport(parent->videoSource(),
                          parent->paneATSC->atscTransport());
                     else
                        scanner->ScanTransports();
                }
                else if (parent->scanType()==ScanTypeSetting::FullScan)
                {
                    if (parent->nCardType == CardUtil::ATSC)
                        scanner->ATSCScanTransport(parent->videoSource(),
                          parent->paneATSC->atscTransport());
                    else if (parent->nCardType == CardUtil::OFDM)
                        scanner->DVBTScanTransport(parent->videoSource(),
                            parent->country());
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
                if (tunerthread_running)
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
    else if ((parent->scanType() == ScanTypeSetting::FullTunedScan) ||
             (parent->scanType() == ScanTypeSetting::FullScan))
    {
        if (!scanner->dvbchannel->TuneTransport(scanner->chan_opts,true))
        {
            e->intValue(ScanWizardScanner::ScannerEvent::ERROR_TUNE);
            QApplication::postEvent(scanner,e);
            return NULL;
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
    if (str.isEmpty())
         return;
    ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::Update);
    e->strValue(str);
    QApplication::postEvent(this,e);
}

void ScanWizardScanner::dvbStatus(const QString& str)
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
    tunerthread_running = false;
    nTransportToTuneTo = parent->transport();
    dvbchannel = new DVBChannel(parent->nVideoDev);

    // These locks and objects might already exist in videosource need to check

    if(!dvbchannel->Open())
       return;

    dvbchannel->StartMonitor();
    scanner = new SIScan(dvbchannel, parent->videoSource());
    
    scanner->SetForceUpdate(true);
    
    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = QString("SELECT freetoaironly"
                               " FROM cardinput "
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
    connect(dvbchannel->monitor,SIGNAL(Status(const QString& )),this,SLOT(dvbStatus(const QString&)));
    connect(dvbchannel->monitor,SIGNAL(StatusSignalToNoise(int)),this,SLOT(dvbSNR(int)));
    connect(dvbchannel->monitor,SIGNAL(StatusSignalStrength(int)),this,SLOT(dvbSignalStrength(int)));

    popupProgress = new ScanProgressPopup(this);
    popupProgress->progress(0);
    popupProgress->exec(this);

    memset(&chan_opts.tuning,0,sizeof(chan_opts.tuning));

    //We can't do a full scan for DVB-C or DVB-S
    int scanType = parent->scanType();
    if ((scanType == ScanTypeSetting::FullScan) &&
         ((parent->nCardType==CardUtil::QPSK) ||
          (parent->nCardType==CardUtil::QAM)))
        scanType = ScanTypeSetting::FullTunedScan;

    if (scanType == ScanTypeSetting::FullTunedScan)
    {
        bool fParseError = false;
        switch (parent->nCardType)
        {
           case CardUtil::OFDM:
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
               {
                    fParseError = true;
                    break;
               }
               break;
           }
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
                   else
                       fParseError = true;
               break;
           case CardUtil::QAM:
           {
               QAMPane *pane = parent->paneQAM;
               if (!chan_opts.tuning.parseQAM(pane->frequency(),
                                        pane->inversion(),
                                        pane->symbolrate(),
                                        pane->fec(),
                                        pane->modulation()))
               {
                    fParseError = true;
                    break;
               }
               break;
           }
           case CardUtil::ATSC:
           {
               ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::TuneComplete);
               e->intValue(ScanWizardScanner::ScannerEvent::OK);
               QApplication::postEvent(this,e);
               return;
           }
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
    else if (scanType == ScanTypeSetting::FullScan)
    {
       ScannerEvent* e=new ScannerEvent(ScanWizardScanner::ScannerEvent::TuneComplete);
       e->intValue(ScanWizardScanner::ScannerEvent::OK);
       QApplication::postEvent(this,e);
       return;
    }

    tunerthread_running = true;
    pthread_create(&tuner_thread, NULL, SpawnTune, this);
}

ScanWizard::ScanWizard() : nVideoDev(-1),
          nCardType(CardUtil::ERROR_PROBE), nCaptureCard(-1)
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
    if ((nCaptureCard != nNewCaptureCard) || (nCardType==CardUtil::ERROR_OPEN))
    {
        nCaptureCard = nNewCaptureCard;
        nVideoDev = CardUtil::videoDeviceFromCardID(nCaptureCard);
        nCardType = CardUtil::ERROR_OPEN;
        if (nVideoDev >= 0)
            nCardType = CardUtil::cardDVBType(nVideoDev);
        emit cardTypeChanged(QString::number(nCardType));
    }
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
    //This is a bit hacky, but it seems to be needed otherwise the
    //trigger pane on page 1 doesn't seem to update, this is seen
    //when you escape back to page 1
    page1->scanTypeChanged(""); 
}

