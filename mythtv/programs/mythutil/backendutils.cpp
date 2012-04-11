// libmyth* headers
#include "exitcodes.h"
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "remoteutil.h"
#include "scheduledrecording.h"

// local headers
#include "backendutils.h"

static int RawSendEvent(const QStringList &eventStringList)
{
    if (eventStringList.isEmpty() || eventStringList[0].isEmpty())
        return GENERIC_EXIT_INVALID_CMDLINE;

    if (gCoreContext->ConnectToMasterServer(false, false))
    {
        QStringList message("MESSAGE");
        message << eventStringList;
        gCoreContext->SendReceiveStringList(message);
        return GENERIC_EXIT_OK;
    }
    return GENERIC_EXIT_CONNECT_ERROR;
}

static int ClearSettingsCache(const MythUtilCommandLineParser &cmdline)
{
    if (gCoreContext->ConnectToMasterServer(false, false))
    {
        gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");
        LOG(VB_GENERAL, LOG_INFO, "Sent CLEAR_SETTINGS_CACHE message");
        return GENERIC_EXIT_OK;
    }

    LOG(VB_GENERAL, LOG_ERR, "Unable to connect to backend, settings "
        "cache will not be cleared.");
    return GENERIC_EXIT_CONNECT_ERROR;
}

static int SendEvent(const MythUtilCommandLineParser &cmdline)
{
    return RawSendEvent(cmdline.toStringList("event"));
}

static int SendSystemEvent(const MythUtilCommandLineParser &cmdline)
{
    return RawSendEvent(QStringList(QString("SYSTEM_EVENT %1 SENDER %2")
                                    .arg(cmdline.toString("systemevent"))
                                    .arg(gCoreContext->GetHostName())));
}

static int Reschedule(const MythUtilCommandLineParser &cmdline)
{
    if (gCoreContext->ConnectToMasterServer(false, false))
    {
        ScheduledRecording::RescheduleMatch(0, 0, 0, QDateTime(),
                                            "MythUtilCommand");
        LOG(VB_GENERAL, LOG_INFO, "Reschedule command sent to master");
        return GENERIC_EXIT_OK;
    }

    LOG(VB_GENERAL, LOG_ERR, "Cannot connect to master for reschedule");
    return GENERIC_EXIT_CONNECT_ERROR;
}

static int ScanVideos(const MythUtilCommandLineParser &cmdline)
{
    if (gCoreContext->ConnectToMasterServer(false, false))
    {
        gCoreContext->SendReceiveStringList(QStringList() << "SCAN_VIDEOS");
        LOG(VB_GENERAL, LOG_INFO, "Requested video scan");
        return GENERIC_EXIT_OK;
    }

    LOG(VB_GENERAL, LOG_ERR, "Cannot connect to master for video scan");
    return GENERIC_EXIT_CONNECT_ERROR;
}

void registerBackendUtils(UtilMap &utilMap)
{
    utilMap["clearcache"]           = &ClearSettingsCache;
    utilMap["event"]                = &SendEvent;
    utilMap["resched"]              = &Reschedule;
    utilMap["scanvideos"]           = &ScanVideos;
    utilMap["systemevent"]          = &SendSystemEvent;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
