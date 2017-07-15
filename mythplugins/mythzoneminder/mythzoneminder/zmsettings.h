/*
    zoneminder settings.
*/

#ifndef ZMSETTINGS_H
#define ZMSETTINGS_H

#include <QCoreApplication>

#include <standardsettings.h>

class ZMSettings : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(ZMSettings);

  public:
    ZMSettings();
};

#endif
