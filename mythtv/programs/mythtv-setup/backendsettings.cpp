// C/C++
#include <cstdio>
#include <unistd.h>

// Qt
#include <QNetworkInterface>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythtv/channelsettings.h" // for ChannelTVFormat::GetFormats()
#include "libmythtv/frequencies.h"

// MythTV Setup
#include "backendsettings.h"

static TransMythUICheckBoxSetting *IsMasterBackend()
{
    auto *gc = new TransMythUICheckBoxSetting();
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
    auto *gc = new GlobalTextEditSetting("MasterServerName");
    gc->setLabel(QObject::tr("Master Backend Name"));
    gc->setValue("");
    gc->setEnabled(true);
    gc->setReadOnly(true);
    gc->setHelpText(QObject::tr(
                    "Host name of Master Backend. This is set by selecting "
                    "\"This server is the Master Backend\" on that server."));
    return gc;
};

static HostCheckBoxSetting *AllowConnFromAll()
{
    auto *gc = new HostCheckBoxSetting("AllowConnFromAll");
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
    auto *gc = new HostComboBoxSetting("BackendServerIP");
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
    auto *gc = new HostComboBoxSetting("BackendServerIP6");
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
    auto *hc = new HostCheckBoxSetting("AllowLinkLocal");
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
     HostComboBoxSetting *m_localServerIP;
     HostComboBoxSetting *m_localServerIP6;
     explicit IpAddressSettings(/*Setting* trigger*/) :
         HostCheckBoxSetting("ListenOnAllIps"),
         m_localServerIP(LocalServerIP()),
         m_localServerIP6(LocalServerIP6())
     {
         setLabel(BackendSettings::tr("Listen on All IP Addresses"));
         setValue(true);
         setHelpText(BackendSettings::tr("Allow this backend to receive "
                        "connections on any IP Address assigned to it. "
                        "Recommended for most users for ease and "
                        "reliability."));

         // show ip addresses if ListenOnAllIps is off
         addTargetedChild("0", m_localServerIP);
         addTargetedChild("0", m_localServerIP6);
         addTargetedChild("0", UseLinkLocal());
     };
};


void BackendSettings::LocalServerPortChanged ()
{
    MythCoreContext::ClearBackendServerPortCache();
}

HostTextEditSetting *BackendSettings::LocalServerPort() const
{
    auto *gc = new HostTextEditSetting("BackendServerPort");
    gc->setLabel(QObject::tr("Port"));
    gc->setValue("6543");
    gc->setHelpText(QObject::tr("Unless you've got good reason, don't "
                    "change this."));
    connect(gc,   &StandardSetting::ChangeSaved,
            this, &BackendSettings::LocalServerPortChanged);
    return gc;
};

static HostTextEditSetting *LocalStatusPort()
{
    auto *gc = new HostTextEditSetting("BackendStatusPort");
    gc->setLabel(QObject::tr("Status port"));
    gc->setValue("6544");
    gc->setHelpText(QObject::tr("Port on which the server will listen for "
                    "HTTP requests, including backend status and MythXML "
                    "requests."));
    return gc;
};

static HostComboBoxSetting *BackendServerAddr()
{
    auto *gc = new HostComboBoxSetting("BackendServerAddr", true);
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
    auto *gc = new GlobalTextEditSetting("MasterServerIP");
    gc->setLabel(QObject::tr("IP address"));
    gc->setValue("127.0.0.1");
    return gc;
};

// Deprecated
static GlobalTextEditSetting *MasterServerPort()
{
    auto *gc = new GlobalTextEditSetting("MasterServerPort");
    gc->setLabel(QObject::tr("Port"));
    gc->setValue("6543");
    return gc;
};

static HostTextEditSetting *LocalSecurityPin()
{
    auto *gc = new HostTextEditSetting("SecurityPin");
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
    auto *gc = new GlobalComboBoxSetting("TVFormat");
    gc->setLabel(QObject::tr("TV format"));

    QStringList list = ChannelTVFormat::GetFormats();
    for (const QString& item : std::as_const(list))
        gc->addSelection(item);

    gc->setHelpText(QObject::tr("The TV standard to use for viewing TV."));
    return gc;
};

