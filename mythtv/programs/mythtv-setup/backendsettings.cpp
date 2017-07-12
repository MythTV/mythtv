#include <cstdio>

#include "backendsettings.h"
#include "frequencies.h"
#include "mythcorecontext.h"
#include "channelsettings.h" // for ChannelTVFormat::GetFormats()
#include <unistd.h>

#include <QNetworkInterface>


static TransMythUICheckBoxSetting *IsMasterBackend()
{
    TransMythUICheckBoxSetting *gc = new TransMythUICheckBoxSetting();
    gc->setLabel(QObject::tr("This server is the Master Backend"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr(
                    "Enable this if this is the only backend or is the "
                    "master backend server. If enabled, all frontend and "
                    "non-master backend machines "
                    "will connect to this server. To change to a new master "
                    "backend, run setup on that server and select it as "
                    "master backend."));
    return gc;
};

static GlobalTextEditSetting *MasterServerName()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("MasterServerName");
    gc->setLabel(QObject::tr("Master Backend Name"));
    gc->setValue("");
    gc->setEnabled(false);
    gc->setHelpText(QObject::tr(
                    "Host name of Master Backend. This is set by selecting "
                    "\"This server is the Master Backend\" on that server."));
    return gc;
};

static HostCheckBoxSetting *AllowConnFromAll()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AllowConnFromAll");
    gc->setLabel(QObject::tr("Allow Connections from all Subnets"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr(
                    "Allow this backend to receive connections from any IP "
                    "address on the internet. NOT recommended for most users. "
                    "Use this only if you have secure IPV4 and IPV6 " "firewalls."));
    return gc;
};

static HostComboBoxSetting *LocalServerIP()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("BackendServerIP");
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

static HostComboBoxSetting *LocalServerIP6()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("BackendServerIP6");
    gc->setLabel(QObject::tr("Listen on IPv6 address"));
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    QList<QHostAddress>::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        if ((*it).protocol() == QAbstractSocket::IPv6Protocol)
        {
            // If it is a link-local IPV6 address with scope,
            // remove the scope.
            it->setScopeId(QString());
            gc->addSelection((*it).toString(), (*it).toString());
        }
    }

    if (list.isEmpty())
    {
        gc->setEnabled(false);
        gc->setValue("");
    }
    else
    {
        if (list.contains(QHostAddress("::1")))
            gc->setValue("::1");
    }

    gc->setHelpText(QObject::tr("Enter the IPv6 address of this machine. "
                    "Use an externally accessible address (ie, not "
                    "::1) if you are going to be running a frontend "
                    "on a different machine than this one."));
    return gc;
}

static HostCheckBoxSetting *UseLinkLocal()
{
    HostCheckBoxSetting *hc = new HostCheckBoxSetting("AllowLinkLocal");
    hc->setLabel(QObject::tr("Listen on Link-Local addresses"));
    hc->setValue(true);
    hc->setHelpText(QObject::tr("Enable servers on this machine to listen on "
                    "link-local addresses. These are auto-configured "
                    "addresses and not accessible outside the local network. "
                    "This must be enabled for anything requiring Bonjour to "
                    "work."));
    return hc;
};

class IpAddressSettings : public HostCheckBoxSetting
{
  public:
     HostComboBoxSetting *localServerIP;
     HostComboBoxSetting *localServerIP6;
     explicit IpAddressSettings(/*Setting* trigger*/) :
         HostCheckBoxSetting("ListenOnAllIps")
     {
         setLabel(QObject::tr("Listen on All IP Addresses"));
         setValue(true);
         setHelpText(tr("Allow this backend to receive connections on any IP "
                        "Address assigned to it. Recommended for most users "
                        "for ease and reliability."));

         localServerIP = LocalServerIP();
         localServerIP6 = LocalServerIP6();
         // show ip addresses if ListenOnAllIps is off
         addTargetedChild("0", localServerIP);
         addTargetedChild("0", localServerIP6);
         addTargetedChild("0", UseLinkLocal());
     };
};



static HostTextEditSetting *LocalServerPort()
{
    HostTextEditSetting *gc = new HostTextEditSetting("BackendServerPort");
    gc->setLabel(QObject::tr("Port"));
    gc->setValue("6543");
    gc->setHelpText(QObject::tr("Unless you've got good reason, don't "
                    "change this."));
    return gc;
};

static HostTextEditSetting *LocalStatusPort()
{
    HostTextEditSetting *gc = new HostTextEditSetting("BackendStatusPort");
    gc->setLabel(QObject::tr("Status port"));
    gc->setValue("6544");
    gc->setHelpText(QObject::tr("Port on which the server will listen for "
                    "HTTP requests, including backend status and MythXML "
                    "requests."));
    return gc;
};

