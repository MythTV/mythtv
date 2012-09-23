// C++ includes
#include <iostream>

// libmyth* includes
#include "exitcodes.h"
#include "jobqueue.h"
#include "mythlogging.h"

// Local includes
#include "jobutils.h"

static int QueueJob(const MythUtilCommandLineParser &cmdline)
{
    ProgramInfo pginfo;
    if (!GetProgramInfo(cmdline, pginfo))
        return GENERIC_EXIT_NO_RECORDING_DATA;

    bool rebuildSeektable = false;
    int jobType = JOB_NONE;

    if (cmdline.toString("queuejob") == "transcode")
        jobType = JOB_TRANSCODE;
    else if (cmdline.toString("queuejob") == "commflag")
        jobType = JOB_COMMFLAG;
    else if (cmdline.toString("queuejob") == "rebuild")
    {
        jobType = JOB_COMMFLAG;
        rebuildSeektable = true;
    }
    else if (cmdline.toString("queuejob") == "metadata")
        jobType = JOB_METADATA;
    else if (cmdline.toString("queuejob") == "userjob1")
        jobType = JOB_USERJOB1;
    else if (cmdline.toString("queuejob") == "userjob2")
        jobType = JOB_USERJOB2;
    else if (cmdline.toString("queuejob") == "userjob3")
        jobType = JOB_USERJOB3;
    else if (cmdline.toString("queuejob") == "userjob4")
        jobType = JOB_USERJOB4;
    else if (cmdline.toInt("queuejob") > 0)
        jobType = cmdline.toInt("queuejob");

    if (jobType == JOB_NONE)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Error, invalid job type given with queuejob option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    
    bool result = JobQueue::QueueJob(jobType,
        pginfo.GetChanID(), pginfo.GetRecordingStartTime(), "", "", "",
        rebuildSeektable, JOB_QUEUED, QDateTime());

    if (result)
    {
        QString tmp = QString("%1 Job Queued for chanid %2 @ %3")
            .arg(cmdline.toString("queuejob"))
            .arg(pginfo.GetChanID())
            .arg(pginfo.GetRecordingStartTime().toString());
        cerr << tmp.toLocal8Bit().constData() << endl;
        return GENERIC_EXIT_OK;
    }

    QString tmp = QString("Error queueing job for chanid %1 @ %2")
        .arg(pginfo.GetChanID())
        .arg(pginfo.GetRecordingStartTime().toString());
    cerr << tmp.toLocal8Bit().constData() << endl;
    return GENERIC_EXIT_DB_ERROR;
}

void registerJobUtils(UtilMap &utilMap)
{
    utilMap["queuejob"]            = &QueueJob;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
