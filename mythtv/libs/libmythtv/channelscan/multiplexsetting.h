/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef MULTIPLEX_SETTING_H
#define MULTIPLEX_SETTING_H

// MythTV headers
#include "libmythui/standardsettings.h"

class MultiplexSetting : public TransMythUIComboBoxSetting
{
    Q_OBJECT

  public:
    MultiplexSetting() { setLabel(tr("Transport")); }

    void Load(void) override; // StandardSetting

    void SetSourceID(uint sourceid);

  protected:
    uint m_sourceid {0};
};

#endif // MULTIPLEX_SETTING_H
