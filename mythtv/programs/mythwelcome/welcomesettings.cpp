#include <unistd.h>

#include "welcomesettings.h"
#include "mythcorecontext.h"
#include "mythdirs.h"

///////////////////////////////////////////////////////////////////
//  daily wakeup/shutdown settings
///////////////////////////////////////////////////////////////////

static GlobalTimeBox *DailyWakeupStart1()
{
    GlobalTimeBox *gc = new GlobalTimeBox("DailyWakeupStartPeriod1", "00:00");
    gc->setLabel(MythWelcomeSettings::tr("Period %1 start time").arg("1"));
    gc->setHelpText(MythWelcomeSettings::tr("Period %1 start time. "
        "Defines a period the master backend should be awake. " 
        "Set both Start & End times to 00:00 to disable.").arg("1"));
    return gc;
};

static GlobalTimeBox *DailyWakeupEnd1()
{
    GlobalTimeBox *gc = new GlobalTimeBox("DailyWakeupEndPeriod1", "00:00");
    gc->setLabel(MythWelcomeSettings::tr("Period %1 end time").arg("1"));
    gc->setHelpText(MythWelcomeSettings::tr("Period %1 end time. "
        "Defines a period the master backend should be awake. "
        "Set both Start & End times to 00:00 to disable.").arg("1"));
    return gc;
};

static GlobalTimeBox *DailyWakeupStart2()
{
    GlobalTimeBox *gc = new GlobalTimeBox("DailyWakeupStartPeriod2", "00:00");
    gc->setLabel(MythWelcomeSettings::tr("Period %1 start time").arg("2"));
    gc->setHelpText(MythWelcomeSettings::tr("Period %1 start time. "
        "Defines a period the master backend should be awake. "
        "Set both Start & End times to 00:00 to disable.").arg("2"));
    return gc;
};

static GlobalTimeBox *DailyWakeupEnd2()
{
    GlobalTimeBox *gc = new GlobalTimeBox("DailyWakeupEndPeriod2", "00:00");
    gc->setLabel(MythWelcomeSettings::tr("Period %1 end time").arg("2"));
    gc->setHelpText(MythWelcomeSettings::tr("Period %1 end time. "
        "Defines a period the master backend should be awake. "
        "Set both Start & End times to 00:00 to disable.").arg("2"));
    return gc;
};

static HostCheckBox *AutoStartFrontend()
{
    HostCheckBox *gc = new HostCheckBox("AutoStartFrontend");
    gc->setLabel(MythWelcomeSettings::tr("Automatically Start mythfrontend"));
    gc->setValue(true);
    gc->setHelpText(MythWelcomeSettings::tr("Mythwelcome will automatically "
                        "start mythfrontend if it is determined that it was "
                        "not started to record a program."));
    return gc;
};

static HostCheckBox *ShutdownWithBE()
{
    HostCheckBox *gc = new HostCheckBox("ShutdownWithMasterBE");
    gc->setLabel(MythWelcomeSettings::tr("Shutdown with Master Backend"));
    gc->setValue(false);
    gc->setHelpText(MythWelcomeSettings::tr("Mythwelcome will automatically "
                        "shutdown this computer when the master backend shuts "
                        "down. Should only be set on frontend only machines"));
    return gc;
};

static HostLineEdit *MythWelcomeDateFormat()
{
    HostLineEdit *gc = new HostLineEdit("MythWelcomeDateFormat");
    gc->setLabel(MythWelcomeSettings::tr("Date Format"));
    gc->setValue("dddd\ndd MMM yyyy");
    gc->setHelpText(MythWelcomeSettings::tr("This is the format to use to "
                        "display the date. "
                        "See http://doc.trolltech.com/3.3/qdate.html#toString "
                        "for a list of valid format specifiers."));
    return gc;
};

MythWelcomeSettings::MythWelcomeSettings()
{
    HorizontalConfigurationGroup* hcg1 =
              new HorizontalConfigurationGroup(true, true);
    HorizontalConfigurationGroup* hcg2 =
              new HorizontalConfigurationGroup(true, true);
    VerticalConfigurationGroup* vcg = 
              new VerticalConfigurationGroup(false);

    vcg->setLabel(tr("MythWelcome Settings"));

    hcg1->setLabel(tr("Daily Wakeup/ShutDown Period %1").arg("1"));
    hcg1->addChild(DailyWakeupStart1());
    hcg1->addChild(DailyWakeupEnd1());
    vcg->addChild(hcg1);

    hcg2->setLabel(tr("Daily Wakeup/ShutDown Period %1").arg("2"));
    hcg2->addChild(DailyWakeupStart2());
    hcg2->addChild(DailyWakeupEnd2());
    vcg->addChild(hcg2);

    vcg->addChild(AutoStartFrontend());

    // this setting only makes sense on frontend only machines
    if (gCoreContext->IsFrontendOnly())
    {
        vcg->addChild(ShutdownWithBE());
    }

    vcg->addChild(MythWelcomeDateFormat());

    addChild(vcg);
}

