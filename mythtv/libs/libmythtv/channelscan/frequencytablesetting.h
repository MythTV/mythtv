// -*- Mode: c++ -*-
// vim: set expandtab tabstop=4 shiftwidth=4:

#ifndef _FREQUENCY_TABLE_SETTING_H_
#define _FREQUENCY_TABLE_SETTING_H_

// MythTV headers
#include "settings.h"

class ScanFrequencyTable: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanFrequencyTable();
};

class ScanCountry : public ComboBoxSetting, public TransientStorage
{
  public:
    ScanCountry();
};

class ScanNetwork : public ComboBoxSetting, public TransientStorage
{
  public:
    ScanNetwork();
};

#endif // _FREQUENCY_TABLE_SETTING_H_
