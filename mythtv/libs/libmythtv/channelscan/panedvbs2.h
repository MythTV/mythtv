/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _PANE_DVBS2_H_
#define _PANE_DVBS2_H_

// MythTV headers
#include "channelscanmiscsettings.h"
#include "modulationsetting.h"

class PaneDVBS2 : public GroupSetting
{
  public:
    PaneDVBS2(const QString &target, StandardSetting *setting)
    {
        setVisible(false);
        setting->addTargetedChildren(target,
                                     {this,
                                      m_pfrequency  = new ScanFrequencykHz(),
                                      m_ppolarity   = new ScanPolarity(),
                                      m_psymbolrate = new ScanSymbolRateDVBS(),
                                      m_pmod_sys    = new ScanModSys(),
                                      m_pfec        = new ScanFec(),
                                      m_pmodulation = new ScanDVBSModulation(),
                                      m_pinversion  = new ScanInversion(),
                                      m_prolloff    = new ScanRollOff()});
    }

    QString frequency(void)  const { return m_pfrequency->getValue();  }
    QString symbolrate(void) const { return m_psymbolrate->getValue(); }
    QString inversion(void)  const { return m_pinversion->getValue();  }
    QString fec(void)        const { return m_pfec->getValue();        }
    QString polarity(void)   const { return m_ppolarity->getValue();   }
    QString modulation(void) const { return m_pmodulation->getValue(); }
    QString mod_sys(void)    const { return m_pmod_sys->getValue();    }
    QString rolloff(void)    const { return m_prolloff->getValue();    }

    void setFrequency(uint frequency)      { m_pfrequency->setValue(frequency);  }
    void setSymbolrate(QString symbolrate) { m_psymbolrate->setValue(symbolrate);}
    void setInversion(QString inversion)   { m_pinversion->setValue(inversion);  }
    void setFec(QString fec)               { m_pfec->setValue(fec);              }
    void setPolarity(QString polarity)     { m_ppolarity->setValue(polarity);    }
    void setModulation(QString modulation) { m_pmodulation->setValue(modulation);}
    void setModsys(QString mod_sys)        { m_pmod_sys->setValue(mod_sys);      }
    void setRolloff(QString rolloff)       { m_prolloff->setValue(rolloff);      }

  protected:
    ScanFrequencykHz   *m_pfrequency  {nullptr};
    ScanSymbolRateDVBS *m_psymbolrate {nullptr};
    ScanInversion      *m_pinversion  {nullptr};
    ScanFec            *m_pfec        {nullptr};
    ScanPolarity       *m_ppolarity   {nullptr};
    ScanDVBSModulation *m_pmodulation {nullptr};
    ScanModSys         *m_pmod_sys    {nullptr};
    ScanRollOff        *m_prolloff    {nullptr};
};

#endif // _PANE_DVBS2_H_
