// POSIX headers
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// ANSI C headers
#include <cstdlib>

// C++ headers
#include <iostream>
using namespace std;

// qt
#include <QDomDocument>
#include <QDir>

// myth
#include <mythcontext.h>
#include <programinfo.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <util.h>

// mytharchive
#include "archiveutil.h"


struct ArchiveDestination ArchiveDestinations[] =
{
    {AD_DVD_SL,
     QT_TRANSLATE_NOOP("SelectDestination", "Single Layer DVD"),
     QT_TRANSLATE_NOOP("SelectDestination", "Single Layer DVD (4482Mb)"),
     4482*1024},
    {AD_DVD_DL,
     QT_TRANSLATE_NOOP("SelectDestination", "Dual Layer DVD"),
     QT_TRANSLATE_NOOP("SelectDestination", "Dual Layer DVD (8964Mb)"),
     8964*1024},
    {AD_DVD_RW,
     QT_TRANSLATE_NOOP("SelectDestination", "DVD +/- RW"),
     QT_TRANSLATE_NOOP("SelectDestination", "Rewritable DVD"),
     4482*1024},
    {AD_FILE,
     QT_TRANSLATE_NOOP("SelectDestination", "File"),
     QT_TRANSLATE_NOOP("SelectDestination", "Any file accessable from your filesystem."),
     -1},
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

QString getTempDirectory(bool showError)
{
    QString tempDir = gCoreContext->GetSetting("MythArchiveTempDir", "");

    if (tempDir == "" && showError)
        ShowOkPopup(QObject::tr("Cannot find the MythArchive work directory.\n"
                                "Have you set the correct path in the settings?"),
                                NULL, NULL);

    if (tempDir == "")
        return "";

    // make sure the temp directory setting ends with a trailing "/"
    if (!tempDir.endsWith("/"))
    {
        tempDir += "/";
        gCoreContext->SaveSetting("MythArchiveTempDir", tempDir);
    }

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
        if( !chmod(qPrintable(tempDir), 0777) )
            VERBOSE(VB_IMPORTANT, QString("Failed to change permissions on archive directory")
                .arg(strerror(errno)));
    }

    dir = QDir(workDir);
    if (!dir.exists())
    {
        dir.mkdir(workDir);
        if( !chmod(qPrintable(workDir), 0777) )
            VERBOSE(VB_IMPORTANT, QString("Failed to change permissions on archive work directory")
                .arg(strerror(errno)));
    }

    dir = QDir(logDir);
    if (!dir.exists())
    {
        dir.mkdir(logDir);
        if( !chmod(qPrintable(logDir), 0777) )
            VERBOSE(VB_IMPORTANT, QString("Failed to change permissions on archive log directory")
                .arg(strerror(errno)));

    }
    dir = QDir(configDir);
    if (!dir.exists())
    {
        dir.mkdir(configDir);
        if( !chmod(qPrintable(configDir), 0777) )
            VERBOSE(VB_IMPORTANT, QString("Failed to change permissions on archive config directory")
                .arg(strerror(errno)));
    }
}

QString getBaseName(const QString &filename)
{
    QString baseName = filename;
    int pos = filename.lastIndexOf('/');
    if (pos > 0)
        baseName = filename.mid(pos + 1);

    return baseName;
}

bool extractDetailsFromFilename(const QString &inFile,
                                QString &chanID, QString &startTime)
{
    VERBOSE(VB_JOBQUEUE, "Extracting details from: " + inFile);

    QString baseName = getBaseName(inFile);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime FROM recorded "
            "WHERE basename = :BASENAME");
    query.bindValue(":BASENAME", baseName);

    if (query.exec() && query.next())
    {
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
        uint chanid = chanID.toUInt();
        QDateTime recstartts = myth_dt_from_string(startTime);
        pinfo = new ProgramInfo(chanid, recstartts);
        if (pinfo->GetChanID())
        {
            pinfo->SetPathname(pinfo->GetPlaybackURL(false, true));
        }
        else
        {
            delete pinfo;
            pinfo = NULL;
        }
    }

    if (!pinfo)
    {
        // file is not a myth recording or is no longer in the db
        pinfo = new ProgramInfo(inFile);
        VERBOSE(VB_JOBQUEUE, "File is not a MythTV recording.");
    }
    else
        VERBOSE(VB_JOBQUEUE, "File is a MythTV recording.");

    return pinfo;
}

bool getFileDetails(ArchiveItem *a)
{
    QString tempDir = gCoreContext->GetSetting("MythArchiveTempDir", "");

    if (!tempDir.endsWith("/"))
        tempDir += "/";

    QString inFile;
    int lenMethod = 0;
    if (a->type == "Recording")
    {
        inFile = a->filename;
        lenMethod = 2;
    }
    else
    {
        inFile = a->filename;
    }

    inFile.replace("\'", "\\\'");
    inFile.replace("\"", "\\\"");
    inFile.replace("`", "\\`");

    QString outFile = tempDir + "/work/file.xml";

    // call mytharchivehelper to get files stream info etc.
    QString command = QString("mytharchivehelper -i \"%1\" \"%2\" %3 > /dev/null 2>&1")
            .arg(inFile).arg(outFile).arg(lenMethod);

    int res = system(qPrintable(command));
    if (WIFEXITED(res))
        res = WEXITSTATUS(res);
    if (res != 0)
        return false;

    QDomDocument doc("mydocument");
    QFile file(outFile);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    if (!doc.setContent( &file ))
    {
        file.close();
        return false;
    }
    file.close();

    // get file type and duration
    QDomElement docElem = doc.documentElement();
    QDomNodeList nodeList = doc.elementsByTagName("file");
    if (nodeList.count() < 1)
        return false;
    QDomNode n = nodeList.item(0);
    QDomElement e = n.toElement();
    a->fileCodec = e.attribute("type");
    a->duration = e.attribute("duration").toInt();
    a->cutDuration = e.attribute("cutduration").toInt();

    // get frame size and video codec
    nodeList = doc.elementsByTagName("video");
    if (nodeList.count() < 1)
        return false;
    n = nodeList.item(0);
    e = n.toElement();
    a->videoCodec = e.attribute("codec");
    a->videoWidth = e.attribute("width").toInt();
    a->videoHeight = e.attribute("height").toInt();

    return true;
}

void showWarningDialog(const QString msg)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythConfirmationDialog *dialog = new MythConfirmationDialog(popupStack, msg, false);

    if (dialog->Create())
        popupStack->AddScreen(dialog);
}

void recalcItemSize(ArchiveItem *item)
{
    EncoderProfile *profile = item->encoderProfile;
    if (!profile)
        return;

    if (profile->name == "NONE")
    {
        if (item->hasCutlist && item->useCutlist)
            item->newsize = (long long) (item->size /
                    ((float)item->duration / (float)item->cutDuration));
        else
            item->newsize = item->size;
    }
    else
    {
        if (item->duration == 0)
            return;

        int length;

        if (item->hasCutlist && item->useCutlist)
            length = item->cutDuration;
        else
            length = item->duration;

        float len = (float) length / 3600;
        item->newsize = (long long) (len * profile->bitrate * 1024 * 1024);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
