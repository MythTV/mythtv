/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 * Copyright (c) 2014 David C J Matthews
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

#ifndef _PANE_DVBT2_H_
#define _PANE_DVBT2_H_

// MythTV headers
#include "channelscanmiscsettings.h"

class PaneDVBT2 : public GroupSetting
{
  public:
    PaneDVBT2(const QString &target, StandardSetting *setting)
    {
        setVisible(false);
        setting->addTargetedChildren(target,
                                     {this,
                                      m_pfrequency       = new ScanFrequency(),
                                      m_pbandwidth       = new ScanBandwidth(),
                                      m_pinversion       = new ScanInversion(),
                                      m_pconstellation   = new ScanConstellation(),
                                      m_pmod_sys         = new ScanDVBTModSys(),
                                      m_pcoderate_lp    = new ScanCodeRateLP(),
                                      m_pcoderate_hp    = new ScanCodeRateHP(),
                                      m_ptrans_mode     = new ScanTransmissionMode(),
                                      m_pguard_interval = new ScanGuardInterval(),
                                      m_phierarchy      = new ScanHierarchy()});
    }

    QString frequency(void)      const { return m_pfrequency->getValue();     }
    QString bandwidth(void)      const { return m_pbandwidth->getValue();     }
    QString inversion(void)      const { return m_pinversion->getValue();     }
    QString constellation(void)  const { return m_pconstellation->getValue(); }
    QString coderate_lp(void)    const { return m_pcoderate_lp->getValue();   }
    QString coderate_hp(void)    const { return m_pcoderate_hp->getValue();   }
    QString trans_mode(void)     const { return m_ptrans_mode->getValue();    }
    QString guard_interval(void) const { return m_pguard_interval->getValue();}
    QString hierarchy(void)      const { return m_phierarchy->getValue();     }
    QString mod_sys(void)        const { return m_pmod_sys->getValue();       }

    void setFrequency(uint frequency)             { m_pfrequency->setValue(frequency);          }
    void setBandwidth(QString bandwidth)          { m_pbandwidth->setValue(bandwidth);          }
    void setInversion(QString inversion)          { m_pinversion->setValue(inversion);          }
    void setConstellation(QString constellation)  { m_pconstellation->setValue(constellation);  }
    void setCodeRateLP(QString coderate_lp)       { m_pcoderate_lp->setValue(coderate_lp);      }
    void setCodeRateHP(QString coderate_hp)       { m_pcoderate_hp->setValue(coderate_hp);      }
    void setTransmode(QString trans_mode)         { m_ptrans_mode->setValue(trans_mode);        }
    void setGuardInterval(QString guard_interval) { m_pguard_interval->setValue(guard_interval);}
    void setHierarchy(QString hierarchy)          { m_phierarchy->setValue(hierarchy);          }
    void setModsys(QString mod_sys)               { m_pmod_sys->setValue(mod_sys);              }

  protected:
    ScanFrequency        *m_pfrequency      {nullptr};
    ScanBandwidth        *m_pbandwidth      {nullptr};
    ScanInversion        *m_pinversion      {nullptr};
    ScanConstellation    *m_pconstellation  {nullptr};
    ScanCodeRateLP       *m_pcoderate_lp    {nullptr};
    ScanCodeRateHP       *m_pcoderate_hp    {nullptr};
    ScanTransmissionMode *m_ptrans_mode     {nullptr};
    ScanGuardInterval    *m_pguard_interval {nullptr};
    ScanHierarchy        *m_phierarchy      {nullptr};
    ScanDVBTModSys       *m_pmod_sys        {nullptr};
};

#endif // _PANE_DVBT2_H_
