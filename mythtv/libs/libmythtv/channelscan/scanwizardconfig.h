/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 *
 * Description:
 *     Collection of classes to provide channel scanning functionallity
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

#ifndef _SCAN_WIZARD_CONFIG_H_
#define _SCAN_WIZARD_CONFIG_H_

// MythTV headers
#include "settings.h"
#include "standardsettings.h"
#include "inputselectorsetting.h"
#include "channelscantypes.h"

class ScanWizard;
class VideoSourceSelector;
class ScanTypeSetting;
class ScanCountry;
class ScanNetwork;
class IgnoreSignalTimeout;
class DesiredServices;
class FreeToAirOnly;
class TrustEncSISetting;

class PaneAll;
class PaneATSC;
class PaneAnalog;
class PaneDVBT;
class PaneDVBT2;
class PaneDVBC;
class PaneDVBS;
class PaneDVBS2;
class PaneSingle;
class PaneDVBUtilsImport;
class PaneExistingScanImport;

class ScanTypeSetting : public TransMythUIComboBoxSetting
{
    Q_OBJECT
  public:
    enum Type
    {
        Error_Open = 0,
        Error_Probe,
        // Scans that check each frequency in a predefined list
        FullScan_Analog,
        FullScan_ATSC,
        FullScan_DVBC,
        FullScan_DVBT,
        FullScan_DVBT2,
        // Scans starting on one frequency that adds each transport
        // seen in the Network Information Tables to the scan.
        NITAddScan_DVBT,
        NITAddScan_DVBT2,
        NITAddScan_DVBS,
        NITAddScan_DVBS2,
        NITAddScan_DVBC,
        // Scan of all transports already in the database
        FullTransportScan,
        // Scan of one transport already in the database
        TransportScan,
        /// Scans the transport when there is no tuner (for ASI)
        CurrentTransportScan,
        // IPTV import of channels from M3U URL
        IPTVImport,
        // IPTV import of channels from M3U URL, with MPTS
        IPTVImportMPTS,
        // Imports lists from dvb-utils scanners
        DVBUtilsImport,
        // Imports lists from previous mythtv scan
        ExistingScanImport,
        // Import using the VBox API to get the channel list
        VBoxImport,
    };

    ScanTypeSetting() : hw_cardid(0)
        { setLabel(QObject::tr("Scan Type")); }

  protected slots:
    void SetInput(const QString &cardids_inputname);

  protected:
    uint    hw_cardid;
};

class ScanOptionalConfig : public GroupSetting
{
    Q_OBJECT

  public:
    explicit ScanOptionalConfig(ScanTypeSetting *_scan_type);

    QString GetFrequencyStandard(void)       const;
    QString GetModulation(void)              const;
    QString GetFrequencyTable(void)          const;
    bool    GetFrequencyTableRange(QString&,QString&) const;
    bool    DoIgnoreSignalTimeout(void)      const;
    bool    DoFollowNIT(void)                const;
    QString GetFilename(void)                const;
    uint    GetMultiplex(void)               const;
    QMap<QString,QString> GetStartChan(void) const;
    uint    GetScanID(void)                  const;

  public slots:
    void SetSourceID(const QString&);

  private:
    ScanTypeSetting      *scanType;
    ScanCountry          *dvbTCountry;
    ScanCountry          *dvbT2Country;
    ScanNetwork          *network;
    PaneDVBT             *paneDVBT;
    PaneDVBT2            *paneDVBT2;
    PaneDVBS             *paneDVBS;
    PaneDVBS2            *paneDVBS2;
    PaneATSC             *paneATSC;
    PaneDVBC             *paneDVBC;
    PaneAnalog           *paneAnalog;
    PaneSingle           *paneSingle;
    PaneAll              *paneAll;
    PaneDVBUtilsImport   *paneDVBUtilsImport;
    PaneExistingScanImport *paneExistingScanImport;
};

#endif // _SCAN_WIZARD_CONFIG_H_
