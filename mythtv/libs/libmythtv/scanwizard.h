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
#include "libmythui/standardsettings.h"
#include "libmythbase/mythdbcon.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelscan/scanwizardconfig.h"
#include "libmythtv/mythtvexp.h"

class ChannelScannerGUI;

class MTV_PUBLIC ScanWizard : public GroupSetting
{
    Q_OBJECT

  public:
    explicit ScanWizard(uint  default_sourceid  = 0,
               uint           default_cardid    = 0,
               const QString& default_inputname = QString());

    ~ScanWizard() override = default;

  protected slots:
    void Scan();
    void SetInput(const QString &cardid_inputname);
    void SetPaneDefaults(const QString &cardid_inputname);

  protected:
    uint               m_lastHWCardID   {0};
    CardUtil::INPUT_TYPES m_lastHWCardType {CardUtil::INPUT_TYPES::ERROR_PROBE};
    ChannelScannerGUI *m_scannerPane    {nullptr};

  // The following are moved from deleted class ScanWizardConfig
  public:
    void SetupConfig(uint default_sourceid, uint default_cardid,
        const QString& default_inputname);

    uint    GetSourceID(void)     const;
    uint    GetScanID(void)       const { return m_scanConfig->GetScanID(); }
    QString GetModulation(void)   const { return m_scanConfig->GetModulation(); }
    int     GetScanType(void)     const { return m_scanType->getValue().toInt();}
    uint    GetCardID(void)       const { return m_input->GetCardID(); }
    QString GetInputName(void)    const { return m_input->GetInputName(); }
    QString GetFilename(void)     const { return m_scanConfig->GetFilename();   }
    uint    GetMultiplex(void)    const { return m_scanConfig->GetMultiplex();  }
    bool    GetFrequencyTableRange(QString &start, QString &end) const
        { return m_scanConfig->GetFrequencyTableRange(start, end); }
    QString GetFrequencyStandard(void) const
        { return m_scanConfig->GetFrequencyStandard(); }
    QString GetFrequencyTable(void) const
        { return m_scanConfig->GetFrequencyTable(); }
    QMap<QString,QString> GetStartChan(void) const
        { return m_scanConfig->GetStartChan(); }
    ServiceRequirements GetServiceRequirements(void) const;
    bool    DoIgnoreSignalTimeout(void) const
        { return m_scanConfig->DoIgnoreSignalTimeout(); }
    bool    DoFollowNIT(void) const
        { return m_scanConfig->DoFollowNIT(); }
    bool    DoFreeToAirOnly(void)        const;
    bool    DoChannelNumbersOnly(void)   const;
    bool    DoCompleteChannelsOnly(void) const;
    bool    DoFullChannelSearch(void)    const;
    bool    DoRemoveDuplicates(void)     const;
    bool    DoAddFullTS(void)            const;
    bool    DoTestDecryption(void)       const;
    bool    DoScanOpenTV(void)           const;

  protected:
    VideoSourceSelector  *m_videoSource   {nullptr};
    InputSelector        *m_input         {nullptr};
    ScanTypeSetting      *m_scanType      {nullptr};
    ScanOptionalConfig   *m_scanConfig    {nullptr};
    DesiredServices      *m_services      {nullptr};
    FreeToAirOnly        *m_ftaOnly       {nullptr};
    ChannelNumbersOnly   *m_lcnOnly       {nullptr};
    CompleteChannelsOnly *m_completeOnly  {nullptr};
    FullChannelSearch    *m_fullSearch    {nullptr};
    RemoveDuplicates     *m_removeDuplicates {nullptr};
    AddFullTS            *m_addFullTS     {nullptr};
    TrustEncSISetting    *m_trustEncSI    {nullptr};
  // End of members moved from ScanWizardConfig
};

#endif // SCANWIZARD_H
