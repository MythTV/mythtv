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

#ifndef _PANE_DVBS_H_
#define _PANE_DVBS_H_

// MythTV headers
#include "channelscanmiscsettings.h"

class PaneDVBS : public HorizontalConfigurationGroup
{
  public:
    PaneDVBS() : HorizontalConfigurationGroup(false, false, true, false)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true);
        left->addChild(pfrequency  = new ScanFrequencykHz());
        left->addChild(ppolarity   = new ScanPolarity());
        left->addChild(psymbolrate = new ScanSymbolRateDVBS());
        right->addChild(pfec       = new ScanFec());
        right->addChild(pinversion = new ScanInversion());
        addChild(left);
        addChild(right);
    }

    QString frequency(void)  const { return pfrequency->getValue();  }
    QString symbolrate(void) const { return psymbolrate->getValue(); }
    QString inversion(void)  const { return pinversion->getValue();  }
    QString fec(void)        const { return pfec->getValue();        }
    QString polarity(void)   const { return ppolarity->getValue();   }

  protected:
    ScanFrequencykHz   *pfrequency;
    ScanSymbolRateDVBS *psymbolrate;
    ScanInversion  *pinversion;
    ScanFec        *pfec;
    ScanPolarity   *ppolarity;
};

#endif // _PANE_DVBS_H_
