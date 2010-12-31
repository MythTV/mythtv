/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2009, Janne Grunau <janne-mythtv@grunau.be>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <iostream>
#include <stdint.h>
#include <sys/wait.h>  // for WIFEXITED and WEXITSTATUS
#include <unistd.h>
#include <cstdlib>

#include <mythconfig.h>
#if CONFIG_DARWIN or defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#elif __linux__
#include <sys/vfs.h>
#endif

using namespace std;


// Qt headers
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QDomElement>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

// MythTV headers
#include <mythcontext.h>
#include <mythcoreutil.h>
#include <mythversion.h>
#include <exitcodes.h>
#include <mythdb.h>
#include <programinfo.h>
#include <mythdirs.h>
#include <mythconfig.h>
extern "C" {
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>
#include "pxsup2dast.h"
}

// mytharchive headers
#include "../mytharchive/archiveutil.h"

class NativeArchive
{
  public:
      NativeArchive(void);
      ~NativeArchive(void);

      int doNativeArchive(const QString &jobFile);
      int doImportArchive(const QString &xmlFile, int chanID);
      bool copyFile(const QString &source, const QString &destination);
      int importRecording(const QDomElement &itemNode,
                          const QString &xmlFile, int chanID);
      int importVideo(const QDomElement &itemNode, const QString &xmlFile);
      int exportRecording(QDomElement &itemNode, const QString &saveDirectory);
      int exportVideo(QDomElement &itemNode, const QString &saveDirectory);
  private:
      QString findNodeText(const QDomElement &elem, const QString &nodeName);
};

NativeArchive::NativeArchive(void)
{
    // create the lock file so the UI knows we're running
    QString tempDir = getTempDirectory();
    QString command = QString("echo %1 > " + tempDir +
                      "/logs/mythburn.lck").arg(getpid());
    int res = system(qPrintable(command));
    if (WIFEXITED(res) == 0)
        VERBOSE(VB_IMPORTANT, "NativeArchive: Failed to create lock file");
}

NativeArchive::~NativeArchive(void)
{
    // remove lock file
    QString tempDir = getTempDirectory();
    if (QFile::exists(tempDir + "/logs/mythburn.lck"))
        QFile::remove(tempDir + "/logs/mythburn.lck");
}

bool NativeArchive::copyFile(const QString &source, const QString &destination)
{
    QFile srcFile(source), destFile(destination);

    VERBOSE(VB_JOBQUEUE, QString("copying from %1").arg(source));
    VERBOSE(VB_JOBQUEUE, QString("to %2").arg(destination));

    if (!srcFile.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_JOBQUEUE, "ERROR: Unable to open source file");
        return false;
    }

    if (!destFile.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_JOBQUEUE, "ERROR: Unable to open destination file");
        VERBOSE(VB_JOBQUEUE, "Do you have write access to the directory?");
        srcFile.close();
        return false;
    }

    // get free space available on destination
    long long dummy;
    long long freeSpace = getDiskSpace(destination, dummy, dummy);

    int srcLen, destLen, percent = 0, lastPercent = 0;
    long long  wroteSize = 0, totalSize = srcFile.size();
    char buffer[1024*1024];

    if (freeSpace != -1 && freeSpace < totalSize / 1024)
    {
        VERBOSE(VB_JOBQUEUE, "ERROR: Not enough free space"
                             " available on destination filesystem.");
        VERBOSE(VB_JOBQUEUE, QString("Available: %1 Needed %2")
                             .arg(freeSpace).arg(totalSize));
        destFile.close();
        srcFile.close();
        return false;
    }

    while ((srcLen = srcFile.read(buffer, sizeof(buffer))) > 0)
    {
        destLen = destFile.write(buffer, srcLen);

        if (destLen == -1 || srcLen != destLen)
        {
            VERBOSE(VB_JOBQUEUE,
                    "ERROR: While trying to write to destination file.");
            srcFile.close();
            destFile.close();
            return false;
        }
        wroteSize += destLen;
        percent = (int) ((100.0 * wroteSize) / totalSize);
        if (percent % 5 == 0  && percent != lastPercent)
        {
            VERBOSE(VB_JOBQUEUE, QString("%1 out of %2 (%3%) completed")
                    .arg(formatSize(wroteSize/1024))
                    .arg(formatSize(totalSize/1024)).arg(percent));
            lastPercent = percent;
        }
    }

    srcFile.close();
    destFile.close();
    if (srcFile.size() != destFile.size())
    {
        VERBOSE(VB_JOBQUEUE, "ERROR: Copy not completed OK - "
                "Source and destination file sizes do not match!!");
        VERBOSE(VB_JOBQUEUE,
                QString("Source is %1 bytes, Destination is %2 bytes")
                .arg(srcFile.size()).arg(destFile.size()));
        return false;
    }
    else
        VERBOSE(VB_JOBQUEUE, "Copy completed OK");

    return true;
}

static bool createISOImage(QString &sourceDirectory)
{
    VERBOSE(VB_JOBQUEUE, "Creating ISO image");

    QString tempDirectory = getTempDirectory();

    tempDirectory += "work/";

    QString mkisofs = gCoreContext->GetSetting("MythArchiveMkisofsCmd", "mkisofs");
    QString command = mkisofs + " -R -J -V 'MythTV Archive' -o ";
    command += tempDirectory + "mythburn.iso " + sourceDirectory;

    int res = system(qPrintable(command));
    if (WIFEXITED(res))
        res = WEXITSTATUS(res);
    if (res != 0)
    {
        VERBOSE(VB_JOBQUEUE, QString("ERROR: Failed while running mkisofs. Result: %1")
                .arg(res));
        return false;
    }

    VERBOSE(VB_JOBQUEUE, "Finished creating ISO image");
    return true;
}

static int burnISOImage(int mediaType, bool bEraseDVDRW, bool nativeFormat)
{
    QString dvdDrive = gCoreContext->GetSetting("MythArchiveDVDLocation",
                                            "/dev/dvd");
    VERBOSE(VB_JOBQUEUE, "Burning ISO image to " + dvdDrive);

    int     driveSpeed    = gCoreContext->GetNumSetting("MythArchiveDriveSpeed");
    QString tempDirectory = getTempDirectory();

    tempDirectory += "work/";

    QString command = gCoreContext->GetSetting("MythArchiveGrowisofsCmd",
                                           "growisofs");

    if (driveSpeed)
        command += " -speed=" + QString::number(driveSpeed);

    if (nativeFormat)
    {
        if (mediaType == AD_DVD_RW && bEraseDVDRW == true)
        {
            command += " -use-the-force-luke -Z " + dvdDrive;
            command += " -V 'MythTV Archive' -R -J " + tempDirectory;
        }
        else
        {
            command += " -Z " + dvdDrive;
            command += " -V 'MythTV Archive' -R -J " + tempDirectory;
        }
    }
    else
    {
        if (mediaType == AD_DVD_RW && bEraseDVDRW == true)
        {
            command += " -dvd-compat -use-the-force-luke -Z " + dvdDrive;
            command += " -dvd-video -V 'MythTV DVD' " + tempDirectory + "/dvd";
        }
        else
        {
            command += " -dvd-compat -Z " + dvdDrive;
            command += " -dvd-video -V 'MythTV DVD' " + tempDirectory + "/dvd";
        }
    }

    int res = system(qPrintable(command));
    if (WIFEXITED(res))
        res = WEXITSTATUS(res);
    if (res == 0)
        VERBOSE(VB_JOBQUEUE, "Finished burning ISO image");
    else
        VERBOSE(VB_JOBQUEUE,
                QString("ERROR: Failed while running growisofs. Result: %1")
                .arg(res));

    return res;
}

static int doBurnDVD(int mediaType, bool bEraseDVDRW, bool nativeFormat)
{
    gCoreContext->SaveSetting("MythArchiveLastRunStart",
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    gCoreContext->SaveSetting("MythArchiveLastRunStatus", "Running");

    int res = burnISOImage(mediaType, bEraseDVDRW, nativeFormat);

    gCoreContext->SaveSetting("MythArchiveLastRunEnd",
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    gCoreContext->SaveSetting("MythArchiveLastRunStatus", "Success");
    return res;
}

int NativeArchive::doNativeArchive(const QString &jobFile)
{
    QString tempDir = getTempDirectory();

    QDomDocument doc("archivejob");
    QFile file(jobFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_JOBQUEUE, "Could not open job file: " + jobFile);
        return 1;
    }

    if (!doc.setContent(&file))
    {
        VERBOSE(VB_JOBQUEUE, "Could not load job file: " + jobFile);
        file.close();
        return 1;
    }

    file.close();

    // get options from job file
    bool bCreateISO = false;
    bool bEraseDVDRW = false;
    bool bDoBurn = false;
    QString saveDirectory;
    int mediaType = 0;

    QDomNodeList nodeList = doc.elementsByTagName("options");
    if (nodeList.count() == 1)
    {
        QDomNode node = nodeList.item(0);
        QDomElement options = node.toElement();
        if (!options.isNull())
        {
            bCreateISO = (options.attribute("createiso", "0") == "1");
            bEraseDVDRW = (options.attribute("erasedvdrw", "0") == "1");
            bDoBurn = (options.attribute("doburn", "0") == "1");
            mediaType = options.attribute("mediatype", "0").toInt();
            saveDirectory = options.attribute("savedirectory", "");
            if (!saveDirectory.endsWith("/"))
                saveDirectory += "/";
        }
    }
    else
    {
        VERBOSE(VB_JOBQUEUE, QString("Found %1 options nodes - should be 1")
                             .arg(nodeList.count()));
        return 1;
    }
    VERBOSE(VB_JOBQUEUE,
            QString("Options - createiso: %1,"
                    " doburn: %2, mediatype: %3, erasedvdrw: %4")
            .arg(bCreateISO).arg(bDoBurn).arg(mediaType).arg(bEraseDVDRW));
    VERBOSE(VB_JOBQUEUE, QString("savedirectory: %1").arg(saveDirectory));

    // figure out where to save files
    if (mediaType != AD_FILE)
    {
        saveDirectory = tempDir;
        if (!saveDirectory.endsWith("/"))
            saveDirectory += "/";

        saveDirectory += "work/";

        int res = system(qPrintable("rm -fr " + saveDirectory + "*"));
        if (!WIFEXITED(res) || WEXITSTATUS(res))
            VERBOSE(VB_IMPORTANT, "NativeArchive: Failed to clear work directory");
    }

    VERBOSE(VB_JOBQUEUE, QString("Saving files to : %1").arg(saveDirectory));

    // get list of file nodes from the job file
    nodeList = doc.elementsByTagName("file");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_JOBQUEUE, "Cannot find any file nodes?");
        return 1;
    }

    // loop though file nodes and archive each file
    QDomNode node;
    QDomElement elem;
    QString type = "";

    for (int x = 0; x < nodeList.count(); x++)
    {
        node = nodeList.item(x);
        elem = node.toElement();
        if (!elem.isNull())
        {
            type = elem.attribute("type");

            if (type.toLower() == "recording")
                exportRecording(elem, saveDirectory);
            else if (type.toLower() == "video")
                exportVideo(elem, saveDirectory);
            else
            {
                VERBOSE(VB_JOBQUEUE,
                        QString("Don't know how to archive items of type '%1'")
                        .arg(type.toLower()));
                continue;
            }
        }
    }

    // burn the dvd if needed
    if (mediaType != AD_FILE && bDoBurn)
    {
        if (!burnISOImage(mediaType, bEraseDVDRW, true))
        {
            VERBOSE(VB_JOBQUEUE, "Native archive job failed to completed");
            return 1;
        }
    }

    // create an iso image if needed
    if (bCreateISO)
    {
        if (!createISOImage(saveDirectory))
        {
            VERBOSE(VB_JOBQUEUE, "Native archive job failed to completed");
            return 1;
        }
    }

    VERBOSE(VB_JOBQUEUE, "Native archive job completed OK");

    return 0;
}

