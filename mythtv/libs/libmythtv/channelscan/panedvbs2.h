/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef PANE_DVBS2_H
#define PANE_DVBS2_H

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
                                      m_transponder = new ScanTransponder(),
                                      m_pfrequency  = new ScanFrequencykHz(),
                                      m_ppolarity   = new ScanPolarity(),
                                      m_psymbolrate = new ScanDVBSSymbolRate(),
                                      m_pmodulation = new ScanDVBSModulation(),
                                      m_pmodsys     = new ScanDVBSModSys(),
                                      m_pfec        = new ScanFec(),
                                      m_pinversion  = new ScanInversion(),
                                      m_prolloff    = new ScanRollOff()});

        // Update default tuning parameters when reference transponder is selected
        connect(m_transponder, qOverload<StandardSetting *>(&StandardSetting::valueChanged),
                this, &PaneDVBS2::SetTuningParameters);
    }

    QString frequency(void)  const { return m_pfrequency->getValue();  }
    QString symbolrate(void) const { return m_psymbolrate->getValue(); }
    QString modulation(void) const { return m_pmodulation->getValue(); }
    QString modsys(void)     const { return m_pmodsys->getValue();     }
    QString inversion(void)  const { return m_pinversion->getValue();  }
    QString fec(void)        const { return m_pfec->getValue();        }
    QString polarity(void)   const { return m_ppolarity->getValue();   }
    QString rolloff(void)    const { return m_prolloff->getValue();    }

    void setFrequency(uint frequency)             { m_pfrequency->setValue(frequency);  }
    void setSymbolrate(const QString& symbolrate) { m_psymbolrate->setValue(symbolrate);}
    void setModulation(const QString& modulation) { m_pmodulation->setValue(modulation);}
    void setModSys(const QString& modsys)         { m_pmodsys->setValue(modsys);        }
    void setInversion(const QString& inversion)   { m_pinversion->setValue(inversion);  }
    void setFec(const QString& fec)               { m_pfec->setValue(fec);              }
    void setPolarity(const QString& polarity)     { m_ppolarity->setValue(polarity);    }
    void setRolloff(const QString& rolloff)       { m_prolloff->setValue(rolloff);      }

  public slots:
    void SetTuningParameters(StandardSetting *setting);

  protected:
    ScanTransponder    *m_transponder {nullptr};
    ScanFrequencykHz   *m_pfrequency  {nullptr};
    ScanDVBSSymbolRate *m_psymbolrate {nullptr};
    ScanDVBSModulation *m_pmodulation {nullptr};
    ScanDVBSModSys     *m_pmodsys     {nullptr};
    ScanFec            *m_pfec        {nullptr};
    ScanPolarity       *m_ppolarity   {nullptr};
    ScanInversion      *m_pinversion  {nullptr};
    ScanRollOff        *m_prolloff    {nullptr};
};

#endif // PANE_DVBS2_H
