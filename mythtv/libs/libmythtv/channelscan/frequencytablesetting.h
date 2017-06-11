// -*- Mode: c++ -*-
// vim: set expandtab tabstop=4 shiftwidth=4:

#ifndef _FREQUENCY_TABLE_SETTING_H_
#define _FREQUENCY_TABLE_SETTING_H_

// MythTV headers
#include "standardsettings.h"

class ScanFrequencyTable: public TransMythUIComboBoxSetting
{
  public:
    ScanFrequencyTable();
};

class ScanCountry : public TransMythUIComboBoxSetting
{
  public:
    ScanCountry();
};

class ScanNetwork : public TransMythUIComboBoxSetting
{
  public:
    ScanNetwork();
};

#endif // _FREQUENCY_TABLE_SETTING_H_
