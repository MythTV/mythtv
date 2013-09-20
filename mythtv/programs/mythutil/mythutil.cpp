// Qt includes
#include <QDateTime>
#include <QString>

// libmyth* includes
#include "mythlogging.h"

// Local includes
#include "mythutil.h"

bool GetProgramInfo(const MythUtilCommandLineParser &cmdline,
                    ProgramInfo &pginfo)
{
    if (cmdline.toBool("video"))
    {
        QString video = cmdline.toString("video");
        pginfo = ProgramInfo(video);
        return true;
    }
    if (!cmdline.toBool("chanid"))
    {
        LOG(VB_GENERAL, LOG_ERR, "No chanid specified");
        return false;
    }

    if (!cmdline.toBool("starttime"))
    {
        LOG(VB_GENERAL, LOG_ERR, "No start time specified");
        return false;
    }

    uint chanid = cmdline.toUInt("chanid");
    QDateTime starttime = cmdline.toDateTime("starttime");
    QString startstring = starttime.toString("yyyyMMddhhmmss");

    const ProgramInfo tmpInfo(chanid, starttime);

    if (!tmpInfo.GetChanID())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("No program data exists for channel %1 at %2")
                .arg(chanid).arg(startstring));
        return false;
    }

    pginfo = tmpInfo;

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
