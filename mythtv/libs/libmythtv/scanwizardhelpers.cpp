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

#include "scanwizardhelpers.h"
#include "mythcontext.h"
#include "scanwizard.h"
#include "videosource.h"
#include "mythdbcon.h"
#include "channel.h"
#include "qlocale.h"

#if defined(USING_DVB) && defined(USING_V4L)
#define CARDTYPES "('DVB','MPEG','V4L','HDTV')"
#elif defined(USING_DVB)
#define CARDTYPES "('DVB')"
#elif defined(USING_V4L)
#define CARDTYPES "('MPEG','V4L','HDTV')"
#else
#define CARDTYPES "('DUMMY')"
#endif

ScanProgressPopup::ScanProgressPopup(ScanWizardScanner *parent,
                                     bool signalmonitors)
    : VerticalConfigurationGroup(false,false)
{
    setLabel(tr("Scan Progress"));

    if (signalmonitors)
    {
        HorizontalConfigurationGroup *box = new HorizontalConfigurationGroup();
        box->addChild(sta = new TransLabelSetting());
        box->addChild(sl = new TransLabelSetting());
        sta->setLabel(QObject::tr("Status"));
        sta->setValue(tr("Tuning"));
        sl->setValue("                           ");
        box->setUseFrame(false);
        addChild(box);
    }

    addChild(progressBar = new ScanSignalMeter(PROGRESS_MAX));
    progressBar->setValue(0);
    progressBar->setLabel(QObject::tr("Scan"));

    if (signalmonitors)
    {
        addChild(ss = new ScanSignalMeter(65535));
        addChild(sn = new ScanSignalMeter(65535));
        ss->setLabel(QObject::tr("Signal Strength"));
        sn->setLabel(QObject::tr("Signal/Noise"));
    }

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

void ScanProgressPopup::dvbLock(int value)
{
    sl->setValue(value ? "Locked" : "No Lock");
}

void ScanProgressPopup::status(const QString& value)
{
    sta->setValue(value);
}

void ScanProgressPopup::exec(ScanWizardScanner *parent)
{
    dialog = (ConfigPopupDialogWidget*)
        dialogWidget(gContext->GetMainWindow());
    connect(dialog, SIGNAL(popupDone(void)),
            parent, SLOT(cancelScan(void)));
    dialog->ShowPopup(this);
}

void VideoSourceSetting::load()
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    QString querystr = QString(
        "SELECT DISTINCT videosource.name, videosource.sourceid "
        "FROM cardinput, videosource, capturecard "
        "WHERE cardinput.sourceid=videosource.sourceid AND "
        "      cardinput.cardid=capturecard.cardid AND "
        "      capturecard.cardtype in " CARDTYPES " AND "
        "      capturecard.hostname = '%1'").arg(gContext->GetHostName());
    
    query.prepare(querystr);

    if (!query.exec() || !query.isActive() || query.size() <= 0)
        return;


    while (query.next())
    {
        addSelection(query.value(0).toString(),
                     query.value(1).toString());
    }
}

void TransportSetting::refresh()
{
    clearSelections();
    
    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = QString(
        "SELECT mplexid,   networkid,  transportid, "
        "       frequency, symbolrate, modulation "
        "FROM dtv_multiplex WHERE sourceid = '%1' "
        "ORDER by frequency, networkid, transportid").arg(nSourceID);

    query.prepare(querystr);

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

void TransportSetting::sourceID(const QString& str)
{
    nSourceID = str.toInt();
    refresh();
}

void CaptureCardSetting::refresh()
{
    clearSelections();

    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = QString(
        "SELECT DISTINCT cardtype, videodevice, capturecard.cardid "
        "FROM capturecard, videosource, cardinput "
        "WHERE videosource.sourceid = %1 AND "
        "      cardinput.sourceid   = videosource.sourceid AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardtype in " CARDTYPES " AND "
        "      capturecard.hostname = '%2';")
        .arg(nSourceID).arg(gContext->GetHostName());
    query.prepare(thequery);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
            addSelection("[ " + query.value(0).toString() + " : " +
                         query.value(1).toString() + " ]",
                         query.value(2).toString());
    }
}

