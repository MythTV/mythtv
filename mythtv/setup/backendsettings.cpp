#include <cstdio>

#include "backendsettings.h"
#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"
#include <unistd.h>


static HostLineEdit *LocalServerIP()
{
    HostLineEdit *gc = new HostLineEdit("BackendServerIP");
    gc->setLabel(QObject::tr("IP address for") + QString(" ") +
                 gContext->GetHostName());
    gc->setValue("127.0.0.1");
    gc->setHelpText(QObject::tr("Enter the IP address of this machine.  "
                    "Use an externally accessible address (ie, not "
                    "127.0.0.1) if you are going to be running a frontend "
                    "on a different machine than this one."));
    return gc;
};

static HostLineEdit *LocalServerPort()
{
    HostLineEdit *gc = new HostLineEdit("BackendServerPort");
    gc->setLabel(QObject::tr("Port the server runs on"));
    gc->setValue("6543");
    gc->setHelpText(QObject::tr("Unless you've got good reason to, don't "
                    "change this."));
    return gc;
};

static HostLineEdit *LocalStatusPort()
{
    HostLineEdit *gc = new HostLineEdit("BackendStatusPort");
    gc->setLabel(QObject::tr("Port the server shows status on"));
    gc->setValue("6544");
    gc->setHelpText(QObject::tr("Port which the server will listen to for "
                    "HTTP requests.  Currently, it shows a little status "
                    "information."));
    return gc;
};

static GlobalLineEdit *MasterServerIP()
{
    GlobalLineEdit *gc = new GlobalLineEdit("MasterServerIP");
    gc->setLabel(QObject::tr("Master Server IP address"));
    gc->setValue("127.0.0.1");
    gc->setHelpText(QObject::tr("The IP address of the master backend "
                    "server. All frontend and non-master backend machines "
                    "will connect to this server.  If you only have one "
                    "backend, this should be the same IP address as "
                    "above."));
    return gc;
};

static GlobalLineEdit *MasterServerPort()
{
    GlobalLineEdit *gc = new GlobalLineEdit("MasterServerPort");
    gc->setLabel(QObject::tr("Port the master server runs on"));
    gc->setValue("6543");
    gc->setHelpText(QObject::tr("Unless you've got good reason to, "
                    "don't change this."));
    return gc;
};

static HostLineEdit *RecordFilePrefix()
{
    HostLineEdit *gc = new HostLineEdit("RecordFilePrefix");
    gc->setLabel(QObject::tr("Directory to hold recordings"));
    gc->setValue("/mnt/store/");
    gc->setHelpText(QObject::tr("All recordings get stored in this "
                    "directory."));
    return gc;
};

static GlobalComboBox *TVFormat()
{
    GlobalComboBox *gc = new GlobalComboBox("TVFormat");
    gc->setLabel(QObject::tr("TV format"));
    gc->addSelection("NTSC");
    gc->addSelection("ATSC");
    gc->addSelection("PAL");
    gc->addSelection("SECAM");
    gc->addSelection("PAL-NC");
    gc->addSelection("PAL-M");
    gc->addSelection("PAL-N");
    gc->addSelection("PAL-BG");
    gc->addSelection("PAL-DK");
    gc->addSelection("PAL-I");
    gc->addSelection("PAL-60");
    gc->addSelection("NTSC-JP");
    gc->setHelpText(QObject::tr("The TV standard to use for viewing TV."));
    return gc;
};

static GlobalComboBox *VbiFormat()
{
    GlobalComboBox *gc = new GlobalComboBox("VbiFormat");
    gc->setLabel(QObject::tr("VBI format"));
    gc->addSelection("None");
    gc->addSelection("PAL Teletext");
    gc->addSelection("NTSC Closed Caption");
    gc->setHelpText(QObject::tr("VBI stands for Vertical Blanking Interrupt.  "
                    "VBI is used to carry Teletext and Closed Captioning "
                    "data."));
    return gc;
};