static GlobalComboBoxSetting *VbiFormat()
{
    auto *gc = new GlobalComboBoxSetting("VbiFormat");
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
    auto *gc = new GlobalComboBoxSetting("FreqTable");
    gc->setLabel(QObject::tr("Channel frequency table"));

    for (const auto &list : gChanLists)
        gc->addSelection(list.name);

    gc->setHelpText(QObject::tr("Select the appropriate frequency table for "
                    "your system. If you have an antenna, use a \"-bcast\" "
                    "frequency."));
    return gc;
};

static GlobalCheckBoxSetting *SaveTranscoding()
{
    auto *gc = new GlobalCheckBoxSetting("SaveTranscoding");
    gc->setLabel(QObject::tr("Save original files after transcoding (globally)"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled and the transcoder is active, the "
                    "original files will be renamed to .old once the "
                    "transcoding is complete."));
    return gc;
};

static HostCheckBoxSetting *TruncateDeletes()
{
    auto *hc = new HostCheckBoxSetting("TruncateDeletesSlowly");
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
    auto *gc = new GlobalCheckBoxSetting("DeletesFollowLinks");
    gc->setLabel(QObject::tr("Follow symbolic links when deleting files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, MythTV will follow symlinks "
                    "when recordings and related files are deleted, instead "
                    "of deleting the symlink and leaving the actual file."));
    return gc;
};

static GlobalSpinBoxSetting *HDRingbufferSize()
{
    auto *bs = new GlobalSpinBoxSetting(
        "HDRingbufferSize", 25*188, 500*188, 25*188);
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
    auto *gc = new GlobalComboBoxSetting("StorageScheduler");
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
    auto *gc = new GlobalCheckBoxSetting("DisableAutomaticBackup");
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
    auto *hc = new HostCheckBoxSetting("DisableFirewireReset");
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
    auto *he = new HostTextEditSetting("MiscStatusScript");
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
    auto *gc = new GlobalSpinBoxSetting("EITTransportTimeout", 1, 15, 1);
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
    auto *gc = new GlobalCheckBoxSetting("MasterBackendOverride");
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
    auto *gc = new GlobalSpinBoxSetting("EITCrawIdleStart", 30, 7200, 30);
    gc->setLabel(QObject::tr("Backend idle before EIT crawl (secs)"));
    gc->setValue(60);
    QString help = QObject::tr(
        "The minimum number of seconds after a recorder becomes idle "
        "to wait before MythTV begins collecting EIT listings data.");
    gc->setHelpText(help);
    return gc;
}

static GlobalSpinBoxSetting *EITScanPeriod()
{
    auto *gc = new GlobalSpinBoxSetting("EITScanPeriod", 5, 60, 5);
    gc->setLabel(QObject::tr("EIT scan period (mins)"));
    gc->setValue(15);
    QString helpText = QObject::tr(
        "Time to do EIT scanning on one capture card before moving "
        "to the next capture card in the same input group that is "
        "configured for EIT scanning. This can happen with multiple "
        "satellite LNBs connected via a DiSEqC switch.");
    gc->setHelpText(helpText);
    return gc;
}

static GlobalSpinBoxSetting *EITEventChunkSize()
{
    auto *gc = new GlobalSpinBoxSetting("EITEventChunkSize", 20, 1000, 20);
    gc->setLabel(QObject::tr("EIT event chunk size"));
    gc->setValue(20);
    QString helpText = QObject::tr(
        "Maximum number of DB inserts per ProcessEvents call. "
        "This limits the rate at which EIT events are processed "
        "in the backend so that there is always enough processing "
        "capacity for the other backend tasks.");
    gc->setHelpText(helpText);
    return gc;
}

static GlobalCheckBoxSetting *EITCachePersistent()
{
    auto *gc = new GlobalCheckBoxSetting("EITCachePersistent");
    gc->setLabel(QObject::tr("EIT cache persistent"));
    gc->setValue(true);
    QString helpText = QObject::tr(
        "Save the content of the EIT cache in the database "
        "and use that at the next start of the backend. "
        "This reduces EIT event processing at a restart of the backend but at the "
        "cost of updating the copy of the EIT cache in the database continuously.");
    gc->setHelpText(helpText);
    return gc;
}

