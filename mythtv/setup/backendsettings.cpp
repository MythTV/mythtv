#include "backendsettings.h"
#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"

class LocalServerIP: public GlobalLineEdit {
public:
    LocalServerIP():
        GlobalLineEdit("BackendServerIP") {
            QString hostname = gContext->GetHostName();
            QString label = QObject::tr("IP address for") + QString(" ") + 
                            gContext->GetHostName();
            setLabel(label);
            setValue("127.0.0.1");
            setHelpText(QObject::tr("Enter the IP address of this machine.  "
                        "Use an externally accessible address (ie, not "
                        "127.0.0.1) if you are going to be running a frontend "
                        "on a different machine than this one."));
    };
};

class LocalServerPort: public GlobalLineEdit {
public:
    LocalServerPort():
        GlobalLineEdit("BackendServerPort") {
            setLabel(QObject::tr("Port the server runs on"));
            setValue("6543");
            setHelpText(QObject::tr("Unless you've got good reason to, don't "
                        "change this."));
    };
};

class LocalStatusPort: public GlobalLineEdit {
public:
    LocalStatusPort():
        GlobalLineEdit("BackendStatusPort") {
            setLabel(QObject::tr("Port the server shows status on"));
            setValue("6544");
            setHelpText(QObject::tr("Port which the server will listen to for "
                        "HTTP requests.  Currently, it shows a little status "
                        "information."));
    };
};

class MasterServerIP: public BackendLineEdit {
public:
    MasterServerIP():
        BackendLineEdit("MasterServerIP") {
            setLabel(QObject::tr("Master Server IP address"));
            setValue("127.0.0.1");
            setHelpText(QObject::tr("The IP address of the master backend "
                        "server. All frontend and non-master backend machines "
                        "will connect to this server.  If you only have one "
                        "backend, this should be the same IP address as "
                        "above."));
    };
};

class MasterServerPort: public BackendLineEdit {
public:
    MasterServerPort():
        BackendLineEdit("MasterServerPort") {
            setLabel(QObject::tr("Port the master server runs on"));
            setValue("6543");
            setHelpText(QObject::tr("Unless you've got good reason to, "
                        "don't change this."));
    };
};

class RecordFilePrefix: public GlobalLineEdit {
public:
    RecordFilePrefix():
        GlobalLineEdit("RecordFilePrefix") {
        setLabel(QObject::tr("Directory to hold recordings"));
        setValue("/mnt/store/");
        setHelpText(QObject::tr("All recordings get stored in this "
                    "directory."));
    };
};

class LiveBufferPrefix: public GlobalLineEdit {
public:
    LiveBufferPrefix():
        GlobalLineEdit("LiveBufferDir") {
        setLabel(QObject::tr("Directory to hold the Live-TV buffers"));
        setValue("/mnt/store/");
        setHelpText(QObject::tr("All Live-TV buffers will get stored in this "
                    "directory. These buffers are used to allow you to pause, "
                    "fast forward and rewind through live TV."));
    };
};

class TVFormat: public BackendComboBox {
public:
    TVFormat():
        BackendComboBox("TVFormat") {
        setLabel(QObject::tr("TV format"));
        addSelection("NTSC");
        addSelection("ATSC");
        addSelection("PAL");
        addSelection("SECAM");
        addSelection("PAL-NC");
        addSelection("PAL-M");
        addSelection("PAL-N");
        addSelection("NTSC-JP");
        setHelpText(QObject::tr("The TV standard to use for viewing TV."));
    };
};

class VbiFormat: public BackendComboBox {
public:
    VbiFormat():
        BackendComboBox("VbiFormat") {
        setLabel(QObject::tr("VBI format"));
        addSelection("None");
        addSelection("PAL Teletext");
        addSelection("NTSC Closed Caption");
        setHelpText(QObject::tr("VBI stands for Vertical Blanking Interrupt.  "
                    "VBI is used to carry Teletext and Closed Captioning "
                    "data."));
    };
};

