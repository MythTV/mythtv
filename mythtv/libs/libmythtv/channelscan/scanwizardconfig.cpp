#include "scanwizardconfig.h"

#include "cardutil.h"
#include "channelscanmiscsettings.h"
#include "channelutil.h"
#include "frequencytablesetting.h"
#include "inputselectorsetting.h"
#include "scanwizard.h"
#include "videosource.h"

#include "paneall.h"
#include "paneanalog.h"
#include "paneatsc.h"
#include "panedvbc.h"
#include "panedvbs.h"
#include "panedvbs2.h"
#include "panedvbt.h"
#include "panedvbt2.h"
#include "panedvbutilsimport.h"
#include "paneexistingscanimport.h"
#include "panesingle.h"

#ifdef USING_SATIP
#include "recorders/satiputils.h"
#endif

// Update default tuning parameters from reference transponder
void PaneDVBS2::SetTuningParameters(StandardSetting *setting)
{
  QString sat = setting->getValue();
  QString frequency = m_transponder->getFrequency(sat);
  if (!frequency.isEmpty())
  {
    setFrequency(frequency.toUInt());
    setPolarity(m_transponder->getPolarity(sat));
    setSymbolrate(m_transponder->getSymbolrate(sat));
    setModulation(m_transponder->getModulation(sat));
    setModSys(m_transponder->getModSys(sat));
    setFec(m_transponder->getFec(sat));
  }
}

void ScanWizard::SetupConfig(
    uint    default_sourceid,  uint default_cardid,
    const QString& default_inputname)
{
    m_videoSource = new VideoSourceSelector(
                    default_sourceid, CardUtil::GetScanableInputTypes(), false);
    m_input = new InputSelector(default_cardid, default_inputname);
    m_scanType = new ScanTypeSetting(),
    m_scanConfig = new ScanOptionalConfig(m_scanType);
    m_services = new DesiredServices();
    m_ftaOnly = new FreeToAirOnly();
    m_lcnOnly = new ChannelNumbersOnly();
    m_completeOnly = new CompleteChannelsOnly();
    m_fullSearch = new FullChannelSearch();
    m_removeDuplicates = new RemoveDuplicates();
    m_addFullTS = new AddFullTS();
    m_trustEncSI = new TrustEncSISetting();

    setLabel(tr("Channel Scan"));

    addChild(m_services);
    addChild(m_ftaOnly);
    addChild(m_lcnOnly);
    addChild(m_completeOnly);
    addChild(m_fullSearch);
    addChild(m_removeDuplicates);
    addChild(m_addFullTS);
    addChild(m_trustEncSI);

    addChild(m_videoSource);
    addChild(m_input);
    addChild(m_scanType);
    addChild(m_scanConfig);

    connect(m_videoSource, qOverload<const QString&>(&StandardSetting::valueChanged),
            m_scanConfig,  &ScanOptionalConfig::SetSourceID);

    connect(m_videoSource, qOverload<const QString&>(&StandardSetting::valueChanged),
            m_input,       &InputSelector::SetSourceID);

    connect(m_input,       qOverload<const QString&>(&StandardSetting::valueChanged),
            m_scanType,    &ScanTypeSetting::SetInput);

    connect(m_input,       qOverload<const QString&>(&StandardSetting::valueChanged),
            this,          &ScanWizard::SetInput);

    connect(m_input,       qOverload<const QString&>(&StandardSetting::valueChanged),
            this,          &ScanWizard::SetPaneDefaults);

}

uint ScanWizard::GetSourceID(void) const
{
    return m_videoSource->getValue().toUInt();
}

ServiceRequirements ScanWizard::GetServiceRequirements(void) const
{
    return m_services->GetServiceRequirements();
}

bool ScanWizard::DoFreeToAirOnly(void) const
{
    return m_ftaOnly->boolValue();
}

bool ScanWizard::DoChannelNumbersOnly(void) const
{
    return m_lcnOnly->boolValue();
}

bool ScanWizard::DoCompleteChannelsOnly(void) const
{
    return m_completeOnly->boolValue();
}

bool ScanWizard::DoFullChannelSearch(void) const
{
    return m_fullSearch->boolValue();
}

bool ScanWizard::DoRemoveDuplicates(void) const
{
    return m_removeDuplicates->boolValue();
}

bool ScanWizard::DoAddFullTS(void) const
{
    return m_addFullTS->boolValue();
}

