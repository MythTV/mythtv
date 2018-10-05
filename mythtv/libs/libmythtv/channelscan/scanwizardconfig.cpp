#include "scanwizardconfig.h"

#include "videosource.h"
#include "cardutil.h"
#include "frequencytablesetting.h"

#include "channelscanmiscsettings.h"
#include "inputselectorsetting.h"
#include "scanwizard.h"

#include "panedvbs.h"
#include "panedvbs2.h"
#include "panedvbc.h"
#include "panedvbt.h"
#include "panedvbt2.h"
#include "paneatsc.h"
#include "paneanalog.h"
#include "panesingle.h"
#include "paneall.h"
#include "panedvbutilsimport.h"
#include "paneexistingscanimport.h"

void ScanWizard::SetupConfig(
    uint    default_sourceid,  uint default_cardid,
    QString default_inputname)
{
    videoSource = new VideoSourceSelector(
                    default_sourceid, CardUtil::GetScanableInputTypes(), false);
    input = new InputSelector(default_cardid, default_inputname);
    scanType = new ScanTypeSetting(),
    scanConfig = new ScanOptionalConfig(scanType);
    services = new DesiredServices();
    ftaOnly = new FreeToAirOnly();
    trustEncSI = new TrustEncSISetting();

    addChild(services);
    addChild(ftaOnly);
    addChild(trustEncSI);

    addChild(videoSource);
    addChild(input);
    addChild(scanType);
    addChild(scanConfig);

    connect(videoSource, SIGNAL(valueChanged(const QString&)),
            scanConfig,  SLOT(  SetSourceID( const QString&)));

    connect(videoSource, SIGNAL(valueChanged(const QString&)),
            input,       SLOT(  SetSourceID( const QString&)));

    connect(input,       SIGNAL(valueChanged(const QString&)),
            scanType,    SLOT(  SetInput(    const QString&)));

    connect(input,       SIGNAL(valueChanged(const QString&)),
            this,     SLOT(  SetInput(    const QString&)));
}

uint ScanWizard::GetSourceID(void) const
{
    return videoSource->getValue().toUInt();
}

ServiceRequirements ScanWizard::GetServiceRequirements(void) const
{
    return services->GetServiceRequirements();
}

bool ScanWizard::DoFreeToAirOnly(void) const
{
    return ftaOnly->getValue().toInt();
}

bool ScanWizard::DoTestDecryption(void) const
{
    return trustEncSI->getValue().toInt();
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void ScanTypeSetting::SetInput(const QString &cardids_inputname)
{
    uint    cardid    = 0;
    QString inputname = QString::null;
    if (!InputSelector::Parse(cardids_inputname, cardid, inputname))
        return;

    // Only refresh if we really have to. If we do it too often
    // Then we end up fighting the scan routine when we want to
    // check the type of dvb card :/
    if (cardid == hw_cardid)
        return;

    hw_cardid       = cardid;
    QString subtype = CardUtil::ProbeSubTypeName(hw_cardid);
    int nCardType   = CardUtil::toInputType(subtype);
    clearSelections();

    switch (nCardType)
    {
        case CardUtil::V4L:
        case CardUtil::MPEG:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_Analog), true);
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            return;
        case CardUtil::DVBT:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBT), true);
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBT));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::DVBT2:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBT2), true);
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBT2));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::DVBS:
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBS));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::DVBS2:
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBS2));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::QAM:
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBC));
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBC));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::ATSC:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_ATSC), true);
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::HDHOMERUN:
            if (CardUtil::HDHRdoesDVB(CardUtil::GetVideoDevice(cardid)))
            {
                addSelection(tr("Full Scan"),
                             QString::number(FullScan_DVBT), true);
                addSelection(tr("Full Scan (Tuned)"),
                             QString::number(NITAddScan_DVBT));
            }
            else
                addSelection(tr("Full Scan"),
                             QString::number(FullScan_ATSC), true);
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::VBOX:
            addSelection(tr("VBox Channel Import"),
                         QString::number(VBoxImport), true);
            return;
        case CardUtil::FREEBOX:
            addSelection(tr("M3U Import with MPTS"),
                         QString::number(IPTVImportMPTS), true);
            addSelection(tr("M3U Import"),
                         QString::number(IPTVImport), true);
            return;
        case CardUtil::ASI:
            addSelection(tr("ASI Scan"),
                         QString::number(CurrentTransportScan), true);
            return;
        case CardUtil::EXTERNAL:
            addSelection(tr("MPTS Scan"),
                         QString::number(CurrentTransportScan), true);
            addSelection(tr("Import from ExternalRecorder"),
                         QString::number(ExternRecImport), true);
            return;
        case CardUtil::ERROR_PROBE:
            addSelection(tr("Failed to probe the card"),
                         QString::number(Error_Probe), true);
            return;
        default:
            addSelection(tr("Failed to open the card"),
                         QString::number(Error_Open), true);
            return;
    }

    addSelection(tr("Scan of all existing transports"),
                 QString::number(FullTransportScan));
    addSelection(tr("Scan of single existing transport"),
                 QString::number(TransportScan));
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ScanOptionalConfig::ScanOptionalConfig(ScanTypeSetting *_scan_type) :
    scanType(_scan_type),
    dvbTCountry(new ScanCountry()),
    dvbT2Country(new ScanCountry()),
    network(new ScanNetwork()),
    paneDVBT(new PaneDVBT(QString::number(ScanTypeSetting::NITAddScan_DVBT), scanType)),
    paneDVBT2(new PaneDVBT2(QString::number(ScanTypeSetting::NITAddScan_DVBT2), scanType)),
    paneDVBS(new PaneDVBS(QString::number(ScanTypeSetting::NITAddScan_DVBS), scanType)),
    paneDVBS2(new PaneDVBS2(QString::number(ScanTypeSetting::NITAddScan_DVBS2), scanType)),
    paneATSC(new PaneATSC(QString::number(ScanTypeSetting::FullScan_ATSC), scanType)),
    paneDVBC(new PaneDVBC(QString::number(ScanTypeSetting::NITAddScan_DVBC), scanType)),
    paneAnalog(new PaneAnalog(QString::number(ScanTypeSetting::FullScan_Analog), scanType)),
    paneSingle(new PaneSingle(QString::number(ScanTypeSetting::TransportScan), scanType)),
    paneAll(new PaneAll(QString::number(ScanTypeSetting::FullTransportScan), scanType)),
    paneDVBUtilsImport(new PaneDVBUtilsImport()),
    paneExistingScanImport(new PaneExistingScanImport(QString::number(ScanTypeSetting::ExistingScanImport), scanType))

