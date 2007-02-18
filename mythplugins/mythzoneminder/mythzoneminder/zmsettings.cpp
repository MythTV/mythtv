/*
	zomeminder settings.cpp
*/

#include <unistd.h>

// myth
#include <mythtv/mythcontext.h>

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

static HostCheckBox *ZMServerUseOpenGL()
{
    HostCheckBox *gc = new HostCheckBox("ZoneMinderUseOpenGL");
    gc->setLabel(QObject::tr("Use OpenGL"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If checked OpenGL will be used to render the video frames "
                               "otherwise Xv will be used."));
    return gc;
};

ZMSettings::ZMSettings()
{
    VerticalConfigurationGroup* vcg1 = new VerticalConfigurationGroup(false);
    vcg1->setLabel(QObject::tr("MythZoneMinder Settings"));
    vcg1->addChild(ZMServerIP());
    vcg1->addChild(ZMServerPort());
    vcg1->addChild(ZMServerUseOpenGL());
    addChild(vcg1);
}