bool ScanWizard::DoTestDecryption(void) const
{
    return m_trustEncSI->boolValue();
}

void ScanWizard::SetPaneDefaults(const QString &cardid_inputname)
{
    const int sourceid = m_videoSource->getValue().toInt();
    uint scanfrequency = 0;
    QString freqtable;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
            "SELECT scanfrequency, freqtable "
            "FROM videosource "
            "WHERE videosource.sourceid = :SOURCEID ;");
    query.bindValue(":SOURCEID", sourceid);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("ScanOptionalConfig::SetPaneDefaults", query);
        return;
    }
    if (query.next())
    {
        scanfrequency = query.value(0).toUInt();
        freqtable = query.value(1).toString();
        LOG(VB_CHANSCAN, LOG_DEBUG,
            QString("SetPaneDefaults cardid_inputname:%1 sourceid:%2 scanfrequency:%3 freqtable:%4")
                .arg(cardid_inputname).arg(sourceid).arg(scanfrequency).arg(freqtable));
    }

    // Channel Frequency Table for ATSC
    // Use general setting if not defined in the videosource
    {
        if (freqtable == "default")
        {
            freqtable = gCoreContext->GetSetting("FreqTable");
        }
        QString table;
        table = (freqtable == "us-bcast"    ) ? "us"      : table;
        table = (freqtable == "us-cable"    ) ? "uscable" : table;
        table = (freqtable == "us-cable-hrc") ? "ushrc"   : table;
        table = (freqtable == "us-cable-irc") ? "usirc"   : table;
        if (!table.isEmpty())
        {
            LOG(VB_CHANSCAN, LOG_DEBUG,
                QString("SetPaneDefaults ATSC frequency table:'%1'").arg(table));
            m_scanConfig->SetTuningPaneValuesATSC(table);
        }
    }

    // Set "Full Scan (Tuned)" defaults only when a frequency has been entered.
    if (scanfrequency == 0)
        return;

    // If we have only a frequency set that as default; if there is a multiplex
    // already at that frequency then use the values of that multiplex as
    // default values for scanning.
    int mplexid = 0;
    mplexid = ChannelUtil::GetMplexID(sourceid, scanfrequency);
    LOG(VB_CHANSCAN, LOG_DEBUG,
        QString("SetPaneDefaults sourceid:%1 frequency:%2 mplexid:%3")
            .arg(sourceid).arg(scanfrequency).arg(mplexid));

    DTVMultiplex mpx;
    if (mplexid > 0)
    {
        DTVTunerType tuner_type = CardUtil::GetTunerTypeFromMultiplex(mplexid);

        mpx.FillFromDB(tuner_type, mplexid);

        LOG(VB_CHANSCAN, LOG_DEBUG,
            QString("SetPaneDefaults sourceid:%1 frequency:%2 mplexid:%3 tuner_type:%4 mpx:%5")
                .arg(sourceid).arg(scanfrequency).arg(mplexid)
                .arg(tuner_type.toString(), mpx.toString()));
    }

    m_scanConfig->SetTuningPaneValues(scanfrequency, mpx);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void ScanTypeSetting::SetInput(const QString &cardids_inputname)
{
    uint    cardid    = 0;
    QString inputname;
    if (!InputSelector::Parse(cardids_inputname, cardid, inputname))
        return;

    // Only refresh if we really have to. If we do it too often
    // Then we end up fighting the scan routine when we want to
    // check the type of dvb card :/
    if (cardid == m_hwCardId)
        return;

    m_hwCardId     = cardid;
    QString subtype = CardUtil::ProbeSubTypeName(m_hwCardId);
    CardUtil::INPUT_TYPES nCardType   = CardUtil::toInputType(subtype);

#ifdef USING_SATIP
    if (nCardType == CardUtil::INPUT_TYPES::SATIP)
    {
        nCardType = SatIP::toDVBInputType(CardUtil::GetVideoDevice(cardid));
    }
#endif // USING_SATIP

    const QString fullScanHelpTextDVBT2 = QObject::tr(
        "For DVB-T/T2 and scan type 'Full Scan' select a country to get the correct set of frequencies.");

    clearSelections();

    switch (nCardType)
    {
        case CardUtil::INPUT_TYPES::V4L:
        case CardUtil::INPUT_TYPES::MPEG:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_Analog), true);
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            return;
        case CardUtil::INPUT_TYPES::DVBT:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBT), true);
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBT));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            setHelpText(fullScanHelpTextDVBT2);
            break;
        case CardUtil::INPUT_TYPES::DVBT2:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBT2), true);
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBT2));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            setHelpText(fullScanHelpTextDVBT2);
            break;
        case CardUtil::INPUT_TYPES::DVBS:
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBS));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::INPUT_TYPES::DVBS2:
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBS2));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::INPUT_TYPES::QAM:  // QAM == DVBC
            addSelection(tr("Full Scan (Tuned)"),
                         QString::number(NITAddScan_DVBC));
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_DVBC));
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::INPUT_TYPES::ATSC:
            addSelection(tr("Full Scan"),
                         QString::number(FullScan_ATSC), true);
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::INPUT_TYPES::HDHOMERUN:
            if (CardUtil::HDHRdoesDVBC(CardUtil::GetVideoDevice(cardid)))
            {
                addSelection(tr("Full Scan (Tuned)"),
                             QString::number(NITAddScan_DVBC));
                addSelection(tr("Full Scan"),
                             QString::number(FullScan_DVBC));
            }
            else if (CardUtil::HDHRdoesDVB(CardUtil::GetVideoDevice(cardid)))
            {
                addSelection(tr("Full Scan"),
                             QString::number(FullScan_DVBT), true);
                addSelection(tr("Full Scan (Tuned)"),
                             QString::number(NITAddScan_DVBT));
                setHelpText(fullScanHelpTextDVBT2);
            }
            else
            {
                addSelection(tr("Full Scan"),
                             QString::number(FullScan_ATSC), true);
            }