{
    setVisible(false);

    scanType->addTargetedChild(QString::number(ScanTypeSetting::Error_Open),
              new PaneError(tr("Failed to open the card")));
    scanType->addTargetedChild(QString::number(ScanTypeSetting::Error_Probe),
              new PaneError(tr("Failed to probe the card")));
    scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBC),
              network);
    scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBT),
              dvbTCountry);
    scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBT2),
              dvbT2Country);
//     scanType->addTargetedChild(QString::number(ScanTypeSetting::DVBUtilsImport),
//               paneDVBUtilsImport);
}

void ScanOptionalConfig::SetSourceID(const QString &sourceid)
{
    paneAnalog->SetSourceID(sourceid.toUInt());
    paneSingle->SetSourceID(sourceid.toUInt());
    paneExistingScanImport->SetSourceID(sourceid.toUInt());
}

QString ScanOptionalConfig::GetFrequencyStandard(void) const
{
    int     st =  scanType->getValue().toInt();

    switch (st)
    {
        case ScanTypeSetting::FullScan_ATSC:
            return "atsc";
        case ScanTypeSetting::FullScan_DVBC:
            return "dvbc";
        case ScanTypeSetting::FullScan_DVBT:
        case ScanTypeSetting::FullScan_DVBT2:
            return "dvbt";
        case ScanTypeSetting::FullScan_Analog:
            return "analog";
        default:
            return "unknown";
    }
}

QString ScanOptionalConfig::GetModulation(void) const
{
    int     st =  scanType->getValue().toInt();

    switch (st)
    {
        case ScanTypeSetting::FullScan_ATSC:
            return paneATSC->GetModulation();
        case ScanTypeSetting::FullScan_DVBC:
            return "qam";
        case ScanTypeSetting::FullScan_DVBT:
        case ScanTypeSetting::FullScan_DVBT2:
            return "ofdm";
        case ScanTypeSetting::FullScan_Analog:
            return "analog";
        default:
            return "unknown";
    }
}

QString ScanOptionalConfig::GetFrequencyTable(void) const
{
    int st =  scanType->getValue().toInt();

    switch (st)
    {
        case ScanTypeSetting::FullScan_ATSC:
            return paneATSC->GetFrequencyTable();
        case ScanTypeSetting::FullScan_DVBC:
            return network->getValue();
        case ScanTypeSetting::FullScan_DVBT:
            return dvbTCountry->getValue();
        case ScanTypeSetting::FullScan_DVBT2:
            return dvbT2Country->getValue();
        case ScanTypeSetting::FullScan_Analog:
            return paneAnalog->GetFrequencyTable();
        default:
            return "unknown";
    }
}