///////////////////////////////////////////////////////////////////
//  mythshutdown script settings
///////////////////////////////////////////////////////////////////

static HostLineEdit *MythShutdownNvramCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownNvramCmd");
    gc->setLabel(MythShutdownSettings::tr("Command to Set Wakeup Time"));
    gc->setValue("/usr/bin/nvram-wakeup --settime $time");
    gc->setHelpText(MythShutdownSettings::tr("Command to set the wakeup time "
                        "in the BIOS. See the README file for more examples."));
    return gc;
};

static HostComboBox *WakeupTimeFormat()
{
    HostComboBox *gc = new HostComboBox("MythShutdownWakeupTimeFmt", true);
    gc->setLabel(MythShutdownSettings::tr("Wakeup time format"));
    gc->addSelection("time_t");
    gc->addSelection("yyyy-MM-dd hh:mm:ss");
    gc->setHelpText(MythShutdownSettings::tr("The format of the time string "
                        "passed to the \'Set Wakeup Time Command\' as $time. "
                        "See QT::QDateTime.toString() for details. Set to "
                        "'time_t' for seconds since epoch (use time_t for "
                        "nvram_wakeup)."));
    return gc;
};

static HostLineEdit *MythShutdownNvramRestartCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownNvramRestartCmd");
    gc->setLabel(MythShutdownSettings::tr("nvram-wakeup Restart Command"));
    gc->setValue("/sbin/grub-set-default 1");
    gc->setHelpText(MythShutdownSettings::tr("Command to run if your bios "
                        "requires you to reboot to allow nvram-wakeup settings "
                        "to take effect. Leave blank if your bios does not "
                        "require a reboot. See the README file for more "
                        "examples."));
    return gc;
};

static HostLineEdit *MythShutdownReboot()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownReboot");
    gc->setLabel(MythShutdownSettings::tr("Command to reboot"));
    gc->setValue("/sbin/reboot");
    gc->setHelpText(MythShutdownSettings::tr("Command to reboot computer.")); 
    return gc;
};

static HostLineEdit *MythShutdownPowerOff()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownPowerOff");
    gc->setLabel(MythShutdownSettings::tr("Command to shutdown"));
    gc->setValue("/sbin/poweroff");
    gc->setHelpText(MythShutdownSettings::tr("Command to shutdown computer."));
    return gc;
};

static HostLineEdit *MythShutdownStartFECmd()
{
    HostLineEdit *gc = new HostLineEdit("MythWelcomeStartFECmd");
    gc->setLabel(MythShutdownSettings::tr("Command to run to start the "
                                          "Frontend"));
    gc->setValue(GetAppBinDir() + "mythfrontend");
    gc->setHelpText(MythShutdownSettings::tr("Command to start mythfrontend."));
    return gc;
};

static HostLineEdit *MythShutdownXTermCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownXTermCmd");
    gc->setLabel(MythShutdownSettings::tr("Command to run Xterm"));
    gc->setValue("xterm");
    gc->setHelpText(MythShutdownSettings::tr("Command to start an Xterm. Can "
                                             "be disabled by leaving this "
                                             "setting blank.")); 
    return gc;
};


MythShutdownSettings::MythShutdownSettings()
{
    VerticalConfigurationGroup* vcg = 
              new VerticalConfigurationGroup(false);
    
    vcg->setLabel(tr("MythShutdown/MythWelcome Settings"));
    vcg->addChild(MythShutdownNvramCmd());
    vcg->addChild(WakeupTimeFormat());
    vcg->addChild(MythShutdownNvramRestartCmd());
    vcg->addChild(MythShutdownReboot());
    vcg->addChild(MythShutdownPowerOff());
    vcg->addChild(MythShutdownXTermCmd());
    vcg->addChild(MythShutdownStartFECmd());

    addChild(vcg);
}
