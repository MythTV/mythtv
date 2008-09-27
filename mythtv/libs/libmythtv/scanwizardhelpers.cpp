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
#include <QApplication>
#include <QLocale>

// MythTV headers
#include "mythcontext.h"
#include "libmythdb/mythdb.h"
#include "libmythdb/mythverbose.h"
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
    cardTypes += "'V4L'";
# ifdef USING_IVTV
    cardTypes += ",'MPEG'";
# endif // USING_IVTV
# ifdef USING_HDPVR
    cardTypes += ",'HDPVR'";
# endif // USING_HDPVR
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

ScanProgressPopup::ScanProgressPopup(bool lock, bool strength, bool snr) :
    ConfigurationPopupDialog(),
    done(false), ss(NULL), sn(NULL), progressBar(NULL), sl(NULL), sta(NULL)
{
    setLabel(tr("Scan Progress"));

    addChild(sta = new TransLabelSetting());
    sta->setLabel(tr("Status"));
    sta->setValue(tr("Tuning"));

    if (lock)
    {
        addChild(sl = new TransLabelSetting());
        sl->setValue("                                  "
                     "                                  ");
    }

    if (strength)
    {
        addChild(ss = new ScanSignalMeter(65535));
        ss->setLabel(tr("Signal Strength"));
    }

    if (snr)
    {
        addChild(sn = new ScanSignalMeter(65535));
        sn->setLabel(tr("Signal/Noise"));
    }

    addChild(progressBar = new ScanSignalMeter(65535));
    progressBar->setValue(0);
    progressBar->setLabel(tr("Scan"));


    TransButtonSetting *cancel = new TransButtonSetting();
    cancel->setLabel(tr("Cancel"));
    addChild(cancel);

    connect(cancel, SIGNAL(pressed(void)),
            this,   SLOT(  reject(void)));

    //Seem to need to do this as the constructor doesn't seem enough
    setUseLabel(false);
    setUseFrame(false);
}

ScanProgressPopup::~ScanProgressPopup()
{
    VERBOSE(VB_SIPARSER, "~ScanProgressPopup()");
}

void ScanProgressPopup::SetStatusSignalToNoise(int value)
{
    if (sn)
        sn->setValue(value);
}

void ScanProgressPopup::SetStatusSignalStrength(int value)
{
    if (ss)
        ss->setValue(value);
}

void ScanProgressPopup::SetStatusLock(int value)
{
    if (sl)
        sl->setValue((value) ? tr("Locked") : tr("No Lock"));
}

void ScanProgressPopup::SetScanProgress(double value)
{
    if (progressBar)
        progressBar->setValue((uint)(value * 65535));
}

void ScanProgressPopup::SetStatusText(const QString &value)
{
    if (sta)
        sta->setValue(value);
}

void ScanProgressPopup::SetStatusTitleText(const QString &value)
{
    QString msg = tr("Scan Progress") + QString(" %1").arg(value);
    setLabel(msg);
}

void ScanProgressPopup::deleteLater(void)
{
    disconnect();
    if (dialog)
    {
        VERBOSE(VB_IMPORTANT, "Programmer Error: "
                "ScanProgressPopup::DeleteDialog() never called.");
    }
    ConfigurationPopupDialog::deleteLater();
}

void ScanProgressPopup::CreateDialog(void)
{
    if (!dialog)
    {
        dialog = (ConfigPopupDialogWidget*)
            dialogWidget(gContext->GetMainWindow(),
                         "ConfigurationPopupDialog");
        dialog->ShowPopup(this, SLOT(Done(void)));
    }
}

void ScanProgressPopup::DeleteDialog(void)
{
    if (dialog)
    {
        dialog->deleteLater();
        dialog = NULL;
    }
}

DialogCode ScanProgressPopup::exec(void)
{
    if (!dialog)
    {
        dialog = (ConfigPopupDialogWidget*)
            dialogWidget(gContext->GetMainWindow(),
                         "ConfigurationPopupDialog");
    }

    dialog->setResult(kDialogCodeRejected);

    done = false;

    // Qt4 requires a QMutex as a parameter...
    // not sure if this is the best solution.  Mutex Must be locked before wait.
    QMutex mutex;
    QMutexLocker locker(&mutex);

    while (!done)
        wait.wait(&mutex, 100);

    return dialog->result();
}