static GlobalSpinBoxSetting *WOLbackendReconnectWaitTime()
{
    auto *gc = new GlobalSpinBoxSetting("WOLbackendReconnectWaitTime", 0, 1200, 5);
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
    auto *gc = new GlobalSpinBoxSetting("WOLbackendConnectRetry", 1, 60, 1);
    gc->setLabel(QObject::tr("Wake attempts"));
    gc->setHelpText(QObject::tr("Number of times the frontend will try to wake "
                    "up the master backend."));
    gc->setValue(5);
    return gc;
};

static GlobalTextEditSetting *WOLbackendCommand()
{
    auto *gc = new GlobalTextEditSetting("WOLbackendCommand");
    gc->setLabel(QObject::tr("Wake command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to wake up your master "
            "backend server (e.g. wakeonlan 00:00:00:00:00:00)."));
    return gc;
};

static HostTextEditSetting *SleepCommand()
{
    auto *gc = new HostTextEditSetting("SleepCommand");
    gc->setLabel(QObject::tr("Sleep command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to put this slave to sleep. "
                    "If set, the master backend will use this command to put "
                    "this slave to sleep when it is not needed for recording."));
    return gc;
};

static HostTextEditSetting *WakeUpCommand()
{
    auto *gc = new HostTextEditSetting("WakeUpCommand");
    gc->setLabel(QObject::tr("Wake command"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to wake up this slave "
                    "from sleep. This setting is not used on the master "
                    "backend."));
    return gc;
};

static GlobalTextEditSetting *BackendStopCommand()
{
    auto *gc = new GlobalTextEditSetting("BackendStopCommand");
    gc->setLabel(QObject::tr("Backend stop command"));
    gc->setValue("killall mythbackend");
    gc->setHelpText(QObject::tr("The command used to stop the backend"
                    " when running on the master backend server "
                    "(e.g. sudo /etc/init.d/mythtv-backend stop)"));
    return gc;
};

static GlobalTextEditSetting *BackendStartCommand()
{
    auto *gc = new GlobalTextEditSetting("BackendStartCommand");
    gc->setLabel(QObject::tr("Backend start command"));
    gc->setValue("mythbackend");
    gc->setHelpText(QObject::tr("The command used to start the backend"
                    " when running on the master backend server "
                    "(e.g. sudo /etc/init.d/mythtv-backend start)."));
    return gc;
};

