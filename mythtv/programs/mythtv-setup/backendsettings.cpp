#include <cstdio>

#include "backendsettings.h"
#include "frequencies.h"
#include "mythcorecontext.h"
#include "settings.h"
#include "channelsettings.h" // for ChannelTVFormat::GetFormats()
#include <unistd.h>

#include <QNetworkInterface>


static HostComboBox *LocalServerIP()
{
    HostComboBox *gc = new HostComboBox("BackendServerIP");
    gc->setLabel(QObject::tr("IPv4 address"));
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    QList<QHostAddress>::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        if ((*it).protocol() == QAbstractSocket::IPv4Protocol)
            gc->addSelection((*it).toString(), (*it).toString());
    }

    gc->setValue("127.0.0.1");
    gc->setHelpText(QObject::tr("Enter the IP address of this machine. "
                    "Use an externally accessible address (ie, not "
                    "127.0.0.1) if you are going to be running a frontend "
                    "on a different machine than this one. Note, in IPv6 "
                    "setups, this is still required for certain extras "
                    "such as UPnP."));
    return gc;
};

static HostComboBox *LocalServerIP6()
{
    HostComboBox *gc = new HostComboBox("BackendServerIP6");
    gc->setLabel(QObject::tr("IPv6 address"));
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    QList<QHostAddress>::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        if ((*it).protocol() == QAbstractSocket::IPv6Protocol)
            gc->addSelection((*it).toString(), (*it).toString());
    }

#if defined(QT_NO_IPV6)
    gc->setEnabled(false);
    gc->setValue("");
#else
    if (list.isEmpty())
    {
        gc->setEnabled(false);
        gc->setValue("");
    }
    else if (list.contains(QHostAddress("::1")))
        gc->setValue("::1");
#endif
        
    gc->setHelpText(QObject::tr("Enter the IPv6 address of this machine. "
                    "Use an externally accessible address (ie, not "
                    "::1) if you are going to be running a frontend "
                    "on a different machine than this one."));
    return gc;
}

static HostCheckBox *UseLinkLocal()
{
    HostCheckBox *hc = new HostCheckBox("AllowLinkLocal");
    hc->setLabel(QObject::tr("Listen on Link-Local addresses"));
    hc->setValue(true);
    hc->setHelpText(QObject::tr("Enable servers on this machine to listen on "
                    "link-local addresses. These are auto-configured "
                    "addresses and not accessible outside the local network. "
                    "This must be enabled for anything requiring Bonjour to "
                    "work."));
    return hc;
};

static HostLineEdit *LocalServerPort()
{
    HostLineEdit *gc = new HostLineEdit("BackendServerPort");
    gc->setLabel(QObject::tr("Port"));
    gc->setValue("6543");
    gc->setHelpText(QObject::tr("Unless you've got good reason, don't "
                    "change this."));
    return gc;
};

static HostLineEdit *LocalStatusPort()
{
    HostLineEdit *gc = new HostLineEdit("BackendStatusPort");
    gc->setLabel(QObject::tr("Status port"));
    gc->setValue("6544");
    gc->setHelpText(QObject::tr("Port on which the server will listen for "
                    "HTTP requests, including backend status and MythXML "
                    "requests."));
    return gc;
};

static GlobalLineEdit *MasterServerIP()
{
    GlobalLineEdit *gc = new GlobalLineEdit("MasterServerIP");
    gc->setLabel(QObject::tr("IP address"));
    gc->setValue("127.0.0.1");
    gc->setHelpText(QObject::tr("The IP address of the master backend "
                    "server. All frontend and non-master backend machines "
                    "will connect to this server. If you only have one "
                    "backend, this should be the same IP address as "
                    "above."));
    return gc;
};

static GlobalLineEdit *MasterServerPort()
{
    GlobalLineEdit *gc = new GlobalLineEdit("MasterServerPort");
    gc->setLabel(QObject::tr("Port"));
    gc->setValue("6543");
    gc->setHelpText(QObject::tr("Unless you've got good reason, "
                    "don't change this."));
    return gc;
};

