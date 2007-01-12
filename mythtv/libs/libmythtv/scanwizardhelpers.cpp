/* -*- Mode: c++ -*-
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

// Qt headers
#include <qlocale.h>

// MythTV headers
#include "mythcontext.h"
#include "frequencies.h"
#include "videosource.h"
#include "cardutil.h"
#include "sourceutil.h"
#include "scanwizardhelpers.h"
#include "scanwizardscanner.h"
#include "scanwizard.h"

static QString card_types(void)
{
    QString cardTypes = "";

#ifdef USING_DVB
    cardTypes += "'DVB'";
#endif // USING_DVB

#ifdef USING_V4L
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'V4L','HDTV'";
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

ScanProgressPopup::ScanProgressPopup(
    ScanWizardScanner *parent, bool signalmonitors) :
    ConfigurationPopupDialog()
{
    setLabel(tr("Scan Progress"));

    if (signalmonitors)
    {
        VerticalConfigurationGroup *box = new VerticalConfigurationGroup();
        box->addChild(sta = new TransLabelSetting());
        box->addChild(sl = new TransLabelSetting());
        sta->setLabel(tr("Status"));
        sta->setValue(tr("Tuning"));
        sl->setValue("                                  "
                     "                                  ");
        box->setUseFrame(false);
        addChild(box);
    }

    addChild(progressBar = new ScanSignalMeter(PROGRESS_MAX));
    progressBar->setValue(0);
    progressBar->setLabel(tr("Scan"));

    if (signalmonitors)
    {
        addChild(ss = new ScanSignalMeter(65535));
        addChild(sn = new ScanSignalMeter(65535));
        ss->setLabel(tr("Signal Strength"));
        sn->setLabel(tr("Signal/Noise"));
    }

    TransButtonSetting *cancel = new TransButtonSetting();
    cancel->setLabel(tr("Cancel"));
    addChild(cancel);

    connect(cancel, SIGNAL(pressed(void)),
            parent, SLOT(  CancelScan(void)));

    //Seem to need to do this as the constructor doesn't seem enough
    setUseLabel(false);
    setUseFrame(false);
}

ScanProgressPopup::~ScanProgressPopup()
{
    VERBOSE(VB_IMPORTANT, "~ScanProgressPopup()");
}

void ScanProgressPopup::signalToNoise(int value)
{
    sn->setValue(value);
}

void ScanProgressPopup::signalStrength(int value)
{
    ss->setValue(value);
}

void ScanProgressPopup::dvbLock(int value)
{
    sl->setValue((value) ? tr("Locked") : tr("No Lock"));
}

void ScanProgressPopup::status(const QString& value)
{
    sta->setValue(value);
}

void ScanProgressPopup::exec(ScanWizardScanner *parent)
{
    dialog = (ConfigPopupDialogWidget*)
        dialogWidget(gContext->GetMainWindow(), "ScanProgressPopup");
    connect(dialog, SIGNAL(popupDone(void)),
            parent, SLOT(  CancelScan(void)));
    dialog->ShowPopup(this);
}

void MultiplexSetting::load(void)
{
    clearSelections();

    if (!sourceid)
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT mplexid,   networkid,  transportid, "
        "       frequency, symbolrate, modulation "
        "FROM dtv_multiplex "
        "WHERE sourceid = :SOURCEID "
        "ORDER by frequency, networkid, transportid");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive() || query.size() <= 0)
        return;

    while (query.next())
    {
        QString DisplayText;
        if (query.value(5).toString() == "8vsb")
        {
            QString ChannelNumber =
                QString("Freq %1").arg(query.value(3).toInt());
            struct CHANLIST* curList = chanlists[0].list;
            int totalChannels = chanlists[0].count;
            int findFrequency = (query.value(3).toInt() / 1000) - 1750;
            for (int x = 0 ; x < totalChannels ; x++)
            {
                if ((curList[x].freq <= findFrequency + 200) &&
                    (curList[x].freq >= findFrequency - 200))
                {
                    ChannelNumber = QString("%1").arg(curList[x].name);
                }
            }
            DisplayText = QObject::tr("ATSC Channel %1").arg(ChannelNumber);
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

void MultiplexSetting::SetSourceID(uint _sourceid)
{
    sourceid = _sourceid;
    load();
}

InputSelector::InputSelector(
    uint _default_cardid, const QString &_default_inputname) :
    ComboBoxSetting(this), sourceid(0), default_cardid(_default_cardid),
    default_inputname(QDeepCopy<QString>(_default_inputname))
{
    setLabel(tr("Input"));
}

void InputSelector::load(void)
{
    clearSelections();

    if (!sourceid)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT capturecard.cardid, cardinput.childcardid, "
        "       cardtype, videodevice, inputname, capturecard.parentid "
        "FROM capturecard, cardinput, videosource "
        "WHERE cardinput.sourceid = videosource.sourceid AND "
        "      hostname           = :HOSTNAME            AND "
        "      cardinput.sourceid = :SOURCEID            AND "
        "      ( ( cardinput.childcardid != '0' AND "
        "          cardinput.childcardid  = capturecard.cardid ) OR "
        "        ( cardinput.childcardid  = '0' AND "
        "          cardinput.cardid       = capturecard.cardid ) "
        "      )");

    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("InputSelector::load()", query);
        return;
    }

    uint which = 0, cnt = 0;
    for (; query.next(); cnt++)
    {
        uint parent_cardid = query.value(0).toUInt();
        uint child_cardid  = query.value(1).toUInt();
        if (child_cardid)
            parent_cardid = query.value(5).toUInt();

        QString inputname  = query.value(4).toString();

        QString desc = CardUtil::GetDeviceLabel(
            parent_cardid,
            query.value(2).toString(), query.value(3).toString());

        if (child_cardid)
        {
            MSqlQuery query2(MSqlQuery::InitCon());
            query2.prepare(
                "SELECT cardtype, videodevice "
                "FROM capturecard "
                "WHERE cardid = :CARDID");

            if (query2.next())
            {
                desc += " " + CardUtil::GetDeviceLabel(
                    child_cardid,
                    query2.value(0).toString(), query2.value(1).toString());
            }
        }

        desc += QString(" (%1)").arg(inputname);

        QString key = QString("%1:%2:%3")
            .arg(parent_cardid).arg(child_cardid).arg(inputname);

        addSelection(desc, key);

        which = (((default_cardid == parent_cardid) ||
                  (default_cardid == child_cardid)) &&
                 (default_inputname == inputname)) ? cnt : which;
    }

    if (cnt)
        setValue(which);
}

void InputSelector::SetSourceID(const QString &_sourceid)
{
    if (sourceid != _sourceid.toUInt())
    {
        sourceid = _sourceid.toUInt();
        load();
    }
}

uint InputSelector::GetParentCardID(void) const
{
    uint    parent_cardid = 0;
    uint    child_cardid  = 0;
    QString inputname     = QString::null;

    Parse(getValue(), parent_cardid, child_cardid, inputname);

    return parent_cardid;
}

uint InputSelector::GetChildCardID(void) const
{
    uint    parent_cardid = 0;
    uint    child_cardid  = 0;
    QString inputname     = QString::null;

    Parse(getValue(), parent_cardid, child_cardid, inputname);

    return child_cardid;
}

QString InputSelector::GetInputName(void) const
{
    uint    parent_cardid = 0;
    uint    child_cardid  = 0;
    QString inputname = QString::null;

    Parse(getValue(), parent_cardid, child_cardid, inputname);

    return inputname;
}

bool InputSelector::Parse(const QString &cardids_inputname,
                          uint &parent_cardid,
                          uint &child_cardid,
                          QString &inputname)
{
    parent_cardid = 0;
    child_cardid  = 0;
    inputname     = QString::null;

    int sep0 = cardids_inputname.find(':');
    if (sep0 < 1)
        return false;

    QString child_cardid_inputname = cardids_inputname.mid(sep0 + 1);
    int sep1 = child_cardid_inputname.find(':');
    if (sep1 < 1)
        return false;

    parent_cardid = cardids_inputname.left(sep0).toUInt();
    child_cardid  = child_cardid_inputname.left(sep1).toUInt();
    inputname     = child_cardid_inputname.mid(sep1 + 1);

    return true;
}

void ScanTypeSetting::SetInput(const QString &cardids_inputname)
{
    uint pcardid, ccardid;
    QString inputname;
    if (!InputSelector::Parse(cardids_inputname, pcardid, ccardid, inputname))
        return;

    const uint new_cardid = ccardid ? ccardid : pcardid;

    // Only refresh if we really have to. If we do it too often
    // Then we end up fighting the scan routine when we want to
    // check the type of dvb card :/
    if (new_cardid == hw_cardid)
        return;

    hw_cardid       = new_cardid;
    QString subtype = CardUtil::ProbeSubTypeName(hw_cardid, 0);
    int nCardType   = CardUtil::toCardType(subtype);
    clearSelections();

    switch (nCardType)
    {
    case CardUtil::V4L:
    case CardUtil::MPEG:
        addSelection(tr("Full Scan"),
                     QString::number(FullScan_Analog), true);
        return;
    case CardUtil::OFDM:
        addSelection(tr("Full Scan"),
                     QString::number(FullScan_OFDM), true);
        addSelection(tr("Full Scan (Tuned)"),
                     QString::number(NITAddScan_OFDM));
        addSelection(tr("Import channels.conf"),
                     QString::number(DVBUtilsImport));
        break;
    case CardUtil::QPSK:
        addSelection(tr("Full Scan (Tuned)"),
                     QString::number(NITAddScan_QPSK));
        addSelection(tr("Import channels.conf"),
                     QString::number(DVBUtilsImport));
        break;
    case CardUtil::QAM:
        addSelection(tr("Full Scan (Tuned)"),
                     QString::number(NITAddScan_QAM));
        addSelection(tr("Import channels.conf"),
                     QString::number(DVBUtilsImport));
        break;
    case CardUtil::ATSC:
    case CardUtil::HDTV:
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

ScanCountry::ScanCountry() : ComboBoxSetting(this)
{
    Country country = AU;
#if (QT_VERSION >= 0x030300)
    QLocale locale = QLocale::system();
    QLocale::Country qtcountry = locale.country();
    if (qtcountry == QLocale::Australia)
        country = AU;
    else if (qtcountry == QLocale::Germany)
        country = DE;
    else if (qtcountry == QLocale::Finland)
        country = FI;
    else if (qtcountry == QLocale::Sweden)
        country = SE;
    else if (qtcountry == QLocale::UnitedKingdom)
        country = UK;
    else if (qtcountry == QLocale::Spain)
        country = ES;
#endif

    setLabel(tr("Country"));
    addSelection(QObject::tr("Australia"),      "au", country == AU);
    addSelection(QObject::tr("Finland"),        "fi", country == FI);
    addSelection(QObject::tr("Sweden"),         "se", country == SE);
    addSelection(QObject::tr("United Kingdom"), "uk", country == UK);
    addSelection(QObject::tr("Germany"),        "de", country == DE);
    addSelection(QObject::tr("Spain"),          "es", country == ES);
}

AnalogPane::AnalogPane() :
    VerticalConfigurationGroup(false, false, true, false),
    freq_table(new TransFreqTableSelector(0)),
    old_channel_treatment(new ScanOldChannelTreatment(false))
{
    addChild(freq_table);
    addChild(old_channel_treatment);
}

void AnalogPane::SetSourceID(uint sourceid)
{
    freq_table->SetSourceID(sourceid);
}

QString AnalogPane::GetFrequencyTable(void) const
{
    return freq_table->getValue();
}

ScanOptionalConfig::ScanOptionalConfig(ScanTypeSetting *_scan_type) :
    TriggeredConfigurationGroup(false, false, true, true,
                                false, false, true, true),
    scanType(_scan_type),
    country(new ScanCountry()),
    ignoreSignalTimeoutAll(new IgnoreSignalTimeout()),
    paneOFDM(new OFDMPane()),     paneQPSK(new QPSKPane()),
    paneDVBS2(new DVBS2Pane()),   paneATSC(new ATSCPane()),
    paneQAM(new QAMPane()),       paneAnalog(new AnalogPane()),
    paneSingle(new STPane()),
    paneDVBUtilsImport(new DVBUtilsImportPane())
{
    setTrigger(scanType);

    // only save settings for the selected pane
    setSaveAll(false);

    // There need to be two IgnoreSignalTimeout instances
    // because otherwise Qt will free the one instance twice..

    // There need to be two IgnoreSignalTimeout instances
    // because otherwise Qt will free the one instance twice..
    VerticalConfigurationGroup *scanAllTransports =
        new VerticalConfigurationGroup(false,false,true,true);
    scanAllTransports->addChild(ignoreSignalTimeoutAll);

    addTarget(QString::number(ScanTypeSetting::Error_Open),
             new ErrorPane(QObject::tr("Failed to open the card")));
    addTarget(QString::number(ScanTypeSetting::Error_Probe),
             new ErrorPane(QObject::tr("Failed to probe the card")));
    addTarget(QString::number(ScanTypeSetting::NITAddScan_QAM),
              paneQAM);
    addTarget(QString::number(ScanTypeSetting::NITAddScan_QPSK),
              paneQPSK);
    addTarget(QString::number(ScanTypeSetting::NITAddScan_OFDM),
              paneOFDM);
    addTarget(QString::number(ScanTypeSetting::FullScan_ATSC),
              paneATSC);
    addTarget(QString::number(ScanTypeSetting::FullScan_OFDM),
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

void ScanOptionalConfig::SetDefaultATSCFormat(const QString &atscFormat)
{
    paneATSC->SetDefaultATSCFormat(atscFormat);
    paneSingle->SetDefaultATSCFormat(atscFormat);
    paneDVBUtilsImport->SetDefaultATSCFormat(atscFormat);
}

QString ScanOptionalConfig::GetATSCFormat(const QString &dfl) const
{
    int     st =  scanType->getValue().toInt();

    bool    ts0 = (ScanTypeSetting::FullScan_ATSC  == st);
    QString vl0 = paneATSC->GetATSCFormat();

    bool    ts1 = (ScanTypeSetting::TransportScan  == st);
    QString vl1 = paneSingle->GetATSCFormat();

    bool    ts2 = (ScanTypeSetting::DVBUtilsImport == st);
    QString vl2 = paneDVBUtilsImport->GetATSCFormat();

    return (ts0) ? vl0 : ((ts1) ? vl1 : (ts2) ? vl2 : dfl);
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
    QString vl0 = paneATSC->atscModulation();

    bool    ts1 = (ScanTypeSetting::FullScan_OFDM == st);
    QString vl1 = "ofdm";

    bool    ts2 = (ScanTypeSetting::FullScan_Analog == st);
    QString vl2 = "analog";

    return (ts0) ? vl0 : ((ts1) ? vl1 : (ts2) ? vl2 : "unknown");
}

QString ScanOptionalConfig::GetFrequencyTable(void) const
{
    int     st =  scanType->getValue().toInt();

    bool    ts0 = (ScanTypeSetting::FullScan_ATSC == st);
    QString vl0 = paneATSC->atscFreqTable();

    bool    ts1 = (ScanTypeSetting::FullScan_OFDM == st);
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

bool ScanOptionalConfig::DoDeleteChannels(void) const
{
    int  st  = scanType->getValue().toInt();

    bool ts0 = (ScanTypeSetting::FullScan_ATSC  == st);
    bool vl0 = paneATSC->DoDeleteChannels();

    bool ts1 = (ScanTypeSetting::TransportScan  == st);
    bool vl1 = paneSingle->DoDeleteChannels();

    bool ts2 = (ScanTypeSetting::DVBUtilsImport == st);
    bool vl2 = paneDVBUtilsImport->DoDeleteChannels();

    bool ts3 = (ScanTypeSetting::FullScan_Analog == st);
    bool vl3 = paneAnalog->DoDeleteChannels();

    return (ts0) ? vl0 : (((ts1) ? vl1 : (ts2) ? vl2 : (ts3) ? vl3 : false));
}

bool ScanOptionalConfig::DoRenameChannels(void) const
{
    int  st  = scanType->getValue().toInt();

    bool ts0 = (ScanTypeSetting::FullScan_ATSC  == st);
    bool vl0 = paneATSC->DoRenameChannels();

    bool ts1 = (ScanTypeSetting::TransportScan  == st);
    bool vl1 = paneSingle->DoRenameChannels();

    bool ts2 = (ScanTypeSetting::DVBUtilsImport == st);
    bool vl2 = paneDVBUtilsImport->DoRenameChannels();

    bool ts3 = (ScanTypeSetting::FullScan_Analog == st);
    bool vl3 = paneAnalog->DoRenameChannels();

    return (ts0) ? vl0 : (((ts1) ? vl1 : (ts2) ? vl2 : (ts3) ? vl3 : false));
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
    if (ScanTypeSetting::NITAddScan_OFDM == st)
    {
        const OFDMPane *pane = paneOFDM;

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
    else if (ScanTypeSetting::NITAddScan_QPSK == st)
    {
        const QPSKPane *pane = paneQPSK;

        startChan["std"]        = "dvb";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = "qpsk";
        startChan["polarity"]   = pane->polarity();
    }
    else if (ScanTypeSetting::NITAddScan_QAM == st)
    {
        const QAMPane *pane = paneQAM;

        startChan["std"]        = "dvb";
        startChan["frequency"]  = pane->frequency();
        startChan["inversion"]  = pane->inversion();
        startChan["symbolrate"] = pane->symbolrate();
        startChan["fec"]        = pane->fec();
        startChan["modulation"] = pane->modulation();
    }

    return startChan;
}

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

QString ScanWizardConfig::GetATSCFormat(void) const
{
    QString dfl = SourceUtil::GetChannelFormat(GetSourceID());
    return scanConfig->GetATSCFormat(dfl);
}

LogList::LogList() : ListBoxSetting(this), n(0)
{
    setSelectionMode(MythListBox::NoSelection);
}

void LogList::updateText(const QString& status)
{
    addSelection(status,QString::number(n));
    setCurrentItem(n);
    n++;
}
