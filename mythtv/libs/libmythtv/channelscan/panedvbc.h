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

#ifndef PANE_DVBC_H
#define PANE_DVBC_H

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
                                      m_psymbolrate  = new ScanDVBCSymbolRate(),
                                      m_pmodulation  = new ScanModulation(),
                                      m_pmodsys      = new ScanDVBCModSys(),
                                      m_pinversion   = new ScanInversion(),
                                      m_pfec         = new ScanFec()});
    }

    QString frequency(void)  const { return m_pfrequency->getValue();  }
    QString symbolrate(void) const { return m_psymbolrate->getValue(); }
    QString modulation(void) const { return m_pmodulation->getValue(); }
    QString modsys(void)     const { return m_pmodsys->getValue();     }
    QString inversion(void)  const { return m_pinversion->getValue();  }
    QString fec(void)        const { return m_pfec->getValue();        }

    void setFrequency(uint frequency)             { m_pfrequency->setValue(frequency);  }
    void setSymbolrate(const QString& symbolrate) { m_psymbolrate->setValue(symbolrate);}
    void setModulation(const QString& modulation) { m_pmodulation->setValue(modulation);}
    void setModsys(const QString& modsys)         { m_pmodsys->setValue(modsys);        }
    void setInversion(const QString& inversion)   { m_pinversion->setValue(inversion);  }
    void setFec(const QString& fec)               { m_pfec->setValue(fec);              }

  protected:
    ScanFrequency      *m_pfrequency  {nullptr};
    ScanDVBCSymbolRate *m_psymbolrate {nullptr};
    ScanModulation     *m_pmodulation {nullptr};
    ScanDVBCModSys     *m_pmodsys     {nullptr};
    ScanInversion      *m_pinversion  {nullptr};
    ScanFec            *m_pfec        {nullptr};
};

#endif // PANE_DVBC_H