class FreqTable: public BackendComboBox {
public:
    FreqTable():
        BackendComboBox("FreqTable") {
        setLabel(QObject::tr("Channel frequency table"));
        addSelection("us-cable");
        addSelection("us-bcast");
        addSelection("us-cable-hrc");
        addSelection("japan-bcast");
        addSelection("japan-cable");
        addSelection("europe-west");
        addSelection("europe-east");
        addSelection("italy");
        addSelection("newzealand");
        addSelection("australia");
        addSelection("ireland");
        addSelection("france");
        addSelection("china-bcast");
        addSelection("southafrica");
        addSelection("argentina");
        addSelection("australia-optus");
        setHelpText(QObject::tr("Select the appropriate frequency table for "
                    "your system.  If you have an antenna, use a \"-bcast\" "
                    "frequency."));
    };
};

class BufferSize: public GlobalSlider {
public:
    BufferSize():
        GlobalSlider("BufferSize", 1, 100, 1)  {
        setLabel(QObject::tr("Live TV buffer (GB)"));
        setValue(5);
        setHelpText(QObject::tr("How large the live TV buffer is allowed to "
                    "grow."));
    };
};

class MaxBufferFill: public GlobalSlider {
public:
    MaxBufferFill():
        GlobalSlider("MaxBufferFill", 1, 100, 1) {
        setLabel(QObject::tr("Minimum free Live TV buffer (MB)"));
        setValue(50);
        setHelpText(QObject::tr("How full the live TV buffer is allowed to "
                    "grow before forcing an unpause."));
    };
};

class SaveTranscoding: public BackendCheckBox {
public:
    SaveTranscoding():
        BackendCheckBox("SaveTranscoding") {
        setLabel(QObject::tr("Save original files after transcoding"));
        setValue(false);
        setHelpText(QObject::tr("When set and the transcoder is active, the "
                    "original nuv files will be renamed to nuv.old once the "
                    "transcoding is complete."));
    };
};

class DeletesFollowLinks: public BackendCheckBox {
public:
    DeletesFollowLinks():
        BackendCheckBox("DeletesFollowLinks") {
        setLabel(QObject::tr("Follow symbolic links when deleting files"));
        setValue(false);
        setHelpText(QObject::tr("This will cause Myth to follow symlinks "
                    "when recordings and related files are deleted, instead "
                    "of deleting the symlink and leaving the actual file."));
    };
};

class TimeOffset: public BackendComboBox {
public:
    TimeOffset():
        BackendComboBox("TimeOffset") {
        setLabel(QObject::tr("Time offset for XMLTV listings"));
        addSelection("None");
        addSelection("Auto");
        addSelection("+0030");
        addSelection("+0100");
        addSelection("+0130");
        addSelection("+0200");
        addSelection("+0230");
        addSelection("+0300");
        addSelection("+0330");
        addSelection("+0400");
        addSelection("+0430");
        addSelection("+0500");
        addSelection("+0530");
        addSelection("+0600");
        addSelection("+0630");
        addSelection("+0700");
        addSelection("+0730");
        addSelection("+0800");
        addSelection("+0830");
        addSelection("+0900");
        addSelection("+0930");
        addSelection("+1000");
        addSelection("+1030");
        addSelection("+1100");
        addSelection("+1130");
        addSelection("+1200");
        addSelection("-1100");
        addSelection("-1030");
        addSelection("-1000");
        addSelection("-0930");
        addSelection("-0900");
        addSelection("-0830");
        addSelection("-0800");
        addSelection("-0730");
        addSelection("-0700");
        addSelection("-0630");
        addSelection("-0600");
        addSelection("-0530");
        addSelection("-0500");
        addSelection("-0430");
        addSelection("-0400");
        addSelection("-0330");
        addSelection("-0300");
        addSelection("-0230");
        addSelection("-0200");
        addSelection("-0130");
        addSelection("-0100");
        addSelection("-0030");
        setHelpText(QObject::tr("If your local timezone does not match the "
                    "timezone returned by XMLTV, use this setting to have "
                    "mythfilldatabase adjust the program start and end times."
                    "None disables this feature, Auto automatically "
                    "detects your local timezone"));
    };
};

