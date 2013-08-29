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

#ifndef _PANE_ALL_H_
#define _PANE_ALL_H_

// MythTV headers
#include "channelscanmiscsettings.h"

class PaneAll : public VerticalConfigurationGroup
{
  public:
    PaneAll() :
        VerticalConfigurationGroup(false, false, true, false),
        ignore_signal_timeout(new IgnoreSignalTimeout()),
        follow_nit(new FollowNITSetting())
    {
        addChild(ignore_signal_timeout);
        addChild(follow_nit);
    }

    bool ignoreSignalTimeout(void) const
        { return ignore_signal_timeout->getValue().toInt(); }
    bool GetFollowNIT(void) const
        { return follow_nit->getValue().toInt(); }

  protected:
    IgnoreSignalTimeout     *ignore_signal_timeout;
    FollowNITSetting        *follow_nit;
};

#endif // _PANE_ALL_H_