static HostComboBoxSetting *BackendServerAddr()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("BackendServerAddr", true);
    gc->setLabel(QObject::tr("Primary IP address / DNS name"));
    gc->setValue("127.0.0.1");
    gc->setHelpText(QObject::tr("The Primary IP address of this backend "
                    "server. You can select an IP "
                    "address from the list or type a DNS name "
                    "or host name. Other systems will contact this "
                    "server using this address. "
                    "If you use a host name make sure it is assigned "
                    "an ip address other than 127.0.0.1 in the hosts "
                    "file."));
    return gc;
};

// Deprecated
static GlobalTextEditSetting *MasterServerIP()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("MasterServerIP");
    gc->setLabel(QObject::tr("IP address"));
    gc->setValue("127.0.0.1");
    return gc;
};

// Deprecated
static GlobalTextEditSetting *MasterServerPort()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("MasterServerPort");
    gc->setLabel(QObject::tr("Port"));
    gc->setValue("6543");
    return gc;
};

static HostTextEditSetting *LocalSecurityPin()
{
    HostTextEditSetting *gc = new HostTextEditSetting("SecurityPin");
    gc->setLabel(QObject::tr("Security PIN (required)"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("PIN code required for a frontend to connect "
                                "to the backend. Blank prevents all "
                                "connections; 0000 allows any client to "
                                "connect."));
    return gc;
};

static GlobalComboBoxSetting *TVFormat()
{
    GlobalComboBoxSetting *gc = new GlobalComboBoxSetting("TVFormat");
    gc->setLabel(QObject::tr("TV format"));

    QStringList list = ChannelTVFormat::GetFormats();
    for (int i = 0; i < list.size(); i++)
        gc->addSelection(list[i]);

    gc->setHelpText(QObject::tr("The TV standard to use for viewing TV."));
    return gc;
};

static GlobalComboBoxSetting *VbiFormat()
{
    GlobalComboBoxSetting *gc = new GlobalComboBoxSetting("VbiFormat");
    gc->setLabel(QObject::tr("VBI format"));
    gc->addSelection("None");
    gc->addSelection("PAL teletext");
    gc->addSelection("NTSC closed caption");
    gc->setHelpText(QObject::tr("The VBI (Vertical Blanking Interval) is "
                    "used to carry Teletext or Closed Captioning "
                    "data."));
    return gc;
};

static GlobalComboBoxSetting *FreqTable()
{
    GlobalComboBoxSetting *gc = new GlobalComboBoxSetting("FreqTable");
    gc->setLabel(QObject::tr("Channel frequency table"));

    for (uint i = 0; chanlists[i].name; i++)
        gc->addSelection(chanlists[i].name);

    gc->setHelpText(QObject::tr("Select the appropriate frequency table for "
                    "your system. If you have an antenna, use a \"-bcast\" "
                    "frequency."));
    return gc;
};

static GlobalCheckBoxSetting *SaveTranscoding()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("SaveTranscoding");
    gc->setLabel(QObject::tr("Save original files after transcoding (globally)"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled and the transcoder is active, the "
                    "original files will be renamed to .old once the "
                    "transcoding is complete."));
    return gc;
};

static HostCheckBoxSetting *TruncateDeletes()
{
    HostCheckBoxSetting *hc = new HostCheckBoxSetting("TruncateDeletesSlowly");
    hc->setLabel(QObject::tr("Delete files slowly"));
    hc->setValue(false);
    hc->setHelpText(QObject::tr("Some filesystems use a lot of resources when "
                    "deleting large files. If enabled, this option makes "
                    "MythTV delete files slowly on this backend to lessen the "
                    "impact."));
    return hc;
};

static GlobalCheckBoxSetting *DeletesFollowLinks()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("DeletesFollowLinks");
    gc->setLabel(QObject::tr("Follow symbolic links when deleting files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, MythTV will follow symlinks "
                    "when recordings and related files are deleted, instead "
                    "of deleting the symlink and leaving the actual file."));
    return gc;
};

