#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <qdir.h>

// myth
#include <mythtv/mythcontext.h>
#include <libmythtv/programinfo.h>

// mytharchive
#include "archiveutil.h"


struct ArchiveDestination ArchiveDestinations[] =
{
    {AD_DVD_SL,   "Single Layer DVD", "Single Layer DVD (4489Mb)", 4489*1024},
    {AD_DVD_DL,   "Dual Layer DVD",   "Dual Layer DVD (8979Mb)",   8979*1024},
    {AD_DVD_RW,   "DVD +/- RW",       "Rewritable DVD",            4489*1024},
    {AD_FILE,     "File",             "Any file accessable from "
            "your filesystem.",          -1},
};

int ArchiveDestinationsCount = sizeof(ArchiveDestinations) / sizeof(ArchiveDestinations[0]);

QString formatSize(long long sizeKB, int prec)
{
    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QString("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QString("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QString("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:prec);
    }
    // Kilobytes
    return QString("%1 KB").arg(sizeKB);
}

QString getTempDirectory()
{
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");

    if (tempDir == "")
        return "";

    if (!tempDir.endsWith("/"))
        tempDir += "/";

    return tempDir;
}

void checkTempDirectory()
{
    QString tempDir = getTempDirectory();
    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    // make sure the 'work', 'logs', and 'config' directories exist
    QDir dir(tempDir);
    if (!dir.exists())
    {
        dir.mkdir(tempDir);
        system("chmod 777 " + tempDir);
    }

    dir = QDir(workDir);
    if (!dir.exists())
    {
        dir.mkdir(workDir);
        system("chmod 777 " + workDir);
    }

    dir = QDir(logDir);
    if (!dir.exists())
    {
        dir.mkdir(logDir);
        system("chmod 777 " + logDir);
    }
    dir = QDir(configDir);
    if (!dir.exists())
    {
        dir.mkdir(configDir);
        system("chmod 777 " + configDir);
    }
}

bool extractDetailsFromFilename(const QString &inFile,
                                QString &chanID, QString &startTime)
{
    VERBOSE(VB_JOBQUEUE, "Extracting details from: " + inFile);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime FROM recorded "
            "WHERE basename = :BASENAME");
    query.bindValue(":BASENAME", inFile);

    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        query.first();
        chanID = query.value(0).toString();
        startTime= query.value(1).toString();
    }
    else
    {
        VERBOSE(VB_JOBQUEUE, 
                QString("Cannot find details for %1").arg(inFile));
        return false;
    }

    VERBOSE(VB_JOBQUEUE, QString("chanid: %1 starttime:%2 ").arg(chanID).arg(startTime));

    return true;
}

ProgramInfo *getProgramInfoForFile(const QString &inFile)
{
    ProgramInfo *pinfo = NULL;
    QString chanID, startTime;
    bool bIsMythRecording = false;

    bIsMythRecording = extractDetailsFromFilename(inFile, chanID, startTime);

    if (bIsMythRecording)
    {
        pinfo = ProgramInfo::GetProgramFromRecorded(chanID, startTime);

        if (pinfo)
        {
            // file is a myth recording
            pinfo->pathname = gContext->GetSetting("RecordFilePrefix") + "/" +
                    pinfo->pathname;
        }
    }

    if (!pinfo)
    {
        // file is not a myth recording or is no longer in the db
        pinfo = new ProgramInfo();
        pinfo->pathname = inFile;
        pinfo->isVideo = true;
        VERBOSE(VB_JOBQUEUE, "File is not a Myth recording.");
    }
    else
        VERBOSE(VB_JOBQUEUE, "File is a Myth recording.");

    return pinfo;
}