int NativeArchive::exportRecording(QDomElement   &itemNode,
                                   const QString &saveDirectory)
{
    QString chanID, startTime, title = "", filename = "";
    bool doDelete = false;
    QString dbVersion = gCoreContext->GetSetting("DBSchemaVer", "");

    title = itemNode.attribute("title");
    filename = itemNode.attribute("filename");
    doDelete = (itemNode.attribute("delete", "0") == "0");
    VERBOSE(VB_JOBQUEUE, QString("Archiving %1 (%2), do delete: %3")
        .arg(title).arg(filename).arg(doDelete));

    if (title == "" || filename == "")
    {
        VERBOSE(VB_JOBQUEUE, "Bad title or filename");
        return 0;
    }

    if (!extractDetailsFromFilename(filename, chanID, startTime))
    {
        VERBOSE(VB_JOBQUEUE,
                QString("Failed to extract chanID and startTime from '%1'")
                .arg(filename));
        return 0;
    }

    // create the directory to hold this items files
    QDir dir(saveDirectory + title);
    if (!dir.exists())
        dir.mkdir(saveDirectory + title);

    VERBOSE(VB_JOBQUEUE, "Creating xml file for " + title);
    QDomDocument doc("MYTHARCHIVEITEM");

    QDomElement root = doc.createElement("item");
    doc.appendChild(root);
    root.setAttribute("type", "recording");
    root.setAttribute("databaseversion", dbVersion);

    QDomElement recorded = doc.createElement("recorded");
    root.appendChild(recorded);

    // get details from recorded
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime, endtime, title, subtitle,"
                  " description, category, hostname, bookmark, editing,"
                  " cutlist, autoexpire, commflagged, recgroup, recordid,"
                  " seriesid, programid, lastmodified, filesize, stars,"
                  " previouslyshown, originalairdate, preserve, findid,"
                  " deletepending, transcoder, timestretch, recpriority,"
                  " basename, progstart, progend, playgroup, profile,"
                  " duplicate, transcoded FROM recorded "
            "WHERE chanid = :CHANID and starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);

    if (query.exec() && query.next())
    {
        QDomElement elem;
        QDomText text;

        elem = doc.createElement("chanid");
        text = doc.createTextNode(query.value(0).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("starttime");
        text = doc.createTextNode(query.value(1).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("endtime");
        text = doc.createTextNode(query.value(2).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("title");
        text = doc.createTextNode(query.value(3).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("subtitle");
        text = doc.createTextNode(query.value(4).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("description");
        text = doc.createTextNode(query.value(5).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("category");
        text = doc.createTextNode(query.value(6).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("hostname");
        text = doc.createTextNode(query.value(7).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("bookmark");
        text = doc.createTextNode(query.value(8).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("editing");
        text = doc.createTextNode(query.value(9).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("cutlist");
        text = doc.createTextNode(query.value(10).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("autoexpire");
        text = doc.createTextNode(query.value(11).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("commflagged");
        text = doc.createTextNode(query.value(12).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("recgroup");
        text = doc.createTextNode(query.value(13).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("recordid");
        text = doc.createTextNode(query.value(14).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("seriesid");
        text = doc.createTextNode(query.value(15).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("programid");
        text = doc.createTextNode(query.value(16).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("lastmodified");
        text = doc.createTextNode(query.value(17).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("filesize");
        text = doc.createTextNode(query.value(18).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("stars");
        text = doc.createTextNode(query.value(19).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("previouslyshown");
        text = doc.createTextNode(query.value(20).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("originalairdate");
        text = doc.createTextNode(query.value(21).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("preserve");
        text = doc.createTextNode(query.value(22).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("findid");
        text = doc.createTextNode(query.value(23).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("deletepending");
        text = doc.createTextNode(query.value(24).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("transcoder");
        text = doc.createTextNode(query.value(25).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("timestretch");
        text = doc.createTextNode(query.value(26).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("recpriority");
        text = doc.createTextNode(query.value(27).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("basename");
        text = doc.createTextNode(query.value(28).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("progstart");
        text = doc.createTextNode(query.value(29).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("progend");
        text = doc.createTextNode(query.value(30).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("playgroup");
        text = doc.createTextNode(query.value(31).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("profile");
        text = doc.createTextNode(query.value(32).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("duplicate");
        text = doc.createTextNode(query.value(33).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);

        elem = doc.createElement("transcoded");
        text = doc.createTextNode(query.value(34).toString());
        elem.appendChild(text);
        recorded.appendChild(elem);
        VERBOSE(VB_JOBQUEUE, "Created recorded element for " + title);
    }

    // add channel details
    query.prepare("SELECT chanid, channum, callsign, name "
            "FROM channel WHERE chanid = :CHANID;");
    query.bindValue(":CHANID", chanID);

    if (query.exec() && query.next())
    {
        QDomElement channel = doc.createElement("channel");
        channel.setAttribute("chanid", query.value(0).toString());
        channel.setAttribute("channum", query.value(1).toString());
        channel.setAttribute("callsign", query.value(2).toString());
        channel.setAttribute("name", query.value(3).toString());
        root.appendChild(channel);
        VERBOSE(VB_JOBQUEUE, "Created channel element for " + title);
    }
    else
    {
        // cannot find the original channel so create a default channel element
        VERBOSE(VB_JOBQUEUE,
                "Cannot find channel details for chanid " + chanID);
        QDomElement channel = doc.createElement("channel");
        channel.setAttribute("chanid", chanID);
        channel.setAttribute("channum", "unknown");
        channel.setAttribute("callsign", "unknown");
        channel.setAttribute("name", "unknown");
        root.appendChild(channel);
        VERBOSE(VB_JOBQUEUE, "Created a default channel element for " + title);
    }

    // add any credits
    query.prepare("SELECT credits.person, role, people.name "
            "FROM recordedcredits AS credits "
            "LEFT JOIN people ON credits.person = people.person "
            "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);

    if (query.exec() && query.size())
    {
        QDomElement credits = doc.createElement("credits");
        while (query.next())
        {
            QDomElement credit = doc.createElement("credit");
            credit.setAttribute("personid", query.value(0).toString());
            credit.setAttribute("name", query.value(2).toString());
            credit.setAttribute("role", query.value(1).toString());
            credits.appendChild(credit);
        }
        root.appendChild(credits);
        VERBOSE(VB_JOBQUEUE, "Created credits element for " + title);
    }

    // add any rating
    query.prepare("SELECT system, rating FROM recordedrating "
            "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);

    if (query.exec() && query.next())
    {
        QDomElement rating = doc.createElement("rating");
        rating.setAttribute("system", query.value(0).toString());
        rating.setAttribute("rating", query.value(1).toString());
        root.appendChild(rating);
        VERBOSE(VB_JOBQUEUE, "Created rating element for " + title);
    }

    // add the recordedmarkup table
    QDomElement recordedmarkup = doc.createElement("recordedmarkup");
    query.prepare("SELECT chanid, starttime, mark, offset, type "
            "FROM recordedmarkup "
            "WHERE chanid = :CHANID and starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);
    if (query.exec() && query.size())
    {
        while (query.next())
        {
            QDomElement mark = doc.createElement("mark");
            mark.setAttribute("mark", query.value(2).toString());
            mark.setAttribute("offset", query.value(3).toString());
            mark.setAttribute("type", query.value(4).toString());
            recordedmarkup.appendChild(mark);
        }
        root.appendChild(recordedmarkup);
        VERBOSE(VB_JOBQUEUE, "Created recordedmarkup element for " + title);
    }

    // add the recordedseek table
    QDomElement recordedseek = doc.createElement("recordedseek");
    query.prepare("SELECT chanid, starttime, mark, offset, type "
            "FROM recordedseek "
            "WHERE chanid = :CHANID and starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);
    if (query.exec() && query.size())
    {
        while (query.next())
        {
            QDomElement mark = doc.createElement("mark");
            mark.setAttribute("mark", query.value(2).toString());
            mark.setAttribute("offset", query.value(3).toString());
            mark.setAttribute("type", query.value(4).toString());
            recordedseek.appendChild(mark);
        }
        root.appendChild(recordedseek);
        VERBOSE(VB_JOBQUEUE, "Created recordedseek element for " + title);
    }

    // finally save the xml to the file
    QString baseName = getBaseName(filename);
    QString xmlFile = saveDirectory + title + "/" + baseName + ".xml";
    QFile f(xmlFile);
    if (!f.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_JOBQUEUE,
                "MythNativeWizard: Failed to open file for writing - "
                + xmlFile);
        return 0;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();

    // copy the file
    VERBOSE(VB_JOBQUEUE, "Copying video file");
    bool res = copyFile(filename, saveDirectory + title + "/" + baseName);
    if (!res)
        return 0;

    // copy preview image
    if (QFile::exists(filename + ".png"))
    {
        VERBOSE(VB_JOBQUEUE, "Copying preview image");
        res = copyFile(filename + ".png", saveDirectory
                       + title + "/" + baseName + ".png");
        if (!res)
            return 0;
    }

    VERBOSE(VB_JOBQUEUE, "Item Archived OK");

    return 1;
}

int NativeArchive::exportVideo(QDomElement   &itemNode,
                               const QString &saveDirectory)
{
    QString title = "", filename = "";
    bool doDelete = false;
    QString dbVersion = gCoreContext->GetSetting("DBSchemaVer", "");
    int intID = 0, categoryID = 0;
    QString coverFile = "";

    title = itemNode.attribute("title");
    filename = itemNode.attribute("filename");
    doDelete = (itemNode.attribute("delete", "0") == "0");
    VERBOSE(VB_JOBQUEUE, QString("Archiving %1 (%2), do delete: %3")
            .arg(title).arg(filename).arg(doDelete));

    if (title == "" || filename == "")
    {
        VERBOSE(VB_JOBQUEUE, "Bad title or filename");
        return 0;
    }

    // create the directory to hold this items files
    QDir dir(saveDirectory + title);
    if (!dir.exists())
        dir.mkdir(saveDirectory + title);

    VERBOSE(VB_JOBQUEUE, "Creating xml file for " + title);
    QDomDocument doc("MYTHARCHIVEITEM");

    QDomElement root = doc.createElement("item");
    doc.appendChild(root);
    root.setAttribute("type", "video");
    root.setAttribute("databaseversion", dbVersion);

    QDomElement video = doc.createElement("videometadata");
    root.appendChild(video);

    // get details from videometadata
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT intid, title, director, plot, rating, inetref, "
            "year, userrating, length, showlevel, filename, coverfile, "
            "childid, browse, playcommand, category "
            "FROM videometadata WHERE filename = :FILENAME;");
    query.bindValue(":FILENAME", filename);

    if (query.exec() && query.next())
    {
        QDomElement elem;
        QDomText text;

        elem = doc.createElement("intid");
        text = doc.createTextNode(query.value(0).toString());
        intID = query.value(0).toInt();
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("title");
        text = doc.createTextNode(query.value(1).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("director");
        text = doc.createTextNode(query.value(2).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("plot");
        text = doc.createTextNode(query.value(3).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("rating");
        text = doc.createTextNode(query.value(4).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("inetref");
        text = doc.createTextNode(query.value(5).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("year");
        text = doc.createTextNode(query.value(6).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("userrating");
        text = doc.createTextNode(query.value(7).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("length");
        text = doc.createTextNode(query.value(8).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("showlevel");
        text = doc.createTextNode(query.value(9).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        // remove the VideoStartupDir part of the filename
        QString fname = query.value(10).toString();
        if (fname.startsWith(gCoreContext->GetSetting("VideoStartupDir")))
            fname = fname.remove(gCoreContext->GetSetting("VideoStartupDir"));

        elem = doc.createElement("filename");
        text = doc.createTextNode(fname);
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("coverfile");
        text = doc.createTextNode(query.value(11).toString());
        coverFile = query.value(11).toString();
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("childid");
        text = doc.createTextNode(query.value(12).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("browse");
        text = doc.createTextNode(query.value(13).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("playcommand");
        text = doc.createTextNode(query.value(14).toString());
        elem.appendChild(text);
        video.appendChild(elem);

        elem = doc.createElement("categoryid");
        text = doc.createTextNode(query.value(15).toString());
        categoryID = query.value(15).toInt();
        elem.appendChild(text);
        video.appendChild(elem);

        VERBOSE(VB_JOBQUEUE, "Created videometadata element for " + title);
    }

    // add category details
    query.prepare("SELECT intid, category "
            "FROM videocategory WHERE intid = :INTID;");
    query.bindValue(":INTID", categoryID);

    if (query.exec() && query.next())
    {
        QDomElement category = doc.createElement("category");
        category.setAttribute("intid", query.value(0).toString());
        category.setAttribute("category", query.value(1).toString());
        root.appendChild(category);
        VERBOSE(VB_JOBQUEUE, "Created videocategory element for " + title);
    }

    //add video country details
    QDomElement countries = doc.createElement("countries");
    root.appendChild(countries);

    query.prepare("SELECT intid, country "
            "FROM videometadatacountry INNER JOIN videocountry "
            "ON videometadatacountry.idcountry = videocountry.intid "
            "WHERE idvideo = :INTID;");
    query.bindValue(":INTID", intID);

    if (!query.exec())
    {
        print_verbose_messages = VB_JOBQUEUE + VB_IMPORTANT;
        MythDB::DBError("select countries", query);
        print_verbose_messages = VB_JOBQUEUE;
    }

    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            QDomElement country = doc.createElement("country");
            country.setAttribute("intid", query.value(0).toString());
            country.setAttribute("country", query.value(1).toString());
            countries.appendChild(country);
        }
        VERBOSE(VB_JOBQUEUE, "Created videocountry element for " + title);
    }

    // add video genre details
    QDomElement genres = doc.createElement("genres");
    root.appendChild(genres);

    query.prepare("SELECT intid, genre "
            "FROM videometadatagenre INNER JOIN videogenre "
            "ON videometadatagenre.idgenre = videogenre.intid "
            "WHERE idvideo = :INTID;");
    query.bindValue(":INTID", intID);

    if (!query.exec())
    {
        print_verbose_messages = VB_JOBQUEUE + VB_IMPORTANT;
        MythDB::DBError("select genres", query);
        print_verbose_messages = VB_JOBQUEUE;
    }

    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            QDomElement genre = doc.createElement("genre");
            genre.setAttribute("intid", query.value(0).toString());
            genre.setAttribute("genre", query.value(1).toString());
            genres.appendChild(genre);
        }
        VERBOSE(VB_JOBQUEUE, "Created videogenre element for " + title);
    }

    // finally save the xml to the file
    QFileInfo fileInfo(filename);
    QString xmlFile = saveDirectory + title + "/"
                      + fileInfo.fileName() + ".xml";
    QFile f(xmlFile);
    if (!f.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_JOBQUEUE,
                "MythNativeWizard: Failed to open file for writing - "
                + xmlFile);
        return 0;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();

    // copy the file
    VERBOSE(VB_JOBQUEUE, "Copying video file");
    bool res = copyFile(filename, saveDirectory + title
                        + "/" + fileInfo.fileName());
    if (!res)
    {
        return 0;
    }

    // copy the cover image
    fileInfo.setFile(coverFile);
    if (fileInfo.exists())
    {
        VERBOSE(VB_JOBQUEUE, "Copying cover file");
        bool res = copyFile(coverFile, saveDirectory + title
                            + "/" + fileInfo.fileName());
        if (!res)
        {
            return 0;
        }
    }

    VERBOSE(VB_JOBQUEUE, "Item Archived OK");

    return 1;
}

int NativeArchive::doImportArchive(const QString &xmlFile, int chanID)
{
    // open xml file
    QDomDocument doc("mydocument");
    QFile file(xmlFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_JOBQUEUE,
                "ERROR: Failed to open file for reading - " + xmlFile);
        return 1;
    }

    if (!doc.setContent(&file))
    {
        file.close();
        VERBOSE(VB_JOBQUEUE,
                "ERROR: Failed to read from xml file - " + xmlFile);
        return 1;
    }
    file.close();

    QString docType = doc.doctype().name();
    QString type, dbVersion;
    QDomNodeList itemNodeList;
    QDomNode node;
    QDomElement itemNode;

    if (docType == "MYTHARCHIVEITEM")
    {
        itemNodeList = doc.elementsByTagName("item");

        if (itemNodeList.count() < 1)
        {
            VERBOSE(VB_JOBQUEUE,
                    "ERROR: Couldn't find an 'item' element in XML file");
            return 1;
        }

        node = itemNodeList.item(0);
        itemNode = node.toElement();
        type = itemNode.attribute("type");
        dbVersion = itemNode.attribute("databaseversion");

        VERBOSE(VB_JOBQUEUE,
                QString("Archive DB version: %1, Local DB version: %2")
                .arg(dbVersion).arg(gCoreContext->GetSetting("DBSchemaVer")));
    }
    else
    {
        VERBOSE(VB_JOBQUEUE,
                "ERROR: Not a native archive xml file - " + xmlFile);
        return 1;
    }

    if (type == "recording")
    {
        return importRecording(itemNode, xmlFile, chanID);
    }
    else if (type == "video")
    {
        return importVideo(itemNode, xmlFile);
    }

    return 1;
}

int NativeArchive::importRecording(const QDomElement &itemNode,
                                   const QString     &xmlFile, int chanID)
{
    VERBOSE(VB_JOBQUEUE,
            QString("Import recording using chanID: %1").arg(chanID));
    VERBOSE(VB_JOBQUEUE,
            QString("Archived recording xml file: %1").arg(xmlFile));

    QString videoFile = xmlFile.left(xmlFile.length() - 4);
    QString basename = videoFile;
    int pos = videoFile.lastIndexOf('/');
    if (pos > 0)
        basename = videoFile.mid(pos + 1);

    QDomNodeList nodeList = itemNode.elementsByTagName("recorded");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_JOBQUEUE,
                "ERROR: Couldn't find a 'recorded' element in XML file");
        return 1;
    }

    QDomNode n = nodeList.item(0);
    QDomElement recordedNode = n.toElement();
    QString startTime = findNodeText(recordedNode, "starttime");

    // check this recording doesn't already exist
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT * FROM recorded "
            "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);
    if (query.exec())
    {
        if (query.isActive() && query.size())
        {
            VERBOSE(VB_JOBQUEUE,
                    "ERROR: This recording appears to already exist!!");
            return 1;
        }
    }

    // find the default storage location for this host
    QString storageDir = "";
    query.prepare("SELECT dirname FROM storagegroup "
            "WHERE groupname = :GROUPNAME AND hostname = :HOSTNAME;");
    query.bindValue(":GROUPNAME", "Default");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (query.exec())
    {
        query.first();
        storageDir = query.value(0).toString();
    }
    else
    {
        VERBOSE(VB_JOBQUEUE, "ERROR: Failed to get 'Default'"
                             " storage directory for this host");
        return 1;
    }

    // copy file to recording directory
    VERBOSE(VB_JOBQUEUE, "Copying video file.");
    if (!copyFile(videoFile,  storageDir + "/" + basename))
        return 1;

    // copy any preview image to recording directory
    if (QFile::exists(videoFile + ".png"))
    {
        VERBOSE(VB_JOBQUEUE, "Copying preview image file.");
        if (!copyFile(videoFile + ".png", storageDir + "/" + basename + ".png"))
            return 1;
    }

    // copy recorded to database
    query.prepare("INSERT INTO recorded (chanid,starttime,endtime,"
            "title,subtitle,description,category,hostname,bookmark,"
            "editing,cutlist,autoexpire, commflagged,recgroup,"
            "recordid, seriesid,programid,lastmodified,filesize,stars,"
            "previouslyshown,originalairdate,preserve,findid,deletepending,"
            "transcoder,timestretch,recpriority,basename,progstart,progend,"
            "playgroup,profile,duplicate,transcoded) "
            "VALUES(:CHANID,:STARTTIME,:ENDTIME,:TITLE,"
            ":SUBTITLE,:DESCRIPTION,:CATEGORY,:HOSTNAME,"
            ":BOOKMARK,:EDITING,:CUTLIST,:AUTOEXPIRE,"
            ":COMMFLAGGED,:RECGROUP,:RECORDID,:SERIESID,"
            ":PROGRAMID,:LASTMODIFIED,:FILESIZE,:STARS,"
            ":PREVIOUSLYSHOWN,:ORIGINALAIRDATE,:PRESERVE,:FINDID,"
            ":DELETEPENDING,:TRANSCODER,:TIMESTRETCH,:RECPRIORITY,"
            ":BASENAME,:PROGSTART,:PROGEND,:PLAYGROUP,:PROFILE,:DUPLICATE,:TRANSCODED);");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);
    query.bindValue(":ENDTIME", findNodeText(recordedNode, "endtime"));
    query.bindValue(":TITLE", findNodeText(recordedNode, "title"));
    query.bindValue(":SUBTITLE", findNodeText(recordedNode, "subtitle"));
    query.bindValue(":DESCRIPTION", findNodeText(recordedNode, "description"));
    query.bindValue(":CATEGORY", findNodeText(recordedNode, "category"));
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    query.bindValue(":BOOKMARK", findNodeText(recordedNode, "bookmark"));
    query.bindValue(":EDITING", findNodeText(recordedNode, "editing"));
    query.bindValue(":CUTLIST", findNodeText(recordedNode, "cutlist"));
    query.bindValue(":AUTOEXPIRE", findNodeText(recordedNode, "autoexpire"));
    query.bindValue(":COMMFLAGGED", findNodeText(recordedNode, "commflagged"));
    query.bindValue(":RECGROUP", findNodeText(recordedNode, "recgroup"));
    query.bindValue(":RECORDID", findNodeText(recordedNode, "recordid"));
    query.bindValue(":SERIESID", findNodeText(recordedNode, "seriesid"));
    query.bindValue(":PROGRAMID", findNodeText(recordedNode, "programid"));
    query.bindValue(":LASTMODIFIED", findNodeText(recordedNode, "lastmodified"));
    query.bindValue(":FILESIZE", findNodeText(recordedNode, "filesize"));
    query.bindValue(":STARS", findNodeText(recordedNode, "stars"));
    query.bindValue(":PREVIOUSLYSHOWN", findNodeText(recordedNode, "previouslyshown"));
    query.bindValue(":ORIGINALAIRDATE", findNodeText(recordedNode, "originalairdate"));
    query.bindValue(":PRESERVE", findNodeText(recordedNode, "preserve"));
    query.bindValue(":FINDID", findNodeText(recordedNode, "findid"));
    query.bindValue(":DELETEPENDING", findNodeText(recordedNode, "deletepending"));
    query.bindValue(":TRANSCODER", findNodeText(recordedNode, "transcoder"));
    query.bindValue(":TIMESTRETCH", findNodeText(recordedNode, "timestretch"));
    query.bindValue(":RECPRIORITY", findNodeText(recordedNode, "recpriority"));
    query.bindValue(":BASENAME", findNodeText(recordedNode, "basename"));
    query.bindValue(":PROGSTART", findNodeText(recordedNode, "progstart"));
    query.bindValue(":PROGEND", findNodeText(recordedNode, "progend"));
    query.bindValue(":PLAYGROUP", findNodeText(recordedNode, "playgroup"));
    query.bindValue(":PROFILE", findNodeText(recordedNode, "profile"));
    query.bindValue(":DUPLICATE", findNodeText(recordedNode, "duplicate"));
    query.bindValue(":TRANSCODED", findNodeText(recordedNode, "transcoded"));

    if (query.exec())
        VERBOSE(VB_JOBQUEUE, "Inserted recorded details into database");
    else
    {
        print_verbose_messages = VB_JOBQUEUE + VB_IMPORTANT;
        MythDB::DBError("recorded insert", query);
        print_verbose_messages = VB_JOBQUEUE;
    }

    // copy recordedmarkup to db
    nodeList = itemNode.elementsByTagName("recordedmarkup");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_JOBQUEUE, "WARNING: Couldn't find a 'recordedmarkup' element in XML file");
    }
    else
    {
        n = nodeList.item(0);
        QDomElement markupNode = n.toElement();

        nodeList = markupNode.elementsByTagName("mark");
        if (nodeList.count() < 1)
        {
            VERBOSE(VB_JOBQUEUE, "WARNING: Couldn't find any 'mark' elements in XML file");
        }
        else
        {
            for (int x = 0; x < nodeList.count(); x++)
            {
                n = nodeList.item(x);
                QDomElement e = n.toElement();
                query.prepare("INSERT INTO recordedmarkup (chanid, starttime, "
                        "mark, offset, type)"
                        "VALUES(:CHANID,:STARTTIME,:MARK,:OFFSET,:TYPE);");
                query.bindValue(":CHANID", chanID);
                query.bindValue(":STARTTIME", startTime);
                query.bindValue(":MARK", e.attribute("mark"));
                query.bindValue(":OFFSET", e.attribute("offset"));
                query.bindValue(":TYPE", e.attribute("type"));

                if (!query.exec())
                {
                    print_verbose_messages = VB_JOBQUEUE + VB_IMPORTANT;
                    MythDB::DBError("recordedmark insert", query);
                    return 1;
                }
            }

            VERBOSE(VB_JOBQUEUE, "Inserted recordedmarkup details into database");
        }
    }

    // copy recordedseek to db
    nodeList = itemNode.elementsByTagName("recordedseek");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_JOBQUEUE, "WARNING: Couldn't find a 'recordedseek' element in XML file");
    }
    else
    {
        n = nodeList.item(0);
        QDomElement markupNode = n.toElement();

        nodeList = markupNode.elementsByTagName("mark");
        if (nodeList.count() < 1)
        {
            VERBOSE(VB_JOBQUEUE, "WARNING: Couldn't find any 'mark' elements in XML file");
        }
        else
        {
            for (int x = 0; x < nodeList.count(); x++)
            {
                n = nodeList.item(x);
                QDomElement e = n.toElement();
                query.prepare("INSERT INTO recordedseek (chanid, starttime, "
                        "mark, offset, type)"
                        "VALUES(:CHANID,:STARTTIME,:MARK,:OFFSET,:TYPE);");
                query.bindValue(":CHANID", chanID);
                query.bindValue(":STARTTIME", startTime);
                query.bindValue(":MARK", e.attribute("mark"));
                query.bindValue(":OFFSET", e.attribute("offset"));
                query.bindValue(":TYPE", e.attribute("type"));

                if (!query.exec())
                {
                    print_verbose_messages = VB_JOBQUEUE + VB_IMPORTANT;
                    MythDB::DBError("recordedseek insert", query);
                    return 1;
                }
            }

            VERBOSE(VB_JOBQUEUE, "Inserted recordedseek details into database");
        }
    }

    // FIXME are these needed?
    // copy credits to DB
    // copy rating to DB

    VERBOSE(VB_JOBQUEUE, "Import completed OK");

    return 0;
}

int NativeArchive::importVideo(const QDomElement &itemNode, const QString &xmlFile)
{
    VERBOSE(VB_JOBQUEUE, "Importing video");
    VERBOSE(VB_JOBQUEUE, QString("Archived video xml file: %1").arg(xmlFile));

    QString videoFile = xmlFile.left(xmlFile.length() - 4);
    QFileInfo fileInfo(videoFile);
    QString basename = fileInfo.fileName();

    QDomNodeList nodeList = itemNode.elementsByTagName("videometadata");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_JOBQUEUE, "ERROR: Couldn't find a 'videometadata' element in XML file");
        return 1;
    }

    QDomNode n = nodeList.item(0);
    QDomElement videoNode = n.toElement();

    // copy file to video directory
    QString path = gCoreContext->GetSetting("VideoStartupDir");
    QString origFilename = findNodeText(videoNode, "filename");
    QStringList dirList = origFilename.split("/", QString::SkipEmptyParts);
    QDir dir;
    for (int x = 0; x < dirList.count() - 1; x++)
    {
        path += "/" + dirList[x];
        if (!dir.exists(path))
        {
            if (!dir.mkdir(path))
            {
                VERBOSE(VB_JOBQUEUE, QString("ERROR: Couldn't create directory '%s'").arg(path));
                return 1;
            }
        }
    }

    VERBOSE(VB_JOBQUEUE, "Copying video file");
    if (!copyFile(videoFile, path + "/" + basename))
    {
        return 1;
    }

    // copy cover image to Video Artwork dir
    QString artworkDir = gCoreContext->GetSetting("VideoArtworkDir");
    // get archive path
    fileInfo.setFile(videoFile);
    QString archivePath = fileInfo.absolutePath();
    // get coverfile filename
    QString coverFilename = findNodeText(videoNode, "coverfile");
    fileInfo.setFile(coverFilename);
    coverFilename = fileInfo.fileName();
    //check file exists
    fileInfo.setFile(archivePath + "/" + coverFilename);
    if (fileInfo.exists())
    {
        VERBOSE(VB_JOBQUEUE, "Copying cover file");

        if (!copyFile(archivePath + "/" + coverFilename, artworkDir + "/" + coverFilename))
        {
            return 1;
        }
    }
    else
        coverFilename = "No Cover";

    // copy videometadata to database
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO videometadata (title, director, plot, rating, inetref, "
            "year, userrating, length, showlevel, filename, coverfile, "
            "childid, browse, playcommand, category) "
            "VALUES(:TITLE,:DIRECTOR,:PLOT,:RATING,:INETREF,:YEAR,"
            ":USERRATING,:LENGTH,:SHOWLEVEL,:FILENAME,:COVERFILE,"
            ":CHILDID,:BROWSE,:PLAYCOMMAND,:CATEGORY);");
    query.bindValue(":TITLE", findNodeText(videoNode, "title"));
    query.bindValue(":DIRECTOR", findNodeText(videoNode, "director"));
    query.bindValue(":PLOT", findNodeText(videoNode, "plot"));
    query.bindValue(":RATING", findNodeText(videoNode, "rating"));
    query.bindValue(":INETREF", findNodeText(videoNode, "inetref"));
    query.bindValue(":YEAR", findNodeText(videoNode, "year"));
    query.bindValue(":USERRATING", findNodeText(videoNode, "userrating"));
    query.bindValue(":LENGTH", findNodeText(videoNode, "length"));
    query.bindValue(":SHOWLEVEL", findNodeText(videoNode, "showlevel"));
    query.bindValue(":FILENAME", path + "/" + basename);
    query.bindValue(":COVERFILE", artworkDir + "/" + coverFilename);
    query.bindValue(":CHILDID", findNodeText(videoNode, "childid"));
    query.bindValue(":BROWSE", findNodeText(videoNode, "browse"));
    query.bindValue(":PLAYCOMMAND", findNodeText(videoNode, "playcommand"));
    query.bindValue(":CATEGORY", 0);

    if (query.exec())
        VERBOSE(VB_JOBQUEUE, "Inserted videometadata details into database");
    else
    {
        print_verbose_messages = VB_JOBQUEUE + VB_IMPORTANT;
        MythDB::DBError("videometadata insert", query);
        print_verbose_messages = VB_JOBQUEUE;
        return 1;
    }

    // get intid field for inserted record
    int intid;
    query.prepare("SELECT intid FROM videometadata WHERE filename = :FILENAME;");
    query.bindValue(":FILENAME", path + "/" + basename);
    if (query.exec() && query.next())
    {
        intid = query.value(0).toInt();
    }
    else
    {
        print_verbose_messages = VB_JOBQUEUE + VB_IMPORTANT;
        MythDB::DBError("Failed to get intid", query);
        print_verbose_messages = VB_JOBQUEUE;
        return 1;
    }

    VERBOSE(VB_JOBQUEUE, QString("'intid' of inserted video is: %1").arg(intid));

    // copy genre to db
    nodeList = itemNode.elementsByTagName("genres");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_JOBQUEUE, "No 'genres' element found in XML file");
    }
    else
    {
        n = nodeList.item(0);
        QDomElement genresNode = n.toElement();

        nodeList = genresNode.elementsByTagName("genre");
        if (nodeList.count() < 1)
        {
            VERBOSE(VB_JOBQUEUE, "WARNING: Couldn't find any 'genre' elements in XML file");
        }
        else
        {
            for (int x = 0; x < nodeList.count(); x++)
            {
                n = nodeList.item(x);
                QDomElement e = n.toElement();
                int genreID = e.attribute("intid").toInt();
                QString genre = e.attribute("genre");

                // see if this genre already exists
                query.prepare("SELECT intid FROM videogenre "
                        "WHERE genre = :GENRE");
                query.bindValue(":GENRE", genre);
                if (query.exec() && query.next())
                {
                    genreID = query.value(0).toInt();
                }
                else
                {
                    // genre doesn't exist so add it
                    query.prepare("INSERT INTO videogenre (genre) VALUES(:GENRE);");
                    query.bindValue(":GENRE", genre);
                    if (!query.exec())
                        MythDB::DBError("NativeArchive::importVideo - "
                                        "insert videogenre", query);

                    // get new intid of genre
                    query.prepare("SELECT intid FROM videogenre "
                            "WHERE genre = :GENRE");
                    query.bindValue(":GENRE", genre);
                    if (query.exec() && query.next())
                    {
                        genreID = query.value(0).toInt();
                    }
                    else
                    {
                        VERBOSE(VB_JOBQUEUE, "Couldn't add genre to database");
                        continue;
                    }
                }

                // now link the genre to the videometadata
                query.prepare("INSERT INTO videometadatagenre (idvideo, idgenre)"
                        "VALUES (:IDVIDEO, :IDGENRE);");
                query.bindValue(":IDVIDEO", intid);
                query.bindValue(":IDGENRE", genreID);
                if (!query.exec())
                    MythDB::DBError("NativeArchive::importVideo - "
                                    "insert videometadatagenre", query);
            }

            VERBOSE(VB_JOBQUEUE, "Inserted genre details into database");
        }
    }

    // copy country to db
    nodeList = itemNode.elementsByTagName("countries");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_JOBQUEUE, "No 'countries' element found in XML file");
    }
    else
    {
        n = nodeList.item(0);
        QDomElement countriesNode = n.toElement();

        nodeList = countriesNode.elementsByTagName("country");
        if (nodeList.count() < 1)
        {
            VERBOSE(VB_JOBQUEUE, "WARNING: Couldn't find any 'country' elements in XML file");
        }
        else
        {
            for (int x = 0; x < nodeList.count(); x++)
            {
                n = nodeList.item(x);
                QDomElement e = n.toElement();
                int countryID = e.attribute("intid").toInt();
                QString country = e.attribute("country");

                // see if this country already exists
                query.prepare("SELECT intid FROM videocountry "
                        "WHERE country = :COUNTRY");
                query.bindValue(":COUNTRY", country);
                if (query.exec() && query.next())
                {
                    countryID = query.value(0).toInt();
                }
                else
                {
                    // country doesn't exist so add it
                    query.prepare("INSERT INTO videocountry (country) VALUES(:COUNTRY);");
                    query.bindValue(":COUNTRY", country);
                    if (!query.exec())
                        MythDB::DBError("NativeArchive::importVideo - "
                                        "insert videocountry", query);

                    // get new intid of country
                    query.prepare("SELECT intid FROM videocountry "
                            "WHERE country = :COUNTRY");
                    query.bindValue(":COUNTRY", country);
                    if (query.exec() && query.next())
                    {
                        countryID = query.value(0).toInt();
                    }
                    else
                    {
                        VERBOSE(VB_JOBQUEUE, "Couldn't add country to database");
                        continue;
                    }
                }

                // now link the country to the videometadata
                query.prepare("INSERT INTO videometadatacountry (idvideo, idcountry)"
                        "VALUES (:IDVIDEO, :IDCOUNTRY);");
                query.bindValue(":IDVIDEO", intid);
                query.bindValue(":IDCOUNTRY", countryID);
                if (!query.exec())
                    MythDB::DBError("NativeArchive::importVideo - "
                                    "insert videometadatacountry", query);
            }

            VERBOSE(VB_JOBQUEUE, "Inserted country details into database");
        }
    }

    // fix the category id
    nodeList = itemNode.elementsByTagName("category");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_JOBQUEUE, "No 'category' element found in XML file");
    }
    else
    {
        n = nodeList.item(0);
        QDomElement e = n.toElement();
        int categoryID = e.attribute("intid").toInt();
        QString category = e.attribute("category");
        // see if this category already exists
        query.prepare("SELECT intid FROM videocategory "
                "WHERE category = :CATEGORY");
        query.bindValue(":CATEGORY", category);
        if (query.exec() && query.next())
        {
            categoryID = query.value(0).toInt();
        }
        else
        {
            // category doesn't exist so add it
            query.prepare("INSERT INTO videocategory (category) VALUES(:CATEGORY);");
            query.bindValue(":CATEGORY", category);
            if (!query.exec())
                MythDB::DBError("NativeArchive::importVideo - "
                                "insert videocategory", query);

            // get new intid of category
            query.prepare("SELECT intid FROM videocategory "
                    "WHERE category = :CATEGORY");
            query.bindValue(":CATEGORY", category);
            if (query.exec() && query.next())
            {
                categoryID = query.value(0).toInt();
            }
            else
            {
                VERBOSE(VB_JOBQUEUE, "Couldn't add category to database");
                categoryID = 0;
            }
        }

        // now fix the categoryid in the videometadata
        query.prepare("UPDATE videometadata "
                "SET category = :CATEGORY "
                "WHERE intid = :INTID;");
        query.bindValue(":CATEGORY", categoryID);
        query.bindValue(":INTID", intid);
        if (!query.exec())
            MythDB::DBError("NativeArchive::importVideo - "
                            "update category", query);

        VERBOSE(VB_JOBQUEUE, "Fixed the category in the database");
    }

    VERBOSE(VB_JOBQUEUE, "Import completed OK");

    return 0;
}

QString NativeArchive::findNodeText(const QDomElement &elem, const QString &nodeName)
{
    QDomNodeList nodeList = elem.elementsByTagName(nodeName);
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find a '%1' element in XML file")
            .arg(nodeName).toLocal8Bit().constData());
        return "";
    }

    QDomNode n = nodeList.item(0);
    QDomElement e = n.toElement();
    QString res = "";

    for (QDomNode node = e.firstChild(); !node.isNull();
         node = node.nextSibling())
    {
        QDomText t = node.toText();
        if (!t.isNull())
        {
            res = t.data();
            break;
        }
    }

    // some fixups
    // FIXME could be a lot smarter
    if (nodeName == "recgroup")
    {
        res = "Default";
    }
    else if (nodeName == "recordid")
    {
        res = "";
    }
    else if (nodeName == "seriesid")
    {
        res = "";
    }
    else if (nodeName == "programid")
    {
        res = "";
    }
    else if (nodeName == "playgroup")
    {
        res = "Default";
    }
    else if (nodeName == "profile")
    {
        res = "";
    }

    return res;
}

static void clearArchiveTable(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems;");

    if (!query.exec())
    {
        print_verbose_messages = VB_JOBQUEUE + VB_IMPORTANT;
        MythDB::DBError("delete archiveitems", query);
        print_verbose_messages = VB_JOBQUEUE;
    }
}

static int doNativeArchive(const QString &jobFile)
{
    gCoreContext->SaveSetting("MythArchiveLastRunType", "Native Export");
    gCoreContext->SaveSetting("MythArchiveLastRunStart", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    gCoreContext->SaveSetting("MythArchiveLastRunStatus", "Running");

    NativeArchive na;
    int res = na.doNativeArchive(jobFile);
    gCoreContext->SaveSetting("MythArchiveLastRunEnd", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    gCoreContext->SaveSetting("MythArchiveLastRunStatus", (res == 0 ? "Success" : "Failed"));

    // clear the archiveitems table if succesful
    if (res == 0)
        clearArchiveTable();

    return res;
}

static int doImportArchive(const QString &inFile, int chanID)
{
    NativeArchive na;
    return na.doImportArchive(inFile, chanID);
}

// Note: copied this function from myth_imgconvert.cpp -- dtk 2009-08-17
static int myth_sws_img_convert(
    AVPicture *dst, PixelFormat dst_pix_fmt, AVPicture *src,
    PixelFormat pix_fmt, int width, int height)
{
    static QMutex lock;
    QMutexLocker locker(&lock);

    static struct SwsContext *convert_ctx;

    convert_ctx = sws_getCachedContext(convert_ctx, width, height, pix_fmt,
                                       width, height, dst_pix_fmt,
                                       SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!convert_ctx)
    {
        VERBOSE(VB_IMPORTANT, "myth_sws_img_convert: Cannot initialize "
                "the image conversion context");
        return -1;
    }

    sws_scale(convert_ctx, src->data, src->linesize,
              0, height, dst->data, dst->linesize);

    return 0;
}

static int grabThumbnail(QString inFile, QString thumbList, QString outFile, int frameCount)
{
    av_register_all();

    AVFormatContext *inputFC = NULL;

    // Open recording
    VERBOSE(VB_JOBQUEUE, QString("grabThumbnail(): ") +
            QString("Opening '%1'").arg(inFile));

    QByteArray inFileBA = inFile.toLocal8Bit();

    int ret = av_open_input_file(&inputFC, inFileBA.constData(), NULL, 0, NULL);
    if (ret)
    {
        VERBOSE(VB_JOBQUEUE,QString("grabThumbnail(), Error: ") +
                "Couldn't open input file" + ENO);
        return 1;
    }

    // Getting stream information
    if ((ret = av_find_stream_info(inputFC)) < 0)
    {
        VERBOSE(VB_JOBQUEUE,
                QString("Couldn't get stream info, error #%1").arg(ret));
        av_close_input_file(inputFC);
        inputFC = NULL;
        return 1;
    }

    // find the first video stream
    int videostream = -1, width, height;
    float fps;

    for (uint i = 0; i < inputFC->nb_streams; i++)
    {
        AVStream *st = inputFC->streams[i];
        if (inputFC->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            videostream = i;
            width = st->codec->width;
            height = st->codec->height;
            if (st->r_frame_rate.den && st->r_frame_rate.num)
                fps = av_q2d(st->r_frame_rate);
            else
                fps = 1/av_q2d(st->time_base);
            break;
        }
    }

    if (videostream == -1)
    {
        VERBOSE(VB_JOBQUEUE, "Couldn't find a video stream");
        return 1;
    }

    // get the codec context for the video stream
    AVCodecContext *codecCtx = inputFC->streams[videostream]->codec;

    // get decoder for video stream
    AVCodec * codec = avcodec_find_decoder(codecCtx->codec_id);

    if (codec == NULL)
    {
        VERBOSE(VB_JOBQUEUE, "Couldn't find codec for video stream");
        return 1;
    }

    // open codec
    if (avcodec_open(codecCtx, codec) < 0)
    {
        VERBOSE(VB_JOBQUEUE, "Couldn't open codec for video stream");
        return 1;
    }

    // get list of required thumbs
    QStringList list = thumbList.split(",", QString::SkipEmptyParts);
    AVFrame *frame = avcodec_alloc_frame();
    AVPacket pkt;
    AVPicture orig;
    AVPicture retbuf;
    memset(&orig, 0, sizeof(AVPicture));
    memset(&retbuf, 0, sizeof(AVPicture));

    int bufflen = width * height * 4;
    unsigned char *outputbuf = new unsigned char[bufflen];

    int frameNo = -1, thumbCount = 0;
    int frameFinished = 0;
    int keyFrame;

    while (av_read_frame(inputFC, &pkt) >= 0)
    {
        if (pkt.stream_index == videostream)
        {
            frameNo++;
            if (list[thumbCount].toInt() == (int)(frameNo / fps))
            {
                thumbCount++;

                avcodec_flush_buffers(codecCtx);
                avcodec_decode_video2(codecCtx, frame, &frameFinished, &pkt);
                keyFrame = frame->key_frame;

                while (!frameFinished || !keyFrame)
                {
                    av_free_packet(&pkt);
                    int res = av_read_frame(inputFC, &pkt);
                    if (res < 0)
                        break;
                    if (pkt.stream_index == videostream)
                    {
                        frameNo++;
                        avcodec_decode_video2(codecCtx, frame, &frameFinished, &pkt);
                        keyFrame = frame->key_frame;
                    }
                }

                if (frameFinished)
                {
                    // work out what format to save to
                    QString saveFormat = "JPEG";
                    if (outFile.right(4) == ".png")
                        saveFormat = "PNG";

                    int count = 0;
                    while (count < frameCount)
                    {
                        QString filename = outFile;
                        if (filename.contains("%1") && filename.contains("%2"))
                            filename = filename.arg(thumbCount).arg(count+1);
                        else if (filename.contains("%1"))
                            filename = filename.arg(thumbCount);

                        avpicture_fill(&retbuf, outputbuf,
                                       PIX_FMT_RGB32, width, height);

                        avpicture_deinterlace((AVPicture*)frame,
                                              (AVPicture*)frame,
                                              codecCtx->pix_fmt, width, height);


                        myth_sws_img_convert(
                                    &retbuf, PIX_FMT_RGB32,
                                    (AVPicture*) frame,
                                    codecCtx->pix_fmt, width, height);

                        QImage img(outputbuf, width, height,
                                   QImage::Format_RGB32);

                        if (!img.save(filename, qPrintable(saveFormat)))
                        {
                            VERBOSE(VB_IMPORTANT,
                                    QString("grabThumbnail(), Error: ") +
                                    QString("Failed to save thumb: '%1'")
                                    .arg(filename));
                        }

                        count++;

                        if (count <= frameCount)
                        {
                            //grab next frame
                            frameFinished = false;
                            while (!frameFinished)
                            {
                                int res = av_read_frame(inputFC, &pkt);
                                if (res < 0)
                                    break;
                                if (pkt.stream_index == videostream)
                                {
                                    frameNo++;
                                    avcodec_decode_video2(codecCtx, frame,
                                                         &frameFinished,
                                                         &pkt);
                                }
                            }
                        }
                    }
                }

                if (thumbCount >= (int) list.count())
                    break;
            }
        }

        av_free_packet(&pkt);
    }

    if (outputbuf)
        delete[] outputbuf;

    // free the frame
    av_free(frame);

    // close the codec
    avcodec_close(codecCtx);

    // close the video file
    av_close_input_file(inputFC);

    return 0;
}

static long long getFrameCount(AVFormatContext *inputFC, int vid_id)
{
    AVPacket pkt;
    long long count = 0;

    VERBOSE(VB_JOBQUEUE, "Calculating frame count");

    av_init_packet(&pkt);

    while (av_read_frame(inputFC, &pkt) >= 0)
    {
        if (pkt.stream_index == vid_id)
        {
            count++;
        }
        av_free_packet(&pkt);
    }

    return count;
}

static long long getCutFrames(const QString &filename, long long lastFrame)
{
    // only wont the filename
    QString basename = filename;
    int pos = filename.lastIndexOf('/');
    if (pos > 0)
        basename = filename.mid(pos + 1);

    ProgramInfo *progInfo = getProgramInfoForFile(basename);
    if (!progInfo)
        return 0;

    if (progInfo->IsVideo())
    {
        delete progInfo;
        return 0;
    }

    frm_dir_map_t cutlist;
    frm_dir_map_t::iterator it;
    uint64_t frames = 0;

    progInfo->QueryCutList(cutlist);

    if (cutlist.size() == 0)
    {
        delete progInfo;
        return 0;
    }

    for (it = cutlist.begin(); it != cutlist.end();)
    {
        uint64_t start = 0, end = 0;

        if (it.value() == MARK_CUT_START)
        {
            start = it.key();
            it++;
            if (it != cutlist.end())
            {
                end = it.key();
                it++;
            }
            else
                end = lastFrame;
        }
        else if (it.value() == MARK_CUT_END)
        {
            start = 0;
            end = it.key();
            it++;
        }
        else
        {
            it++;
            continue;
        }

        frames += end - start;
    }

    delete progInfo;
    return frames;
}

static long long getFrameCount(const QString &filename, float fps)
{
    // only wont the filename
    QString basename = filename;
    int pos = filename.lastIndexOf('/');
    if (pos > 0)
        basename = filename.mid(pos + 1);

    int keyframedist = -1;
    frm_pos_map_t posMap;

    ProgramInfo *progInfo = getProgramInfoForFile(basename);
    if (!progInfo)
        return 0;

    progInfo->QueryPositionMap(posMap, MARK_GOP_BYFRAME);
    if (!posMap.empty())
    {
        keyframedist = 1;
    }
    else
    {
        progInfo->QueryPositionMap(posMap, MARK_GOP_START);
        if (!posMap.empty())
        {
            keyframedist = 15;
            if (fps < 26 && fps > 24)
                keyframedist = 12;
        }
        else
        {
            progInfo->QueryPositionMap(posMap, MARK_KEYFRAME);
            if (!posMap.empty())
            {
                // keyframedist should be set in the fileheader so no
                // need to try to determine it in this case
                return 0;
            }
        }
    }

    if (posMap.empty())
        return 0; // no position map in recording

    frm_pos_map_t::const_iterator it = posMap.end();
    it--;
    uint64_t totframes = it.key() * keyframedist;
    return totframes;
}

static int getFileInfo(QString inFile, QString outFile, int lenMethod)
{
    const char *type = NULL;

    av_register_all();

    AVFormatContext *inputFC = NULL;
    AVInputFormat *fmt = NULL;

    if (type)
        fmt = av_find_input_format(type);

    // Open recording
    VERBOSE(VB_JOBQUEUE, QString("getFileInfo(): ") +
            QString("Opening '%1'").arg(inFile));

    QByteArray inFileBA = inFile.toLocal8Bit();

    int ret = av_open_input_file(&inputFC, inFileBA.constData(), fmt, 0, NULL);

    if (ret)
    {
        VERBOSE(VB_JOBQUEUE, QString("getFileInfo(), Error: ") +
                "Couldn't open input file" + ENO);
        return 1;
    }

    // Getting stream information
    ret = av_find_stream_info(inputFC);

    if (ret < 0)
    {
        VERBOSE(VB_JOBQUEUE,
            QString("Couldn't get stream info, error #%1").arg(ret));
        av_close_input_file(inputFC);
        inputFC = NULL;
        return 1;
    }

    av_estimate_timings(inputFC, 0);

    // Dump stream information
    dump_format(inputFC, 0, inFileBA.constData(), 0);

    QDomDocument doc("FILEINFO");

    QDomElement root = doc.createElement("file");
    doc.appendChild(root);
    root.setAttribute("type", inputFC->iformat->name);
    root.setAttribute("filename", inFile);

    QDomElement streams = doc.createElement("streams");

    root.appendChild(streams);
    streams.setAttribute("count", inputFC->nb_streams);
    int ffmpegIndex = 0;
    uint duration = 0;

    for (uint i = 0; i < inputFC->nb_streams; i++)
    {
        AVStream *st = inputFC->streams[i];
        char buf[256];

        avcodec_string(buf, sizeof(buf), st->codec, false);

        switch (inputFC->streams[i]->codec->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                QStringList param = QString(buf).split(',', QString::SkipEmptyParts);
                QString codec = param[0].remove("Video:", Qt::CaseInsensitive);
                QDomElement stream = doc.createElement("video");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);
                stream.setAttribute("codec", codec.trimmed());
                stream.setAttribute("width", st->codec->width);
                stream.setAttribute("height", st->codec->height);
                stream.setAttribute("bitrate", st->codec->bit_rate);

                float fps;
                if (st->r_frame_rate.den && st->r_frame_rate.num)
                    fps = av_q2d(st->r_frame_rate);
                else
                    fps = 1/av_q2d(st->time_base);

                stream.setAttribute("fps", fps);

                if (st->codec->sample_aspect_ratio.den && st->codec->sample_aspect_ratio.num)
                {
                    float aspect_ratio = av_q2d(st->codec->sample_aspect_ratio);
                    if (QString(inputFC->iformat->name) != "nuv")
                        aspect_ratio = ((float)st->codec->width / st->codec->height) * aspect_ratio;

                    stream.setAttribute("aspectratio", aspect_ratio);
                }
                else
                    stream.setAttribute("aspectratio", "N/A");

                stream.setAttribute("id", st->id);

                if (st->start_time != (int) AV_NOPTS_VALUE)
                {
                    int secs, us;
                    secs = st->start_time / AV_TIME_BASE;
                    us = st->start_time % AV_TIME_BASE;
                    stream.setAttribute("start_time", QString("%1.%2")
                            .arg(secs).arg(av_rescale(us, 1000000, AV_TIME_BASE)));
                }
                else
                    stream.setAttribute("start_time", 0);

                streams.appendChild(stream);

                // TODO: probably should add a better way to choose which
                // video stream we use to calc the duration
                if (duration == 0)
                {
                    long long frameCount = 0;

                    switch (lenMethod)
                    {
                        case 0:
                        {
                            // use duration guess from avformat
                            if (inputFC->duration != (uint) AV_NOPTS_VALUE)
                            {
                                duration = (uint) (inputFC->duration / AV_TIME_BASE);
                                root.setAttribute("duration", duration);
                                VERBOSE(VB_JOBQUEUE, QString("duration = %1")
                                        .arg(duration));
                                frameCount = (long long)(duration * fps);
                            }
                            else
                                root.setAttribute("duration", "N/A");
                            break;
                        }
                        case 1:
                        {
                            // calc duration of the file by counting the video frames
                            frameCount = getFrameCount(inputFC, i);
                            VERBOSE(VB_JOBQUEUE, QString("frames = %1").arg(frameCount));
                            duration = (uint)(frameCount / fps);
                            VERBOSE(VB_JOBQUEUE, QString("duration = %1").arg(duration));
                            root.setAttribute("duration", duration);
                            break;
                        }
                        case 2:
                        {
                            // use info from pos map in db
                            // (only useful if the file is a myth recording)
                            frameCount = getFrameCount(inFile, fps);
                            VERBOSE(VB_JOBQUEUE, QString("frames = %1").arg(frameCount));
                            duration = (uint)(frameCount / fps);
                            VERBOSE(VB_JOBQUEUE, QString("duration = %1").arg(duration));
                            root.setAttribute("duration", duration);
                            break;
                        }
                        default:
                            root.setAttribute("duration", "N/A");
                            VERBOSE(VB_JOBQUEUE, QString("Unknown lenMethod (%1)").arg(lenMethod));
                    }

                    // add duration after all cuts are removed
                    long long cutFrames = getCutFrames(inFile, frameCount);
                    VERBOSE(VB_JOBQUEUE, QString("cutframes = %1").arg(cutFrames));
                    int cutduration = (int)(cutFrames / fps);
                    VERBOSE(VB_JOBQUEUE, QString("cutduration = %1").arg(cutduration));
                    root.setAttribute("cutduration", duration - cutduration);
                }

                break;
            }

            case CODEC_TYPE_AUDIO:
            {
                QStringList param = QString(buf).split(',', QString::SkipEmptyParts);
                QString codec = param[0].remove("Audio:", Qt::CaseInsensitive);

                QDomElement stream = doc.createElement("audio");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);

                // change any streams identified as "liba52" to "AC3" which is what
                // the mythburn.py script expects to get.
                if (codec.trimmed().toLower() == "liba52")
                    stream.setAttribute("codec", "AC3");
                else
                    stream.setAttribute("codec", codec.trimmed());

                stream.setAttribute("channels", st->codec->channels);
                if (strlen(st->language) > 0)
                    stream.setAttribute("language", st->language);
                else
                    stream.setAttribute("language", "N/A");

                stream.setAttribute("id", st->id);

                stream.setAttribute("samplerate", st->codec->sample_rate);
                stream.setAttribute("bitrate", st->codec->bit_rate);

                if (st->start_time != (int) AV_NOPTS_VALUE)
                {
                    int secs, us;
                    secs = st->start_time / AV_TIME_BASE;
                    us = st->start_time % AV_TIME_BASE;
                    stream.setAttribute("start_time", QString("%1.%2")
                            .arg(secs).arg(av_rescale(us, 1000000, AV_TIME_BASE)));
                }
                else
                    stream.setAttribute("start_time", 0);

                streams.appendChild(stream);

                break;
            }

            case CODEC_TYPE_SUBTITLE:
            {
                QStringList param = QString(buf).split(',', QString::SkipEmptyParts);
                QString codec = param[0].remove("Subtitle:", Qt::CaseInsensitive);

                QDomElement stream = doc.createElement("subtitle");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);
                stream.setAttribute("codec", codec.trimmed());
                if (strlen(st->language) > 0)
                    stream.setAttribute("language", st->language);
                else
                    stream.setAttribute("language", "N/A");

                stream.setAttribute("id", st->id);

                streams.appendChild(stream);

                break;
            }

            case CODEC_TYPE_DATA:
            {
                QDomElement stream = doc.createElement("data");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("codec", buf);
                streams.appendChild(stream);

                break;
            }

            default:
                VERBOSE(VB_JOBQUEUE, QString(
                    "Skipping unsupported codec %1 on stream %2")
                     .arg(inputFC->streams[i]->codec->codec_type).arg(i));
                break;
        }
    }

    // finally save the xml to the file
    QFile f(outFile);
    if (!f.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_JOBQUEUE, "Failed to open file for writing - " + outFile);
        return 1;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();

    // Close input file
    av_close_input_file(inputFC);
    inputFC = NULL;

    return 0;
}

static int getDBParamters(QString outFile)
{
    DatabaseParams params = gContext->GetDatabaseParams();

    // save the db paramters to the file
    QFile f(outFile);
    if (!f.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("MythArchiveHelper: "
                "Failed to open file for writing - %1")
                .arg(outFile.toLocal8Bit().constData()));
        return 1;
    }

    QTextStream t(&f);
    t << params.dbHostName << endl;
    t << params.dbUserName << endl;
    t << params.dbPassword << endl;
    t << params.dbName << endl;
    t << gCoreContext->GetHostName() << endl;
    t << GetInstallPrefix() << endl;
    f.close();

    return 0;
}

static int isRemote(QString filename)
{
    // check if the file exists
    if (!QFile::exists(filename))
        return 0;

    struct statfs statbuf;
    memset(&statbuf, 0, sizeof(statbuf));

#if CONFIG_DARWIN
    if ((statfs(qPrintable(filename), &statbuf) == 0) &&
        ((!strcmp(statbuf.f_fstypename, "nfs")) ||      // NFS|FTP
            (!strcmp(statbuf.f_fstypename, "afpfs")) || // ApplShr
            (!strcmp(statbuf.f_fstypename, "smbfs"))))  // SMB
        return 2;
#elif __linux__
    if ((statfs(qPrintable(filename), &statbuf) == 0) &&
        ((statbuf.f_type == 0x6969) ||      // NFS
            (statbuf.f_type == 0x517B)))    // SMB
        return 2;
#endif

    return 1;
}

static void showUsage()
{
    cout << "Usage of mytharchivehelper\n";
    cout << "-t/--createthumbnail infile thumblist outfile framecount\n";
    cout << "       (create one or more thumbnails)\n";
    cout << "       infile     - filename of recording to grab thumbs from\n";
    cout << "       thumblist  - comma seperated list of required thumbs in seconds\n";
    cout << "       outfile    - name of file to save thumbs to eg 'thumb%1-%2.jpg'\n";
    cout << "                    %1 will be replaced with the no. of the thumb\n";
    cout << "                    %2 will be replaced with the frame no.\n";
    cout << "       framecount - no of frames to grab\n\n";
    cout << "-i/--getfileinfo infile outfile method\n";
    cout << "       (write some file information to outfile for infile)\n";
    cout << "       method is the method to used to calc the file duration\n";
    cout << "       0 - use av_estimate_timings() (quick but not very accurate)\n";
    cout << "       1 - read all frames (most accurate but slow)\n";
    cout << "       2 - use position map in DB (quick, only works for MythTV recordings)\n\n";
    cout << "-p/--getdbparameters outfile\n";
    cout << "       (write the mysql database parameters to outfile)\n\n";
    cout << "-n/--nativearchive jobfile \n";
    cout << "       (archive files to a native archive format)\n";
    cout << "       jobfile    - filename of archive job file\n\n";
    cout << "-f/--importarchive xmlfile chanID\n";
    cout << "       (import an archived file)\n";
    cout << "       xmlfile    - filename of the archive xml file\n";
    cout << "       chanID     - chanID to use when inserting records in DB\n\n";
    cout << "-r/--isremote file\n";
    cout << "       (check if file is on a remote filesystem)\n";
    cout << "       file       - filename to check\n";
    cout << "       returns    - 0 on error or file not found\n";
    cout << "                  - 1 file is on a local filesystem\n";
    cout << "                  - 2 file is on a remote filesystem\n\n";
    cout << "-b/--burndvd mediatype erasedvdrw format\n";
    cout << "       (burn a created dvd to a blank disk)\n";
    cout << "       mediatype  - 0 = single layer DVD\n";
    cout << "                    1 = dual layer DVD\n";
    cout << "                    2 = rewritable DVD\n";
    cout << "       erasedvdrw - 0 = don't erase\n";
    cout << "                    1 = force erase\n";
    cout << "       native     - 0 = not a native archive\n";
    cout << "                    1 = is a native archive\n\n";
    cout << "-s/--sup2dast supfile ifofile delay\n";
    cout << "       (convert projectX subtitles to dvd subtitles)\n";
    cout << "       supfile    - input sup file\n";
    cout << "       ifofile    - input sup file\n";
    cout << "       delay      - delay to add to subtitles (default is 0ms)\n\n";
    cout << "-l/--log logfile\n";
    cout << "       (name of log file to send messages)\n\n";
    cout << "-v/--verbose debug-level\n";
    cout << "       (Use '-v help' for level info)\n\n";
    cout << "-h/--help\n";
    cout << "       (shows this usage)\n";
}

int main(int argc, char **argv)
{
    // by default we only output our messages
    print_verbose_messages = VB_JOBQUEUE;

    QApplication a(argc, argv, false);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        cout << "mytharchivehelper: Could not initialize MythContext. "
                "Exiting." << endl;
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    int res = 0;
    bool bShowUsage = false;
    bool bGrabThumbnail = false;
    bool bGetDBParameters = false;
    bool bNativeArchive = false;
    bool bImportArchive = false;
    bool bGetFileInfo = false;
    bool bIsRemote = false;
    bool bDoBurn = false;
    int  mediaType = AD_DVD_SL;
    bool bEraseDVDRW = false;
    bool bNativeFormat = false;
    bool bSup2Dast = false;
    QString thumbList;
    QString inFile;
    QString outFile;
    QString logFile;
    QString ifoFile;
    int lenMethod = 0;
    int chanID = -1;
    int frameCount = 1;
    int delay = 0;

    //  Check command line arguments
    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-v") ||
             !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-t") ||
                  !strcmp(a.argv()[argpos],"--grabthumbnail"))
        {
            bGrabThumbnail = true;

            if (a.argc()-1 > argpos)
            {
                inFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing infile in -t/--grabthumbnail option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                thumbList = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing thumblist in -t/--grabthumbnail option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                outFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing outfile in -t/--grabthumbnail option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                QString arg(a.argv()[argpos+1]);
                frameCount = arg.toInt();
                ++argpos;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-p") ||
             !strcmp(a.argv()[argpos],"--getdbparameters"))
        {
            bGetDBParameters = true;
            if (a.argc()-1 > argpos)
            {
                outFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -p/--getdbparameters option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-r") ||
                  !strcmp(a.argv()[argpos],"--isremote"))
        {
            bIsRemote = true;
            if (a.argc()-1 > argpos)
            {
                inFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -r/--isremote option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-b") ||
                  !strcmp(a.argv()[argpos],"--burndvd"))
        {
            bDoBurn = true;
            if (a.argc()-1 > argpos)
            {
                QString arg(a.argv()[argpos+1]);
                mediaType = arg.toInt();
                if (mediaType < 0 || mediaType > 2)
                {
                    cerr << "Invalid mediatype given: " << mediaType << "\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -b/--burndvd option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                QString arg(a.argv()[argpos+1]);
                int value = arg.toInt();
                if (value < 0 || value > 1)
                {
                    cerr << "Invalid erasedvd parameter given: " << value << "\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                bEraseDVDRW = (value == 1);
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -b/--burndvd option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                QString arg(a.argv()[argpos+1]);
                int value = arg.toInt();
                if (value < 0 || value > 1)
                {
                    cerr << "Invalid native format parameter given: " << value << "\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                bNativeFormat = (value == 1);
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -b/--burndvd option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-n") ||
                  !strcmp(a.argv()[argpos],"--nativearchive"))
        {
            bNativeArchive = true;
            if (a.argc()-1 > argpos)
            {
                outFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -r/--nawarchive option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-f") ||
                  !strcmp(a.argv()[argpos],"--importarchive"))
        {
            bImportArchive = true;
            if (a.argc()-1 > argpos)
            {
                inFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing filename argument to -f/--importarchive option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                QString arg(a.argv()[argpos+1]);
                chanID = arg.toInt();
                ++argpos;
            }
            else
            {
                cerr << "Missing chanID argument to -f/--importarchive option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-l") ||
                  !strcmp(a.argv()[argpos],"--log"))
        {
            //FIXME
            if (a.argc()-1 > argpos)
            {
                logFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -l/--log option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
                  !strcmp(a.argv()[argpos],"--help"))
        {
            bShowUsage = true;
        }
        else if (!strcmp(a.argv()[argpos],"-i") ||
                  !strcmp(a.argv()[argpos],"--getfileinfo"))
        {
            bGetFileInfo = true;

            if (a.argc()-1 > argpos)
            {
                inFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing infile in -i/--getfileinfo option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                outFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing outfile in -i/--getfileinfo option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                QString arg(a.argv()[argpos+1]);
                lenMethod = arg.toInt();
                ++argpos;
            }
            else
            {
                cerr << "Missing method in -i/--getfileinfo option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-s") ||
                  !strcmp(a.argv()[argpos],"--sup2dast"))
        {
            bSup2Dast = true;

            if (a.argc()-1 > argpos)
            {
                inFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing supfile in -s/--sup2dast option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                ifoFile = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing ifofile in -s/--sup2dast option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos)
            {
                QString arg(a.argv()[argpos+1]);
                delay = arg.toInt();
                ++argpos;
            }
            else
            {
                cerr << "Missing delay in -s/--sup2dast option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else
        {
            cout << "Invalid argument: " << a.argv()[argpos] << endl;
            showUsage();
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
    }

    if (bShowUsage)
        showUsage();
    else if (bGrabThumbnail)
        res = grabThumbnail(inFile, thumbList, outFile, frameCount);
    else if (bGetDBParameters)
        res = getDBParamters(outFile);
    else if (bNativeArchive)
        res = doNativeArchive(outFile);
    else if (bImportArchive)
        res = doImportArchive(inFile, chanID);
    else if (bGetFileInfo)
        res = getFileInfo(inFile, outFile, lenMethod);
    else if (bIsRemote)
        res = isRemote(inFile);
    else if (bDoBurn)
        res = doBurnDVD(mediaType, bEraseDVDRW, bNativeFormat);
    else if (bSup2Dast)
    {
        QByteArray inFileBA = inFile.toLocal8Bit();
        QByteArray ifoFileBA = ifoFile.toLocal8Bit();
        res = sup2dast(inFileBA.constData(), ifoFileBA.constData(), delay);
    }
    else
        showUsage();

    return res;
}