//             addSelection(tr("Import channels.conf"),
//                          QString::number(DVBUtilsImport));
            addSelection(tr("HDHomeRun Channel Import"),
                         QString::number(HDHRImport));
            addSelection(tr("Import existing scan"),
                         QString::number(ExistingScanImport));
            break;
        case CardUtil::INPUT_TYPES::VBOX:
            addSelection(tr("VBox Channel Import"),
                         QString::number(VBoxImport), true);
            return;
        case CardUtil::INPUT_TYPES::FREEBOX:
            addSelection(tr("M3U Import with MPTS"),
                         QString::number(IPTVImportMPTS), true);
            addSelection(tr("M3U Import"),
                         QString::number(IPTVImport), true);
            return;
        case CardUtil::INPUT_TYPES::ASI:
            addSelection(tr("ASI Scan"),
                         QString::number(CurrentTransportScan), true);
            return;
        case CardUtil::INPUT_TYPES::EXTERNAL:
            addSelection(tr("MPTS Scan"),
                         QString::number(CurrentTransportScan), true);
            addSelection(tr("Import from ExternalRecorder"),
                         QString::number(ExternRecImport), true);
            return;
        case CardUtil::INPUT_TYPES::ERROR_PROBE:
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
    m_scanType(_scan_type),
    m_dvbTCountry(new ScanCountry()),
    m_dvbT2Country(new ScanCountry()),
    m_network(new ScanNetwork()),
    m_paneDVBT(new PaneDVBT(QString::number(ScanTypeSetting::NITAddScan_DVBT), m_scanType)),
    m_paneDVBT2(new PaneDVBT2(QString::number(ScanTypeSetting::NITAddScan_DVBT2), m_scanType)),
    m_paneDVBS(new PaneDVBS(QString::number(ScanTypeSetting::NITAddScan_DVBS), m_scanType)),
    m_paneDVBS2(new PaneDVBS2(QString::number(ScanTypeSetting::NITAddScan_DVBS2), m_scanType)),
    m_paneATSC(new PaneATSC(QString::number(ScanTypeSetting::FullScan_ATSC), m_scanType)),
    m_paneDVBC(new PaneDVBC(QString::number(ScanTypeSetting::NITAddScan_DVBC), m_scanType)),
    m_paneAnalog(new PaneAnalog(QString::number(ScanTypeSetting::FullScan_Analog), m_scanType)),
    m_paneSingle(new PaneSingle(QString::number(ScanTypeSetting::TransportScan), m_scanType)),
    m_paneAll(new PaneAll(QString::number(ScanTypeSetting::FullTransportScan), m_scanType)),
    m_paneDVBUtilsImport(new PaneDVBUtilsImport()),
    m_paneExistingScanImport(new PaneExistingScanImport(QString::number(ScanTypeSetting::ExistingScanImport), m_scanType))
{
    setVisible(false);

    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::Error_Open),
              new PaneError(tr("Failed to open the card")));
    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::Error_Probe),
              new PaneError(tr("Failed to probe the card")));
    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBC),
              m_network);
    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBT),
              m_dvbTCountry);
    m_scanType->addTargetedChild(QString::number(ScanTypeSetting::FullScan_DVBT2),
              m_dvbT2Country);