static GlobalComboBox *FreqTable()
{
    GlobalComboBox *gc = new GlobalComboBox("FreqTable");
    gc->setLabel(QObject::tr("Channel frequency table"));
    gc->addSelection("us-cable");
    gc->addSelection("us-bcast");
    gc->addSelection("us-cable-hrc");
    gc->addSelection("us-cable-irc");
    gc->addSelection("japan-bcast");
    gc->addSelection("japan-cable");
    gc->addSelection("europe-west");
    gc->addSelection("europe-east");
    gc->addSelection("italy");
    gc->addSelection("newzealand");
    gc->addSelection("australia");
    gc->addSelection("ireland");
    gc->addSelection("france");
    gc->addSelection("china-bcast");
    gc->addSelection("southafrica");
    gc->addSelection("argentina");
    gc->addSelection("australia-optus");
    gc->setHelpText(QObject::tr("Select the appropriate frequency table for "
                    "your system.  If you have an antenna, use a \"-bcast\" "
                    "frequency."));
    return gc;
};

static GlobalCheckBox *SaveTranscoding()
{
    GlobalCheckBox *gc = new GlobalCheckBox("SaveTranscoding");
    gc->setLabel(QObject::tr("Save original files after transcoding (globally)"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("When set and the transcoder is active, the "
                    "original files will be renamed to .old once the "
                    "transcoding is complete."));
    return gc;
};

static GlobalCheckBox *DeletesFollowLinks()
{
    GlobalCheckBox *gc = new GlobalCheckBox("DeletesFollowLinks");
    gc->setLabel(QObject::tr("Follow symbolic links when deleting files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("This will cause Myth to follow symlinks "
                    "when recordings and related files are deleted, instead "
                    "of deleting the symlink and leaving the actual file."));
    return gc;
};

static GlobalComboBox *TimeOffset()
{
    GlobalComboBox *gc = new GlobalComboBox("TimeOffset");
    gc->setLabel(QObject::tr("Time offset for XMLTV listings"));
    gc->addSelection("None");
    gc->addSelection("Auto");
    gc->addSelection("+0030");
    gc->addSelection("+0100");
    gc->addSelection("+0130");
    gc->addSelection("+0200");
    gc->addSelection("+0230");
    gc->addSelection("+0300");
    gc->addSelection("+0330");
    gc->addSelection("+0400");
    gc->addSelection("+0430");
    gc->addSelection("+0500");
    gc->addSelection("+0530");
    gc->addSelection("+0600");
    gc->addSelection("+0630");
    gc->addSelection("+0700");
    gc->addSelection("+0730");
    gc->addSelection("+0800");
    gc->addSelection("+0830");
    gc->addSelection("+0900");
    gc->addSelection("+0930");
    gc->addSelection("+1000");
    gc->addSelection("+1030");
    gc->addSelection("+1100");
    gc->addSelection("+1130");
    gc->addSelection("+1200");
    gc->addSelection("-1100");
    gc->addSelection("-1030");
    gc->addSelection("-1000");
    gc->addSelection("-0930");
    gc->addSelection("-0900");
    gc->addSelection("-0830");
    gc->addSelection("-0800");
    gc->addSelection("-0730");
    gc->addSelection("-0700");
    gc->addSelection("-0630");
    gc->addSelection("-0600");
    gc->addSelection("-0530");
    gc->addSelection("-0500");
    gc->addSelection("-0430");
    gc->addSelection("-0400");
    gc->addSelection("-0330");
    gc->addSelection("-0300");
    gc->addSelection("-0230");
    gc->addSelection("-0200");
    gc->addSelection("-0130");
    gc->addSelection("-0100");
    gc->addSelection("-0030");
    gc->setHelpText(QObject::tr("Adjust the relative timezone of the XMLTV EPG data read "
                    "by mythfilldatabase.  'Auto' converts the XMLTV time to local time "
                    "using your computer's timezone.  'None' ignores the "
                    "XMLTV timezone, interpreting times as local."));
    return gc;
};

static GlobalSpinBox *EITTransportTimeout()
{
    GlobalSpinBox *gc = new GlobalSpinBox("EITTransportTimeout", 1, 15, 1);
    gc->setLabel(QObject::tr("EIT Transport Timeout (mins)"));
    gc->setValue(5);
    QString helpText = QObject::tr(
        "Maximum time to spend waiting for listings data on one DTV channel "
        "before checking for new listings data on the next channel.");
    gc->setHelpText(helpText);
    return gc;
}

static GlobalCheckBox *MasterBackendOverride()
{
    GlobalCheckBox *gc = new GlobalCheckBox("MasterBackendOverride");
    gc->setLabel(QObject::tr("Master Backend Override"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the master backend will stream and"
                    " delete files if it finds them in the video directory. "
                    "Useful if you are using a central storage location, like "
                    "a NFS share, and your slave backend isn't running."));
    return gc;
};

static GlobalCheckBox *EITIgnoresSource()
{
    GlobalCheckBox *gc = new GlobalCheckBox("EITIgnoresSource");
    gc->setLabel(QObject::tr("Cross Source EIT"));
    gc->setValue(false);
    QString help = QObject::tr(
        "If enabled, listings data collected on one Video Source will be "
        "applied to the first matching DVB channel on any Video Source. "
        "This is sometimes useful for DVB-S, but may insert bogus "
        "data into any ATSC listings stored in the same database.");
    gc->setHelpText(help);
    return gc;
};

static GlobalSpinBox *EITCrawIdleStart()
{
    GlobalSpinBox *gc = new GlobalSpinBox("EITCrawIdleStart", 30, 7200, 30);
    gc->setLabel(QObject::tr("Backend Idle Before EIT Craw (seconds)"));
    gc->setValue(60);
    QString help = QObject::tr(
        "The minimum number of seconds after a recorder becomes idle "
        "to wait before MythTV begins collecting EIT listings data.");
    gc->setHelpText(help);
    return gc;
}

static GlobalSpinBox *WOLbackendReconnectWaitTime()
{
    GlobalSpinBox *gc = new GlobalSpinBox("WOLbackendReconnectWaitTime", 0, 1200, 5);
    gc->setLabel(QObject::tr("Reconnect wait time (secs)"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("Length of time the frontend waits between the "
                    "tries to wake up the master backend. This should be the "
                    "time your masterbackend needs to startup. Set 0 to "
                    "disable."));
    return gc;
};

static GlobalSpinBox *WOLbackendConnectRetry()
{
    GlobalSpinBox *gc = new GlobalSpinBox("WOLbackendConnectRetry", 1, 60, 1);
    gc->setLabel(QObject::tr("Count of reconnect tries"));
    gc->setHelpText(QObject::tr("Number of times the frontend will try to wake "
                    "up the master backend."));
    gc->setValue(5);
    return gc;
};

static GlobalLineEdit *WOLbackendCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("WOLbackendCommand");
    gc->setLabel(QObject::tr("Wake Command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to wake up your master "
                    "backend server."));
    return gc;
};

static GlobalLineEdit *WOLslaveBackendsCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("WOLslaveBackendsCommand");
    gc->setLabel(QObject::tr("Wake command for slaves"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to wakeup your slave "
                    "backends. Leave empty to disable."));
    return gc;
};

