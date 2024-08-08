// C++ headers
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

// qt
#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>

// myth
#include <libmyth/mythcontext.h>
#include <libmythbase/exitcodes.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythbase/programinfo.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>

// mytharchive
#include "archiveutil.h"

std::vector<ArchiveDestination> ArchiveDestinations
{
    {AD_DVD_SL,
     QT_TRANSLATE_NOOP("SelectDestination", "Single Layer DVD"),
     QT_TRANSLATE_NOOP("SelectDestination", "Single Layer DVD (4,482 MB)"),
     4482*1024LL},
    {AD_DVD_DL,
     QT_TRANSLATE_NOOP("SelectDestination", "Dual Layer DVD"),
     QT_TRANSLATE_NOOP("SelectDestination", "Dual Layer DVD (8,964 MB)"),
     8964*1024LL},
    {AD_DVD_RW,
     QT_TRANSLATE_NOOP("SelectDestination", "DVD +/- RW"),
     QT_TRANSLATE_NOOP("SelectDestination", "Rewritable DVD"),
     4482*1024LL},
    {AD_FILE,
     QT_TRANSLATE_NOOP("SelectDestination", "File"),
     QT_TRANSLATE_NOOP("SelectDestination", "Any file accessable from your filesystem."),
     -1LL},
};

QString getTempDirectory(bool showError)
{
    QString tempDir = gCoreContext->GetSetting("MythArchiveTempDir", "");

    if (tempDir == "" && showError)
    {
        ShowOkPopup(QCoreApplication::translate("(ArchiveUtils)", 
            "Cannot find the MythArchive work directory.\n"
            "Have you set the correct path in the settings?"));
    }

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
        if( chmod(qPrintable(tempDir), 0777) != 0 )
            LOG(VB_GENERAL, LOG_ERR,
                "Failed to change permissions on archive directory: " + ENO);
    }

    dir.setPath(workDir);;
    if (!dir.exists())
    {
        dir.mkdir(workDir);
        if( chmod(qPrintable(workDir), 0777) != 0 )
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Failed to change permissions on archive work directory: " +
                ENO);
        }
    }

    dir.setPath(logDir);;
    if (!dir.exists())
    {
        dir.mkdir(logDir);
        if( chmod(qPrintable(logDir), 0777) != 0 )
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Failed to change permissions on archive log directory: " +
                ENO);
        }
    }
    dir.setPath(configDir);;
    if (!dir.exists())
    {
        dir.mkdir(configDir);
        if( chmod(qPrintable(configDir), 0777) != 0 )
        {
            LOG(VB_GENERAL, LOG_ERR, 
                "Failed to change permissions on archive config directory: " +
                ENO);
        }
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
    LOG(VB_JOBQUEUE, LOG_INFO, "Extracting details from: " + inFile);

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
        LOG(VB_JOBQUEUE, LOG_ERR,
            QString("Cannot find details for %1").arg(inFile));
        return false;
    }

    LOG(VB_JOBQUEUE, LOG_INFO,
        QString("chanid: %1 starttime:%2 ").arg(chanID, startTime));

    return true;
}

ProgramInfo *getProgramInfoForFile(const QString &inFile)
{
    ProgramInfo *pinfo = nullptr;
    QString chanID;
    QString startTime;

    bool bIsMythRecording = extractDetailsFromFilename(inFile, chanID, startTime);

    if (bIsMythRecording)
    {
        uint chanid = chanID.toUInt();
        QDateTime recstartts = MythDate::fromString(startTime);
        pinfo = new ProgramInfo(chanid, recstartts);
        if (pinfo->GetChanID())
        {
            pinfo->SetPathname(pinfo->GetPlaybackURL(false, true));
        }
        else
        {
            delete pinfo;
            pinfo = nullptr;
        }
    }

    if (!pinfo)
    {
        // file is not a myth recording or is no longer in the db
        pinfo = new ProgramInfo(inFile);
        LOG(VB_JOBQUEUE, LOG_NOTICE, "File is not a MythTV recording.");
    }
    else
    {
        LOG(VB_JOBQUEUE, LOG_NOTICE, "File is a MythTV recording.");
    }

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

    QString outFile = tempDir + "work/file.xml";

    // call mytharchivehelper to get files stream info etc.
    QString command = QString("mytharchivehelper --getfileinfo --infile \"%1\" "
                              "--outfile \"%2\" --method %3")
            .arg(inFile, outFile, QString::number(lenMethod));
    command += logPropagateArgs;
    if (!logPropagateQuiet())
        command += " --quiet";

    uint flags = kMSDontBlockInputDevs | kMSDontDisableDrawing;
    if (myth_system(command, flags) != GENERIC_EXIT_OK)
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

void showWarningDialog(const QString &msg)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *dialog = new MythConfirmationDialog(popupStack, msg, false);

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
        {
            item->newsize = (int64_t) (item->size /
                    ((float)item->duration / (float)item->cutDuration));
        }
        else
        {
            item->newsize = item->size;
        }
    }
    else
    {
        if (item->duration == 0)
            return;

        int length = 0;
        if (item->hasCutlist && item->useCutlist)
            length = item->cutDuration;
        else
            length = item->duration;

        float len = (float) length / 3600;
        item->newsize = (int64_t) (len * profile->bitrate * 1024 * 1024);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
