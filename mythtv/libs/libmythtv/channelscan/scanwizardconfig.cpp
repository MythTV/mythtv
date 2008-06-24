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
#include "paneatsc.h"
#include "paneanalog.h"
#include "panesingle.h"
#include "panedvbutilsimport.h"

static QString card_types(void)
{
    QString cardTypes = "";

#ifdef USING_DVB
    cardTypes += "'DVB'";
#endif // USING_DVB

#ifdef USING_V4L
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'V4L'";
# ifdef USING_IVTV
    cardTypes += ",'MPEG'";
# endif // USING_IVTV
#endif // USING_V4L

#ifdef USING_IPTV
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'FREEBOX'";
#endif // USING_IPTV

#ifdef USING_HDHOMERUN
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'HDHOMERUN'";
#endif // USING_HDHOMERUN

    if (cardTypes.isEmpty())
        cardTypes = "'DUMMY'";

    return QString("(%1)").arg(cardTypes);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ScanWizardConfig::ScanWizardConfig(
    ScanWizard *_parent,
    uint    default_sourceid,  uint default_cardid,
    QString default_inputname) :
    VerticalConfigurationGroup(false, true, false, false),
    videoSource(new VideoSourceSelector(
                    default_sourceid, card_types(), false)),
    input(new InputSelector(default_cardid, default_inputname)),
    scanType(new ScanTypeSetting()),
    scanConfig(new ScanOptionalConfig(scanType))
{
    setLabel(tr("Scan Configuration"));

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
            _parent,     SLOT(  SetInput(    const QString&)));
}

