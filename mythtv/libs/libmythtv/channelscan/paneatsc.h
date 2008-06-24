/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _PANE_ATSC_H_
#define _PANE_ATSC_H_

// MythTV headers
#include "channelscanmiscsettings.h"
#include "frequencytablesetting.h"
#include "modulationsetting.h"

class PaneATSC : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    PaneATSC() :
        VerticalConfigurationGroup(false, false, true, false),
        atsc_table(new ScanFrequencyTable()),
        atsc_modulation(new ScanATSCModulation())
    {
        addChild(atsc_table);
        addChild(atsc_modulation);

        connect(atsc_table, SIGNAL(valueChanged(    const QString&)),
                this,       SLOT(  FreqTableChanged(const QString&)));
    }

    QString GetFrequencyTable(void) const
        { return atsc_table->getValue(); }
    QString GetModulation(void) const
        { return atsc_modulation->getValue(); }

  protected slots:
    void FreqTableChanged(const QString &freqtbl)
    {
        if (freqtbl == "us")
            atsc_modulation->setValue(0);
        else if (atsc_modulation->getValue() == "vsb8")
            atsc_modulation->setValue(1);
    }

  protected:
    ScanFrequencyTable      *atsc_table;
    ScanATSCModulation      *atsc_modulation;
    QString                  old_freq_table;
};

#endif // _PANE_ATSC_H_
