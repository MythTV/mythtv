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
                                      pfrequency  = new ScanFrequencykHz(),
                                      ppolarity   = new ScanPolarity(),
                                      psymbolrate = new ScanSymbolRateDVBS(),
                                      pmod_sys    = new ScanModSys(),
                                      pfec        = new ScanFec(),
                                      pmodulation = new ScanDVBSModulation(),
                                      pinversion  = new ScanInversion(),
                                      prolloff    = new ScanRollOff()});
    }

    QString frequency(void)  const { return pfrequency->getValue();  }
    QString symbolrate(void) const { return psymbolrate->getValue(); }
    QString inversion(void)  const { return pinversion->getValue();  }
    QString fec(void)        const { return pfec->getValue();        }
    QString polarity(void)   const { return ppolarity->getValue();   }
    QString modulation(void) const { return pmodulation->getValue(); }
    QString mod_sys(void)    const { return pmod_sys->getValue();    }
    QString rolloff(void)    const { return prolloff->getValue();    }

  protected:
    ScanFrequencykHz   *pfrequency;
    ScanSymbolRateDVBS *psymbolrate;
    ScanInversion  *pinversion;
    ScanFec        *pfec;
    ScanPolarity   *ppolarity;
    ScanDVBSModulation *pmodulation;
    ScanModSys     *pmod_sys;
    ScanRollOff    *prolloff;
};

#endif // _PANE_DVBS2_H_
