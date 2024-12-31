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

#include <utility>

#include "libmythbase/mythlogging.h"

#include "channelscan/channelimporter.h"
#include "channelscan/channelscanner_gui.h"
#include "channelscan/scaninfo.h"
#include "channelscan/scanwizardconfig.h"
#include "scanwizard.h"
#include "sourceutil.h"
#include "videosource.h"

#define LOC QString("SWiz: ")

ScanWizard::ScanWizard(uint           default_sourceid,
                       uint           default_cardid,
                       const QString& default_inputname) :
    m_scannerPane(new ChannelScannerGUI())
{
    SetupConfig(default_sourceid, default_cardid, default_inputname);
    auto *scanButton = new ButtonStandardSetting(tr("Scan"));
    connect(scanButton, &ButtonStandardSetting::clicked, this, &ScanWizard::Scan);
    addChild(scanButton);
}

void ScanWizard::Scan()
{
    QMap<QString,QString> start_chan;
    DTVTunerType parse_type(DTVTunerType::kTunerTypeUnknown);

    uint    cardid    = GetCardID();
    QString inputname = GetInputName();
    uint    sourceid  = GetSourceID();
    int     scantype  = GetScanType();
    bool    do_scan   = true;

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Scan(): " +
        QString("type(%1) cardid(%2) inputname(%3)")
            .arg(scantype).arg(cardid).arg(inputname));

    if (scantype == ScanTypeSetting::DVBUtilsImport)
    {
        m_scannerPane->ImportDVBUtils(sourceid, m_lastHWCardType,
                                      GetFilename());
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBT)
    {
        start_chan = GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBT;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBT2)
    {
        start_chan = GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBT2;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBS)
    {
        start_chan = GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBS1;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBS2)
    {
        start_chan = GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBS2;
    }
    else if (scantype == ScanTypeSetting::NITAddScan_DVBC)
    {
        start_chan = GetStartChan();
        parse_type = DTVTunerType::kTunerTypeDVBC;
    }
    else if (scantype == ScanTypeSetting::IPTVImport)
    {
        do_scan = false;
        m_scannerPane->ImportM3U(cardid, inputname, sourceid, false);
    }
    else if (scantype == ScanTypeSetting::VBoxImport)
    {
        do_scan = false;
        m_scannerPane->ImportVBox(cardid, inputname, sourceid,
                                  DoFreeToAirOnly(),
                                  GetServiceRequirements());
    }
    else if (scantype == ScanTypeSetting::ExternRecImport)
    {
        do_scan = false;
        m_scannerPane->ImportExternRecorder(cardid, inputname, sourceid);
    }
    else if (scantype == ScanTypeSetting::HDHRImport)
    {
        do_scan = false;
        m_scannerPane->ImportHDHR(cardid, inputname, sourceid,
                                  GetServiceRequirements());
    }
    else if (scantype == ScanTypeSetting::IPTVImportMPTS)
    {
        m_scannerPane->ImportM3U(cardid, inputname, sourceid, true);
    }
    else if ((scantype == ScanTypeSetting::FullScan_ATSC)     ||
             (scantype == ScanTypeSetting::FullTransportScan) ||
             (scantype == ScanTypeSetting::TransportScan)     ||
             (scantype == ScanTypeSetting::CurrentTransportScan) ||
             (scantype == ScanTypeSetting::FullScan_DVBC)     ||
             (scantype == ScanTypeSetting::FullScan_DVBT)     ||
             (scantype == ScanTypeSetting::FullScan_DVBT2)    ||
             (scantype == ScanTypeSetting::FullScan_Analog))
    {
        ;
    }
    else if (scantype == ScanTypeSetting::ExistingScanImport)
    {
        do_scan = false;
        uint scanid = GetScanID();
        ScanDTVTransportList transports = LoadScan(scanid);
        ChannelImporter ci(true, true, true, true, false,
                           DoFreeToAirOnly(),
                           DoChannelNumbersOnly(),
                           DoCompleteChannelsOnly(),
                           DoFullChannelSearch(),
                           DoRemoveDuplicates(),
                           GetServiceRequirements());
        ci.Process(transports, sourceid);
    }
    else
    {
        do_scan = false;
        LOG(VB_CHANSCAN, LOG_ERR, LOC + "SetPage(): " +
            QString("type(%1) src(%2) cardid(%3) not handled")
                .arg(scantype).arg(sourceid).arg(cardid));

        ShowOkPopup(tr("Programmer Error, see console"));
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
        ShowOkPopup(tr("Error parsing parameters"));

        do_scan = false;
    }

    if (do_scan)
    {
        QString table_start;
        QString table_end;
        GetFrequencyTableRange(table_start, table_end);

        m_scannerPane->Scan(
            GetScanType(),            GetCardID(),
            GetInputName(),           GetSourceID(),
            DoIgnoreSignalTimeout(),  DoFollowNIT(),
            DoTestDecryption(),       DoFreeToAirOnly(),
            DoChannelNumbersOnly(),   DoCompleteChannelsOnly(),
            DoFullChannelSearch(),
            DoRemoveDuplicates(),
            DoAddFullTS(),
            GetServiceRequirements(),

            // stuff needed for particular scans
            GetMultiplex(),         start_chan,
            GetFrequencyStandard(), GetModulation(),
            GetFrequencyTable(),
            table_start, table_end);
    }
}

void ScanWizard::SetInput(const QString &cardid_inputname)
{
    uint    cardid = 0;
    QString inputname;
    if (!InputSelector::Parse(cardid_inputname, cardid, inputname))
        return;

    // Work out what kind of card we've got
    // We need to check against the last capture card so that we don't
    // try and probe a card which is already open by scan()
    if ((m_lastHWCardID != cardid) ||
        (m_lastHWCardType == CardUtil::INPUT_TYPES::ERROR_OPEN))
    {
        m_lastHWCardID    = cardid;
        QString subtype = CardUtil::ProbeSubTypeName(cardid);
        m_lastHWCardType  = CardUtil::toInputType(subtype);
    }
}