static HostLineEdit *LocalSecurityPin()
{
    HostLineEdit *gc = new HostLineEdit("SecurityPin");
    gc->setLabel(QObject::tr("Security PIN (required)"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("PIN code required for a frontend to connect "
                                "to the backend. Blank prevents all "
                                "connections; 0000 allows any client to "
                                "connect."));
    return gc;
};

static GlobalComboBox *TVFormat()
{
    GlobalComboBox *gc = new GlobalComboBox("TVFormat");
    gc->setLabel(QObject::tr("TV format"));

    QStringList list = ChannelTVFormat::GetFormats();
    for (int i = 0; i < list.size(); i++)
        gc->addSelection(list[i]);

    gc->setHelpText(QObject::tr("The TV standard to use for viewing TV."));
    return gc;
};

static GlobalComboBox *VbiFormat()
{
    GlobalComboBox *gc = new GlobalComboBox("VbiFormat");
    gc->setLabel(QObject::tr("VBI format"));
    gc->addSelection("None");
    gc->addSelection("PAL teletext");
    gc->addSelection("NTSC closed caption");
    gc->setHelpText(QObject::tr("The VBI (Vertical Blanking Interval) is "
                    "used to carry Teletext or Closed Captioning "
                    "data."));
    return gc;
};

static GlobalComboBox *FreqTable()
{
    GlobalComboBox *gc = new GlobalComboBox("FreqTable");
    gc->setLabel(QObject::tr("Channel frequency table"));

    for (uint i = 0; chanlists[i].name; i++)
        gc->addSelection(chanlists[i].name);

    gc->setHelpText(QObject::tr("Select the appropriate frequency table for "
                    "your system. If you have an antenna, use a \"-bcast\" "
                    "frequency."));
    return gc;
};

static GlobalCheckBox *SaveTranscoding()
{
    GlobalCheckBox *gc = new GlobalCheckBox("SaveTranscoding");
    gc->setLabel(QObject::tr("Save original files after transcoding (globally)"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled and the transcoder is active, the "
                    "original files will be renamed to .old once the "
                    "transcoding is complete."));
    return gc;
};

static HostCheckBox *TruncateDeletes()
{
    HostCheckBox *hc = new HostCheckBox("TruncateDeletesSlowly");
    hc->setLabel(QObject::tr("Delete files slowly"));
    hc->setValue(false);
    hc->setHelpText(QObject::tr("Some filesystems use a lot of resources when "
                    "deleting large files. If enabled, this option makes "
                    "MythTV delete files slowly on this backend to lessen the "
                    "impact."));
    return hc;
};

static GlobalCheckBox *DeletesFollowLinks()
{
    GlobalCheckBox *gc = new GlobalCheckBox("DeletesFollowLinks");
    gc->setLabel(QObject::tr("Follow symbolic links when deleting files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, MythTV will follow symlinks "
                    "when recordings and related files are deleted, instead "
                    "of deleting the symlink and leaving the actual file."));
    return gc;
};

static GlobalSpinBox *HDRingbufferSize()
{
    GlobalSpinBox *bs = new GlobalSpinBox(
        "HDRingbufferSize", 25*188, 512*188, 25*188);
    bs->setLabel(QObject::tr("HD ringbuffer size (kB)"));
    bs->setHelpText(QObject::tr("The HD device ringbuffer allows the "
                    "backend to weather moments of stress. "
                    "The larger the ringbuffer (in kilobytes), the longer "
                    "the moments of stress can be. However, "
                    "setting the size too large can cause "
                    "swapping, which is detrimental."));
    bs->setValue(50*188);
    return bs;
}

static GlobalComboBox *StorageScheduler()
{
    GlobalComboBox *gc = new GlobalComboBox("StorageScheduler");
    gc->setLabel(QObject::tr("Storage Group disk scheduler"));
    gc->addSelection(QObject::tr("Balanced free space"), "BalancedFreeSpace");
    gc->addSelection(QObject::tr("Balanced percent free space"), "BalancedPercFreeSpace");
    gc->addSelection(QObject::tr("Balanced disk I/O"), "BalancedDiskIO");
    gc->addSelection(QObject::tr("Combination"), "Combination");
    gc->setValue("BalancedFreeSpace");
    gc->setHelpText(QObject::tr("This setting controls how the Storage Group "
                    "scheduling code will balance new recordings across "
                    "directories. 'Balanced Free Space' is the recommended "
                    "method for most users." ));
    return gc;
};

static GlobalCheckBox *DisableAutomaticBackup()
{
    GlobalCheckBox *gc = new GlobalCheckBox("DisableAutomaticBackup");
    gc->setLabel(QObject::tr("Disable automatic database backup"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, MythTV will not backup the "
                                "database before upgrades. You should "
                                "therefore have your own database backup "
                                "strategy in place."));
    return gc;
};

static HostCheckBox *DisableFirewireReset()
{
    HostCheckBox *hc = new HostCheckBox("DisableFirewireReset");
    hc->setLabel(QObject::tr("Disable FireWire reset"));
    hc->setHelpText(
        QObject::tr(
            "By default, MythTV resets the FireWire bus when a "
            "FireWire recorder stops responding to commands. If "
            "this causes problems, you can disable this behavior "
            "here."));
    hc->setValue(false);
    return hc;
}

static HostLineEdit *MiscStatusScript()
{
    HostLineEdit *he = new HostLineEdit("MiscStatusScript");
    he->setLabel(QObject::tr("Miscellaneous status application"));
    he->setValue("");
    he->setHelpText(QObject::tr("External application or script that outputs "
                                "extra information for inclusion in the "
                                "backend status page. See http://www.mythtv."
                                "org/wiki/Miscellaneous_Status_Information"));
    return he;
}

static GlobalSpinBox *EITTransportTimeout()
{
    GlobalSpinBox *gc = new GlobalSpinBox("EITTransportTimeout", 1, 15, 1);
    gc->setLabel(QObject::tr("EIT transport timeout (mins)"));
    gc->setValue(5);
    QString helpText = QObject::tr(
        "Maximum time to spend waiting (in minutes) for listings data "
        "on one digital TV channel before checking for new listings data "
        "on the next channel.");
    gc->setHelpText(helpText);
    return gc;
}

static GlobalCheckBox *MasterBackendOverride()
{
    GlobalCheckBox *gc = new GlobalCheckBox("MasterBackendOverride");
    gc->setLabel(QObject::tr("Master backend override"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the master backend will stream and"
                    " delete files if it finds them in a storage directory. "
                    "Useful if you are using a central storage location, like "
                    "a NFS share, and your slave backend isn't running."));
    return gc;
};

static GlobalSpinBox *EITCrawIdleStart()
{
    GlobalSpinBox *gc = new GlobalSpinBox("EITCrawIdleStart", 30, 7200, 30);
    gc->setLabel(QObject::tr("Backend idle before EIT crawl (secs)"));
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
    gc->setLabel(QObject::tr("Delay between wake attempts (secs)"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("Length of time the frontend waits between "
                    "tries to wake up the master backend. This should be the "
                    "time your master backend needs to startup. Set to 0 to "
                    "disable."));
    return gc;
};

static GlobalSpinBox *WOLbackendConnectRetry()
{
    GlobalSpinBox *gc = new GlobalSpinBox("WOLbackendConnectRetry", 1, 60, 1);
    gc->setLabel(QObject::tr("Wake attempts"));
    gc->setHelpText(QObject::tr("Number of times the frontend will try to wake "
                    "up the master backend."));
    gc->setValue(5);
    return gc;
};

static GlobalLineEdit *WOLbackendCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("WOLbackendCommand");
    gc->setLabel(QObject::tr("Wake command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to wake up your master "
            "backend server (e.g. sudo /etc/init.d/mythtv-backend restart)."));
    return gc;
};

static HostLineEdit *SleepCommand()
{
    HostLineEdit *gc = new HostLineEdit("SleepCommand");
    gc->setLabel(QObject::tr("Sleep command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to put this slave to sleep. "
                    "If set, the master backend will use this command to put "
                    "this slave to sleep when it is not needed for recording."));
    return gc;
};

static HostLineEdit *WakeUpCommand()
{
    HostLineEdit *gc = new HostLineEdit("WakeUpCommand");
    gc->setLabel(QObject::tr("Wake command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to wake up this slave "
                    "from sleep. This setting is not used on the master "
                    "backend."));
    return gc;
};

static GlobalLineEdit *BackendStopCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("BackendStopCommand");
    gc->setLabel(QObject::tr("Backend stop command"));
    gc->setValue("killall mythbackend");
    gc->setHelpText(QObject::tr("The command used to stop the backend"
                    " when running on the master backend server "
                    "(e.g. sudo /etc/init.d/mythtv-backend stop)"));
    return gc;
};