static GlobalSpinBoxSetting *idleTimeoutSecs()
{
    auto *gc = new GlobalSpinBoxSetting("idleTimeoutSecs", 0, 1200, 5);
    gc->setLabel(QObject::tr("Idle shutdown timeout (secs)"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("The number of seconds the master backend "
                    "idles before it shuts down all other backends. Set to 0 to "
                    "disable automatic shutdown."));
    return gc;
};

static GlobalSpinBoxSetting *idleWaitForRecordingTime()
{
    auto *gc = new GlobalSpinBoxSetting("idleWaitForRecordingTime", 0, 300, 1);
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
    auto *gc = new GlobalSpinBoxSetting("StartupSecsBeforeRecording", 0, 1200, 5);
    gc->setLabel(QObject::tr("Startup before recording (secs)"));
    gc->setValue(120);
    gc->setHelpText(QObject::tr("The number of seconds the master backend "
                    "will be woken up before a recording starts."));
    return gc;
};

static GlobalTextEditSetting *WakeupTimeFormat()
{
    auto *gc = new GlobalTextEditSetting("WakeupTimeFormat");
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
    auto *gc = new GlobalTextEditSetting("SetWakeuptimeCommand");
    gc->setLabel(QObject::tr("Command to set wakeup time"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command used to set the wakeup time "
                                "(passed as $time) for the Master Backend"));
    return gc;
};

static GlobalTextEditSetting *ServerHaltCommand()
{
    auto *gc = new GlobalTextEditSetting("ServerHaltCommand");
    gc->setLabel(QObject::tr("Server halt command"));
    gc->setValue("sudo /sbin/halt -p");
    gc->setHelpText(QObject::tr("The command used to halt the backends."));
    return gc;
};

static GlobalTextEditSetting *preSDWUCheckCommand()
{
    auto *gc = new GlobalTextEditSetting("preSDWUCheckCommand");
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
    auto *gc = new GlobalCheckBoxSetting("blockSDWUwithoutClient");
    gc->setLabel(QObject::tr("Block shutdown before client connected"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the automatic shutdown routine will "
                    "be disabled until a client connects."));
    return gc;
};

static GlobalTextEditSetting *startupCommand()
{
    auto *gc = new GlobalTextEditSetting("startupCommand");
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
    auto *gc = new HostSpinBoxSetting("JobQueueMaxSimultaneousJobs", 1, 10, 1);
    gc->setLabel(QObject::tr("Maximum simultaneous jobs on this backend"));
    gc->setHelpText(QObject::tr("The Job Queue will be limited to running "
                    "this many simultaneous jobs on this backend."));
    gc->setValue(1);
    return gc;
};

static HostSpinBoxSetting *JobQueueCheckFrequency()
{
    auto *gc = new HostSpinBoxSetting("JobQueueCheckFrequency", 5, 300, 5);
    gc->setLabel(QObject::tr("Job Queue check frequency (secs)"));
    gc->setHelpText(QObject::tr("When looking for new jobs to process, the "
                    "Job Queue will wait this many seconds between checks."));
    gc->setValue(60);
    return gc;
};

static HostComboBoxSetting *JobQueueCPU()
{
    auto *gc = new HostComboBoxSetting("JobQueueCPU");
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
    auto *gc = new HostTimeBoxSetting("JobQueueWindowStart", "00:00");
    gc->setLabel(QObject::tr("Job Queue start time"));
    gc->setHelpText(QObject::tr("This setting controls the start of the "
                    "Job Queue time window, which determines when new jobs "
                    "will be started."));
    return gc;
};

static HostTimeBoxSetting *JobQueueWindowEnd()
{
    auto *gc = new HostTimeBoxSetting("JobQueueWindowEnd", "23:59");
    gc->setLabel(QObject::tr("Job Queue end time"));
    gc->setHelpText(QObject::tr("This setting controls the end of the "
                    "Job Queue time window, which determines when new jobs "
                    "will be started."));
    return gc;
};

static GlobalCheckBoxSetting *JobsRunOnRecordHost()
{
    auto *gc = new GlobalCheckBoxSetting("JobsRunOnRecordHost");
    gc->setLabel(QObject::tr("Run jobs only on original recording backend"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, jobs in the queue will be required "
                                "to run on the backend that made the "
                                "original recording."));
    return gc;
};

static GlobalCheckBoxSetting *AutoTranscodeBeforeAutoCommflag()
{
    auto *gc = new GlobalCheckBoxSetting("AutoTranscodeBeforeAutoCommflag");
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
    auto *gc = new GlobalCheckBoxSetting("AutoCommflagWhileRecording");
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
    auto *gc = new GlobalTextEditSetting(QString("UserJob%1").arg(job_num));
    gc->setLabel(QObject::tr("User Job #%1 command").arg(job_num));
    gc->setValue("");
    gc->setHelpText(QObject::tr("The command to run whenever this User Job "
                    "number is scheduled."));
    return gc;
};

static GlobalTextEditSetting *UserJobDesc(uint job_num)
{
    auto *gc = new GlobalTextEditSetting(QString("UserJobDesc%1")
                                            .arg(job_num));
    gc->setLabel(QObject::tr("User Job #%1 description").arg(job_num));
    gc->setValue(QObject::tr("User Job #%1").arg(job_num));
    gc->setHelpText(QObject::tr("The description for this User Job."));
    return gc;
};

static HostCheckBoxSetting *JobAllowMetadata()
{
    auto *gc = new HostCheckBoxSetting("JobAllowMetadata");
    gc->setLabel(QObject::tr("Allow metadata lookup jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static HostCheckBoxSetting *JobAllowCommFlag()
{
    auto *gc = new HostCheckBoxSetting("JobAllowCommFlag");
    gc->setLabel(QObject::tr("Allow commercial-detection jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static HostCheckBoxSetting *JobAllowTranscode()
{
    auto *gc = new HostCheckBoxSetting("JobAllowTranscode");
    gc->setLabel(QObject::tr("Allow transcoding jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static HostCheckBoxSetting *JobAllowPreview()
{
    auto *gc = new HostCheckBoxSetting("JobAllowPreview");
    gc->setLabel(QObject::tr("Allow preview jobs"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, allow jobs of this type to "
                                "run on this backend."));
    return gc;
};

static GlobalTextEditSetting *JobQueueTranscodeCommand()
{
    auto *gc = new GlobalTextEditSetting("JobQueueTranscodeCommand");
    gc->setLabel(QObject::tr("Transcoder command"));
    gc->setValue("mythtranscode");
    gc->setHelpText(QObject::tr("The program used to transcode recordings. "
                    "The default is 'mythtranscode' if this setting is empty."));
    return gc;
};

static GlobalTextEditSetting *JobQueueCommFlagCommand()
{
    auto *gc = new GlobalTextEditSetting("JobQueueCommFlagCommand");
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

    auto *bc = new HostCheckBoxSetting(dbStr);
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
    auto *gc = new GlobalComboBoxSetting("UPnP/WMPSource");
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
    auto *bc = new GlobalCheckBoxSetting("MythFillEnabled");
    bc->setLabel(QObject::tr("Automatically update program listings"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("If enabled, the guide data program "
                                "will be run automatically."));
    return bc;
}

static GlobalSpinBoxSetting *MythFillMinHour()
{
    auto *bs = new GlobalSpinBoxSetting("MythFillMinHour", 0, 23, 1);
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
    auto *bs = new GlobalSpinBoxSetting("MythFillMaxHour", 0, 23, 1);
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
    auto *bc = new GlobalCheckBoxSetting("MythFillGrabberSuggestsTime");
    bc->setLabel(QObject::tr("Run guide data program at time suggested by the "
                             "grabber."));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("If enabled, allow a guide data "
                    "provider to specify the next download time in order "
                    "to distribute load on their servers. Guide data program "
                    "execution start/end times are also ignored."));
    return bc;
}

static GlobalTextEditSetting *MythFillDatabasePath()
{
    auto *be = new GlobalTextEditSetting("MythFillDatabasePath");
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
    auto *be = new GlobalTextEditSetting("MythFillDatabaseArgs");
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

BackendSettings::BackendSettings()
  : m_isMasterBackend(IsMasterBackend()),
    m_localServerPort(LocalServerPort()),
    m_backendServerAddr(BackendServerAddr()),
    m_masterServerName(MasterServerName()),
    m_ipAddressSettings(new IpAddressSettings()),
    // These two are included for backward compatibility -
    // used by python bindings. They could be removed later
    m_masterServerIP(MasterServerIP()),
    m_masterServerPort(MasterServerPort())
{
    //++ Host Address Backend Setup ++
    auto* server = new GroupSetting();
    server->setLabel(tr("Host Address Backend Setup"));
    server->addChild(m_localServerPort);
    server->addChild(LocalStatusPort());
    server->addChild(LocalSecurityPin());
    server->addChild(AllowConnFromAll());
    //+++ IP Addresses +++
    server->addChild(m_ipAddressSettings);
    connect(m_ipAddressSettings, &HostCheckBoxSetting::valueChanged,
            this, &BackendSettings::listenChanged);
    connect(m_ipAddressSettings->m_localServerIP,
            static_cast<void (StandardSetting::*)(const QString&)>(&StandardSetting::valueChanged),
            this, &BackendSettings::listenChanged);
    connect(m_ipAddressSettings->m_localServerIP6,
            static_cast<void (StandardSetting::*)(const QString&)>(&StandardSetting::valueChanged),
            this, &BackendSettings::listenChanged);
    server->addChild(m_backendServerAddr);
    //++ Master Backend ++
    connect(m_isMasterBackend, &TransMythUICheckBoxSetting::valueChanged,
            this, &BackendSettings::masterBackendChanged);
    server->addChild(m_isMasterBackend);
    server->addChild(m_masterServerName);
    addChild(server);

    //++ Locale Settings ++
    auto* locale = new GroupSetting();
    locale->setLabel(QObject::tr("Locale Settings"));
    locale->addChild(TVFormat());
    locale->addChild(VbiFormat());
    locale->addChild(FreqTable());
    addChild(locale);

    auto* group2 = new GroupSetting();
    group2->setLabel(QObject::tr("Miscellaneous Settings"));

    auto* fm = new GroupSetting();
    fm->setLabel(QObject::tr("File Management Settings"));
    fm->addChild(MasterBackendOverride());
    fm->addChild(DeletesFollowLinks());
    fm->addChild(TruncateDeletes());
    fm->addChild(HDRingbufferSize());
    fm->addChild(StorageScheduler());
    group2->addChild(fm);
    auto* upnp = new GroupSetting();
    upnp->setLabel(QObject::tr("UPnP Server Settings"));
    //upnp->addChild(UPNPShowRecordingUnderVideos());
    upnp->addChild(UPNPWmpSource());
    group2->addChild(upnp);
    group2->addChild(MiscStatusScript());
    group2->addChild(DisableAutomaticBackup());
    group2->addChild(DisableFirewireReset());
    addChild(group2);

    auto* group2a1 = new GroupSetting();
    group2a1->setLabel(QObject::tr("EIT Scanner Options"));
    group2a1->addChild(EITTransportTimeout());
    group2a1->addChild(EITCrawIdleStart());
    group2a1->addChild(EITScanPeriod());
    group2a1->addChild(EITEventChunkSize());
    group2a1->addChild(EITCachePersistent());
    addChild(group2a1);

    auto* group3 = new GroupSetting();
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

    auto* group4 = new GroupSetting();
    group4->setLabel(QObject::tr("Backend Wakeup settings"));

    auto* backend = new GroupSetting();
    backend->setLabel(QObject::tr("Master Backend"));
    backend->addChild(WOLbackendReconnectWaitTime());
    backend->addChild(WOLbackendConnectRetry());
    backend->addChild(WOLbackendCommand());
    group4->addChild(backend);

    auto* slaveBackend = new GroupSetting();
    slaveBackend->setLabel(QObject::tr("Slave Backends"));
    slaveBackend->addChild(SleepCommand());
    slaveBackend->addChild(WakeUpCommand());
    group4->addChild(slaveBackend);
    addChild(group4);

    auto* backendControl = new GroupSetting();
    backendControl->setLabel(QObject::tr("Backend Control"));
    backendControl->addChild(BackendStopCommand());
    backendControl->addChild(BackendStartCommand());
    addChild(backendControl);

    auto* group5 = new GroupSetting();
    group5->setLabel(QObject::tr("Job Queue (Backend-Specific)"));
    group5->addChild(JobQueueMaxSimultaneousJobs());
    group5->addChild(JobQueueCheckFrequency());
    group5->addChild(JobQueueWindowStart());
    group5->addChild(JobQueueWindowEnd());
    group5->addChild(JobQueueCPU());
    group5->addChild(JobAllowMetadata());
    group5->addChild(JobAllowCommFlag());
    group5->addChild(JobAllowTranscode());
    group5->addChild(JobAllowPreview());
    group5->addChild(JobAllowUserJob(1));
    group5->addChild(JobAllowUserJob(2));
    group5->addChild(JobAllowUserJob(3));
    group5->addChild(JobAllowUserJob(4));
    addChild(group5);

    auto* group6 = new GroupSetting();
    group6->setLabel(QObject::tr("Job Queue (Global)"));
    group6->addChild(JobsRunOnRecordHost());
    group6->addChild(AutoCommflagWhileRecording());
    group6->addChild(JobQueueCommFlagCommand());
    group6->addChild(JobQueueTranscodeCommand());
    group6->addChild(AutoTranscodeBeforeAutoCommflag());
    group6->addChild(SaveTranscoding());
    addChild(group6);

    auto* group7 = new GroupSetting();
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

    auto *mythfill = new MythFillSettings();
    addChild(mythfill);

}

void BackendSettings::masterBackendChanged()
{
    if (!m_isLoaded)
        return;
    bool ismasterchecked = m_isMasterBackend->boolValue();
    if (ismasterchecked)
        m_masterServerName->setValue(gCoreContext->GetHostName());
    else
        m_masterServerName->setValue(m_priorMasterName);
}

void BackendSettings::listenChanged()
{
    if (!m_isLoaded)
        return;
    bool addrChanged = m_backendServerAddr->haveChanged();
    QString currentsetting = m_backendServerAddr->getValue();
    m_backendServerAddr->clearSelections();
    if (m_ipAddressSettings->boolValue())
    {
        QList<QHostAddress> list = QNetworkInterface::allAddresses();
        QList<QHostAddress>::iterator it;
        for (it = list.begin(); it != list.end(); ++it)
        {
            it->setScopeId(QString());
            m_backendServerAddr->addSelection((*it).toString(), (*it).toString());
        }
    }
    else
    {
        m_backendServerAddr->addSelection(
            m_ipAddressSettings->m_localServerIP->getValue());
        m_backendServerAddr->addSelection(
            m_ipAddressSettings->m_localServerIP6->getValue());
    }
    // Remove the blank entry that is caused by clearSelections
    // TODO probably not needed anymore?
    // m_backendServerAddr->removeSelection(QString());

    QHostAddress addr;
    if (addr.setAddress(currentsetting))
    {
        // if prior setting is an ip address
        // it only if it is now in the list
        if (m_backendServerAddr->getValueIndex(currentsetting)
                > -1)
            m_backendServerAddr->setValue(currentsetting);
        else
            m_backendServerAddr->setValue(0);
    }
    else if (! currentsetting.isEmpty())
    {
        // if prior setting was not an ip address, it must
        // have been a dns name so add it back and select it.
        m_backendServerAddr->addSelection(currentsetting);
        m_backendServerAddr->setValue(currentsetting);
    }
    else
    {
        m_backendServerAddr->setValue(0);
    }
    m_backendServerAddr->setChanged(addrChanged);
}


void BackendSettings::Load(void)
{
    m_isLoaded=false;
    GroupSetting::Load();

    // These two are included for backward compatibility - only used by python
    // bindings. They should be removed later
    m_masterServerIP->Load();
    m_masterServerPort->Load();

    QString mastername = m_masterServerName->getValue();
    // new installation - default to master
    bool newInstall=false;
    if (mastername.isEmpty())
    {
        mastername = gCoreContext->GetHostName();
        newInstall=true;
    }
    bool ismaster = (mastername == gCoreContext->GetHostName());
    m_isMasterBackend->setValue(ismaster);
    m_priorMasterName = mastername;
    m_isLoaded=true;
    masterBackendChanged();
    listenChanged();
    if (!newInstall)
    {
        m_backendServerAddr->setChanged(false);
        m_isMasterBackend->setChanged(false);
        m_masterServerName->setChanged(false);
    }
}

void BackendSettings::Save(void)
{
    // Setup deprecated backward compatibility settings
    if (m_isMasterBackend->boolValue())
    {
        QString addr = m_backendServerAddr->getValue();
        QString ip = MythCoreContext::resolveAddress(addr);
        m_masterServerIP->setValue(ip);
        m_masterServerPort->setValue(m_localServerPort->getValue());
    }

    // If listen on all is specified, set up values for the
    // specific IPV4 and IPV6 addresses for backward
    // compatibilty with other things that may use them
    if (m_ipAddressSettings->boolValue())
    {
        QString bea = m_backendServerAddr->getValue();
        // initialize them to localhost values
        m_ipAddressSettings->m_localServerIP->setValue(0);
        m_ipAddressSettings->m_localServerIP6->setValue(0);
        QString ip4 = MythCoreContext::resolveAddress
            (bea,MythCoreContext::ResolveIPv4);
        QString ip6 = MythCoreContext::resolveAddress
            (bea,MythCoreContext::ResolveIPv6);
        // the setValue calls below only set the value if it is in the list.
        m_ipAddressSettings->m_localServerIP->setValue(ip4);
        m_ipAddressSettings->m_localServerIP6->setValue(ip6);
    }

    GroupSetting::Save();

    // These two are included for backward compatibility - only used by python
    // bindings. They should be removed later
    m_masterServerIP->Save();
    m_masterServerPort->Save();
}

BackendSettings::~BackendSettings()
{
    delete m_masterServerIP;
    m_masterServerIP=nullptr;
    delete m_masterServerPort;
    m_masterServerPort=nullptr;
}
