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
        left->addChild( pfrequency  = new ScanFrequency());
        left->addChild( ppolarity   = new ScanPolarity());
        left->addChild( psymbolrate = new ScanSymbolRate());
        right->addChild(pfec        = new ScanFec());
        right->addChild(pmodulation = new ScanModulation());
        right->addChild(pinversion  = new ScanInversion());
        addChild(left);
        addChild(right);     
    }

    QString frequency(void)  const { return pfrequency->getValue();  }
    QString symbolrate(void) const { return psymbolrate->getValue(); }
    QString inversion(void)  const { return pinversion->getValue();  }
    QString fec(void)        const { return pfec->getValue();        }
    QString polarity(void)   const { return ppolarity->getValue();   }
    QString modulation(void) const { return pmodulation->getValue(); }

  protected:
    ScanFrequency  *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion  *pinversion;
    ScanFec        *pfec;
    ScanPolarity   *ppolarity;
    ScanModulation *pmodulation;
};

#endif // _PANE_DVBS2_H_