static GlobalSpinBox *idleTimeoutSecs()
{
    GlobalSpinBox *gc = new GlobalSpinBox("idleTimeoutSecs", 0, 1200, 5);
    gc->setLabel(QObject::tr("Idle timeout (secs)"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("The amount of time the master backend idles "
                    "before it shuts down all backends. Set to 0 to disable "
                    "auto shutdown."));
    return gc;
};
    
static GlobalSpinBox *idleWaitForRecordingTime()
{
    GlobalSpinBox *gc = new GlobalSpinBox("idleWaitForRecordingTime", 0, 120, 1);
    gc->setLabel(QObject::tr("Max. wait for recording (min)"));
    gc->setValue(15);
    gc->setHelpText(QObject::tr("The amount of time the master backend waits "
                    "for a recording.  If it's idle but a recording starts "
                    "within this time period, the backends won't shut down."));
    return gc;
};

static GlobalSpinBox *StartupSecsBeforeRecording()
{
    GlobalSpinBox *gc = new GlobalSpinBox("StartupSecsBeforeRecording", 0, 1200, 5);
    gc->setLabel(QObject::tr("Startup before rec. (secs)"));
    gc->setValue(120);
    gc->setHelpText(QObject::tr("The amount of time the master backend will be "
                    "woken up before a recording starts."));
    return gc;
};

static GlobalLineEdit *WakeupTimeFormat()
{
    GlobalLineEdit *gc = new GlobalLineEdit("WakeupTimeFormat");
    gc->setLabel(QObject::tr("Wakeup time format"));
    gc->setValue("hh:mm yyyy-MM-dd");
    gc->setHelpText(QObject::tr("The format of the time string passed to the "
                    "\'setWakeuptime Command\' as $time. See "
                    "QT::QDateTime.toString() for details. Set to 'time_t' for "
                    "seconds since epoch."));
    return gc;
};

static GlobalLineEdit *SetWakeuptimeCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("SetWakeuptimeCommand");
    gc->setLabel(QObject::tr("Set wakeuptime command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to set the time (passed as "
                    "$time) to wake up the masterbackend"));
    return gc;
};

