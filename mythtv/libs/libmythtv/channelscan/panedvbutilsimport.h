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

#ifndef _PANE_DVB_UTILS_IMPORT_H_
#define _PANE_DVB_UTILS_IMPORT_H_

// Qt headers
#include <QString>

// MythTV headers
#include "channelscanmiscsettings.h"

class PaneDVBUtilsImport : public GroupSetting
{
    Q_OBJECT

  public:
    PaneDVBUtilsImport() :
        m_filename(new TransTextEditSetting()),
        m_ignore_signal_timeout(new IgnoreSignalTimeout())
    {
        m_filename->setLabel(tr("File location"));
        m_filename->setHelpText(tr("Location of the channels.conf file."));
        addChild(m_filename);
        addChild(m_ignore_signal_timeout);
    }

    QString GetFilename(void)   const { return m_filename->getValue();    }
    bool DoIgnoreSignalTimeout(void) const
        { return m_ignore_signal_timeout->getValue().toInt(); }

  private:
    TransTextEditSetting    *m_filename              {nullptr};
    IgnoreSignalTimeout     *m_ignore_signal_timeout {nullptr};
};

#endif // _PANE_DVB_UTILS_IMPORT_H_