//   m_scanType->addTargetedChild(QString::number(ScanTypeSetting::DVBUtilsImport),
//               paneDVBUtilsImport);
}

void ScanOptionalConfig::SetSourceID(const QString &sourceid)
{
    m_paneAnalog->SetSourceID(sourceid.toUInt());
    m_paneSingle->SetSourceID(sourceid.toUInt());
    m_paneExistingScanImport->SetSourceID(sourceid.toUInt());
}

QString ScanOptionalConfig::GetFrequencyStandard(void) const
{
    int     st =  m_scanType->getValue().toInt();

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
    int     st =  m_scanType->getValue().toInt();

    switch (st)
    {
        case ScanTypeSetting::FullScan_ATSC:
            return m_paneATSC->GetModulation();
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
    int st =  m_scanType->getValue().toInt();

    switch (st)
    {
        case ScanTypeSetting::FullScan_ATSC:
            return m_paneATSC->GetFrequencyTable();
        case ScanTypeSetting::FullScan_DVBC:
            return m_network->getValue();
        case ScanTypeSetting::FullScan_DVBT:
            return m_dvbTCountry->getValue();
        case ScanTypeSetting::FullScan_DVBT2:
            return m_dvbT2Country->getValue();
        case ScanTypeSetting::FullScan_Analog:
            return m_paneAnalog->GetFrequencyTable();
        default:
            return "unknown";
    }
}

bool ScanOptionalConfig::GetFrequencyTableRange(
    QString &start, QString &end) const
{
    start.clear();
    end.clear();

    int st = m_scanType->getValue().toInt();
    if (ScanTypeSetting::FullScan_ATSC == st)
        return m_paneATSC->GetFrequencyTableRange(start, end);

    return false;
}

bool ScanOptionalConfig::DoIgnoreSignalTimeout(void) const
{
    int  st  = m_scanType->getValue().toInt();

    switch (st)
    {
    case ScanTypeSetting::TransportScan:
        return m_paneSingle->ignoreSignalTimeout();
    case ScanTypeSetting::FullTransportScan:
        return m_paneAll->ignoreSignalTimeout();
    case ScanTypeSetting::DVBUtilsImport:
        return m_paneDVBUtilsImport->DoIgnoreSignalTimeout();
    default:
        return false;
    }
}

bool ScanOptionalConfig::DoFollowNIT(void) const
{
    int  st  = m_scanType->getValue().toInt();
    switch (st)
    {
    case ScanTypeSetting::TransportScan:
        return m_paneSingle->GetFollowNIT();
    case ScanTypeSetting::FullTransportScan:
        return m_paneAll->GetFollowNIT();
    default:
        return false;
    }
}

QString ScanOptionalConfig::GetFilename(void) const
{
    return m_paneDVBUtilsImport->GetFilename();
}

uint ScanOptionalConfig::GetMultiplex(void) const
{
    int mplexid = m_paneSingle->GetMultiplex();
    return (mplexid <= 0) ? 0 : mplexid;
}

uint ScanOptionalConfig::GetScanID(void) const
{
    return m_paneExistingScanImport->GetScanID();
}

QMap<QString,QString> ScanOptionalConfig::GetStartChan(void) const
{
    QMap<QString,QString> startChan;

    int st = m_scanType->getValue().toInt();
    if (ScanTypeSetting::NITAddScan_DVBT == st)
    {
        const PaneDVBT *pane = m_paneDVBT;

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
        const PaneDVBT2 *pane = m_paneDVBT2;

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
        startChan["mod_sys"]        = pane->modsys();
    }
    else if (ScanTypeSetting::NITAddScan_DVBS == st)
    {
        const PaneDVBS *pane = m_paneDVBS;

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
        const PaneDVBC *pane = m_paneDVBC;

        startChan["std"]        = "dvb";
        startChan["type"]       = "QAM";
        startChan["frequency"]  = pane->frequency();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["modulation"] = pane->modulation();
        startChan["mod_sys"]    = pane->modsys();
        startChan["inversion"]  = pane->inversion();
        startChan["fec"]        = pane->fec();
    }
    else if (ScanTypeSetting::NITAddScan_DVBS2 == st)
    {
        const PaneDVBS2 *pane = m_paneDVBS2;

        startChan["std"]        = "dvb";
        startChan["type"]       = "DVB_S2";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = pane->modulation();
        startChan["polarity"]   = pane->polarity();
        startChan["mod_sys"]    = pane->modsys();
        startChan["rolloff"]    = pane->rolloff();
    }

    return startChan;
}

void ScanOptionalConfig::SetTuningPaneValues(uint frequency, const DTVMultiplex &mpx)
{
    const int st =  m_scanType->getValue().toInt();

    if (st == ScanTypeSetting::FullScan_DVBT  ||
        st == ScanTypeSetting::NITAddScan_DVBT )
    {
        PaneDVBT *pane = m_paneDVBT;

        pane->setFrequency(frequency);
        if (frequency == mpx.m_frequency)
        {
            pane->setInversion(mpx.m_inversion.toString());
            pane->setBandwidth(mpx.m_bandwidth.toString());
            pane->setCodeRateHP(mpx.m_hpCodeRate.toString());
            pane->setCodeRateLP(mpx.m_lpCodeRate.toString());
            pane->setConstellation(mpx.m_modulation.toString());
            pane->setTransmode(mpx.m_transMode.toString());
            pane->setGuardInterval(mpx.m_guardInterval.toString());
            pane->setHierarchy(mpx.m_hierarchy.toString());
        }
    }
    else if (st == ScanTypeSetting::FullScan_DVBT2  ||
             st == ScanTypeSetting::NITAddScan_DVBT2 )
    {
        PaneDVBT2 *pane = m_paneDVBT2;

        pane->setFrequency(frequency);
        if (frequency == mpx.m_frequency)
        {
            pane->setInversion(mpx.m_inversion.toString());
            pane->setBandwidth(mpx.m_bandwidth.toString());
            pane->setCodeRateHP(mpx.m_hpCodeRate.toString());
            pane->setCodeRateLP(mpx.m_lpCodeRate.toString());
            pane->setConstellation(mpx.m_modulation.toString());
            pane->setTransmode(mpx.m_transMode.toString());
            pane->setGuardInterval(mpx.m_guardInterval.toString());
            pane->setHierarchy(mpx.m_hierarchy.toString());
            pane->setModsys(mpx.m_modSys.toString());
        }
    }
    else if (st == ScanTypeSetting::FullScan_DVBC  ||
             st == ScanTypeSetting::NITAddScan_DVBC )
    {
        PaneDVBC *pane = m_paneDVBC;

        pane->setFrequency(frequency);
        if (frequency == mpx.m_frequency)
        {
            pane->setInversion(mpx.m_inversion.toString());
            pane->setSymbolrate(QString("%1").arg(mpx.m_symbolRate));
            pane->setFec(mpx.m_fec.toString());
            pane->setModulation(mpx.m_modulation.toString());
            pane->setModsys(mpx.m_modSys.toString());
        }
    }
    else if (st == ScanTypeSetting::NITAddScan_DVBS)
    {
        PaneDVBS *pane = m_paneDVBS;

        pane->setFrequency(frequency);
        if (frequency == mpx.m_frequency)
        {
            pane->setSymbolrate(QString("%1").arg(mpx.m_symbolRate));
            pane->setInversion(mpx.m_inversion.toString());
            pane->setFec(mpx.m_fec.toString());
            pane->setPolarity(mpx.m_polarity.toString());
        }
    }
    else if (st == ScanTypeSetting::NITAddScan_DVBS2)
    {
        PaneDVBS2 *pane = m_paneDVBS2;

        pane->setFrequency(frequency);
        if (frequency == mpx.m_frequency)
        {
            pane->setSymbolrate(QString("%1").arg(mpx.m_symbolRate));
            pane->setInversion(mpx.m_inversion.toString());
            pane->setFec(mpx.m_fec.toString());
            pane->setPolarity(mpx.m_polarity.toString());
            pane->setModulation(mpx.m_modulation.toString());
            pane->setModSys(mpx.m_modSys.toString());
            pane->setRolloff(mpx.m_rolloff.toString());
        }
    }
}

void ScanOptionalConfig::SetTuningPaneValuesATSC(const QString &freqtable)
{
    const int st =  m_scanType->getValue().toInt();

    if (st == ScanTypeSetting::FullScan_ATSC)
    {
        PaneATSC *pane = m_paneATSC;

        pane->SetFrequencyTable(freqtable);
    }
}