static GlobalLineEdit *ServerHaltCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("ServerHaltCommand");
    gc->setLabel(QObject::tr("Server halt command"));
    gc->setValue("sudo /sbin/halt -p");
    gc->setHelpText(QObject::tr("The command used to halt the backends."));
    return gc;
};

static GlobalLineEdit *preSDWUCheckCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("preSDWUCheckCommand");
    gc->setLabel(QObject::tr("Pre Shutdown check-command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("A command executed before the backend would "
                    "shutdown. The return value determines if "
                    "the backend can shutdown. 0 - yes, "
                    "1 - restart idleing, "
                    "2 - reset the backend to wait for a frontend."));
    return gc;
};

static GlobalCheckBox *blockSDWUwithoutClient()
{
    GlobalCheckBox *gc = new GlobalCheckBox("blockSDWUwithoutClient");
    gc->setLabel(QObject::tr("Block shutdown before client connected"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, the automatic shutdown routine will "
                    "be disabled until a client connects."));
    return gc;
};

static GlobalLineEdit *startupCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("startupCommand");
    gc->setLabel(QObject::tr("Startup command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("This command is executed right after starting "
                    "the BE. As a parameter \'$status\' is replaced by either "
                    "\'auto\' if the machine was started automatically or "
                    "\'user\' if a user switched it on."));
    return gc;
};

static HostSpinBox *JobQueueMaxSimultaneousJobs()
{
    HostSpinBox *gc = new HostSpinBox("JobQueueMaxSimultaneousJobs", 1, 10, 1);
    gc->setLabel(QObject::tr("Maximum simultaneous jobs on this backend"));
    gc->setHelpText(QObject::tr("The Job Queue will be limited to running "
                    "this many simultaneous jobs on this backend."));
    gc->setValue(1);
    return gc;
};

static HostSpinBox *JobQueueCheckFrequency()
{
    HostSpinBox *gc = new HostSpinBox("JobQueueCheckFrequency", 5, 300, 5);
    gc->setLabel(QObject::tr("Job Queue Check frequency (in seconds)"));
    gc->setHelpText(QObject::tr("When looking for new jobs to process, the "
                    "Job Queue will wait this long between checks."));
    gc->setValue(60);
    return gc;
};

