/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2008 Daniel Kristjansson
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

#ifndef PANE_EXISTING_SCAN_IMPORT_H
#define PANE_EXISTING_SCAN_IMPORT_H

// Qt headers
#include <QString>
#include <QObject>

// MythTV headers
#include "libmythbase/mythdate.h"
#include "channelscanmiscsettings.h"
#include "scaninfo.h"

class PaneExistingScanImport : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(PaneExistingScanImport)

  public:
    PaneExistingScanImport(const QString &target, StandardSetting *setting) :
        m_scanSelect(new TransMythUIComboBoxSetting())
    {
        setVisible(false);
        m_scanSelect->setLabel(tr("Scan to Import"));
        setting->addTargetedChildren(target, {this, m_scanSelect});
    }

    void Load(void) override // GroupSetting
    {
        m_scanSelect->clearSelections();
        if (!m_sourceid)
            return;

        std::vector<ScanInfo> scans = LoadScanList(m_sourceid);
        for (auto it = scans.rbegin(); it != scans.rend(); ++it)
        {
            ScanInfo &scan   = *it;
            QString scanDate = MythDate::toString(scan.m_scandate, MythDate::kDateTimeFull);
            QString proc     = (scan.m_processed) ? tr("processed") : tr("unprocessed");

            m_scanSelect->addSelection(
                QString("%1 %2").arg(scanDate, proc),
                QString::number(scan.m_scanid));
        }
    }

    void SetSourceID(uint sourceid)
    {
        m_sourceid = sourceid;
        Load();
    }

    uint GetScanID(void) const { return m_scanSelect->getValue().toUInt(); }

  private:
    uint                        m_sourceid   {0};
    TransMythUIComboBoxSetting *m_scanSelect {nullptr};
};

#endif // PANE_EXISTING_SCAN_IMPORT_H