bool ScanOptionalConfig::GetFrequencyTableRange(
    QString &start, QString &end) const
{
    start = end  = QString::null;

    int st = scanType->getValue().toInt();
    if (ScanTypeSetting::FullScan_ATSC == st)
        return paneATSC->GetFrequencyTableRange(start, end);

    return false;
}

bool ScanOptionalConfig::DoIgnoreSignalTimeout(void) const
{
    int  st  = scanType->getValue().toInt();

    switch (st)
    {
    case ScanTypeSetting::TransportScan:
        return paneSingle->ignoreSignalTimeout();
    case ScanTypeSetting::FullTransportScan:
        return paneAll->ignoreSignalTimeout();
    case ScanTypeSetting::DVBUtilsImport:
        return paneDVBUtilsImport->DoIgnoreSignalTimeout();
    default:
        return false;
    }
}

bool ScanOptionalConfig::DoFollowNIT(void) const
{
    int  st  = scanType->getValue().toInt();
    switch (st)
    {
    case ScanTypeSetting::TransportScan:
        return paneSingle->GetFollowNIT();
    case ScanTypeSetting::FullTransportScan:
        return paneAll->GetFollowNIT();
    default:
        return false;
    }
}

QString ScanOptionalConfig::GetFilename(void) const
{
    return paneDVBUtilsImport->GetFilename();
}

uint ScanOptionalConfig::GetMultiplex(void) const
{
    int mplexid = paneSingle->GetMultiplex();
    return (mplexid <= 0) ? 0 : mplexid;
}

uint ScanOptionalConfig::GetScanID(void) const
{
    return paneExistingScanImport->GetScanID();
}

QMap<QString,QString> ScanOptionalConfig::GetStartChan(void) const
{
    QMap<QString,QString> startChan;

    int st = scanType->getValue().toInt();
    if (ScanTypeSetting::NITAddScan_DVBT == st)
    {
        const PaneDVBT *pane = paneDVBT;

        startChan["std"]            = "dvb";
        startChan["type"]           = "OFDM";
        startChan["frequency"]      = pane->frequency();
        startChan["inversion"]      = pane->inversion();
        startChan["bandwidth"]      = pane->bandwidth();
        startChan["coderate_hp"]    = pane->coderate_hp();
        startChan["coderate_lp"]    = pane->coderate_lp();
        startChan["constellation"]  = pane->constellation();
        startChan["trans_mode"]     = pane->trans_mode();
        startChan["guard_interval"] = pane->guard_interval();
        startChan["hierarchy"]      = pane->hierarchy();
    }
    else if (ScanTypeSetting::NITAddScan_DVBT2 == st)
    {
        const PaneDVBT2 *pane = paneDVBT2;

        startChan["std"]            = "dvb";
        startChan["type"]           = "DVB_T2";
        startChan["frequency"]      = pane->frequency();
        startChan["inversion"]      = pane->inversion();
        startChan["bandwidth"]      = pane->bandwidth();
        startChan["coderate_hp"]    = pane->coderate_hp();
        startChan["coderate_lp"]    = pane->coderate_lp();
        startChan["constellation"]  = pane->constellation();
        startChan["trans_mode"]     = pane->trans_mode();
        startChan["guard_interval"] = pane->guard_interval();
        startChan["hierarchy"]      = pane->hierarchy();
        startChan["mod_sys"]        = pane->mod_sys();
    }
    else if (ScanTypeSetting::NITAddScan_DVBS == st)
    {
        const PaneDVBS *pane = paneDVBS;

        startChan["std"]        = "dvb";
        startChan["type"]       = "QPSK";
        startChan["modulation"] = "qpsk";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["polarity"]   = pane->polarity();
    }
    else if (ScanTypeSetting::NITAddScan_DVBC == st)
    {
        const PaneDVBC *pane = paneDVBC;

        startChan["std"]        = "dvb";
        startChan["type"]       = "QAM";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = pane->modulation();
    }
    else if (ScanTypeSetting::NITAddScan_DVBS2 == st)
    {
        const PaneDVBS2 *pane = paneDVBS2;

        startChan["std"]        = "dvb";
        startChan["type"]       = "DVB_S2";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = pane->modulation();
        startChan["polarity"]   = pane->polarity();
        startChan["mod_sys"]    = pane->mod_sys();
        startChan["rolloff"]    = pane->rolloff();
    }

    return startChan;
}
