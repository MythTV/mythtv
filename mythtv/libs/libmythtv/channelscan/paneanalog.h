/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _PANE_ANALOG_H_
#define _PANE_ANALOG_H_

#include "settings.h"

class TransFreqTableSelector;

class PaneAnalog : public VerticalConfigurationGroup
{
  public:
    PaneAnalog();

    void SetSourceID(uint sourceid);

    QString GetFrequencyTable(void) const;

  protected:
    TransFreqTableSelector  *freq_table;
};

#endif // _PANE_ANALOG_H_
