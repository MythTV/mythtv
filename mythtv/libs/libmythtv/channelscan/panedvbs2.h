/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _PANE_DVBS2_H_
#define _PANE_DVBS2_H_

// MythTV headers
#include "channelscanmiscsettings.h"
#include "modulationsetting.h"

class PaneDVBS2 : public HorizontalConfigurationGroup
{
  public:
    PaneDVBS2() : HorizontalConfigurationGroup(false,false,true,false)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true);
        left->addChild( pfrequency  = new ScanFrequencykHz());
        left->addChild( ppolarity   = new ScanPolarity());
        left->addChild( psymbolrate = new ScanSymbolRateDVBS());
        left->addChild( pmod_sys    = new ScanModSys());
        right->addChild(pfec        = new ScanFec());
        right->addChild(pmodulation = new ScanDVBSModulation());
        right->addChild(pinversion  = new ScanInversion());
        right->addChild(prolloff    = new ScanRollOff());
        addChild(left);
        addChild(right);
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
