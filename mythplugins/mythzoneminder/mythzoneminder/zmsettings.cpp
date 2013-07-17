/*
    zomeminder settings.cpp
*/

#include <unistd.h>

// myth
#include <mythcontext.h>
#include <mythdate.h>

// mythzoneminder
#include "zmsettings.h"


static HostLineEdit *ZMServerIP()
{
    HostLineEdit *gc = new HostLineEdit("ZoneMinderServerIP");
    gc->setLabel(ZMSettings::tr("IP address of the MythZoneMinder server"));
    gc->setValue("127.0.0.1");
    gc->setHelpText(ZMSettings::tr("Enter the IP address of the MythZoneMinder "
                                   "server that this frontend should connect "
                                   "to."));
    return gc;
};

static HostLineEdit *ZMServerPort()
{
    HostLineEdit *gc = new HostLineEdit("ZoneMinderServerPort");
    gc->setLabel(ZMSettings::tr("Port the server runs on"));
    gc->setValue("6548");
    gc->setHelpText(ZMSettings::tr("Unless you've got good reason to, don't "
                                   "change this."));
    return gc;
};

static HostComboBox *ZMDateFormat()
{
    HostComboBox *gc = new HostComboBox("ZoneMinderDateFormat");
    gc->setLabel(ZMSettings::tr("Date format"));

    QDate sampdate = MythDate::current().toLocalTime().date();
    QString sampleStr = ZMSettings::tr("Samples are shown using today's date.");

    if (sampdate.month() == sampdate.day())
    {
        sampdate = sampdate.addDays(1);
        sampleStr = ZMSettings::tr("Samples are shown using tomorrow's date.");
    }

    gc->addSelection(sampdate.toString("ddd - dd/MM"), "ddd - dd/MM");
    gc->addSelection(sampdate.toString("ddd MMM d"), "ddd MMM d");
    gc->addSelection(sampdate.toString("ddd MMMM d"), "ddd MMMM d");
    gc->addSelection(sampdate.toString("MMM d"), "MMM d");
    gc->addSelection(sampdate.toString("MM/dd"), "MM/dd");
    gc->addSelection(sampdate.toString("MM.dd"), "MM.dd");
    gc->addSelection(sampdate.toString("ddd d MMM"), "ddd d MMM");
    gc->addSelection(sampdate.toString("M/d/yyyy"), "M/d/yyyy");
    gc->addSelection(sampdate.toString("dd.MM.yyyy"), "dd.MM.yyyy");
    gc->addSelection(sampdate.toString("yyyy-MM-dd"), "yyyy-MM-dd");
    gc->addSelection(sampdate.toString("ddd MMM d yyyy"), "ddd MMM d yyyy"); 
    gc->addSelection(sampdate.toString("ddd yyyy-MM-dd"), "ddd yyyy-MM-dd");
    gc->addSelection(sampdate.toString("ddd dd MMM yyyy"), "ddd dd MMM yyyy");

    //: %1 gives additional info on the date used
    gc->setHelpText(ZMSettings::tr("Your preferred date format to use on the "
                                   "events screens. %1") 
                                   .arg(sampleStr));
    return gc;
}

static HostComboBox *ZMTimeFormat()
{
    HostComboBox *gc = new HostComboBox("ZoneMinderTimeFormat");
    gc->setLabel(ZMSettings::tr("Time format"));

    QTime samptime = QTime::currentTime();

    gc->addSelection(samptime.toString("hh:mm AP"), "hh:mm AP");
    gc->addSelection(samptime.toString("hh:mm"), "hh:mm");
    gc->addSelection(samptime.toString("hh:mm:ss"), "hh:mm:ss");

    gc->setHelpText(ZMSettings::tr("Your preferred time format to display "
                                   "on the events screens."));
    return gc;
}

ZMSettings::ZMSettings()
{
    VerticalConfigurationGroup* vcg1 = new VerticalConfigurationGroup(false);
    vcg1->setLabel(tr("MythZoneMinder Settings"));
    vcg1->addChild(ZMServerIP());
    vcg1->addChild(ZMServerPort());
    vcg1->addChild(ZMDateFormat());
    vcg1->addChild(ZMTimeFormat());
    addChild(vcg1);
}
