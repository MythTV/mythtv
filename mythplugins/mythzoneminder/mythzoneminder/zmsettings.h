/*
    zoneminder settings.
*/

#ifndef ZMSETTINGS_H
#define ZMSETTINGS_H

#include <QCoreApplication>

#include <settings.h>

class ZMSettings : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(ZMSettings)

  public:
    ZMSettings();
};

#endif
