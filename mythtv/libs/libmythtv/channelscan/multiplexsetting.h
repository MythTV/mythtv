/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _MULTIPLEX_SETTING_H_
#define _MULTIPLEX_SETTING_H_

// MythTV headers
#include "standardsettings.h"

class MultiplexSetting : public TransMythUIComboBoxSetting
{
    Q_OBJECT

  public:
    MultiplexSetting() : sourceid(0)
        { setLabel(tr("Transport")); }

    void Load(void) override; // StandardSetting

    void SetSourceID(uint _sourceid);

  protected:
    uint sourceid;
};

#endif // _MULTIPLEX_SETTING_H_