static GlobalLineEdit *BackendStartCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("BackendStartCommand");
    gc->setLabel(QObject::tr("Backend start command"));
    gc->setValue("mythbackend");
    gc->setHelpText(QObject::tr("The command used to start the backend"
                    " when running on the master backend server "
                    "(e.g. sudo /etc/init.d/mythtv-backend start)."));
    return gc;
};

static GlobalSpinBox *idleTimeoutSecs()
{
    GlobalSpinBox *gc = new GlobalSpinBox("idleTimeoutSecs", 0, 1200, 5);
    gc->setLabel(QObject::tr("Idle shutdown timeout (secs)"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("The number of seconds the master backend "
                    "idles before it shuts down all other backends. Set to 0 to "
                    "disable automatic shutdown."));
    return gc;
};

static GlobalSpinBox *idleWaitForRecordingTime()
{
    GlobalSpinBox *gc = new GlobalSpinBox("idleWaitForRecordingTime", 0, 300, 1);
    gc->setLabel(QObject::tr("Maximum wait for recording (mins)"));
    gc->setValue(15);
    gc->setHelpText(QObject::tr("The number of minutes the master backend "
                    "waits for a recording. If the backend is idle but a "
                    "recording starts within this time period, it won't "
                    "shut down."));
    return gc;
};

static GlobalSpinBox *StartupSecsBeforeRecording()
{
    GlobalSpinBox *gc = new GlobalSpinBox("StartupSecsBeforeRecording", 0, 1200, 5);
    gc->setLabel(QObject::tr("Startup before recording (secs)"));
    gc->setValue(120);
    gc->setHelpText(QObject::tr("The number of seconds the master backend "
                    "will be woken up before a recording starts."));
    return gc;
};

static GlobalLineEdit *WakeupTimeFormat()
{
    GlobalLineEdit *gc = new GlobalLineEdit("WakeupTimeFormat");
    gc->setLabel(QObject::tr("Wakeup time format"));
    gc->setValue("hh:mm yyyy-MM-dd");
    gc->setHelpText(QObject::tr("The format of the time string passed to the "
                    "'Command to set wakeup time' as $time. See "
                    "QT::QDateTime.toString() for details. Set to 'time_t' for "
                    "seconds since epoch."));
    return gc;
};

static GlobalLineEdit *SetWakeuptimeCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("SetWakeuptimeCommand");
    gc->setLabel(QObject::tr("Command to set wakeup time"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to set the wakeup time "
                                "(passed as $time) for the Master Backend"));
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
    gc->setLabel(QObject::tr("Pre-shutdown-check command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("A command executed before the backend would "
                    "shutdown. The return value determines if "
                    "the backend can shutdown. 0 - yes, "
                    "1 - restart idling, "
                    "2 - reset the backend to wait for a frontend."));
    return gc;
};

