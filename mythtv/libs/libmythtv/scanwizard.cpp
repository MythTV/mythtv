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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "scanwizardhelpers.h"
#include "scanwizardscanner.h"
#include "scanwizard.h"
#include "sourceutil.h"
#include "cardutil.h"
#include "videosource.h"

#define LOC QString("SWiz: ")
#define LOC_ERR QString("SWiz, Error: ")

ScanWizard::ScanWizard(uint    default_sourceid,
                       uint    default_cardid,
                       QString default_inputname) :
    lastHWCardID(0),
    lastHWCardType(CardUtil::ERROR_PROBE),
    configPane(new ScanWizardConfig(
                   this, default_sourceid, default_cardid, default_inputname)),
    scannerPane(new ScanWizardScanner())
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
    VERBOSE(VB_SIPARSER, QString("SetPage(%1)").arg(pageTitle));
    if (pageTitle != ScanWizardScanner::kTitle)
        return;

    QMap<QString,QString> start_chan;
    DTVTunerType parse_type = DTVTunerType::kTunerTypeUnknown;

    uint    pcardid   = configPane->GetParentCardID();
    QString inputname = configPane->GetInputName();
    uint    sourceid  = configPane->GetSourceID();
    int     scantype  = configPane->GetScanType();
    bool    do_scan   = true;

    VERBOSE(VB_SIPARSER, LOC + "SetPage(): " +
            QString("type(%1) pcardid(%2) inputname(%3)")
            .arg(scantype).arg(pcardid).arg(inputname));

    if (scantype == ScanTypeSetting::DVBUtilsImport)
    {
        scannerPane->ImportDVBUtils(sourceid, lastHWCardType,
                                    configPane->GetFilename());
    }
    else if (scantype == ScanTypeSetting::NITAddScan_OFDM)
    {
        start_chan = configPane->GetStartChan();
        parse_type = DTVTunerType::kTunerTypeOFDM;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_QPSK)
    {
        start_chan = configPane->GetStartChan();
        parse_type = DTVTunerType::kTunerTypeQPSK;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_QAM)
    {
        start_chan = configPane->GetStartChan();
        parse_type = DTVTunerType::kTunerTypeQAM;
    }
    else if (scantype == ScanTypeSetting::IPTVImport)
    {
        do_scan = false;
        scannerPane->ImportM3U(pcardid, inputname, sourceid);
    }
    else if ((scantype == ScanTypeSetting::FullScan_ATSC)     ||
             (scantype == ScanTypeSetting::FullTransportScan) ||
             (scantype == ScanTypeSetting::TransportScan)     ||
             (scantype == ScanTypeSetting::FullScan_OFDM)     ||
             (scantype == ScanTypeSetting::FullScan_Analog))
    {
        ;
    }
    else
    {
        do_scan = false;
        VERBOSE(VB_SIPARSER, LOC_ERR + "SetPage(): " +
                QString("type(%1) src(%2) pcardid(%3) not handled")
                .arg(scantype).arg(sourceid).arg(pcardid));

        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), tr("ScanWizard"),
            "Programmer Error, see console");
    }

    // Just verify what we get from the UI...
    DTVMultiplex tuning;
    if ((DTVTunerType::kTunerTypeUnknown != parse_type) &&
        !tuning.ParseTuningParams(
            parse_type,
            start_chan["frequency"],      start_chan["inversion"],
            start_chan["symbolrate"],     start_chan["fec"],
            start_chan["polarity"],
            start_chan["coderate_hp"],    start_chan["coderate_lp"],
            start_chan["constellation"],  start_chan["trans_mode"],
            start_chan["guard_interval"], start_chan["hierarchy"],
            start_chan["modulation"],     start_chan["bandwidth"]))
    {
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), tr("ScanWizard"),
            tr("Error parsing parameters"));

        do_scan = false;
    }

    if (do_scan)
    {
        scannerPane->Scan(
            configPane->GetScanType(),       configPane->GetParentCardID(),
            configPane->GetChildCardID(),    configPane->GetInputName(),
            configPane->GetSourceID(),
            configPane->DoDeleteChannels(),  configPane->DoRenameChannels(),
            configPane->DoIgnoreSignalTimeout(), configPane->GetMultiplex(),
            start_chan,
            configPane->GetFrequencyStandard(), configPane->GetModulation(),
            configPane->GetFrequencyTable(), configPane->GetATSCFormat());
    }
}

void ScanWizard::SetInput(const QString &cardids_inputname)
{
    uint pcardid, ccardid;
    QString inputname;
    if (!InputSelector::Parse(cardids_inputname, pcardid, ccardid, inputname))
        return;

    uint hw_cardid = (ccardid) ? ccardid : pcardid;

    // Work out what kind of card we've got
    // We need to check against the last capture card so that we don't
    // try and probe a card which is already open by scan()
    if ((lastHWCardID != hw_cardid) ||
        (lastHWCardType == CardUtil::ERROR_OPEN))
    {
        lastHWCardID    = hw_cardid;
        QString subtype = CardUtil::ProbeSubTypeName(hw_cardid, 0);
        lastHWCardType  = CardUtil::toCardType(subtype);
        configPane->SetDefaultATSCFormat(
            SourceUtil::GetChannelFormat(configPane->GetSourceID()));
    }
}