class MasterBackendOverride: public BackendCheckBox {
public:
    MasterBackendOverride():
        BackendCheckBox("MasterBackendOverride") {
        setLabel(QObject::tr("Master Backend Override"));
        setValue(true);
        setHelpText(QObject::tr("If enabled, the master backend will stream and"
                    " delete files if it finds them in the video directory. "
                    "Useful if you are using a central storage location, like "
                    "a NFS share, and your slave backend isn't running."));
    };
};

class WOLbackendReconnectWaitTime: public BackendSpinBox {
public:
    WOLbackendReconnectWaitTime():
        BackendSpinBox("WOLbackendReconnectWaitTime", 0, 1200, 5) {
        setLabel(QObject::tr("Reconnect wait time (secs)"));
        setValue(0);
        setHelpText(QObject::tr("Length of time the frontend waits between the "
                    "tries to wake up the master backend. This should be the "
                    "time your masterbackend needs to startup. Set 0 to "
                    "disable."));
    };
};

class WOLbackendConnectRetry: public BackendSpinBox {
public:
    WOLbackendConnectRetry():
        BackendSpinBox("WOLbackendConnectRetry", 1, 60, 1) {
        setLabel(QObject::tr("Count of reconnect tries"));
        setHelpText(QObject::tr("Number of times the frontend will try to wake "
                    "up the master backend."));
        setValue(5);
    };
};

class WOLbackendCommand: public BackendLineEdit {
public:
    WOLbackendCommand():
        BackendLineEdit("WOLbackendCommand") {
        setLabel(QObject::tr("Wake Command"));
        setValue("");
        setHelpText(QObject::tr("The command used to wake up your master "
                    "backend server."));
    };
};

class WOLslaveBackendsCommand: public BackendLineEdit {
public:
    WOLslaveBackendsCommand():
        BackendLineEdit("WOLslaveBackendsCommand") {
        setLabel(QObject::tr("Wake command for slaves"));
        setValue("");
        setHelpText(QObject::tr("The command used to wakeup your slave "
                    "backends. Leave empty to disable."));
    };
};

class idleTimeoutSecs: public BackendSpinBox {
public:
    idleTimeoutSecs():
        BackendSpinBox("idleTimeoutSecs", 0, 1200, 5) {
        setLabel(QObject::tr("Idle timeout (secs)"));
        setValue(0);
        setHelpText(QObject::tr("The amount of time the master backend idles "
                    "before it shuts down all backends. Set to 0 to disable "
                    "auto shutdown."));
    };
};
    
class idleWaitForRecordingTime: public BackendSpinBox {
public:
    idleWaitForRecordingTime():
        BackendSpinBox("idleWaitForRecordingTime", 0, 120, 1) {
        setLabel(QObject::tr("Max. wait for recording (min)"));
        setValue(15);
        setHelpText(QObject::tr("The amount of time the master backend waits "
                    "for a recording.  If it's idle but a recording starts "
                    "within this time period, the backends won't shut down."));
    };
};

class StartupSecsBeforeRecording: public BackendSpinBox {
public:
    StartupSecsBeforeRecording():
        BackendSpinBox("StartupSecsBeforeRecording", 0, 1200, 5) {
        setLabel(QObject::tr("Startup before rec. (secs)"));
        setValue(120);
        setHelpText(QObject::tr("The amount of time the master backend will be "
                    "woken up before a recording starts."));
    };
};

class WakeupTimeFormat: public BackendLineEdit {
public:
    WakeupTimeFormat():
        BackendLineEdit("WakeupTimeFormat") {
        setLabel(QObject::tr("Wakeup time format"));
        setValue("hh:mm yyyy-MM-dd");
        setHelpText(QObject::tr("The format of the time string passed to the "
                    "\'setWakeuptime Command\' as $time. See "
                    "QT::QDateTime.toString() for details. Set to 'time_t' for "
                    "seconds since epoch."));
    };
};

