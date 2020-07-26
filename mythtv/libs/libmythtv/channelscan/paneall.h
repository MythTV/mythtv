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

#ifndef PANE_ALL_H
#define PANE_ALL_H

// MythTV headers
#include "channelscanmiscsettings.h"

class PaneAll : public GroupSetting
{
  public:
    PaneAll(const QString &target, StandardSetting *setting) :
        m_ignoreSignalTimeout(new IgnoreSignalTimeout()),
        m_followNit(new FollowNITSetting())
    {
        setVisible(false);
        setting->addTargetedChildren(target,
                                     {this,
                                      m_ignoreSignalTimeout,
                                      m_followNit});
    }

    bool ignoreSignalTimeout(void) const
        { return m_ignoreSignalTimeout->getValue().toInt() != 0; }
    bool GetFollowNIT(void) const
        { return m_followNit->getValue().toInt() != 0; }

  protected:
    IgnoreSignalTimeout *m_ignoreSignalTimeout {nullptr};
    FollowNITSetting    *m_followNit {nullptr};
};

#endif // PANE_ALL_H