static GlobalCheckBox *blockSDWUwithoutClient()
{
    GlobalCheckBox *gc = new GlobalCheckBox("blockSDWUwithoutClient");
    gc->setLabel(QObject::tr("Block shutdown before client connected"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the automatic shutdown routine will "
                    "be disabled until a client connects."));
    return gc;
};

static GlobalLineEdit *startupCommand()
{
    GlobalLineEdit *gc = new GlobalLineEdit("startupCommand");
    gc->setLabel(QObject::tr("Startup command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("This command is executed right after starting "
                    "the BE. As a parameter '$status' is replaced by either "
                    "'auto' if the machine was started automatically or "
                    "'user' if a user switched it on."));
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
    gc->setLabel(QObject::tr("Job Queue check frequency (secs)"));
    gc->setHelpText(QObject::tr("When looking for new jobs to process, the "
                    "Job Queue will wait this many seconds between checks."));
    gc->setValue(60);
    return gc;
};

static HostComboBox *JobQueueCPU()
{
    HostComboBox *gc = new HostComboBox("JobQueueCPU");
    gc->setLabel(QObject::tr("CPU usage"));
    gc->addSelection(QObject::tr("Low"), "0");
    gc->addSelection(QObject::tr("Medium"), "1");
    gc->addSelection(QObject::tr("High"), "2");
    gc->setHelpText(QObject::tr("This setting controls approximately how "
                    "much CPU jobs in the queue may consume. "
                    "On 'High', all available CPU time may be used, "
                    "which could cause problems on slower systems." ));
    return gc;
};