static GlobalSpinBoxSetting *HDRingbufferSize()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting(
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

static GlobalComboBoxSetting *StorageScheduler()
{
    GlobalComboBoxSetting *gc = new GlobalComboBoxSetting("StorageScheduler");
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

static GlobalCheckBoxSetting *DisableAutomaticBackup()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("DisableAutomaticBackup");
    gc->setLabel(QObject::tr("Disable automatic database backup"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, MythTV will not backup the "
                                "database before upgrades. You should "
                                "therefore have your own database backup "
                                "strategy in place."));
    return gc;
};

static HostCheckBoxSetting *DisableFirewireReset()
{
    HostCheckBoxSetting *hc = new HostCheckBoxSetting("DisableFirewireReset");
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

static HostTextEditSetting *MiscStatusScript()
{
    HostTextEditSetting *he = new HostTextEditSetting("MiscStatusScript");
    he->setLabel(QObject::tr("Miscellaneous status application"));
    he->setValue("");
    he->setHelpText(QObject::tr("External application or script that outputs "
                                "extra information for inclusion in the "
                                "backend status page. See http://www.mythtv."
                                "org/wiki/Miscellaneous_Status_Information"));
    return he;
}

static GlobalSpinBoxSetting *EITTransportTimeout()
{
    GlobalSpinBoxSetting *gc = new GlobalSpinBoxSetting("EITTransportTimeout", 1, 15, 1);
    gc->setLabel(QObject::tr("EIT transport timeout (mins)"));
    gc->setValue(5);
    QString helpText = QObject::tr(
        "Maximum time to spend waiting (in minutes) for listings data "
        "on one digital TV channel before checking for new listings data "
        "on the next channel.");
    gc->setHelpText(helpText);
    return gc;
}

static GlobalCheckBoxSetting *MasterBackendOverride()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("MasterBackendOverride");
    gc->setLabel(QObject::tr("Master backend override"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the master backend will stream and"
                    " delete files if it finds them in a storage directory. "
                    "Useful if you are using a central storage location, like "
                    "a NFS share, and your slave backend isn't running."));
    return gc;
};

static GlobalSpinBoxSetting *EITCrawIdleStart()
{
    GlobalSpinBoxSetting *gc = new GlobalSpinBoxSetting("EITCrawIdleStart", 30, 7200, 30);
    gc->setLabel(QObject::tr("Backend idle before EIT crawl (secs)"));
    gc->setValue(60);
    QString help = QObject::tr(
        "The minimum number of seconds after a recorder becomes idle "
        "to wait before MythTV begins collecting EIT listings data.");
    gc->setHelpText(help);
    return gc;
}

static GlobalSpinBoxSetting *WOLbackendReconnectWaitTime()
{
    GlobalSpinBoxSetting *gc = new GlobalSpinBoxSetting("WOLbackendReconnectWaitTime", 0, 1200, 5);
    gc->setLabel(QObject::tr("Delay between wake attempts (secs)"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("Length of time the frontend waits between "
                    "tries to wake up the master backend. This should be the "
                    "time your master backend needs to startup. Set to 0 to "
                    "disable."));
    return gc;
};

static GlobalSpinBoxSetting *WOLbackendConnectRetry()
{
    GlobalSpinBoxSetting *gc = new GlobalSpinBoxSetting("WOLbackendConnectRetry", 1, 60, 1);
    gc->setLabel(QObject::tr("Wake attempts"));
    gc->setHelpText(QObject::tr("Number of times the frontend will try to wake "
                    "up the master backend."));
    gc->setValue(5);
    return gc;
};

static GlobalTextEditSetting *WOLbackendCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("WOLbackendCommand");
    gc->setLabel(QObject::tr("Wake command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to wake up your master "
            "backend server (e.g. wakeonlan 00:00:00:00:00:00)."));
    return gc;
};

static HostTextEditSetting *SleepCommand()
{
    HostTextEditSetting *gc = new HostTextEditSetting("SleepCommand");
    gc->setLabel(QObject::tr("Sleep command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to put this slave to sleep. "
                    "If set, the master backend will use this command to put "
                    "this slave to sleep when it is not needed for recording."));
    return gc;
};

static HostTextEditSetting *WakeUpCommand()
{
    HostTextEditSetting *gc = new HostTextEditSetting("WakeUpCommand");
    gc->setLabel(QObject::tr("Wake command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to wake up this slave "
                    "from sleep. This setting is not used on the master "
                    "backend."));
    return gc;
};

static GlobalTextEditSetting *BackendStopCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("BackendStopCommand");
    gc->setLabel(QObject::tr("Backend stop command"));
    gc->setValue("killall mythbackend");
    gc->setHelpText(QObject::tr("The command used to stop the backend"
                    " when running on the master backend server "
                    "(e.g. sudo /etc/init.d/mythtv-backend stop)"));
    return gc;
};

static GlobalTextEditSetting *BackendStartCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("BackendStartCommand");
    gc->setLabel(QObject::tr("Backend start command"));
    gc->setValue("mythbackend");
    gc->setHelpText(QObject::tr("The command used to start the backend"
                    " when running on the master backend server "
                    "(e.g. sudo /etc/init.d/mythtv-backend start)."));
    return gc;
};