uint ScanWizardConfig::GetSourceID(void) const
{
    return videoSource->getValue().toUInt();
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
    int nCardType   = CardUtil::toCardType(subtype);
    clearSelections();

    switch (nCardType)
    {
    case CardUtil::V4L:
    case CardUtil::MPEG:
        addSelection(tr("Full Scan"),
                     QString::number(FullScan_Analog), true);
        return;
    case CardUtil::DVBT:
        addSelection(tr("Full Scan"),
                     QString::number(FullScan_DVBT), true);
        addSelection(tr("Full Scan (Tuned)"),
                     QString::number(NITAddScan_DVBT));
        addSelection(tr("Import channels.conf"),
                     QString::number(DVBUtilsImport));
        break;
    case CardUtil::DVBS:
        addSelection(tr("Full Scan (Tuned)"),
                     QString::number(NITAddScan_DVBS));
        addSelection(tr("Import channels.conf"),
                     QString::number(DVBUtilsImport));
        break;
    case CardUtil::QAM:
        addSelection(tr("Full Scan (Tuned)"),
                     QString::number(NITAddScan_DVBC));
        addSelection(tr("Import channels.conf"),
                     QString::number(DVBUtilsImport));
        break;
    case CardUtil::ATSC:
    case CardUtil::HDHOMERUN:
        addSelection(tr("Full Scan"),
                     QString::number(FullScan_ATSC), true);
        addSelection(tr("Import channels.conf"),
                     QString::number(DVBUtilsImport));
        break;
    case CardUtil::FREEBOX:
        addSelection(tr("M3U Import"),
                     QString::number(IPTVImport), true);
        return;
    case CardUtil::ERROR_PROBE:
        addSelection(QObject::tr("Failed to probe the card"),
                     QString::number(Error_Probe), true);
        return;
    default:
        addSelection(QObject::tr("Failed to open the card"),
                     QString::number(Error_Open), true);
        return;
    }

    addSelection(tr("Full Scan of Existing Transports"),
                 QString::number(FullTransportScan));
    addSelection(tr("Existing Transport Scan"),
                 QString::number(TransportScan));
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ScanOptionalConfig::ScanOptionalConfig(ScanTypeSetting *_scan_type) :
    TriggeredConfigurationGroup(false, false, true, true,
                                false, false, true, true),
    scanType(_scan_type),
    country(new ScanCountry()),
    ignoreSignalTimeoutAll(new IgnoreSignalTimeout()),
    paneDVBT(new PaneDVBT()),     paneDVBS(new PaneDVBS()),
    paneDVBS2(new PaneDVBS2()),   paneATSC(new PaneATSC()),
    paneDVBC(new PaneDVBC()),     paneAnalog(new PaneAnalog()),
    paneSingle(new PaneSingle()),
    paneDVBUtilsImport(new PaneDVBUtilsImport())
{
    setTrigger(scanType);

    // only save settings for the selected pane
    setSaveAll(false);

    VerticalConfigurationGroup *scanAllTransports =
        new VerticalConfigurationGroup(false,false,true,true);
    scanAllTransports->addChild(ignoreSignalTimeoutAll);

    addTarget(QString::number(ScanTypeSetting::Error_Open),
             new PaneError(QObject::tr("Failed to open the card")));
    addTarget(QString::number(ScanTypeSetting::Error_Probe),
             new PaneError(QObject::tr("Failed to probe the card")));
    addTarget(QString::number(ScanTypeSetting::NITAddScan_DVBC),
              paneDVBC);
    addTarget(QString::number(ScanTypeSetting::NITAddScan_DVBS),
              paneDVBS);
    addTarget(QString::number(ScanTypeSetting::NITAddScan_DVBT),
              paneDVBT);
    addTarget(QString::number(ScanTypeSetting::FullScan_ATSC),
              paneATSC);
    addTarget(QString::number(ScanTypeSetting::FullScan_DVBT),
              country);
    addTarget(QString::number(ScanTypeSetting::FullScan_Analog),
              paneAnalog);
    addTarget(QString::number(ScanTypeSetting::TransportScan),
              paneSingle);
    addTarget(QString::number(ScanTypeSetting::FullTransportScan),
              scanAllTransports);
    addTarget(QString::number(ScanTypeSetting::IPTVImport),
              new BlankSetting());
    addTarget(QString::number(ScanTypeSetting::DVBUtilsImport),
              paneDVBUtilsImport);
}

void ScanOptionalConfig::triggerChanged(const QString& value)
{
    TriggeredConfigurationGroup::triggerChanged(value);
}

void ScanOptionalConfig::SetSourceID(const QString &sourceid)
{
    paneAnalog->SetSourceID(sourceid.toUInt());
    paneSingle->SetSourceID(sourceid.toUInt());
}

QString ScanOptionalConfig::GetFrequencyStandard(void) const
{
    int     st =  scanType->getValue().toInt();

    bool    ts0 = (ScanTypeSetting::FullScan_ATSC   == st);
    bool    ts1 = (ScanTypeSetting::FullScan_Analog == st);

    return (ts0) ? "atsc" : ((ts1) ? "analog" : "dvbt");
}

QString ScanOptionalConfig::GetModulation(void) const
{
    int     st =  scanType->getValue().toInt();

    bool    ts0 = (ScanTypeSetting::FullScan_ATSC == st);
    QString vl0 = paneATSC->GetModulation();

    bool    ts1 = (ScanTypeSetting::FullScan_DVBT == st);
    QString vl1 = "ofdm";

    bool    ts2 = (ScanTypeSetting::FullScan_Analog == st);
    QString vl2 = "analog";

    return (ts0) ? vl0 : ((ts1) ? vl1 : (ts2) ? vl2 : "unknown");
}

QString ScanOptionalConfig::GetFrequencyTable(void) const
{
    int     st =  scanType->getValue().toInt();

    bool    ts0 = (ScanTypeSetting::FullScan_ATSC == st);
    QString vl0 = paneATSC->GetFrequencyTable();

    bool    ts1 = (ScanTypeSetting::FullScan_DVBT == st);
    QString vl1 = country->getValue();

    bool    ts2 = (ScanTypeSetting::FullScan_Analog == st);
    QString vl2 = paneAnalog->GetFrequencyTable();

    return (ts0) ? vl0 : ((ts1) ? vl1 : (ts2) ? vl2 : "unknown");
}

bool ScanOptionalConfig::DoIgnoreSignalTimeout(void) const
{
    int  st  = scanType->getValue().toInt();

    bool ts0 = (ScanTypeSetting::TransportScan     == st);
    bool vl0 = paneSingle->ignoreSignalTimeout();

    bool ts1 = (ScanTypeSetting::FullTransportScan == st);
    bool vl1 = (ignoreSignalTimeoutAll->getValue().toInt());

    bool ts2 = (ScanTypeSetting::DVBUtilsImport    == st);
    bool vl2 = paneDVBUtilsImport->DoIgnoreSignalTimeout();

    return (ts0) ? vl0 : ((ts1) ? vl1 : (ts2) ? vl2 : false);
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

QMap<QString,QString> ScanOptionalConfig::GetStartChan(void) const
{
    QMap<QString,QString> startChan;

    int st = scanType->getValue().toInt();
    if (ScanTypeSetting::NITAddScan_DVBT == st)
    {
        const PaneDVBT *pane = paneDVBT;

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
    }
    else if (ScanTypeSetting::NITAddScan_DVBS == st)
    {
        const PaneDVBS *pane = paneDVBS;

        startChan["std"]        = "dvb";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = "qpsk";
        startChan["polarity"]   = pane->polarity();
    }
    else if (ScanTypeSetting::NITAddScan_DVBC == st)
    {
        const PaneDVBC *pane = paneDVBC;

        startChan["std"]        = "dvb";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = pane->modulation();
    }

    return startChan;
}

