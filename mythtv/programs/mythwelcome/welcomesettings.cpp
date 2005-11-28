#include <mythcontext.h>

#include <unistd.h>

#include "welcomesettings.h"

///////////////////////////////////////////////////////////////////
//  daily wakeup/shutdown settings
///////////////////////////////////////////////////////////////////

static GlobalTimeBox *DailyWakeupStart1()
{
    GlobalTimeBox *gc = new GlobalTimeBox("DailyWakeupStartPeriod1", "00:00");
    gc->setLabel(QObject::tr("Period") + " 1 " +  QObject::tr("start time"));
    gc->setHelpText(QObject::tr("Period") + " 1 " + QObject::tr("start time") +
            ". " + QObject::tr("Defines a period the master backend should be awake") +
            ". " + QObject::tr("Set both Start & End times to 00:00 "
                                       "to disable."));
    return gc;
};

static GlobalTimeBox *DailyWakeupEnd1()
{
    GlobalTimeBox *gc = new GlobalTimeBox("DailyWakeupEndPeriod1", "00:00");
    gc->setLabel(QObject::tr("Period") + " 1 " +  QObject::tr("end time"));
    gc->setHelpText(QObject::tr("Period") + " 1 " + QObject::tr("end time") +
            ". " + QObject::tr("Defines a period the master backend should be awake") +
            ". " + QObject::tr("Set both Start & End times to 00:00 "
                                       "to disable."));
    return gc;
};

static GlobalTimeBox *DailyWakeupStart2()
{
    GlobalTimeBox *gc = new GlobalTimeBox("DailyWakeupStartPeriod2", "00:00");
    gc->setLabel(QObject::tr("Period") + " 2 " +  QObject::tr("start time"));
    gc->setHelpText(QObject::tr("Period") + " 2 " + QObject::tr("start time") +
            ". " + QObject::tr("Defines a period the master backend should be awake") +
            ". " + QObject::tr("Set both Start & End times to 00:00 "
                                       "to disable."));
    return gc;
};

static GlobalTimeBox *DailyWakeupEnd2()
{
    GlobalTimeBox *gc = new GlobalTimeBox("DailyWakeupEndPeriod2", "00:00");
    gc->setLabel(QObject::tr("Period") + " 2 " +  QObject::tr("end time"));
    gc->setHelpText(QObject::tr("Period") + " 2 " + QObject::tr("end time") +
            ". " + QObject::tr("Defines a period the master backend should be awake") +
            ". " + QObject::tr("Set both Start & End times to 00:00 "
                                       "to disable."));
    return gc;
};

static HostCheckBox *AutoStartFrontend()
{
    HostCheckBox *gc = new HostCheckBox("AutoStartFrontend");
    gc->setLabel(QObject::tr("Automatically Start Myth Frontend"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Mythwelcome will automatically start the "
                    "mythfrontend if it is determined that it was not started to "
                    "record a program."));
    return gc;
};

static HostCheckBox *ShutdownWithBE()
{
    HostCheckBox *gc = new HostCheckBox("ShutdownWithMasterBE");
    gc->setLabel(QObject::tr("Shutdown with Master Backend"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Mythwelcome will automatically shutdown "
                    "this computer when the master backend shuts down. "
                    "Should only be set on frontend only machines"));
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

    vcg->setLabel(QObject::tr("MythWelcome Settings"));

    hcg1->setLabel(QObject::tr("Daily Wakeup/ShutDown Period") + " 1");
    hcg1->addChild(DailyWakeupStart1());
    hcg1->addChild(DailyWakeupEnd1());
    vcg->addChild(hcg1);

    hcg2->setLabel(QObject::tr("Daily Wakeup/ShutDown Period") + " 2");
    hcg2->addChild(DailyWakeupStart2());
    hcg2->addChild(DailyWakeupEnd2());
    vcg->addChild(hcg2);

    vcg->addChild(AutoStartFrontend());

    // this setting only makes sense on frontend only machines
    if (gContext->IsFrontendOnly())
    {
        vcg->addChild(ShutdownWithBE());
    }

    addChild(vcg);
}

///////////////////////////////////////////////////////////////////
//  mythshutdown script settings
///////////////////////////////////////////////////////////////////

static HostLineEdit *MythShutdownNvramCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownNvramCmd");
    gc->setLabel(QObject::tr("nvram-wakeup Command"));
    gc->setValue("/usr/bin/nvram-wakeup");
    gc->setHelpText(QObject::tr("Command to set the wakeup time in the BIOS. "
                    "See the README file for more examples."));
    return gc;
};

static HostLineEdit *MythShutdownNvramRestartCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownNvramRestartCmd");
    gc->setLabel(QObject::tr("nvram-wakeup Restart Command"));
    gc->setValue("/sbin/grub-set-default 1");
    gc->setHelpText(QObject::tr("Command to run if your bios requires you to "
                                "reboot to allow nvram-wakeup settings to "
                                "take effect. " 
                                "See the README file for more examples."));
    return gc;
};

static HostLineEdit *MythShutdownReboot()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownReboot");
    gc->setLabel(QObject::tr("Command to reboot"));
    gc->setValue("/sbin/reboot");
    gc->setHelpText(QObject::tr("Command to reboot computer.")); 
    return gc;
};

static HostLineEdit *MythShutdownPowerOff()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownPowerOff");
    gc->setLabel(QObject::tr("Command to shutdown"));
    gc->setValue("/sbin/poweroff");
    gc->setHelpText(QObject::tr("Command to shutdown computer.")); 
    return gc;
};

static HostLineEdit *MythShutdownStartFECmd()
{
    HostLineEdit *gc = new HostLineEdit("MythWelcomeStartFECmd");
    gc->setLabel(QObject::tr("Command to run to start the Frontend"));
    gc->setValue(gContext->GetInstallPrefix() + "/bin/mythfrontend");
    gc->setHelpText(QObject::tr("Command to start mythfrontend.")); 
    return gc;
};

static HostLineEdit *MythShutdownXTermCmd()
{
    HostLineEdit *gc = new HostLineEdit("MythShutdownXTermCmd");
    gc->setLabel(QObject::tr("Command to run Xterm"));
    gc->setValue("xterm");
    gc->setHelpText(QObject::tr("Command to start an Xterm. Can be disabled by " 
                                "leaving this setting blank.")); 
    return gc;
};


MythShutdownSettings::MythShutdownSettings()
{
    VerticalConfigurationGroup* vcg = 
              new VerticalConfigurationGroup(false);
    
    vcg->setLabel(QObject::tr("MythShutdown/MythWelcome Settings"));
    vcg->addChild(MythShutdownNvramCmd());
    vcg->addChild(MythShutdownNvramRestartCmd());
    vcg->addChild(MythShutdownReboot());
    vcg->addChild(MythShutdownPowerOff());
    vcg->addChild(MythShutdownXTermCmd());
    vcg->addChild(MythShutdownStartFECmd());

    addChild(vcg);
}