class SetWakeuptimeCommand: public BackendLineEdit {
public:
    SetWakeuptimeCommand():
        BackendLineEdit("SetWakeuptimeCommand") {
        setLabel(QObject::tr("Set wakeuptime command"));
        setValue("");
        setHelpText(QObject::tr("The command used to set the time (passed as "
                    "$time) to wake up the masterbackend"));
    };
};

class ServerHaltCommand: public BackendLineEdit {
public:
    ServerHaltCommand():
        BackendLineEdit("ServerHaltCommand") {
        setLabel(QObject::tr("Server halt command"));
        setValue("sudo /sbin/halt -p");
        setHelpText(QObject::tr("The command used to halt the backends."));
    };
};

class preSDWUCheckCommand: public BackendLineEdit {
public:
    preSDWUCheckCommand():
        BackendLineEdit("preSDWUCheckCommand") {
        setLabel(QObject::tr("Pre Shutdown check-command"));
        setValue("");
        setHelpText(QObject::tr("A command executed before the backend would "
                    "shutdown. The return value determines if "
                    "the backend can shutdown. 0 - yes, "
                    "1 - restart idleing, "
                    "2 - reset the backend to wait for frontend"));
    };
};

class blockSDWUwithoutClient: public BackendCheckBox {
public:
    blockSDWUwithoutClient():
        BackendCheckBox("blockSDWUwithoutClient") {
        setLabel(QObject::tr("Block shutdown before client connected"));
        setValue(true);
        setHelpText(QObject::tr("If set, the automatic shutdown routine will "
                    "be disabled until a client connects."));
    };
};

class startupCommand: public BackendLineEdit {
public:
    startupCommand():
        BackendLineEdit("startupCommand") {
        setLabel(QObject::tr("Startup command"));
        setValue("");
        setHelpText(QObject::tr("This command is executed right after starting "
                    "the BE. As a parameter \'$status\' is replaced by either "
                    "\'auto\' if the machine was started automatically or "
                    "\'user\' if a user switched it on."));
    };
};

class JobQueueMaxSimultaneousJobs: public GlobalSpinBox {
public:
    JobQueueMaxSimultaneousJobs():
        GlobalSpinBox("JobQueueMaxSimultaneousJobs", 1, 5, 1) {
        setLabel(QObject::tr("Maximum simultaneous jobs on this backend"));
        setHelpText(QObject::tr("The Job Queue will be limited to running "
                    "this many simultaneous jobs on this backend."));
        setValue(1);
    };
};

class JobQueueCheckFrequency: public GlobalSpinBox {
public:
    JobQueueCheckFrequency():
        GlobalSpinBox("JobQueueCheckFrequency", 10, 300, 5) {
        setLabel(QObject::tr("Job Queue Check frequency (in seconds)"));
        setHelpText(QObject::tr("When looking for new jobs to process, the "
                    "Job Queue will wait this long between checks."));
        setValue(60);
    };
};

class JobQueueCPU: public GlobalComboBox {
public:
    JobQueueCPU():
        GlobalComboBox("JobQueueCPU") {

        setLabel(QObject::tr("CPU Usage"));
        addSelection(QObject::tr("Low"), "0");
        addSelection(QObject::tr("Medium"), "1");
        addSelection(QObject::tr("High"), "2");
        setHelpText(QObject::tr("This setting controls approximately how "
                    "much CPU jobs in the queue may consume. "
                    "On 'High', all available CPU time may be used "
                    "which could cause problems on slower systems." ));
    };
};

class JobsRunOnRecordHost: public BackendCheckBox {
public:
    JobsRunOnRecordHost():
        BackendCheckBox("JobsRunOnRecordHost") {
        setLabel(QObject::tr("Run Jobs only on original recording host"));
        setValue(false);
        setHelpText(QObject::tr("If set, jobs in the queue will be required "
                                "to run on the backend that made the "
                                "original recording."));
    };
};

