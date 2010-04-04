/*
	zomeminder settings.cpp
*/

#include <unistd.h>

// myth
#include <mythcontext.h>

// mythzoneminder
#include "zmsettings.h"


static HostLineEdit *ZMServerIP()
{
    HostLineEdit *gc = new HostLineEdit("ZoneMinderServerIP");
    gc->setLabel(QObject::tr("IP address of the mythzoneminder server"));
    gc->setValue("127.0.0.1");
    gc->setHelpText(QObject::tr("Enter the IP address of the mythzoneminder server "
            "that this frontend should connect to."));
    return gc;
};

static HostLineEdit *ZMServerPort()
{
    HostLineEdit *gc = new HostLineEdit("ZoneMinderServerPort");
    gc->setLabel(QObject::tr("Port the server runs on"));
    gc->setValue("6548");
    gc->setHelpText(QObject::tr("Unless you've got good reason to, don't "
            "change this."));
    return gc;
};

ZMSettings::ZMSettings()
{
    VerticalConfigurationGroup* vcg1 = new VerticalConfigurationGroup(false);
    vcg1->setLabel(QObject::tr("MythZoneMinder Settings"));
    vcg1->addChild(ZMServerIP());
    vcg1->addChild(ZMServerPort());
    addChild(vcg1);
}