static HostTimeBox *JobQueueWindowStart()
{
    HostTimeBox *gc = new HostTimeBox("JobQueueWindowStart", "00:00");
    gc->setLabel(QObject::tr("Job Queue start time"));
    gc->setHelpText(QObject::tr("This setting controls the start of the "
                    "Job Queue time window, which determines when new jobs "
                    "will be started."));
    return gc;
};

static HostTimeBox *JobQueueWindowEnd()
{
    HostTimeBox *gc = new HostTimeBox("JobQueueWindowEnd", "23:59");
    gc->setLabel(QObject::tr("Job Queue end time"));
    gc->setHelpText(QObject::tr("This setting controls the end of the "
                    "Job Queue time window, which determines when new jobs "
                    "will be started."));
    return gc;
};

static GlobalCheckBox *JobsRunOnRecordHost()
{
    GlobalCheckBox *gc = new GlobalCheckBox("JobsRunOnRecordHost");
    gc->setLabel(QObject::tr("Run jobs only on original recording backend"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, jobs in the queue will be required "
                                "to run on the backend that made the "
                                "original recording."));
    return gc;
};

static GlobalCheckBox *AutoTranscodeBeforeAutoCommflag()
{
    GlobalCheckBox *gc = new GlobalCheckBox("AutoTranscodeBeforeAutoCommflag");
    gc->setLabel(QObject::tr("Run transcode jobs before auto commercial "
                             "detection"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, and if both auto-transcode and "
                                "commercial detection are turned ON for a "
                                "recording, transcoding will run first; "
                                "otherwise, commercial detection runs first."));
    return gc;
};

static GlobalCheckBox *AutoCommflagWhileRecording()
{
    GlobalCheckBox *gc = new GlobalCheckBox("AutoCommflagWhileRecording");
    gc->setLabel(QObject::tr("Start auto-commercial-detection jobs when the "
                             "recording starts"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, and Auto Commercial Detection is "
                                "ON for a recording, the flagging job will be "
                                "started as soon as the recording starts. NOT "
                                "recommended on underpowered systems."));
    return gc;
};

static GlobalLineEdit *UserJob(uint job_num)
{
    GlobalLineEdit *gc = new GlobalLineEdit(QString("UserJob%1").arg(job_num));
    gc->setLabel(QObject::tr("User Job #%1 command").arg(job_num));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    return gc;
};

static GlobalLineEdit *UserJobDesc(uint job_num)
{
    GlobalLineEdit *gc = new GlobalLineEdit(QString("UserJobDesc%1")
                                            .arg(job_num));
    gc->setLabel(QObject::tr("User Job #%1 description").arg(job_num));
    gc->setValue(QObject::tr("User Job #%1").arg(job_num));
    gc->setHelpText(QObject::tr("The description for this User Job."));
    return gc;
};