void ScanProgressPopup::Done(void)
{
    done = true;
    wait.wakeAll();
}

void post_event(QObject *dest, ScannerEvent::TYPE type, int val)
{
    ScannerEvent *e = new ScannerEvent(type);
    e->intValue(val);
    QApplication::postEvent(dest, e);
}

void post_event(QObject *dest, ScannerEvent::TYPE type, const QString &val)
{
    ScannerEvent *e = new ScannerEvent(type);
    QString tmp = val; tmp.detach();
    e->strValue(tmp);
    QApplication::postEvent(dest, e);
}

void post_event(QObject *dest, ScannerEvent::TYPE type, int val,
                ScanProgressPopup *spp)
{
    ScannerEvent *e = new ScannerEvent(type);
    e->intValue(val);
    e->ScanProgressPopupValue(spp);
    QApplication::postEvent(dest, e);
}

void MultiplexSetting::Load(void)
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
    Load();
}

InputSelector::InputSelector(
    uint _default_cardid, const QString &_default_inputname) :
    ComboBoxSetting(this), sourceid(0), default_cardid(_default_cardid),
    default_inputname(_default_inputname)
{
    default_inputname.detach();
    setLabel(tr("Input"));
}

void InputSelector::Load(void)
{
    clearSelections();

    if (!sourceid)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT capturecard.cardid, cardtype, videodevice, inputname "
        "FROM capturecard, cardinput, videosource "
        "WHERE cardinput.sourceid = videosource.sourceid AND "
        "      hostname           = :HOSTNAME            AND "
        "      cardinput.sourceid = :SOURCEID            AND "
        "      cardinput.cardid   = capturecard.cardid");

    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("InputSelector::Load()", query);
        return;
    }

    uint which = 0, cnt = 0;
    for (; query.next(); cnt++)
    {
        uint    cardid     = query.value(0).toUInt();
        QString inputname  = query.value(3).toString();

        QString desc = CardUtil::GetDeviceLabel(
            cardid, query.value(1).toString(), query.value(2).toString());

        desc += QString(" (%1)").arg(inputname);

        QString key = QString("%1:%2").arg(cardid).arg(inputname);

        addSelection(desc, key);

        which = (default_cardid == cardid) ? cnt : which;
    }

    if (cnt)
        setValue(which);
}

void InputSelector::SetSourceID(const QString &_sourceid)
{
    if (sourceid != _sourceid.toUInt())
    {
        sourceid = _sourceid.toUInt();
        Load();
    }
}

uint InputSelector::GetCardID(void) const
{
    uint    cardid    = 0;
    QString inputname = QString::null;

    Parse(getValue(), cardid, inputname);

    return cardid;
}

QString InputSelector::GetInputName(void) const
{
    uint    cardid    = 0;
    QString inputname = QString::null;

    Parse(getValue(), cardid, inputname);

    return inputname;
}

bool InputSelector::Parse(const QString &cardid_inputname,
                          uint          &cardid,
                          QString       &inputname)
{
    cardid    = 0;
    inputname = QString::null;

    int sep0 = cardid_inputname.find(':');
    if (sep0 < 1)
        return false;

    cardid    = cardid_inputname.left(sep0).toUInt();
    inputname = cardid_inputname.mid(sep0 + 1);

    return true;
}

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
    else if (qtcountry == QLocale::NewZealand)
        country = NZ;
    else if (qtcountry == QLocale::France)
        country = FR;

    setLabel(tr("Country"));
    addSelection(QObject::tr("Australia"),      "au", country == AU);
    addSelection(QObject::tr("Finland"),        "fi", country == FI);
    addSelection(QObject::tr("Sweden"),         "se", country == SE);
    addSelection(QObject::tr("United Kingdom"), "uk", country == UK);
    addSelection(QObject::tr("Germany"),        "de", country == DE);
    addSelection(QObject::tr("Spain"),          "es", country == ES);
    addSelection(QObject::tr("New Zealand"),    "nz", country == NZ);
    addSelection(QObject::tr("France"),         "fr", country == FR);
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