class UserJobDesc1: public BackendLineEdit {
public:
    UserJobDesc1():
        BackendLineEdit("UserJobDesc1") {
        setLabel(QObject::tr("User Job #1 Description"));
        setValue("User Job #1");
        setHelpText(QObject::tr("The Description for this User Job."));
    };
};

class UserJob1: public BackendLineEdit {
public:
    UserJob1():
        BackendLineEdit("UserJob1") {
        setLabel(QObject::tr("User Job #1 Command"));
        setValue("");
        setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    };
};

class UserJobDesc2: public BackendLineEdit {
public:
    UserJobDesc2():
        BackendLineEdit("UserJobDesc2") {
        setLabel(QObject::tr("User Job #2 Description"));
        setValue("User Job #2");
        setHelpText(QObject::tr("The Description for this User Job."));
    };
};

class UserJob2: public BackendLineEdit {
public:
    UserJob2():
        BackendLineEdit("UserJob2") {
        setLabel(QObject::tr("User Job #2 Command"));
        setValue("");
        setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    };
};

class UserJobDesc3: public BackendLineEdit {
public:
    UserJobDesc3():
        BackendLineEdit("UserJobDesc3") {
        setLabel(QObject::tr("User Job #3 Description"));
        setValue("User Job #3");
        setHelpText(QObject::tr("The Description for this User Job."));
    };
};

class UserJob3: public BackendLineEdit {
public:
    UserJob3():
        BackendLineEdit("UserJob3") {
        setLabel(QObject::tr("User Job #3 Command"));
        setValue("");
        setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    };
};

class UserJobDesc4: public BackendLineEdit {
public:
    UserJobDesc4():
        BackendLineEdit("UserJobDesc4") {
        setLabel(QObject::tr("User Job #4 Description"));
        setValue("User Job #4");
        setHelpText(QObject::tr("The Description for this User Job."));
    };
};

class UserJob4: public BackendLineEdit {
public:
    UserJob4():
        BackendLineEdit("UserJob4") {
        setLabel(QObject::tr("User Job #4 Command"));
        setValue("");
        setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    };
};

class JobAllowTranscode: public GlobalCheckBox {
public:
    JobAllowTranscode():
        GlobalCheckBox("JobAllowTranscode") {
        setLabel(QObject::tr("Allow Transcoding jobs"));
        setValue(true);
        setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    };
};

class JobAllowCommFlag: public GlobalCheckBox {
public:
    JobAllowCommFlag():
        GlobalCheckBox("JobAllowCommFlag") {
        setLabel(QObject::tr("Allow Commercial Detection jobs"));
        setValue(true);
        setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    };
};

class JobAllowUserJob1: public GlobalCheckBox {
public:
    JobAllowUserJob1():
        GlobalCheckBox("JobAllowUserJob1") {
        setLabel(QObject::tr("Allow 'User Job #1' jobs"));
        setValue(false);
        setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    };
};

class JobAllowUserJob2: public GlobalCheckBox {
public:
    JobAllowUserJob2():
        GlobalCheckBox("JobAllowUserJob2") {
        setLabel(QObject::tr("Allow 'User Job #2' jobs"));
        setValue(false);
        setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    };
};

class JobAllowUserJob3: public GlobalCheckBox {
public:
    JobAllowUserJob3():
        GlobalCheckBox("JobAllowUserJob3") {
        setLabel(QObject::tr("Allow 'User Job #3' jobs"));
        setValue(false);
        setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    };
};

class JobAllowUserJob4: public GlobalCheckBox {
public:
    JobAllowUserJob4():
        GlobalCheckBox("JobAllowUserJob4") {
        setLabel(QObject::tr("Allow 'User Job #4' jobs"));
        setValue(false);
        setHelpText(QObject::tr("Allow jobs of this type to run on this "
                                "backend."));
    };
};