void CaptureCardSetting::sourceID(const QString& str)
{
    nSourceID = str.toInt();
    refresh();
}

void ScanTypeSetting::refresh(const QString& card)
{
    int nCard = card.toInt();
    // Only refresh if we really have to. If we do it too often
    // Then we end up fighting the scan routine when we want to
    // check the type of dvb card :/
    if (nCard == nCaptureCard)
        return;

    nCaptureCard = nCard;
    int nCardType = CardUtil::GetCardType(nCard);
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
                     QString::number(FullTunedScan_OFDM));
        addSelection(tr("Import channels.conf"),
                     QString::number(Import));
        break;
    case CardUtil::QPSK:
        addSelection(tr("Full Scan (Tuned)"),
                     QString::number(FullTunedScan_QPSK));
        addSelection(tr("Import channels.conf"),
                     QString::number(Import));
        break;
    case CardUtil::QAM:
        addSelection(tr("Full Scan (Tuned)"),
                     QString::number(FullTunedScan_QAM));
        addSelection(tr("Import channels.conf"),
                     QString::number(Import));
        break;
    case CardUtil::ATSC:
    case CardUtil::HDTV:
        addSelection(tr("Full Scan"),
                     QString::number(FullScan_ATSC), true);
        break;
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

ScanCountry::ScanCountry()
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
#endif

    setLabel(tr("Country"));
    addSelection(QObject::tr("Australia"),      "au", country == AU);
    addSelection(QObject::tr("Finland"),        "fi", country == FI);
    addSelection(QObject::tr("Sweden"),         "se", country == SE);
    addSelection(QObject::tr("United Kingdom"), "uk", country == UK);
    addSelection(QObject::tr("Germany"),        "de", country == DE);
}

ScanOptionalConfig::ScanOptionalConfig(ScanWizard *wizard,
                                      ScanTypeSetting *scanType) : 
                  VerticalConfigurationGroup(false,false,true,true)
{
    setUseLabel(false);
    setTrigger(scanType);
    // only save settings for the selected pane
    setSaveAll(false);

    addTarget(QString::number(ScanTypeSetting::Error_Open),
             new ErrorPane(QObject::tr("Failed to open the card")));
    addTarget(QString::number(ScanTypeSetting::Error_Probe),
             new ErrorPane(QObject::tr("Failed to probe the card")));

    addTarget(QString::number(ScanTypeSetting::FullTunedScan_QAM),wizard->paneQAM);
    addTarget(QString::number(ScanTypeSetting::FullTunedScan_QPSK),wizard->paneQPSK);
    addTarget(QString::number(ScanTypeSetting::FullTunedScan_OFDM),wizard->paneOFDM);
    addTarget(QString::number(ScanTypeSetting::FullScan_ATSC),wizard->paneATSC);
    country = new ScanCountry();
    addTarget(QString::number(ScanTypeSetting::FullScan_OFDM),country);
    addTarget(QString::number(ScanTypeSetting::FullScan_Analog),new BlankSetting());
    transport = new TransportSetting();
    addTarget(QString::number(ScanTypeSetting::TransportScan),transport);
    addTarget(QString::number(ScanTypeSetting::FullTransportScan),new BlankSetting());
    filename = new ScanFileImport();
    addTarget(QString::number(ScanTypeSetting::Import),filename);
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

    HorizontalConfigurationGroup *h1 =
        new HorizontalConfigurationGroup(false,false,true,true);
    h1->addChild(videoSource);
    h1->addChild(capturecard);
    addChild(h1);
    scanType = new ScanTypeSetting();
    addChild(scanType);
    scanConfig = new ScanOptionalConfig(_parent,scanType);
    addChild(scanConfig);

    connect(videoSource, SIGNAL(valueChanged(const QString&)),
        scanConfig->transport, SLOT(sourceID(const QString&)));
    connect(videoSource, SIGNAL(valueChanged(const QString&)),
        capturecard, SLOT(sourceID(const QString&)));

    //Setup signals to refill the scan types
    connect(capturecard, SIGNAL(valueChanged(const QString&)),
        scanType, SLOT(refresh(const QString&)));

    connect(capturecard, SIGNAL(valueChanged(const QString&)),
        parent, SLOT(captureCard(const QString&)));
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