static HostComboBox *JobQueueCPU()
{
    HostComboBox *gc = new HostComboBox("JobQueueCPU");
    gc->setLabel(QObject::tr("CPU Usage"));
    gc->addSelection(QObject::tr("Low"), "0");
    gc->addSelection(QObject::tr("Medium"), "1");
    gc->addSelection(QObject::tr("High"), "2");
    gc->setHelpText(QObject::tr("This setting controls approximately how "
                    "much CPU jobs in the queue may consume. "
                    "On 'High', all available CPU time may be used "
                    "which could cause problems on slower systems." ));
    return gc;
};

static HostTimeBox *JobQueueWindowStart()
{
    HostTimeBox *gc = new HostTimeBox("JobQueueWindowStart", "00:00");
    gc->setLabel(QObject::tr("Job Queue Start Time"));
    gc->setHelpText(QObject::tr("This setting controls the start of the "
                    "Job Queue time window which determines when new jobs will "
                    "be started."));
    return gc;
};

static HostTimeBox *JobQueueWindowEnd()
{
    HostTimeBox *gc = new HostTimeBox("JobQueueWindowEnd", "23:59");
    gc->setLabel(QObject::tr("Job Queue End Time"));
    gc->setHelpText(QObject::tr("This setting controls the end of the "
                    "Job Queue time window which determines when new jobs will "
                    "be started."));
    return gc;
};

static GlobalCheckBox *JobsRunOnRecordHost()
{
    GlobalCheckBox *gc = new GlobalCheckBox("JobsRunOnRecordHost");
    gc->setLabel(QObject::tr("Run Jobs only on original recording host"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, jobs in the queue will be required "
                                "to run on the backend that made the "
                                "original recording."));
    return gc;
};

static GlobalCheckBox *AutoTranscodeBeforeAutoCommflag()
{
    GlobalCheckBox *gc = new GlobalCheckBox("AutoTranscodeBeforeAutoCommflag");
    gc->setLabel(QObject::tr("Run Transcode Jobs before Auto-Commercial Flagging"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, if both auto-transcode and "
                                "auto commercial flagging are turned ON for a "
                                "recording, transcoding will run first, "
                                "otherwise, commercial flagging runs first."));
    return gc;
};

static GlobalCheckBox *AutoCommflagWhileRecording()
{
    GlobalCheckBox *gc = new GlobalCheckBox("AutoCommflagWhileRecording");
    gc->setLabel(QObject::tr("Start Auto-Commercial Flagging jobs when the "
                             "recording starts"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set and Auto Commercial Flagging is ON for "
                                "a recording, the flagging job will be started "
                                "as soon as the recording starts.  NOT "
                                "recommended on underpowered systems."));
    return gc;
};

static GlobalLineEdit *UserJobDesc1()
{
    GlobalLineEdit *gc = new GlobalLineEdit("UserJobDesc1");
    gc->setLabel(QObject::tr("User Job #1 Description"));
    gc->setValue("User Job #1");
    gc->setHelpText(QObject::tr("The Description for this User Job."));
    return gc;
};

