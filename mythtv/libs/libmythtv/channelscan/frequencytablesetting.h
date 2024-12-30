// -*- Mode: c++ -*-
// vim: set expandtab tabstop=4 shiftwidth=4:

#ifndef FREQUENCY_TABLE_SETTING_H
#define FREQUENCY_TABLE_SETTING_H

// MythTV headers
#include "libmythui/standardsettings.h"

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

#endif // FREQUENCY_TABLE_SETTING_H