static GlobalSpinBoxSetting *idleTimeoutSecs()
{
    GlobalSpinBoxSetting *gc = new GlobalSpinBoxSetting("idleTimeoutSecs", 0, 1200, 5);
    gc->setLabel(QObject::tr("Idle shutdown timeout (secs)"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("The number of seconds the master backend "
                    "idles before it shuts down all other backends. Set to 0 to "
                    "disable automatic shutdown."));
    return gc;
};

static GlobalSpinBoxSetting *idleWaitForRecordingTime()
{
    GlobalSpinBoxSetting *gc = new GlobalSpinBoxSetting("idleWaitForRecordingTime", 0, 300, 1);
    gc->setLabel(QObject::tr("Maximum wait for recording (mins)"));
    gc->setValue(15);
    gc->setHelpText(QObject::tr("The number of minutes the master backend "
                    "waits for a recording. If the backend is idle but a "
                    "recording starts within this time period, it won't "
                    "shut down."));
    return gc;
};

static GlobalSpinBoxSetting *StartupSecsBeforeRecording()
{
    GlobalSpinBoxSetting *gc = new GlobalSpinBoxSetting("StartupSecsBeforeRecording", 0, 1200, 5);
    gc->setLabel(QObject::tr("Startup before recording (secs)"));
    gc->setValue(120);
    gc->setHelpText(QObject::tr("The number of seconds the master backend "
                    "will be woken up before a recording starts."));
    return gc;
};

static GlobalTextEditSetting *WakeupTimeFormat()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("WakeupTimeFormat");
    gc->setLabel(QObject::tr("Wakeup time format"));
    gc->setValue("hh:mm yyyy-MM-dd");
    gc->setHelpText(QObject::tr("The format of the time string passed to the "
                    "'Command to set wakeup time' as $time. See "
                    "QT::QDateTime.toString() for details. Set to 'time_t' for "
                    "seconds since epoch."));
    return gc;
};

static GlobalTextEditSetting *SetWakeuptimeCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("SetWakeuptimeCommand");
    gc->setLabel(QObject::tr("Command to set wakeup time"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to set the wakeup time "
                                "(passed as $time) for the Master Backend"));
    return gc;
};

static GlobalTextEditSetting *ServerHaltCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("ServerHaltCommand");
    gc->setLabel(QObject::tr("Server halt command"));
    gc->setValue("sudo /sbin/halt -p");
    gc->setHelpText(QObject::tr("The command used to halt the backends."));
    return gc;
};

static GlobalTextEditSetting *preSDWUCheckCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("preSDWUCheckCommand");
    gc->setLabel(QObject::tr("Pre-shutdown-check command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("A command executed before the backend would "
                    "shutdown. The return value determines if "
                    "the backend can shutdown. 0 - yes, "
                    "1 - restart idling, "
                    "2 - reset the backend to wait for a frontend."));
    return gc;
};

static GlobalCheckBoxSetting *blockSDWUwithoutClient()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("blockSDWUwithoutClient");
    gc->setLabel(QObject::tr("Block shutdown before client connected"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the automatic shutdown routine will "
                    "be disabled until a client connects."));
    return gc;
};

static GlobalTextEditSetting *startupCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("startupCommand");
    gc->setLabel(QObject::tr("Startup command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("This command is executed right after starting "
                    "the BE. As a parameter '$status' is replaced by either "
                    "'auto' if the machine was started automatically or "
                    "'user' if a user switched it on."));
    return gc;
};

static HostSpinBoxSetting *JobQueueMaxSimultaneousJobs()
{
    HostSpinBoxSetting *gc = new HostSpinBoxSetting("JobQueueMaxSimultaneousJobs", 1, 10, 1);
    gc->setLabel(QObject::tr("Maximum simultaneous jobs on this backend"));
    gc->setHelpText(QObject::tr("The Job Queue will be limited to running "
                    "this many simultaneous jobs on this backend."));
    gc->setValue(1);
    return gc;
};

static HostSpinBoxSetting *JobQueueCheckFrequency()
{
    HostSpinBoxSetting *gc = new HostSpinBoxSetting("JobQueueCheckFrequency", 5, 300, 5);
    gc->setLabel(QObject::tr("Job Queue check frequency (secs)"));
    gc->setHelpText(QObject::tr("When looking for new jobs to process, the "
                    "Job Queue will wait this many seconds between checks."));
    gc->setValue(60);
    return gc;
};

static HostComboBoxSetting *JobQueueCPU()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("JobQueueCPU");
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

static HostTimeBoxSetting *JobQueueWindowStart()
{
    HostTimeBoxSetting *gc = new HostTimeBoxSetting("JobQueueWindowStart", "00:00");
    gc->setLabel(QObject::tr("Job Queue start time"));
    gc->setHelpText(QObject::tr("This setting controls the start of the "
                    "Job Queue time window, which determines when new jobs "
                    "will be started."));
    return gc;
};