static GlobalLineEdit *UserJob1()
{
    GlobalLineEdit *gc = new GlobalLineEdit("UserJob1");
    gc->setLabel(QObject::tr("User Job #1 Command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    return gc;
};

static GlobalLineEdit *UserJobDesc2()
{
    GlobalLineEdit *gc = new GlobalLineEdit("UserJobDesc2");
    gc->setLabel(QObject::tr("User Job #2 Description"));
    gc->setValue("User Job #2");
    gc->setHelpText(QObject::tr("The Description for this User Job."));
    return gc;
};

static GlobalLineEdit *UserJob2()
{
    GlobalLineEdit *gc = new GlobalLineEdit("UserJob2");
    gc->setLabel(QObject::tr("User Job #2 Command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    return gc;
};

static GlobalLineEdit *UserJobDesc3()
{
    GlobalLineEdit *gc = new GlobalLineEdit("UserJobDesc3");
    gc->setLabel(QObject::tr("User Job #3 Description"));
    gc->setValue("User Job #3");
    gc->setHelpText(QObject::tr("The Description for this User Job."));
    return gc;
};

static GlobalLineEdit *UserJob3()
{
    GlobalLineEdit *gc = new GlobalLineEdit("UserJob3");
    gc->setLabel(QObject::tr("User Job #3 Command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    return gc;
};

static GlobalLineEdit *UserJobDesc4()
{
    GlobalLineEdit *gc = new GlobalLineEdit("UserJobDesc4");
    gc->setLabel(QObject::tr("User Job #4 Description"));
    gc->setValue("User Job #4");
    gc->setHelpText(QObject::tr("The Description for this User Job."));
    return gc;
};

static GlobalLineEdit *UserJob4()
{
    GlobalLineEdit *gc = new GlobalLineEdit("UserJob4");
    gc->setLabel(QObject::tr("User Job #4 Command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    return gc;
};

static HostCheckBox *JobAllowCommFlag()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowCommFlag");
    gc->setLabel(QObject::tr("Allow Commercial Detection jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    return gc;
};

static HostCheckBox *JobAllowTranscode()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowTranscode");
    gc->setLabel(QObject::tr("Allow Transcoding jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    return gc;
};

static GlobalLineEdit *JobQueueTranscodeCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("JobQueueTranscodeCommand");
    gc->setLabel(QObject::tr("Transcoder command"));
    gc->setValue("mythtranscode");
    gc->setHelpText(QObject::tr("The program used to transcode recordings. "
                    "The default is 'mythtranscode' if this setting is empty."));
    return gc;
};

static GlobalLineEdit *JobQueueCommFlagCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("JobQueueCommFlagCommand");
    gc->setLabel(QObject::tr("Commercial Flagger command"));
    gc->setValue("mythcommflag");
    gc->setHelpText(QObject::tr("The program used to detect commercials in a "
                    "recording.  The default is 'mythcommflag' "
                    "if this setting is empty."));
    return gc;
};

static HostCheckBox *JobAllowUserJob1()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowUserJob1");
    gc->setLabel(QObject::tr("Allow 'User Job #1' jobs"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    return gc;
};

static HostCheckBox *JobAllowUserJob2()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowUserJob2");
    gc->setLabel(QObject::tr("Allow 'User Job #2' jobs"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    return gc;
};

static HostCheckBox *JobAllowUserJob3()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowUserJob3");
    gc->setLabel(QObject::tr("Allow 'User Job #3' jobs"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    return gc;
};

static HostCheckBox *JobAllowUserJob4()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowUserJob4");
    gc->setLabel(QObject::tr("Allow 'User Job #4' jobs"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    return gc;
};


BackendSettings::BackendSettings() {
    VerticalConfigurationGroup* server = new VerticalConfigurationGroup(false);
    server->setLabel(QObject::tr("Host Address Backend Setup"));
    server->addChild(LocalServerIP());
    server->addChild(LocalServerPort());
    server->addChild(LocalStatusPort());
    server->addChild(MasterServerIP());
    server->addChild(MasterServerPort());
    addChild(server);

    VerticalConfigurationGroup* group1 = new VerticalConfigurationGroup(false);
    group1->setLabel(QObject::tr("Host-specific Backend Setup"));
    group1->addChild(RecordFilePrefix());
    addChild(group1);

    VerticalConfigurationGroup* group2 = new VerticalConfigurationGroup(false);
    group2->setLabel(QObject::tr("Global Backend Setup"));
    group2->addChild(TVFormat());
    group2->addChild(VbiFormat());
    group2->addChild(FreqTable());
    group2->addChild(TimeOffset());
    group2->addChild(MasterBackendOverride());
    group2->addChild(DeletesFollowLinks());
    addChild(group2);

    VerticalConfigurationGroup* group2a1 = new VerticalConfigurationGroup(false);
    group2a1->setLabel(QObject::tr("EIT Scanner Options"));                
    group2a1->addChild(EITTransportTimeout());
    group2a1->addChild(EITIgnoresSource());
    group2a1->addChild(EITCrawIdleStart());
    addChild(group2a1);

    VerticalConfigurationGroup* group3 = new VerticalConfigurationGroup(false);
    group3->setLabel(QObject::tr("Shutdown/Wakeup Options"));
    group3->addChild(startupCommand());
    group3->addChild(blockSDWUwithoutClient());
    group3->addChild(idleTimeoutSecs());
    group3->addChild(idleWaitForRecordingTime());
    group3->addChild(StartupSecsBeforeRecording());
    group3->addChild(WakeupTimeFormat());
    group3->addChild(SetWakeuptimeCommand());
    group3->addChild(ServerHaltCommand());
    group3->addChild(preSDWUCheckCommand());
    addChild(group3);    

    VerticalConfigurationGroup* group4 = new VerticalConfigurationGroup(false);
    group4->setLabel(QObject::tr("WakeOnLan settings"));

    VerticalConfigurationGroup* backend = new VerticalConfigurationGroup();
    backend->setLabel(QObject::tr("MasterBackend"));
    backend->addChild(WOLbackendReconnectWaitTime());
    backend->addChild(WOLbackendConnectRetry());
    backend->addChild(WOLbackendCommand());
    group4->addChild(backend);
    
    group4->addChild(WOLslaveBackendsCommand());
    addChild(group4);

    VerticalConfigurationGroup* group5 = new VerticalConfigurationGroup(false);
    group5->setLabel(QObject::tr("Job Queue (Host-Specific)"));
    group5->addChild(JobQueueMaxSimultaneousJobs());
    group5->addChild(JobQueueCheckFrequency());

    HorizontalConfigurationGroup* group5a =
              new HorizontalConfigurationGroup(false, false);
    VerticalConfigurationGroup* group5a1 =
              new VerticalConfigurationGroup(false, false);
    group5a1->addChild(JobQueueWindowStart());
    group5a1->addChild(JobQueueWindowEnd());
    group5a1->addChild(JobQueueCPU());
    group5a1->addChild(JobAllowCommFlag());
    group5a1->addChild(JobAllowTranscode());
    group5a->addChild(group5a1);

    VerticalConfigurationGroup* group5a2 =
            new VerticalConfigurationGroup(false, false);
    group5a2->addChild(JobAllowUserJob1());
    group5a2->addChild(JobAllowUserJob2());
    group5a2->addChild(JobAllowUserJob3());
    group5a2->addChild(JobAllowUserJob4());
    group5a->addChild(group5a2);
    group5->addChild(group5a);
    addChild(group5);    

    VerticalConfigurationGroup* group6 = new VerticalConfigurationGroup(false);
    group6->setLabel(QObject::tr("Job Queue (Global)"));
    group6->addChild(JobsRunOnRecordHost());
    group6->addChild(AutoCommflagWhileRecording());
    group6->addChild(JobQueueCommFlagCommand());
    group6->addChild(JobQueueTranscodeCommand());
    group6->addChild(AutoTranscodeBeforeAutoCommflag());
    group6->addChild(SaveTranscoding());
    addChild(group6);    

    VerticalConfigurationGroup* group7 = new VerticalConfigurationGroup(false);
    group7->setLabel(QObject::tr("Job Queue (Job Commands)"));
    group7->addChild(UserJobDesc1());
    group7->addChild(UserJob1());
    group7->addChild(UserJobDesc2());
    group7->addChild(UserJob2());
    group7->addChild(UserJobDesc3());
    group7->addChild(UserJob3());
    group7->addChild(UserJobDesc4());
    group7->addChild(UserJob4());
    addChild(group7);    
}
