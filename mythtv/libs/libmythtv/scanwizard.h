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

#ifndef SCANWIZARD_H
#define SCANWIZARD_H

// MythTV headers
#include "mythtvexp.h"
#include "mythdbcon.h"
#include "mythwizard.h"
#include "settings.h"
#include "standardsettings.h"
#include "scanwizardconfig.h"

class ChannelScannerGUI;

class MTV_PUBLIC ScanWizard : public GroupSetting
{
    Q_OBJECT

  public:
    ScanWizard(uint    default_sourceid  = 0,
               uint    default_cardid    = 0,
               QString default_inputname = QString());

    ~ScanWizard() { }

  protected slots:
    void Scan();
    void SetInput(const QString &cardid_inputname);

  protected:
    uint               lastHWCardID;
    uint               lastHWCardType;
    ChannelScannerGUI *scannerPane;

  // The following are moved from deleted class ScanWizardConfig
  public:
    void SetupConfig(uint default_sourceid, uint default_cardid,
        QString default_inputname);

    uint    GetSourceID(void)     const;
    uint    GetScanID(void)       const { return scanConfig->GetScanID(); }
    QString GetModulation(void)   const { return scanConfig->GetModulation(); }
    int     GetScanType(void)     const { return scanType->getValue().toInt();}
    uint    GetCardID(void)       const { return input->GetCardID(); }
    QString GetInputName(void)    const { return input->GetInputName(); }
    QString GetFilename(void)     const { return scanConfig->GetFilename();   }
    uint    GetMultiplex(void)    const { return scanConfig->GetMultiplex();  }
    bool    GetFrequencyTableRange(QString &start, QString &end) const
        { return scanConfig->GetFrequencyTableRange(start, end); }
    QString GetFrequencyStandard(void) const
        { return scanConfig->GetFrequencyStandard(); }
    QString GetFrequencyTable(void) const
        { return scanConfig->GetFrequencyTable(); }
    QMap<QString,QString> GetStartChan(void) const
        { return scanConfig->GetStartChan(); }
    ServiceRequirements GetServiceRequirements(void) const;
    bool    DoIgnoreSignalTimeout(void) const
        { return scanConfig->DoIgnoreSignalTimeout(); }
    bool    DoFollowNIT(void) const
        { return scanConfig->DoFollowNIT(); }
    bool    DoFreeToAirOnly(void)  const;
    bool    DoTestDecryption(void) const;

  protected:
    VideoSourceSelector *videoSource;
    InputSelector       *input;
    ScanTypeSetting     *scanType;
    ScanOptionalConfig  *scanConfig;
    DesiredServices     *services;
    FreeToAirOnly       *ftaOnly;
    TrustEncSISetting   *trustEncSI;
// End of members moved from ScanWizardConfig
};

#endif // SCANWIZARD_H
