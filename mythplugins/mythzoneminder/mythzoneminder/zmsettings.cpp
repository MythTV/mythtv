/*
	zomeminder settings.cpp
*/

#include <unistd.h>

// myth
#include <mythtv/mythcontext.h>

// mythzoneminder
#include "zmsettings.h"


static HostLineEdit *ZMConfigDir()
{
    HostLineEdit *gc = new HostLineEdit("ZoneMinderConfigDir");
    gc->setLabel(QObject::tr("ZoneMinder Config Directory"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("Location where ZoneMinder keeps "
            "its zm.conf configuration file."));
    return gc;
};

ZMSettings::ZMSettings()
{
    VerticalConfigurationGroup* vcg1 = new VerticalConfigurationGroup(false);
    vcg1->setLabel(QObject::tr("MythZoneMinder Settings"));
    vcg1->addChild(ZMConfigDir());
    addChild(vcg1);
}
