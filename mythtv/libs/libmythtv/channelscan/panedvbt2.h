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

#ifndef PANE_DVBT2_H
#define PANE_DVBT2_H

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
                                      m_pfrequency      = new ScanFrequency(),
                                      m_pbandwidth      = new ScanBandwidth(),
                                      m_pinversion      = new ScanInversion(),
                                      m_pconstellation  = new ScanConstellation(),
                                      m_pmodsys         = new ScanDVBTModSys(),
                                      m_pcoderateLp    = new ScanCodeRateLP(),
                                      m_pcoderateHp    = new ScanCodeRateHP(),
                                      m_ptransMode     = new ScanTransmissionMode(),
                                      m_pguardInterval = new ScanGuardInterval(),
                                      m_phierarchy      = new ScanHierarchy()});
    }

    QString frequency(void)      const { return m_pfrequency->getValue();     }
    QString bandwidth(void)      const { return m_pbandwidth->getValue();     }
    QString inversion(void)      const { return m_pinversion->getValue();     }
    QString constellation(void)  const { return m_pconstellation->getValue(); }
    QString coderate_lp(void)    const { return m_pcoderateLp->getValue();    }
    QString coderate_hp(void)    const { return m_pcoderateHp->getValue();    }
    QString trans_mode(void)     const { return m_ptransMode->getValue();     }
    QString guard_interval(void) const { return m_pguardInterval->getValue(); }
    QString hierarchy(void)      const { return m_phierarchy->getValue();     }
    QString modsys(void)         const { return m_pmodsys->getValue();        }

    void setFrequency(uint frequency)                    { m_pfrequency->setValue(frequency);          }
    void setBandwidth(const QString& bandwidth)          { m_pbandwidth->setValue(bandwidth);          }
    void setInversion(const QString& inversion)          { m_pinversion->setValue(inversion);          }
    void setConstellation(const QString& constellation)  { m_pconstellation->setValue(constellation);  }
    void setCodeRateLP(const QString& coderate_lp)       { m_pcoderateLp->setValue(coderate_lp);       }
    void setCodeRateHP(const QString& coderate_hp)       { m_pcoderateHp->setValue(coderate_hp);       }
    void setTransmode(const QString& trans_mode)         { m_ptransMode->setValue(trans_mode);         }
    void setGuardInterval(const QString& guard_interval) { m_pguardInterval->setValue(guard_interval); }
    void setHierarchy(const QString& hierarchy)          { m_phierarchy->setValue(hierarchy);          }
    void setModsys(const QString& mod_sys)               { m_pmodsys->setValue(mod_sys);               }

  protected:
    ScanFrequency        *m_pfrequency      {nullptr};
    ScanBandwidth        *m_pbandwidth      {nullptr};
    ScanInversion        *m_pinversion      {nullptr};
    ScanConstellation    *m_pconstellation  {nullptr};
    ScanCodeRateLP       *m_pcoderateLp     {nullptr};
    ScanCodeRateHP       *m_pcoderateHp     {nullptr};
    ScanTransmissionMode *m_ptransMode      {nullptr};
    ScanGuardInterval    *m_pguardInterval  {nullptr};
    ScanHierarchy        *m_phierarchy      {nullptr};
    ScanDVBTModSys       *m_pmodsys         {nullptr};
};

#endif // PANE_DVBT2_H
