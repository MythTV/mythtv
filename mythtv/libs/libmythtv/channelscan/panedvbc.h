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

#ifndef _PANE_DVBC_H_
#define _PANE_DVBC_H_

// MythTV headers
#include "channelscanmiscsettings.h"

class PaneDVBC : public GroupSetting
{
  public:
    PaneDVBC(const QString &target, StandardSetting *setting)
    {
        setVisible(false);
        setting->addTargetedChildren(target,
                                     {this,
                                      m_pfrequency   = new ScanFrequency(),
                                      m_psymbolrate  = new ScanSymbolRateDVBC(),
                                      m_pinversion   = new ScanInversion(),
                                      m_pmodulation = new ScanModulation(),
                                      m_pfec        = new ScanFec()});
    }

    QString frequency(void)  const { return m_pfrequency->getValue();  }
    QString symbolrate(void) const { return m_psymbolrate->getValue(); }
    QString inversion(void)  const { return m_pinversion->getValue();  }
    QString fec(void)        const { return m_pfec->getValue();        }
    QString modulation(void) const { return m_pmodulation->getValue(); }

    void setFrequency(uint frequency)      { m_pfrequency->setValue(frequency);  }
    void setSymbolrate(QString symbolrate) { m_psymbolrate->setValue(symbolrate);}
    void setInversion(QString inversion)   { m_pinversion->setValue(inversion);  }
    void setModulation(QString modulation) { m_pmodulation->setValue(modulation);}
    void setFec(QString fec)               { m_pfec->setValue(fec);              }

  protected:
    ScanFrequency      *m_pfrequency  {nullptr};
    ScanSymbolRateDVBC *m_psymbolrate {nullptr};
    ScanInversion      *m_pinversion  {nullptr};
    ScanModulation     *m_pmodulation {nullptr};
    ScanFec            *m_pfec        {nullptr};
};

#endif // _PANE_DVBC_H_
