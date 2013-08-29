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

#ifndef _PANE_DVBT_H_
#define _PANE_DVBT_H_

// MythTV headers
#include "channelscanmiscsettings.h"

class PaneDVBT : public HorizontalConfigurationGroup
{
  public:
    PaneDVBT() : HorizontalConfigurationGroup(false, false, true, true)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true,true,false);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true,true,false);
        left->addChild(pfrequency       = new ScanFrequency());
        left->addChild(pbandwidth       = new ScanBandwidth());
        left->addChild(pinversion       = new ScanInversion());
        left->addChild(pconstellation   = new ScanConstellation());
        right->addChild(pcoderate_lp    = new ScanCodeRateLP());
        right->addChild(pcoderate_hp    = new ScanCodeRateHP());
        right->addChild(ptrans_mode     = new ScanTransmissionMode());
        right->addChild(pguard_interval = new ScanGuardInterval());
        right->addChild(phierarchy      = new ScanHierarchy());
        addChild(left);
        addChild(right);
    }

    QString frequency(void)      const { return pfrequency->getValue();     }
    QString bandwidth(void)      const { return pbandwidth->getValue();     }
    QString inversion(void)      const { return pinversion->getValue();     }
    QString constellation(void)  const { return pconstellation->getValue(); }
    QString coderate_lp(void)    const { return pcoderate_lp->getValue();   }
    QString coderate_hp(void)    const { return pcoderate_hp->getValue();   }
    QString trans_mode(void)     const { return ptrans_mode->getValue();    }
    QString guard_interval(void) const { return pguard_interval->getValue(); }
    QString hierarchy(void)      const { return phierarchy->getValue();     }

  protected:
    ScanFrequency        *pfrequency;
    ScanInversion        *pinversion;
    ScanBandwidth        *pbandwidth;
    ScanConstellation    *pconstellation;
    ScanCodeRateLP       *pcoderate_lp;
    ScanCodeRateHP       *pcoderate_hp;
    ScanTransmissionMode *ptrans_mode;
    ScanGuardInterval    *pguard_interval;
    ScanHierarchy        *phierarchy;
};

#endif // _PANE_DVBT_H_