static HostTimeBoxSetting *JobQueueWindowEnd()
{
    HostTimeBoxSetting *gc = new HostTimeBoxSetting("JobQueueWindowEnd", "23:59");
    gc->setLabel(QObject::tr("Job Queue end time"));
    gc->setHelpText(QObject::tr("This setting controls the end of the "
                    "Job Queue time window, which determines when new jobs "
                    "will be started."));
    return gc;
};

static GlobalCheckBoxSetting *JobsRunOnRecordHost()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("JobsRunOnRecordHost");
    gc->setLabel(QObject::tr("Run jobs only on original recording backend"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, jobs in the queue will be required "
                                "to run on the backend that made the "
                                "original recording."));
    return gc;
};

static GlobalCheckBoxSetting *AutoTranscodeBeforeAutoCommflag()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("AutoTranscodeBeforeAutoCommflag");
    gc->setLabel(QObject::tr("Run transcode jobs before auto commercial "
                             "detection"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, and if both auto-transcode and "
                                "commercial detection are turned ON for a "
                                "recording, transcoding will run first; "
                                "otherwise, commercial detection runs first."));
    return gc;
};

static GlobalCheckBoxSetting *AutoCommflagWhileRecording()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("AutoCommflagWhileRecording");
    gc->setLabel(QObject::tr("Start auto-commercial-detection jobs when the "
                             "recording starts"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, and Auto Commercial Detection is "
                                "ON for a recording, the flagging job will be "
                                "started as soon as the recording starts. NOT "
                                "recommended on underpowered systems."));
    return gc;
};

static GlobalTextEditSetting *UserJob(uint job_num)
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting(QString("UserJob%1").arg(job_num));
    gc->setLabel(QObject::tr("User Job #%1 command").arg(job_num));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    return gc;
};

static GlobalTextEditSetting *UserJobDesc(uint job_num)
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting(QString("UserJobDesc%1")
                                            .arg(job_num));
    gc->setLabel(QObject::tr("User Job #%1 description").arg(job_num));
    gc->setValue(QObject::tr("User Job #%1").arg(job_num));
    gc->setHelpText(QObject::tr("The description for this User Job."));
    return gc;
};

