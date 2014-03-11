/* -*- Mode: c++ -*-
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "scanwizardconfig.h"
#include "channelscanner_gui.h"
#include "scanwizard.h"
#include "sourceutil.h"
#include "cardutil.h"
#include "videosource.h"
#include "scaninfo.h"
#include "channelimporter.h"
#include "mythlogging.h"

#define LOC QString("SWiz: ")

ScanWizard::ScanWizard(uint    default_sourceid,
                       uint    default_cardid,
                       QString default_inputname) :
    lastHWCardID(0),
    lastHWCardType(CardUtil::ERROR_PROBE),
    configPane(new ScanWizardConfig(
                   this, default_sourceid, default_cardid, default_inputname)),
    scannerPane(new ChannelScannerGUI())
{
    addChild(configPane);
    addChild(scannerPane);
}

MythDialog *ScanWizard::dialogWidget(MythMainWindow *parent, const char*)
{
    MythWizard *wizard = (MythWizard*)
        ConfigurationWizard::dialogWidget(parent, "ScanWizard");

    connect(wizard, SIGNAL(selected(const QString&)),
            this,   SLOT(  SetPage( const QString&)));

    return wizard;
}

void ScanWizard::SetPage(const QString &pageTitle)
{
    LOG(VB_CHANSCAN, LOG_INFO, QString("SetPage(%1)").arg(pageTitle));
    if (pageTitle != ChannelScannerGUI::kTitle)
    {
        scannerPane->quitScanning();
        return;
    }

    QMap<QString,QString> start_chan;
    DTVTunerType parse_type = DTVTunerType::kTunerTypeUnknown;

    uint    cardid    = configPane->GetCardID();
    QString inputname = configPane->GetInputName();
    uint    sourceid  = configPane->GetSourceID();
    int     scantype  = configPane->GetScanType();
    bool    do_scan   = true;

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "SetPage(): " +
        QString("type(%1) cardid(%2) inputname(%3)")
            .arg(scantype).arg(cardid).arg(inputname));

    if (scantype == ScanTypeSetting::DVBUtilsImport)
    {
        scannerPane->ImportDVBUtils(sourceid, lastHWCardType,
                                    configPane->GetFilename());
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBT)
    {
        start_chan = configPane->GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBT;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBS)
    {
        start_chan = configPane->GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBS1;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBS2)
    {
        start_chan = configPane->GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBS2;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBC)
    {
        start_chan = configPane->GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBC;
    }
    else if (scantype == ScanTypeSetting::IPTVImport)
    {
        do_scan = false;
        scannerPane->ImportM3U(cardid, inputname, sourceid, false);
    }
    else if (scantype == ScanTypeSetting::IPTVImportMPTS)
    {
        scannerPane->ImportM3U(cardid, inputname, sourceid, true);
    }
    else if ((scantype == ScanTypeSetting::FullScan_ATSC)     ||
             (scantype == ScanTypeSetting::FullTransportScan) ||
             (scantype == ScanTypeSetting::TransportScan)     ||
             (scantype == ScanTypeSetting::CurrentTransportScan) ||
             (scantype == ScanTypeSetting::FullScan_DVBC)     ||
             (scantype == ScanTypeSetting::FullScan_DVBT)     ||
             (scantype == ScanTypeSetting::FullScan_Analog))
    {
        ;
    }
    else if (scantype == ScanTypeSetting::ExistingScanImport)
    {
        do_scan = false;
        uint scanid = configPane->GetScanID();
        ScanDTVTransportList transports = LoadScan(scanid);
        ChannelImporter ci(true, true, true, true, false,
                           configPane->DoFreeToAirOnly(),
                           configPane->GetServiceRequirements());
        ci.Process(transports);
    }
    else
    {
        do_scan = false;
        LOG(VB_CHANSCAN, LOG_ERR, LOC + "SetPage(): " +
            QString("type(%1) src(%2) cardid(%3) not handled")
                .arg(scantype).arg(sourceid).arg(cardid));

        MythPopupBox::showOkPopup(
            GetMythMainWindow(), tr("ScanWizard"),
            tr("Programmer Error, see console"));
    }

    // Just verify what we get from the UI...
    DTVMultiplex tuning;
    if ((parse_type != DTVTunerType::kTunerTypeUnknown) &&
        !tuning.ParseTuningParams(
            parse_type,
            start_chan["frequency"],      start_chan["inversion"],
            start_chan["symbolrate"],     start_chan["fec"],
            start_chan["polarity"],
            start_chan["coderate_hp"],    start_chan["coderate_lp"],
            start_chan["constellation"],  start_chan["trans_mode"],
            start_chan["guard_interval"], start_chan["hierarchy"],
            start_chan["modulation"],     start_chan["bandwidth"],
            start_chan["mod_sys"],        start_chan["rolloff"]))
    {
        MythPopupBox::showOkPopup(
            GetMythMainWindow(), tr("ScanWizard"),
            tr("Error parsing parameters"));

        do_scan = false;
    }

    if (do_scan)
    {
        QString table_start, table_end;
        configPane->GetFrequencyTableRange(table_start, table_end);

        scannerPane->Scan(
            configPane->GetScanType(),            configPane->GetCardID(),
            configPane->GetInputName(),           configPane->GetSourceID(),
            configPane->DoIgnoreSignalTimeout(),  configPane->DoFollowNIT(),
            configPane->DoTestDecryption(),       configPane->DoFreeToAirOnly(),
            configPane->GetServiceRequirements(),
            // stuff needed for particular scans
            configPane->GetMultiplex(),         start_chan,
            configPane->GetFrequencyStandard(), configPane->GetModulation(),
            configPane->GetFrequencyTable(),
            table_start, table_end);
    }
}

void ScanWizard::SetInput(const QString &cardids_inputname)
{
    uint    cardid;
    QString inputname;
    if (!InputSelector::Parse(cardids_inputname, cardid, inputname))
        return;

    // Work out what kind of card we've got
    // We need to check against the last capture card so that we don't
    // try and probe a card which is already open by scan()
    if ((lastHWCardID != cardid) ||
        (lastHWCardType == CardUtil::ERROR_OPEN))
    {
        lastHWCardID    = cardid;
        QString subtype = CardUtil::ProbeSubTypeName(cardid);
        lastHWCardType  = CardUtil::toCardType(subtype);
    }
}