BackendSettings::BackendSettings() {
    VerticalConfigurationGroup* server = new VerticalConfigurationGroup(false);
    server->setLabel(QObject::tr("Host Address Backend Setup"));
    server->addChild(new LocalServerIP());
    server->addChild(new LocalServerPort());
    server->addChild(new LocalStatusPort());
    server->addChild(new MasterServerIP());
    server->addChild(new MasterServerPort());
    addChild(server);

    VerticalConfigurationGroup* group1 = new VerticalConfigurationGroup(false);
    group1->setLabel(QObject::tr("Host-specific Backend Setup"));
    group1->addChild(new RecordFilePrefix());
    group1->addChild(new LiveBufferPrefix());
    group1->addChild(new BufferSize());
    group1->addChild(new MaxBufferFill());
    group1->addChild(new SaveTranscoding());
    addChild(group1);

    VerticalConfigurationGroup* group2 = new VerticalConfigurationGroup(false);
    group2->setLabel(QObject::tr("Global Backend Setup"));
    group2->addChild(new TVFormat());
    group2->addChild(new VbiFormat());
    group2->addChild(new FreqTable());
    group2->addChild(new TimeOffset());
    group2->addChild(new MasterBackendOverride());
    group2->addChild(new DeletesFollowLinks());
    addChild(group2);

    VerticalConfigurationGroup* group3 = new VerticalConfigurationGroup(false);
    group3->setLabel(QObject::tr("Shutdown/Wakeup Options"));
    group3->addChild(new startupCommand());
    group3->addChild(new blockSDWUwithoutClient());
    group3->addChild(new idleTimeoutSecs());
    group3->addChild(new idleWaitForRecordingTime());
    group3->addChild(new StartupSecsBeforeRecording());
    group3->addChild(new WakeupTimeFormat());
    group3->addChild(new SetWakeuptimeCommand());
    group3->addChild(new ServerHaltCommand());
    group3->addChild(new preSDWUCheckCommand());
    addChild(group3);    

    VerticalConfigurationGroup* group4 = new VerticalConfigurationGroup(false);
    group4->setLabel(QObject::tr("WakeOnLan settings"));

    VerticalConfigurationGroup* backend = new VerticalConfigurationGroup();
    backend->setLabel(QObject::tr("MasterBackend"));
    backend->addChild(new WOLbackendReconnectWaitTime());
    backend->addChild(new WOLbackendConnectRetry());
    backend->addChild(new WOLbackendCommand());
    group4->addChild(backend);
    
    group4->addChild(new WOLslaveBackendsCommand());
    addChild(group4);

    VerticalConfigurationGroup* group5 = new VerticalConfigurationGroup(false);
    group5->setLabel(QObject::tr("Job Queue (Host-Specific)"));
    group5->addChild(new JobQueueMaxSimultaneousJobs());
    group5->addChild(new JobsRunOnRecordHost());
    group5->addChild(new JobQueueCheckFrequency());

    HorizontalConfigurationGroup* group5a =
              new HorizontalConfigurationGroup(false, false);
    VerticalConfigurationGroup* group5a1 =
              new VerticalConfigurationGroup(false, false);
    group5a1->addChild(new JobQueueCPU());
//    group5a1->addChild(new JobAllowTranscode());
    group5a1->addChild(new JobAllowCommFlag());
    group5a1->addChild(new JobAllowUserJob1());
    group5a->addChild(group5a1);

    VerticalConfigurationGroup* group5a2 =
            new VerticalConfigurationGroup(false, false);
    group5a2->addChild(new JobAllowUserJob2());
    group5a2->addChild(new JobAllowUserJob3());
    group5a2->addChild(new JobAllowUserJob4());
    group5a->addChild(group5a2);
    group5->addChild(group5a);
    addChild(group5);    

    VerticalConfigurationGroup* group6 = new VerticalConfigurationGroup(false);
    group6->setLabel(QObject::tr("Job Queue (Global)"));
    group6->addChild(new UserJobDesc1());
    group6->addChild(new UserJob1());
    group6->addChild(new UserJobDesc2());
    group6->addChild(new UserJob2());
    group6->addChild(new UserJobDesc3());
    group6->addChild(new UserJob3());
    group6->addChild(new UserJobDesc4());
    group6->addChild(new UserJob4());
    addChild(group6);    
}

