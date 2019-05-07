/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _PANE_ANALOG_H_
#define _PANE_ANALOG_H_

#include "standardsettings.h"

class TransFreqTableSelector;

class PaneAnalog : public GroupSetting
{
  public:
    PaneAnalog(const QString &target, StandardSetting *setting);

    void SetSourceID(uint sourceid);

    QString GetFrequencyTable(void) const;

  protected:
    TransFreqTableSelector  *m_freq_table {nullptr};
};

#endif // _PANE_ANALOG_H_