static HostCheckBox *JobAllowMetadata()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowMetadata");
    gc->setLabel(QObject::tr("Allow metadata lookup jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static HostCheckBox *JobAllowCommFlag()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowCommFlag");
    gc->setLabel(QObject::tr("Allow commercial-detection jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static HostCheckBox *JobAllowTranscode()
{
    HostCheckBox *gc = new HostCheckBox("JobAllowTranscode");
    gc->setLabel(QObject::tr("Allow transcoding jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
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
    gc->setLabel(QObject::tr("Commercial-detection command"));
    gc->setValue("mythcommflag");
    gc->setHelpText(QObject::tr("The program used to detect commercials in a "
                    "recording. The default is 'mythcommflag' "
                    "if this setting is empty."));
    return gc;
};

static HostCheckBox *JobAllowUserJob(uint job_num)
{
    QString dbStr = QString("JobAllowUserJob%1").arg(job_num);
    QString desc  = gCoreContext->GetSetting(QString("UserJobDesc%1").arg(job_num));
    QString label = QObject::tr("Allow %1 jobs").arg(desc);

    HostCheckBox *bc = new HostCheckBox(dbStr);
    bc->setLabel(label);
    bc->setValue(false);
    // FIXME:
    // It would be nice to disable inactive jobs,
    // but enabling them currently requires a restart of mythtv-setup
    // after entering the job command string. Will improve this logic later:
    // if (QString(gCoreContext->GetSetting(QString("UserJob%1").arg(job_num)))
    //            .length() == 0)
    //     bc->setEnabled(false);
    bc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                    "run on this backend."));
    return bc;
}

#if 0
static GlobalCheckBox *UPNPShowRecordingUnderVideos()
{
    GlobalCheckBox *gc = new GlobalCheckBox("UPnP/RecordingsUnderVideos");
    gc->setLabel(QObject::tr("Include recordings in video list"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, the master backend will include"
                    " the list of recorded shows in the list of videos. "
                    " This is mainly to accommodate UPnP players which do not"
                    " allow more than 1 video section." ));
    return gc;
};
#endif

static GlobalComboBox *UPNPWmpSource()
{
    GlobalComboBox *gc = new GlobalComboBox("UPnP/WMPSource");
    gc->setLabel(QObject::tr("Video content to show a WMP client"));
    gc->addSelection(QObject::tr("Recordings"),"0");
    gc->addSelection(QObject::tr("Videos"),"1");
    gc->setValue("0");
    gc->setHelpText(QObject::tr("Which tree to show a Windows Media Player "
                    "client when it requests a list of videos."));
    return gc;
};

static GlobalCheckBox *MythFillEnabled()
{
    GlobalCheckBox *bc = new GlobalCheckBox("MythFillEnabled");
    bc->setLabel(QObject::tr("Automatically update program listings"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("If enabled, the guide data program "
                                "will be run automatically."));
    return bc;
}

static GlobalSpinBox *MythFillMinHour()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MythFillMinHour", 0, 23, 1);
    bs->setLabel(QObject::tr("Guide data program execution start"));
    bs->setValue(0);
    bs->setHelpText(QObject::tr("This setting and the following one define a "
                    "time period when the guide data program is allowed "
                    "to run. For example, setting start to 11 and "
                    "end to 13 would mean that the program would only "
                    "run between 11:00 AM and 1:59 PM."));
    return bs;
}

static GlobalSpinBox *MythFillMaxHour()
{
    GlobalSpinBox *bs = new GlobalSpinBox("MythFillMaxHour", 0, 23, 1);
    bs->setLabel(QObject::tr("Guide data program execution end"));
    bs->setValue(23);
    bs->setHelpText(QObject::tr("This setting and the preceding one define a "
                    "time period when the guide data program is allowed "
                    "to run. For example, setting start to 11 and "
                    "end to 13 would mean that the program would only "
                    "run between 11:00 AM and 1:59 PM."));
    return bs;
}

static GlobalCheckBox *MythFillGrabberSuggestsTime()
{
    GlobalCheckBox *bc = new GlobalCheckBox("MythFillGrabberSuggestsTime");
    bc->setLabel(QObject::tr("Run guide data program at time suggested by the "
                             "grabber."));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("If enabled, allow a DataDirect guide data "
                    "provider to specify the next download time in order "
                    "to distribute load on their servers. Guide data program "
                    "execution start/end times are also ignored."));
    return bc;
}

static GlobalLineEdit *MythFillDatabasePath()
{
    GlobalLineEdit *be = new GlobalLineEdit("MythFillDatabasePath");
    be->setLabel(QObject::tr("Guide data program"));
    be->setValue("mythfilldatabase");
    be->setHelpText(QObject::tr(
                        "Use 'mythfilldatabase' or the name of a custom "
                        "script that will populate the program guide info "
                        "for all your video sources."));
    return be;
}

static GlobalLineEdit *MythFillDatabaseArgs()
{
    GlobalLineEdit *be = new GlobalLineEdit("MythFillDatabaseArgs");
    be->setLabel(QObject::tr("Guide data arguments"));
    be->setValue("");
    be->setHelpText(QObject::tr("Any arguments you want passed to the "
                                "guide data program."));
    return be;
}

class MythFillSettings : public TriggeredConfigurationGroup
{
  public:
     MythFillSettings() :
         TriggeredConfigurationGroup(false, true, false, false)
     {
         setLabel(QObject::tr("Program Schedule Downloading Options"));
         setUseLabel(false);

         Setting* fillEnabled = MythFillEnabled();
         addChild(fillEnabled);
         setTrigger(fillEnabled);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(MythFillDatabasePath());
         settings->addChild(MythFillDatabaseArgs());
         settings->addChild(MythFillMinHour());
         settings->addChild(MythFillMaxHour());
         settings->addChild(MythFillGrabberSuggestsTime());
         addTarget("1", settings);

         // show nothing if fillEnabled is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

BackendSettings::BackendSettings() {
    VerticalConfigurationGroup* server = new VerticalConfigurationGroup(false);
    server->setLabel(QObject::tr("Host Address Backend Setup"));
    VerticalConfigurationGroup* localServer = new VerticalConfigurationGroup();
    localServer->setLabel(QObject::tr("Local Backend") + " (" +
                          gCoreContext->GetHostName() + ")");
    HorizontalConfigurationGroup* localIP =
              new HorizontalConfigurationGroup(false, false, true, true);
    localIP->addChild(LocalServerIP());
    localServer->addChild(localIP);
    HorizontalConfigurationGroup* localIP6 =
              new HorizontalConfigurationGroup(false, false, true, true);
    localIP6->addChild(LocalServerIP6());
    localServer->addChild(localIP6);
    HorizontalConfigurationGroup *localUseLL =
              new HorizontalConfigurationGroup(false, false, true, true);
    localUseLL->addChild(UseLinkLocal());
    localServer->addChild(localUseLL);
    HorizontalConfigurationGroup* localPorts =
              new HorizontalConfigurationGroup(false, false, true, true);
    localPorts->addChild(LocalServerPort());
    localPorts->addChild(LocalStatusPort());
    localServer->addChild(localPorts);
    HorizontalConfigurationGroup* localPin =
              new HorizontalConfigurationGroup(false, false, true, true);
    localPin->addChild(LocalSecurityPin());
    localServer->addChild(localPin);
    VerticalConfigurationGroup* masterServer = new VerticalConfigurationGroup();
    masterServer->setLabel(QObject::tr("Master Backend"));
    HorizontalConfigurationGroup* master =
              new HorizontalConfigurationGroup(false, false, true, true);
    master->addChild(MasterServerIP());
    master->addChild(MasterServerPort());
    masterServer->addChild(master);
    server->addChild(localServer);
    server->addChild(masterServer);
    addChild(server);

    VerticalConfigurationGroup* locale = new VerticalConfigurationGroup(false);
    locale->setLabel(QObject::tr("Locale Settings"));
    locale->addChild(TVFormat());
    locale->addChild(VbiFormat());
    locale->addChild(FreqTable());
    addChild(locale);

    VerticalConfigurationGroup* group2 = new VerticalConfigurationGroup(false);
    group2->setLabel(QObject::tr("Miscellaneous Settings"));

    VerticalConfigurationGroup* fm = new VerticalConfigurationGroup();
    fm->setLabel(QObject::tr("File Management Settings"));
    fm->addChild(MasterBackendOverride());
    HorizontalConfigurationGroup *fmh1 =
        new HorizontalConfigurationGroup(false, false, true, true);
    fmh1->addChild(DeletesFollowLinks());
    fmh1->addChild(TruncateDeletes());
    fm->addChild(fmh1);
    fm->addChild(HDRingbufferSize());
    fm->addChild(StorageScheduler());
    group2->addChild(fm);
    VerticalConfigurationGroup* upnp = new VerticalConfigurationGroup();
    upnp->setLabel(QObject::tr("UPnP Server Settings"));
    //upnp->addChild(UPNPShowRecordingUnderVideos());
    upnp->addChild(UPNPWmpSource());
    group2->addChild(upnp);
    group2->addChild(MiscStatusScript());
    group2->addChild(DisableAutomaticBackup());
    group2->addChild(DisableFirewireReset());
    addChild(group2);

    VerticalConfigurationGroup* group2a1 = new VerticalConfigurationGroup(false);
    group2a1->setLabel(QObject::tr("EIT Scanner Options"));
    group2a1->addChild(EITTransportTimeout());
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
    group4->setLabel(QObject::tr("Backend Wakeup settings"));

    VerticalConfigurationGroup* backend = new VerticalConfigurationGroup();
    backend->setLabel(QObject::tr("Master Backend"));
    backend->addChild(WOLbackendReconnectWaitTime());
    backend->addChild(WOLbackendConnectRetry());
    backend->addChild(WOLbackendCommand());
    group4->addChild(backend);

    VerticalConfigurationGroup* slaveBackend = new VerticalConfigurationGroup();
    slaveBackend->setLabel(QObject::tr("Slave Backends"));
    slaveBackend->addChild(SleepCommand());
    slaveBackend->addChild(WakeUpCommand());
    group4->addChild(slaveBackend);
    addChild(group4);

    VerticalConfigurationGroup* backendControl = new VerticalConfigurationGroup();
    backendControl->setLabel(QObject::tr("Backend Control"));
    backendControl->addChild(BackendStopCommand());
    backendControl->addChild(BackendStartCommand());
    addChild(backendControl);

    VerticalConfigurationGroup* group5 = new VerticalConfigurationGroup(false);
    group5->setLabel(QObject::tr("Job Queue (Backend-Specific)"));
    group5->addChild(JobQueueMaxSimultaneousJobs());
    group5->addChild(JobQueueCheckFrequency());

    HorizontalConfigurationGroup* group5a =
              new HorizontalConfigurationGroup(false, false);
    VerticalConfigurationGroup* group5a1 =
              new VerticalConfigurationGroup(false, false);
    group5a1->addChild(JobQueueWindowStart());
    group5a1->addChild(JobQueueWindowEnd());
    group5a1->addChild(JobQueueCPU());
    group5a1->addChild(JobAllowMetadata());
    group5a1->addChild(JobAllowCommFlag());
    group5a1->addChild(JobAllowTranscode());
    group5a->addChild(group5a1);

    VerticalConfigurationGroup* group5a2 =
            new VerticalConfigurationGroup(false, false);
    group5a2->addChild(JobAllowUserJob(1));
    group5a2->addChild(JobAllowUserJob(2));
    group5a2->addChild(JobAllowUserJob(3));
    group5a2->addChild(JobAllowUserJob(4));
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
    group7->addChild(UserJobDesc(1));
    group7->addChild(UserJob(1));
    group7->addChild(UserJobDesc(2));
    group7->addChild(UserJob(2));
    group7->addChild(UserJobDesc(3));
    group7->addChild(UserJob(3));
    group7->addChild(UserJobDesc(4));
    group7->addChild(UserJob(4));
    addChild(group7);

    MythFillSettings *mythfill = new MythFillSettings();
    addChild(mythfill);

}
