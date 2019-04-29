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

#ifndef _PANE_EXISTING_SCAN_IMPORT_H_
#define _PANE_EXISTING_SCAN_IMPORT_H_

// Qt headers
#include <QString>
#include <QObject>

// MythTV headers
#include "channelscanmiscsettings.h"
#include "scaninfo.h"
#include "mythdate.h"

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

        vector<ScanInfo> scans = LoadScanList();
        for (uint i = 0; i < scans.size(); i++)
        {
            if (scans[i].m_sourceid != m_sourceid)
                continue;

            QString scanDate = MythDate::toString(
                scans[i].m_scandate, MythDate::kDateTimeFull);
            QString proc     = (scans[i].m_processed) ?
                tr("processed") : tr("unprocessed");

            m_scanSelect->addSelection(
                QString("%1 %2").arg(scanDate).arg(proc),
                QString::number(scans[i].m_scanid));
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

#endif // _PANE_EXISTING_SCAN_IMPORT_H_
