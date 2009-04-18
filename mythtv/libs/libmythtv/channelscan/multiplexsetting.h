/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _MULTIPLEX_SETTING_H_
#define _MULTIPLEX_SETTING_H_

// MythTV headers
#include "settings.h"

class MultiplexSetting : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT

  public:
    MultiplexSetting() : ComboBoxSetting(this), sourceid(0)
        { setLabel(tr("Transport")); }

    virtual void Load(void);

    void SetSourceID(uint _sourceid);

  protected:
    uint sourceid;
};

#endif // _MULTIPLEX_SETTING_H_