static HostCheckBoxSetting *JobAllowMetadata()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("JobAllowMetadata");
    gc->setLabel(QObject::tr("Allow metadata lookup jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static HostCheckBoxSetting *JobAllowCommFlag()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("JobAllowCommFlag");
    gc->setLabel(QObject::tr("Allow commercial-detection jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static HostCheckBoxSetting *JobAllowTranscode()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("JobAllowTranscode");
    gc->setLabel(QObject::tr("Allow transcoding jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static GlobalTextEditSetting *JobQueueTranscodeCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("JobQueueTranscodeCommand");
    gc->setLabel(QObject::tr("Transcoder command"));
    gc->setValue("mythtranscode");
    gc->setHelpText(QObject::tr("The program used to transcode recordings. "
                    "The default is 'mythtranscode' if this setting is empty."));
    return gc;
};

static GlobalTextEditSetting *JobQueueCommFlagCommand()
{
    GlobalTextEditSetting *gc = new GlobalTextEditSetting("JobQueueCommFlagCommand");
    gc->setLabel(QObject::tr("Commercial-detection command"));
    gc->setValue("mythcommflag");
    gc->setHelpText(QObject::tr("The program used to detect commercials in a "
                    "recording. The default is 'mythcommflag' "
                    "if this setting is empty."));
    return gc;
};

static HostCheckBoxSetting *JobAllowUserJob(uint job_num)
{
    QString dbStr = QString("JobAllowUserJob%1").arg(job_num);
    QString desc  = gCoreContext->GetSetting(QString("UserJobDesc%1").arg(job_num));
    QString label = QObject::tr("Allow %1 jobs").arg(desc);

    HostCheckBoxSetting *bc = new HostCheckBoxSetting(dbStr);
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
static GlobalCheckBoxSetting *UPNPShowRecordingUnderVideos()
{
    GlobalCheckBoxSetting *gc = new GlobalCheckBoxSetting("UPnP/RecordingsUnderVideos");
    gc->setLabel(QObject::tr("Include recordings in video list"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, the master backend will include"
                    " the list of recorded shows in the list of videos. "
                    " This is mainly to accommodate UPnP players which do not"
                    " allow more than 1 video section." ));
    return gc;
};
#endif

static GlobalComboBoxSetting *UPNPWmpSource()
{
    GlobalComboBoxSetting *gc = new GlobalComboBoxSetting("UPnP/WMPSource");
    gc->setLabel(QObject::tr("Video content to show a WMP client"));
    gc->addSelection(QObject::tr("Recordings"),"0");
    gc->addSelection(QObject::tr("Videos"),"1");
    gc->setValue("0");
    gc->setHelpText(QObject::tr("Which tree to show a Windows Media Player "
                    "client when it requests a list of videos."));
    return gc;
};

static GlobalCheckBoxSetting *MythFillEnabled()
{
    GlobalCheckBoxSetting *bc = new GlobalCheckBoxSetting("MythFillEnabled");
    bc->setLabel(QObject::tr("Automatically update program listings"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("If enabled, the guide data program "
                                "will be run automatically."));
    return bc;
}

static GlobalSpinBoxSetting *MythFillMinHour()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("MythFillMinHour", 0, 23, 1);
    bs->setLabel(QObject::tr("Guide data program execution start"));
    bs->setValue(0);
    bs->setHelpText(QObject::tr("This setting and the following one define a "
                    "time period when the guide data program is allowed "
                    "to run. For example, setting start to 11 and "
                    "end to 13 would mean that the program would only "
                    "run between 11:00 AM and 1:59 PM."));
    return bs;
}

static GlobalSpinBoxSetting *MythFillMaxHour()
{
    GlobalSpinBoxSetting *bs = new GlobalSpinBoxSetting("MythFillMaxHour", 0, 23, 1);
    bs->setLabel(QObject::tr("Guide data program execution end"));
    bs->setValue(23);
    bs->setHelpText(QObject::tr("This setting and the preceding one define a "
                    "time period when the guide data program is allowed "
                    "to run. For example, setting start to 11 and "
                    "end to 13 would mean that the program would only "
                    "run between 11:00 AM and 1:59 PM."));
    return bs;
}

static GlobalCheckBoxSetting *MythFillGrabberSuggestsTime()
{
    GlobalCheckBoxSetting *bc = new GlobalCheckBoxSetting("MythFillGrabberSuggestsTime");
    bc->setLabel(QObject::tr("Run guide data program at time suggested by the "
                             "grabber."));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("If enabled, allow a DataDirect guide data "
                    "provider to specify the next download time in order "
                    "to distribute load on their servers. Guide data program "
                    "execution start/end times are also ignored."));
    return bc;
}

static GlobalTextEditSetting *MythFillDatabasePath()
{
    GlobalTextEditSetting *be = new GlobalTextEditSetting("MythFillDatabasePath");
    be->setLabel(QObject::tr("Guide data program"));
    be->setValue("mythfilldatabase");
    be->setHelpText(QObject::tr(
                        "Use 'mythfilldatabase' or the name of a custom "
                        "script that will populate the program guide info "
                        "for all your video sources."));
    return be;
}

static GlobalTextEditSetting *MythFillDatabaseArgs()
{
    GlobalTextEditSetting *be = new GlobalTextEditSetting("MythFillDatabaseArgs");
    be->setLabel(QObject::tr("Guide data arguments"));
    be->setValue("");
    be->setHelpText(QObject::tr("Any arguments you want passed to the "
                                "guide data program."));
    return be;
}

class MythFillSettings : public GroupSetting
{
  public:
     MythFillSettings()
     {
         setLabel(QObject::tr("Program Schedule Downloading Options"));

         GlobalCheckBoxSetting* fillEnabled = MythFillEnabled();
         addChild(fillEnabled);

         fillEnabled->addTargetedChild("1", MythFillDatabasePath());
         fillEnabled->addTargetedChild("1", MythFillDatabaseArgs());
         fillEnabled->addTargetedChild("1", MythFillMinHour());
         fillEnabled->addTargetedChild("1", MythFillMaxHour());
         fillEnabled->addTargetedChild("1", MythFillGrabberSuggestsTime());
     }
};

BackendSettings::BackendSettings() :
    isMasterBackend(0),
    localServerPort(0),
    backendServerAddr(0),
    masterServerName(0),
    ipAddressSettings(0),
    isLoaded(false),
    masterServerIP(0),
    masterServerPort(0)
{
    // These two are included for backward compatibility -
    // used by python bindings. They could be removed later
    masterServerIP = MasterServerIP();
    masterServerPort = MasterServerPort();

    //++ Host Address Backend Setup ++
    GroupSetting* server = new GroupSetting();
    server->setLabel(tr("Host Address Backend Setup"));
    localServerPort = LocalServerPort();
    server->addChild(localServerPort);
    server->addChild(LocalStatusPort());
    server->addChild(LocalSecurityPin());
    server->addChild(AllowConnFromAll());
    //+++ IP Addresses +++
    ipAddressSettings = new IpAddressSettings();
    server->addChild(ipAddressSettings);
    connect(ipAddressSettings, &HostCheckBoxSetting::valueChanged,
            this, &BackendSettings::listenChanged);
    connect(ipAddressSettings->localServerIP,
            static_cast<void (StandardSetting::*)(const QString&)>(&StandardSetting::valueChanged),
            this, &BackendSettings::listenChanged);
    connect(ipAddressSettings->localServerIP6,
            static_cast<void (StandardSetting::*)(const QString&)>(&StandardSetting::valueChanged),
            this, &BackendSettings::listenChanged);
    backendServerAddr = BackendServerAddr();
    server->addChild(backendServerAddr);
    //++ Master Backend ++
    isMasterBackend = IsMasterBackend();
    connect(isMasterBackend, &TransMythUICheckBoxSetting::valueChanged,
            this, &BackendSettings::masterBackendChanged);
    server->addChild(isMasterBackend);
    masterServerName = MasterServerName();
    server->addChild(masterServerName);
    addChild(server);

    //++ Locale Settings ++
    GroupSetting* locale = new GroupSetting();
    locale->setLabel(QObject::tr("Locale Settings"));
    locale->addChild(TVFormat());
    locale->addChild(VbiFormat());
    locale->addChild(FreqTable());
    addChild(locale);

    GroupSetting* group2 = new GroupSetting();
    group2->setLabel(QObject::tr("Miscellaneous Settings"));

    GroupSetting* fm = new GroupSetting();
    fm->setLabel(QObject::tr("File Management Settings"));
    fm->addChild(MasterBackendOverride());
    fm->addChild(DeletesFollowLinks());
    fm->addChild(TruncateDeletes());
    fm->addChild(HDRingbufferSize());
    fm->addChild(StorageScheduler());
    group2->addChild(fm);
    GroupSetting* upnp = new GroupSetting();
    upnp->setLabel(QObject::tr("UPnP Server Settings"));
    //upnp->addChild(UPNPShowRecordingUnderVideos());
    upnp->addChild(UPNPWmpSource());
    group2->addChild(upnp);
    group2->addChild(MiscStatusScript());
    group2->addChild(DisableAutomaticBackup());
    group2->addChild(DisableFirewireReset());
    addChild(group2);

    GroupSetting* group2a1 = new GroupSetting();
    group2a1->setLabel(QObject::tr("EIT Scanner Options"));
    group2a1->addChild(EITTransportTimeout());
    group2a1->addChild(EITCrawIdleStart());
    addChild(group2a1);

    GroupSetting* group3 = new GroupSetting();
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

    GroupSetting* group4 = new GroupSetting();
    group4->setLabel(QObject::tr("Backend Wakeup settings"));

    GroupSetting* backend = new GroupSetting();
    backend->setLabel(QObject::tr("Master Backend"));
    backend->addChild(WOLbackendReconnectWaitTime());
    backend->addChild(WOLbackendConnectRetry());
    backend->addChild(WOLbackendCommand());
    group4->addChild(backend);

    GroupSetting* slaveBackend = new GroupSetting();
    slaveBackend->setLabel(QObject::tr("Slave Backends"));
    slaveBackend->addChild(SleepCommand());
    slaveBackend->addChild(WakeUpCommand());
    group4->addChild(slaveBackend);
    addChild(group4);

    GroupSetting* backendControl = new GroupSetting();
    backendControl->setLabel(QObject::tr("Backend Control"));
    backendControl->addChild(BackendStopCommand());
    backendControl->addChild(BackendStartCommand());
    addChild(backendControl);

    GroupSetting* group5 = new GroupSetting();
    group5->setLabel(QObject::tr("Job Queue (Backend-Specific)"));
    group5->addChild(JobQueueMaxSimultaneousJobs());
    group5->addChild(JobQueueCheckFrequency());
    group5->addChild(JobQueueWindowStart());
    group5->addChild(JobQueueWindowEnd());
    group5->addChild(JobQueueCPU());
    group5->addChild(JobAllowMetadata());
    group5->addChild(JobAllowCommFlag());
    group5->addChild(JobAllowTranscode());
    group5->addChild(JobAllowUserJob(1));
    group5->addChild(JobAllowUserJob(2));
    group5->addChild(JobAllowUserJob(3));
    group5->addChild(JobAllowUserJob(4));
    addChild(group5);

    GroupSetting* group6 = new GroupSetting();
    group6->setLabel(QObject::tr("Job Queue (Global)"));
    group6->addChild(JobsRunOnRecordHost());
    group6->addChild(AutoCommflagWhileRecording());
    group6->addChild(JobQueueCommFlagCommand());
    group6->addChild(JobQueueTranscodeCommand());
    group6->addChild(AutoTranscodeBeforeAutoCommflag());
    group6->addChild(SaveTranscoding());
    addChild(group6);

    GroupSetting* group7 = new GroupSetting();
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

void BackendSettings::masterBackendChanged()
{
    if (!isLoaded)
        return;
    bool ismasterchecked = isMasterBackend->boolValue();
    if (ismasterchecked)
        masterServerName->setValue(gCoreContext->GetHostName());
    else
        masterServerName->setValue(priorMasterName);
}

void BackendSettings::listenChanged()
{
    if (!isLoaded)
        return;
    bool addrChanged = backendServerAddr->haveChanged();
    QString currentsetting = backendServerAddr->getValue();
    backendServerAddr->clearSelections();
    if (ipAddressSettings->boolValue())
    {
        QList<QHostAddress> list = QNetworkInterface::allAddresses();
        QList<QHostAddress>::iterator it;
        for (it = list.begin(); it != list.end(); ++it)
        {
            it->setScopeId(QString());
            backendServerAddr->addSelection((*it).toString(), (*it).toString());
        }
    }
    else
    {
        backendServerAddr->addSelection(
            ipAddressSettings->localServerIP->getValue());
        backendServerAddr->addSelection(
            ipAddressSettings->localServerIP6->getValue());
    }
    // Remove the blank entry that is caused by clearSelections
    // TODO probably not needed anymore?
    // backendServerAddr->removeSelection(QString());

    QHostAddress addr;
    if (addr.setAddress(currentsetting))
    {
        // if prior setting is an ip address
        // it only if it is now in the list
        if (backendServerAddr->getValueIndex(currentsetting)
                > -1)
            backendServerAddr->setValue(currentsetting);
        else
            backendServerAddr->setValue(0);
    }
    else if (! currentsetting.isEmpty())
    {
        // if prior setting was not an ip address, it must
        // have been a dns name so add it back and select it.
        backendServerAddr->addSelection(currentsetting);
        backendServerAddr->setValue(currentsetting);
    }
    else
        backendServerAddr->setValue(0);
    backendServerAddr->setChanged(addrChanged);
}


void BackendSettings::Load(void)
{
    isLoaded=false;
    GroupSetting::Load();

    // These two are included for backward compatibility - only used by python
    // bindings. They should be removed later
    masterServerIP->Load();
    masterServerPort->Load();

    QString mastername = masterServerName->getValue();
    // new installation - default to master
    bool newInstall=false;
    if (mastername.isEmpty())
    {
        mastername = gCoreContext->GetHostName();
        newInstall=true;
    }
    bool ismaster = (mastername == gCoreContext->GetHostName());
    isMasterBackend->setValue(ismaster);
    priorMasterName = mastername;
    isLoaded=true;
    masterBackendChanged();
    listenChanged();
    if (!newInstall)
    {
        backendServerAddr->setChanged(false);
        isMasterBackend->setChanged(false);
        masterServerName->setChanged(false);
    }
}

void BackendSettings::Save(void)
{
    // Setup deprecated backward compatibility settings
    if (isMasterBackend->boolValue())
    {
        QString addr = backendServerAddr->getValue();
        QString ip = gCoreContext->resolveAddress(addr);
        masterServerIP->setValue(ip);
        masterServerPort->setValue(localServerPort->getValue());
    }

    // If listen on all is specified, set up values for the
    // specific IPV4 and IPV6 addresses for backward
    // compatibilty with other things that may use them
    if (ipAddressSettings->boolValue())
    {
        QString bea = backendServerAddr->getValue();
        // initialize them to localhost values
        ipAddressSettings->localServerIP->setValue(0);
        ipAddressSettings->localServerIP6->setValue(0);
        QString ip4 = gCoreContext->resolveAddress
            (bea,MythCoreContext::ResolveIPv4);
        QString ip6 = gCoreContext->resolveAddress
            (bea,MythCoreContext::ResolveIPv6);
        // the setValue calls below only set the value if it is in the list.
        ipAddressSettings->localServerIP->setValue(ip4);
        ipAddressSettings->localServerIP6->setValue(ip6);
    }

    GroupSetting::Save();

    // These two are included for backward compatibility - only used by python
    // bindings. They should be removed later
    masterServerIP->Save();
    masterServerPort->Save();
}

BackendSettings::~BackendSettings()
{
    if (masterServerIP)
        delete masterServerIP;
    masterServerIP=0;
    if (masterServerPort)
        delete masterServerPort;
    masterServerPort=0;
}
