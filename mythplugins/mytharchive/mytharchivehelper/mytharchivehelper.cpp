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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

// Qt headers
#include <QApplication>
#include <QDir>
#include <QDomElement>
#include <QFile>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QTextStream>

// MythTV headers
#include <mythconfig.h>
#include <libmyth/mythavframe.h>
#include <libmyth/mythcontext.h>
#include <libmythbase/exitcodes.h>
#include <libmythbase/filesysteminfo.h>
#include <libmythbase/mythcommandlineparser.h>
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythmiscutil.h>
#include <libmythbase/mythpluginexport.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythbase/mythversion.h>
#include <libmythbase/programinfo.h>
#include <libmythtv/mythavutil.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include "external/pxsup2dast.h"
}

// mytharchive headers
#include "../mytharchive/archiveutil.h"
#include "../mytharchive/remoteavformatcontext.h"

class NativeArchive
{
  public:
      NativeArchive(void);
      ~NativeArchive(void);

      static int doNativeArchive(const QString &jobFile);
      static int doImportArchive(const QString &xmlFile, int chanID);
      static bool copyFile(const QString &source, const QString &destination);
      static int importRecording(const QDomElement &itemNode,
                                 const QString &xmlFile, int chanID);
      static int importVideo(const QDomElement &itemNode, const QString &xmlFile);
      static int exportRecording(QDomElement &itemNode, const QString &saveDirectory);
      static int exportVideo(QDomElement &itemNode, const QString &saveDirectory);
  private:
      static QString findNodeText(const QDomElement &elem, const QString &nodeName);
      static int getFieldList(QStringList &fieldList, const QString &tableName);
};

NativeArchive::NativeArchive(void)
{
    // create the lock file so the UI knows we're running
    QString tempDir = getTempDirectory();
    QFile file(tempDir + "/logs/mythburn.lck");

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        LOG(VB_GENERAL, LOG_ERR, "NativeArchive: Failed to create lock file");

    QString pid = QString("%1").arg(getpid());
    file.write(pid.toLatin1());
    file.close();
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
    QString command = QString("mythutil --copyfile --infile '%1' --outfile '%2'")
                              .arg(source, destination);
    uint res = myth_system(command);
    if (res != GENERIC_EXIT_OK)
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            QString("Failed while running %1. Result: %2").arg(command).arg(res));
        return false;
    }

    return true;
}

static bool createISOImage(QString &sourceDirectory)
{
    LOG(VB_JOBQUEUE, LOG_INFO, "Creating ISO image");

    QString tempDirectory = getTempDirectory();

    tempDirectory += "work/";

    QString mkisofs = gCoreContext->GetSetting("MythArchiveMkisofsCmd", "mkisofs");
    QString command = mkisofs + " -R -J -V 'MythTV Archive' -o ";
    command += tempDirectory + "mythburn.iso " + sourceDirectory;

    uint res = myth_system(command);
    if (res != GENERIC_EXIT_OK)
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            QString("Failed while running mkisofs. Result: %1") .arg(res));
        return false;
    }

    LOG(VB_JOBQUEUE, LOG_INFO, "Finished creating ISO image");
    return true;
}

static int burnISOImage(int mediaType, bool bEraseDVDRW, bool nativeFormat)
{
    QString dvdDrive = gCoreContext->GetSetting("MythArchiveDVDLocation",
                                            "/dev/dvd");
    LOG(VB_JOBQUEUE, LOG_INFO, "Burning ISO image to " + dvdDrive);

    int     driveSpeed    = gCoreContext->GetNumSetting("MythArchiveDriveSpeed");
    QString tempDirectory = getTempDirectory();

    tempDirectory += "work/";

    QString command = gCoreContext->GetSetting("MythArchiveGrowisofsCmd",
                                           "growisofs");

    if (driveSpeed)
        command += " -speed=" + QString::number(driveSpeed);

    if (nativeFormat)
    {
        if (mediaType == AD_DVD_RW && bEraseDVDRW)
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
        if (mediaType == AD_DVD_RW && bEraseDVDRW)
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

    uint res = myth_system(command);
    if (res != GENERIC_EXIT_OK)
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            QString("Failed while running growisofs. Result: %1") .arg(res));
    }
    else
    {
        LOG(VB_JOBQUEUE, LOG_INFO, "Finished burning ISO image");
    }

    return res;
}

static int doBurnDVD(int mediaType, bool bEraseDVDRW, bool nativeFormat)
{
    gCoreContext->SaveSetting(
        "MythArchiveLastRunStart",
        MythDate::toString(MythDate::current(), MythDate::kDatabase));
    gCoreContext->SaveSetting("MythArchiveLastRunStatus", "Running");

    int res = burnISOImage(mediaType, bEraseDVDRW, nativeFormat);

    gCoreContext->SaveSetting(
        "MythArchiveLastRunEnd",
        MythDate::toString(MythDate::current(), MythDate::kDatabase));
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
        LOG(VB_JOBQUEUE, LOG_ERR, "Could not open job file: " + jobFile);
        return 1;
    }

    if (!doc.setContent(&file))
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "Could not load job file: " + jobFile);
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
        LOG(VB_JOBQUEUE, LOG_ERR,
            QString("Found %1 options nodes - should be 1")
                .arg(nodeList.count()));
        return 1;
    }
    LOG(VB_JOBQUEUE, LOG_INFO,
        QString("Options - createiso: %1,"
                " doburn: %2, mediatype: %3, erasedvdrw: %4")
            .arg(bCreateISO).arg(bDoBurn).arg(mediaType).arg(bEraseDVDRW));
    LOG(VB_JOBQUEUE, LOG_INFO, QString("savedirectory: %1").arg(saveDirectory));

    // figure out where to save files
    if (mediaType != AD_FILE)
    {
        saveDirectory = tempDir;
        if (!saveDirectory.endsWith("/"))
            saveDirectory += "/";

        saveDirectory += "work/";

        QDir dir(saveDirectory);
        if (dir.exists())
        {
            if (!MythRemoveDirectory(dir))
                LOG(VB_GENERAL, LOG_ERR,
                    "NativeArchive: Failed to clear work directory");
        }
        dir.mkpath(saveDirectory);
    }

    LOG(VB_JOBQUEUE, LOG_INFO,
        QString("Saving files to : %1").arg(saveDirectory));

    // get list of file nodes from the job file
    nodeList = doc.elementsByTagName("file");
    if (nodeList.count() < 1)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "Cannot find any file nodes?");
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
                LOG(VB_JOBQUEUE, LOG_ERR,
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
            LOG(VB_JOBQUEUE, LOG_ERR,
                "Native archive job failed to complete");
            return 1;
        }
    }

    // create an iso image if needed
    if (bCreateISO)
    {
        if (!createISOImage(saveDirectory))
        {
            LOG(VB_JOBQUEUE, LOG_ERR, "Native archive job failed to complete");
            return 1;
        }
    }

    LOG(VB_JOBQUEUE, LOG_INFO, "Native archive job completed OK");

    return 0;
}

static const QRegularExpression badChars { R"((/|\\|:|'|"|\?|\|))" };

static QString fixFilename(const QString &filename)
{
    QString ret = filename;
    ret.replace(badChars, "_");
    return ret;
}

int NativeArchive::getFieldList(QStringList &fieldList, const QString &tableName)
{
    fieldList.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.exec("DESCRIBE " + tableName))
    {
        while (query.next())
        {
            fieldList.append(query.value(0).toString());
        }
    }
    else
    {
        MythDB::DBError("describe table", query);
    }

    return fieldList.count();
}

int NativeArchive::exportRecording(QDomElement   &itemNode,
                                   const QString &saveDirectory)
{
    QString chanID;
    QString startTime;
    QString dbVersion = gCoreContext->GetSetting("DBSchemaVer", "");

    QString title = fixFilename(itemNode.attribute("title"));
    QString filename = itemNode.attribute("filename");
    bool doDelete = (itemNode.attribute("delete", "0") == "0");
    LOG(VB_JOBQUEUE, LOG_INFO, QString("Archiving %1 (%2), do delete: %3")
            .arg(title, filename, doDelete ? "true" : "false"));

    if (title == "" || filename == "")
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "Bad title or filename");
        return 0;
    }

    if (!extractDetailsFromFilename(filename, chanID, startTime))
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            QString("Failed to extract chanID and startTime from '%1'")
                .arg(filename));
        return 0;
    }

    // create the directory to hold this items files
    QDir dir(saveDirectory + title);
    if (!dir.exists())
        dir.mkpath(saveDirectory + title);
    if (!dir.exists())
        LOG(VB_GENERAL, LOG_ERR, "Failed to create savedir: " + ENO);

    LOG(VB_JOBQUEUE, LOG_INFO, "Creating xml file for " + title);
    QDomDocument doc("MYTHARCHIVEITEM");

    QDomElement root = doc.createElement("item");
    doc.appendChild(root);
    root.setAttribute("type", "recording");
    root.setAttribute("databaseversion", dbVersion);

    QDomElement recorded = doc.createElement("recorded");
    root.appendChild(recorded);

    // get details from recorded
    QStringList fieldList;
    getFieldList(fieldList, "recorded");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT " + fieldList.join(",")
                + " FROM recorded"
                  " WHERE chanid = :CHANID and starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);

    if (query.exec() && query.next())
    {
        QDomElement elem;
        QDomText text;

        for (int x = 0; x < fieldList.size(); x++)
        {
            elem = doc.createElement(fieldList[x]);
            text = doc.createTextNode(query.value(x).toString());
            elem.appendChild(text);
            recorded.appendChild(elem);
        }

        LOG(VB_JOBQUEUE, LOG_INFO, "Created recorded element for " + title);
    }
    else
    {
        LOG(VB_JOBQUEUE, LOG_INFO, "Failed to get recorded field list");
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
        LOG(VB_JOBQUEUE, LOG_INFO, "Created channel element for " + title);
    }
    else
    {
        // cannot find the original channel so create a default channel element
        LOG(VB_JOBQUEUE, LOG_ERR,
            "Cannot find channel details for chanid " + chanID);
        QDomElement channel = doc.createElement("channel");
        channel.setAttribute("chanid", chanID);
        channel.setAttribute("channum", "unknown");
        channel.setAttribute("callsign", "unknown");
        channel.setAttribute("name", "unknown");
        root.appendChild(channel);
        LOG(VB_JOBQUEUE, LOG_INFO,
            "Created a default channel element for " + title);
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
        LOG(VB_JOBQUEUE, LOG_INFO, "Created credits element for " + title);
    }

    // add any rating
    query.prepare("SELECT `system`, rating FROM recordedrating "
            "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);

    if (query.exec() && query.next())
    {
        QDomElement rating = doc.createElement("rating");
        rating.setAttribute("system", query.value(0).toString());
        rating.setAttribute("rating", query.value(1).toString());
        root.appendChild(rating);
        LOG(VB_JOBQUEUE, LOG_INFO, "Created rating element for " + title);
    }

    // add the recordedmarkup table
    QDomElement recordedmarkup = doc.createElement("recordedmarkup");
    query.prepare("SELECT chanid, starttime, mark, type, data "
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
            mark.setAttribute("type", query.value(3).toString());
            mark.setAttribute("data", query.value(4).toString());
            recordedmarkup.appendChild(mark);
        }
        root.appendChild(recordedmarkup);
        LOG(VB_JOBQUEUE, LOG_INFO, "Created recordedmarkup element for " + title);
    }

    // add the recordedseek table
    QDomElement recordedseek = doc.createElement("recordedseek");
    query.prepare("SELECT chanid, starttime, mark, `offset`, type "
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
        LOG(VB_JOBQUEUE, LOG_INFO,
            "Created recordedseek element for " + title);
    }

    // finally save the xml to the file
    QString baseName = getBaseName(filename);
    QString xmlFile = saveDirectory + title + "/" + baseName + ".xml";
    QFile f(xmlFile);
    if (!f.open(QIODevice::WriteOnly))
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            "MythNativeWizard: Failed to open file for writing - " + xmlFile);
        return 0;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();

    // copy the file
    LOG(VB_JOBQUEUE, LOG_INFO, "Copying video file");
    bool res = copyFile(filename, saveDirectory + title + "/" + baseName);
    if (!res)
        return 0;

    // copy preview image
    if (QFile::exists(filename + ".png"))
    {
        LOG(VB_JOBQUEUE, LOG_INFO, "Copying preview image");
        res = copyFile(filename + ".png", saveDirectory
                       + title + "/" + baseName + ".png");
        if (!res)
            return 0;
    }

    LOG(VB_JOBQUEUE, LOG_INFO, "Item Archived OK");

    return 1;
}

int NativeArchive::exportVideo(QDomElement   &itemNode,
                               const QString &saveDirectory)
{
    QString dbVersion = gCoreContext->GetSetting("DBSchemaVer", "");
    int intID = 0;
    int categoryID = 0;
    QString coverFile = "";

    QString title = fixFilename(itemNode.attribute("title"));
    QString filename = itemNode.attribute("filename");
    bool doDelete = (itemNode.attribute("delete", "0") == "0");
    LOG(VB_JOBQUEUE, LOG_INFO, QString("Archiving %1 (%2), do delete: %3")
            .arg(title, filename, doDelete ? "true" : "false"));

    if (title == "" || filename == "")
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "Bad title or filename");
        return 0;
    }

    // create the directory to hold this items files
    QDir dir(saveDirectory + title);
    if (!dir.exists())
        dir.mkdir(saveDirectory + title);

    LOG(VB_JOBQUEUE, LOG_INFO, "Creating xml file for " + title);
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

        LOG(VB_JOBQUEUE, LOG_INFO,
            "Created videometadata element for " + title);
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
        LOG(VB_JOBQUEUE, LOG_INFO,
            "Created videocategory element for " + title);
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
        MythDB::DBError("select countries", query);

    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            QDomElement country = doc.createElement("country");
            country.setAttribute("intid", query.value(0).toString());
            country.setAttribute("country", query.value(1).toString());
            countries.appendChild(country);
        }
        LOG(VB_JOBQUEUE, LOG_INFO, "Created videocountry element for " + title);
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
        MythDB::DBError("select genres", query);

    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            QDomElement genre = doc.createElement("genre");
            genre.setAttribute("intid", query.value(0).toString());
            genre.setAttribute("genre", query.value(1).toString());
            genres.appendChild(genre);
        }
        LOG(VB_JOBQUEUE, LOG_INFO, "Created videogenre element for " + title);
    }

    // finally save the xml to the file
    QFileInfo fileInfo(filename);
    QString xmlFile = saveDirectory + title + "/"
                      + fileInfo.fileName() + ".xml";
    QFile f(xmlFile);
    if (!f.open(QIODevice::WriteOnly))
    {
        LOG(VB_JOBQUEUE, LOG_INFO,
            "MythNativeWizard: Failed to open file for writing - " + xmlFile);
        return 0;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();

    // copy the file
    LOG(VB_JOBQUEUE, LOG_INFO, "Copying video file");
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
        LOG(VB_JOBQUEUE, LOG_INFO, "Copying cover file");
        res = copyFile(coverFile, saveDirectory + title
                            + "/" + fileInfo.fileName());
        if (!res)
        {
            return 0;
        }
    }

    LOG(VB_JOBQUEUE, LOG_INFO, "Item Archived OK");

    return 1;
}

int NativeArchive::doImportArchive(const QString &xmlFile, int chanID)
{
    // open xml file
    QDomDocument doc("mydocument");
    QFile file(xmlFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            "Failed to open file for reading - " + xmlFile);
        return 1;
    }

    if (!doc.setContent(&file))
    {
        file.close();
        LOG(VB_JOBQUEUE, LOG_ERR,
            "Failed to read from xml file - " + xmlFile);
        return 1;
    }
    file.close();

    QString docType = doc.doctype().name();
    QString type;
    QString dbVersion;
    QDomNodeList itemNodeList;
    QDomNode node;
    QDomElement itemNode;

    if (docType == "MYTHARCHIVEITEM")
    {
        itemNodeList = doc.elementsByTagName("item");

        if (itemNodeList.count() < 1)
        {
            LOG(VB_JOBQUEUE, LOG_ERR,
                "Couldn't find an 'item' element in XML file");
            return 1;
        }

        node = itemNodeList.item(0);
        itemNode = node.toElement();
        type = itemNode.attribute("type");
        dbVersion = itemNode.attribute("databaseversion");

        LOG(VB_JOBQUEUE, LOG_INFO,
            QString("Archive DB version: %1, Local DB version: %2")
                .arg(dbVersion, gCoreContext->GetSetting("DBSchemaVer")));
    }
    else
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "Not a native archive xml file - " + xmlFile);
        return 1;
    }

    if (type == "recording")
    {
        return importRecording(itemNode, xmlFile, chanID);
    }
    if (type == "video")
    {
        return importVideo(itemNode, xmlFile);
    }

    return 1;
}

int NativeArchive::importRecording(const QDomElement &itemNode,
                                   const QString     &xmlFile, int chanID)
{
    LOG(VB_JOBQUEUE, LOG_INFO,
        QString("Import recording using chanID: %1").arg(chanID));
    LOG(VB_JOBQUEUE, LOG_INFO,
        QString("Archived recording xml file: %1").arg(xmlFile));

    QString videoFile = xmlFile.left(xmlFile.length() - 4);
    QString basename = videoFile;
    int pos = videoFile.lastIndexOf('/');
    if (pos > 0)
        basename = videoFile.mid(pos + 1);

    QDomNodeList nodeList = itemNode.elementsByTagName("recorded");
    if (nodeList.count() < 1)
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            "Couldn't find a 'recorded' element in XML file");
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
            LOG(VB_JOBQUEUE, LOG_ERR,
                "This recording appears to already exist!!");
            return 1;
        }
    }

    QString destFile = MythCoreContext::GenMythURL(gCoreContext->GetMasterHostName(),
                                                   MythCoreContext::GetMasterServerPort(),
                                                   basename , "Default");

    // copy file to recording directory
    LOG(VB_JOBQUEUE, LOG_INFO, "Copying video file to: " + destFile);
    if (!copyFile(videoFile,  destFile))
        return 1;

    // copy any preview image to recording directory
    if (QFile::exists(videoFile + ".png"))
    {
        LOG(VB_JOBQUEUE, LOG_INFO, "Copying preview image file to: " + destFile + ".png");
        if (!copyFile(videoFile + ".png", destFile + ".png"))
            return 1;
    }

    // get a list of fields from the xmlFile
    QStringList fieldList;
    QStringList bindList;
    QDomNodeList nodes =  recordedNode.childNodes();

    for (int x = 0; x < nodes.count(); x++)
    {
        QDomNode n2 = nodes.item(x);
        QString field = n2.nodeName();
        fieldList.append(field);
        bindList.append(":" + field.toUpper());
    }

    // copy recorded to database
    query.prepare("INSERT INTO recorded (" + fieldList.join(",") + ") "
                  "VALUES (" + bindList.join(",") + ");");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":STARTTIME", startTime);

    for (int x = 0; x < fieldList.count(); x++)
        query.bindValue(bindList.at(x), findNodeText(recordedNode, fieldList.at(x)));

    if (query.exec())
        LOG(VB_JOBQUEUE, LOG_INFO, "Inserted recorded details into database");
    else
        MythDB::DBError("recorded insert", query);

    // copy recordedmarkup to db
    nodeList = itemNode.elementsByTagName("recordedmarkup");
    if (nodeList.count() < 1)
    {
        LOG(VB_JOBQUEUE, LOG_WARNING,
            "Couldn't find a 'recordedmarkup' element in XML file");
    }
    else
    {
        QDomNode n3 = nodeList.item(0);
        QDomElement markupNode = n3.toElement();

        nodeList = markupNode.elementsByTagName("mark");
        if (nodeList.count() < 1)
        {
            LOG(VB_JOBQUEUE, LOG_WARNING,
                "Couldn't find any 'mark' elements in XML file");
        }
        else
        {
            // delete any records for this recordings
            query.prepare("DELETE FROM recordedmarkup "
                          "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
            query.bindValue(":CHANID", chanID);
            query.bindValue(":STARTTIME", startTime);

            if (!query.exec())
                MythDB::DBError("recordedmarkup delete", query);

            // add any new records for this recording
            for (int x = 0; x < nodeList.count(); x++)
            {
                QDomNode n4 = nodeList.item(x);
                QDomElement e = n4.toElement();
                query.prepare("INSERT INTO recordedmarkup (chanid, starttime, "
                        "mark, type, data)"
                        "VALUES(:CHANID,:STARTTIME,:MARK,:TYPE,:DATA);");
                query.bindValue(":CHANID", chanID);
                query.bindValue(":STARTTIME", startTime);
                query.bindValue(":MARK", e.attribute("mark"));
                query.bindValue(":TYPE", e.attribute("type"));
                query.bindValue(":DATA", e.attribute("data"));

                if (!query.exec())
                {
                    MythDB::DBError("recordedmark insert", query);
                    return 1;
                }
            }

            LOG(VB_JOBQUEUE, LOG_INFO,
                "Inserted recordedmarkup details into database");
        }
    }

    // copy recordedseek to db
    nodeList = itemNode.elementsByTagName("recordedseek");
    if (nodeList.count() < 1)
    {
        LOG(VB_JOBQUEUE, LOG_WARNING,
            "Couldn't find a 'recordedseek' element in XML file");
    }
    else
    {
        QDomNode n5 = nodeList.item(0);
        QDomElement markupNode = n5.toElement();

        nodeList = markupNode.elementsByTagName("mark");
        if (nodeList.count() < 1)
        {
            LOG(VB_JOBQUEUE, LOG_WARNING,
                "Couldn't find any 'mark' elements in XML file");
        }
        else
        {
            // delete any records for this recordings
            query.prepare("DELETE FROM recordedseek "
                          "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
                query.bindValue(":CHANID", chanID);
                query.bindValue(":STARTTIME", startTime);
            query.exec();

            // add the new records for this recording
            for (int x = 0; x < nodeList.count(); x++)
            {
                QDomNode n6 = nodeList.item(x);
                QDomElement e = n6.toElement();
                query.prepare("INSERT INTO recordedseek (chanid, starttime, "
                        "mark, `offset`, type)"
                        "VALUES(:CHANID,:STARTTIME,:MARK,:OFFSET,:TYPE);");
                query.bindValue(":CHANID", chanID);
                query.bindValue(":STARTTIME", startTime);
                query.bindValue(":MARK", e.attribute("mark"));
                query.bindValue(":OFFSET", e.attribute("offset"));
                query.bindValue(":TYPE", e.attribute("type"));

                if (!query.exec())
                {
                    MythDB::DBError("recordedseek insert", query);
                    return 1;
                }
            }

            LOG(VB_JOBQUEUE, LOG_INFO,
                "Inserted recordedseek details into database");
        }
    }

    // FIXME are these needed?
    // copy credits to DB
    // copy rating to DB

    LOG(VB_JOBQUEUE, LOG_INFO, "Import completed OK");

    return 0;
}

int NativeArchive::importVideo(const QDomElement &itemNode, const QString &xmlFile)
{
    LOG(VB_JOBQUEUE, LOG_INFO, "Importing video");
    LOG(VB_JOBQUEUE, LOG_INFO,
        QString("Archived video xml file: %1").arg(xmlFile));

    QString videoFile = xmlFile.left(xmlFile.length() - 4);
    QFileInfo fileInfo(videoFile);
    QString basename = fileInfo.fileName();

    QDomNodeList nodeList = itemNode.elementsByTagName("videometadata");
    if (nodeList.count() < 1)
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            "Couldn't find a 'videometadata' element in XML file");
        return 1;
    }

    QDomNode n = nodeList.item(0);
    QDomElement videoNode = n.toElement();

    // copy file to video directory
    QString path = gCoreContext->GetSetting("VideoStartupDir");
    QString origFilename = findNodeText(videoNode, "filename");
    QStringList dirList = origFilename.split("/", Qt::SkipEmptyParts);
    QDir dir;
    for (int x = 0; x < dirList.count() - 1; x++)
    {
        path += "/" + dirList[x];
        if (!dir.exists(path))
        {
            if (!dir.mkdir(path))
            {
                LOG(VB_JOBQUEUE, LOG_ERR,
                    QString("Couldn't create directory '%1'").arg(path));
                return 1;
            }
        }
    }

    LOG(VB_JOBQUEUE, LOG_INFO, "Copying video file");
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
        LOG(VB_JOBQUEUE, LOG_INFO, "Copying cover file");

        if (!copyFile(archivePath + "/" + coverFilename, artworkDir + "/" + coverFilename))
        {
            return 1;
        }
    }
    else
    {
        coverFilename = "No Cover";
    }

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
    {
        LOG(VB_JOBQUEUE, LOG_INFO,
            "Inserted videometadata details into database");
    }
    else
    {
        MythDB::DBError("videometadata insert", query);
        return 1;
    }

    // get intid field for inserted record
    int intid = 0;
    query.prepare("SELECT intid FROM videometadata WHERE filename = :FILENAME;");
    query.bindValue(":FILENAME", path + "/" + basename);
    if (query.exec() && query.next())
    {
        intid = query.value(0).toInt();
    }
    else
    {
        MythDB::DBError("Failed to get intid", query);
        return 1;
    }

    LOG(VB_JOBQUEUE, LOG_INFO,
        QString("'intid' of inserted video is: %1").arg(intid));

    // copy genre to db
    nodeList = itemNode.elementsByTagName("genres");
    if (nodeList.count() < 1)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "No 'genres' element found in XML file");
    }
    else
    {
        n = nodeList.item(0);
        QDomElement genresNode = n.toElement();

        nodeList = genresNode.elementsByTagName("genre");
        if (nodeList.count() < 1)
        {
            LOG(VB_JOBQUEUE, LOG_WARNING,
                "Couldn't find any 'genre' elements in XML file");
        }
        else
        {
            for (int x = 0; x < nodeList.count(); x++)
            {
                n = nodeList.item(x);
                QDomElement e = n.toElement();
                int genreID = 0;
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
                    if (!query.exec() || !query.next())
                    {
                        LOG(VB_JOBQUEUE, LOG_ERR,
                            "Couldn't add genre to database");
                        continue;
                    }
                    genreID = query.value(0).toInt();
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

            LOG(VB_JOBQUEUE, LOG_INFO, "Inserted genre details into database");
        }
    }

    // copy country to db
    nodeList = itemNode.elementsByTagName("countries");
    if (nodeList.count() < 1)
    {
        LOG(VB_JOBQUEUE, LOG_INFO, "No 'countries' element found in XML file");
    }
    else
    {
        n = nodeList.item(0);
        QDomElement countriesNode = n.toElement();

        nodeList = countriesNode.elementsByTagName("country");
        if (nodeList.count() < 1)
        {
            LOG(VB_JOBQUEUE, LOG_WARNING,
                "Couldn't find any 'country' elements in XML file");
        }
        else
        {
            for (int x = 0; x < nodeList.count(); x++)
            {
                n = nodeList.item(x);
                QDomElement e = n.toElement();
                int countryID = 0;
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
                    if (!query.exec() || !query.next())
                    {
                        LOG(VB_JOBQUEUE, LOG_ERR,
                            "Couldn't add country to database");
                        continue;
                    }
                    countryID = query.value(0).toInt();
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

            LOG(VB_JOBQUEUE, LOG_INFO,
                "Inserted country details into database");
        }
    }

    // fix the category id
    nodeList = itemNode.elementsByTagName("category");
    if (nodeList.count() < 1)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "No 'category' element found in XML file");
    }
    else
    {
        n = nodeList.item(0);
        QDomElement e = n.toElement();
        int categoryID = 0;
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
                LOG(VB_JOBQUEUE, LOG_ERR, "Couldn't add category to database");
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

        LOG(VB_JOBQUEUE, LOG_INFO, "Fixed the category in the database");
    }

    LOG(VB_JOBQUEUE, LOG_INFO, "Import completed OK");

    return 0;
}

QString NativeArchive::findNodeText(const QDomElement &elem, const QString &nodeName)
{
    QDomNodeList nodeList = elem.elementsByTagName(nodeName);
    if (nodeList.count() < 1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Couldn't find a '%1' element in XML file") .arg(nodeName));
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
    if ((nodeName == "recgroup") ||
        (nodeName == "playgroup"))
    {
        res = "Default";
    }
    else if ((nodeName == "recordid")  ||
             (nodeName == "seriesid")  ||
             (nodeName == "programid") ||
             (nodeName == "profile"))
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
        MythDB::DBError("delete archiveitems", query);
}

static int doNativeArchive(const QString &jobFile)
{
    gCoreContext->SaveSetting("MythArchiveLastRunType", "Native Export");
    gCoreContext->SaveSetting(
        "MythArchiveLastRunStart",
        MythDate::toString(MythDate::current(), MythDate::kDatabase));
    gCoreContext->SaveSetting("MythArchiveLastRunStatus", "Running");

    int res = NativeArchive::doNativeArchive(jobFile);
    gCoreContext->SaveSetting(
        "MythArchiveLastRunEnd",
        MythDate::toString(MythDate::current(), MythDate::kDatabase));
    gCoreContext->SaveSetting("MythArchiveLastRunStatus",
                              (res == 0 ? "Success" : "Failed"));

    // clear the archiveitems table if succesful
    if (res == 0)
        clearArchiveTable();

    return res;
}

static int doImportArchive(const QString &inFile, int chanID)
{
    return NativeArchive::doImportArchive(inFile, chanID);
}

static int grabThumbnail(const QString& inFile, const QString& thumbList, const QString& outFile, int frameCount)
{
    // Open recording
    LOG(VB_JOBQUEUE, LOG_INFO, QString("grabThumbnail(): Opening '%1'")
            .arg(inFile));

    MythCodecMap codecmap;
    ArchiveRemoteAVFormatContext inputFC(inFile);
    if (!inputFC.isOpen())
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "grabThumbnail(): Couldn't open input file" +
                                  ENO);
        return 1;
    }

    // Getting stream information
    int ret = avformat_find_stream_info(inputFC, nullptr);
    if (ret < 0)
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            QString("Couldn't get stream info, error #%1").arg(ret));
        return 1;
    }

    // find the first video stream
    int videostream = -1;
    int width = 0;
    int height = 0;
    float fps = NAN;

    for (uint i = 0; i < inputFC->nb_streams; i++)
    {
        AVStream *st = inputFC->streams[i];
        if (inputFC->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videostream = i;
            width = st->codecpar->width;
            height = st->codecpar->height;
            if (st->r_frame_rate.den && st->r_frame_rate.num)
                fps = av_q2d(st->r_frame_rate);
            else
                fps = 1/av_q2d(st->time_base);
            break;
        }
    }

    if (videostream == -1)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "Couldn't find a video stream");
        return 1;
    }

    // get the codec context for the video stream
    AVCodecContext *codecCtx = codecmap.GetCodecContext(inputFC->streams[videostream]);

    // get decoder for video stream
    const AVCodec * codec = avcodec_find_decoder(codecCtx->codec_id);

    if (codec == nullptr)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "Couldn't find codec for video stream");
        return 1;
    }

    // open codec
    if (avcodec_open2(codecCtx, codec, nullptr) < 0)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "Couldn't open codec for video stream");
        return 1;
    }

    // get list of required thumbs
    QStringList list = thumbList.split(",", Qt::SkipEmptyParts);
    MythAVFrame frame;
    if (!frame)
    {
        return 1;
    }
    AVPacket pkt;
    AVFrame orig;
    AVFrame retbuf;
    memset(&orig, 0, sizeof(AVFrame));
    memset(&retbuf, 0, sizeof(AVFrame));
    MythAVCopy copyframe;

    int bufflen = width * height * 4;
    auto *outputbuf = new unsigned char[bufflen];

    int frameNo = -1;
    int thumbCount = 0;
    bool frameFinished = false;

    while (av_read_frame(inputFC, &pkt) >= 0)
    {
        if (pkt.stream_index == videostream)
        {
            frameNo++;
            if (list[thumbCount].toInt() == (int)(frameNo / fps))
            {
                thumbCount++;

                avcodec_flush_buffers(codecCtx);
                av_frame_unref(frame);
                frameFinished = false;
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == 0)
                    frameFinished = true;
                if (ret == 0 || ret == AVERROR(EAGAIN))
                    avcodec_send_packet(codecCtx, &pkt);
                bool keyFrame = (frame->flags & AV_FRAME_FLAG_KEY) != 0;

                while (!frameFinished || !keyFrame)
                {
                    av_packet_unref(&pkt);
                    int res = av_read_frame(inputFC, &pkt);
                    if (res < 0)
                        break;
                    if (pkt.stream_index == videostream)
                    {
                        frameNo++;
                        av_frame_unref(frame);
                        ret = avcodec_receive_frame(codecCtx, frame);
                        if (ret == 0)
                            frameFinished = true;
                        if (ret == 0 || ret == AVERROR(EAGAIN))
                            avcodec_send_packet(codecCtx, &pkt);
                        keyFrame = (frame->flags & AV_FRAME_FLAG_KEY) != 0;
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

                        av_image_fill_arrays(retbuf.data, retbuf.linesize, outputbuf,
                            AV_PIX_FMT_RGB32, width, height, IMAGE_ALIGN);

                        AVFrame *tmp = frame;
                        MythAVUtil::DeinterlaceAVFrame(tmp);

                        copyframe.Copy(&retbuf, AV_PIX_FMT_RGB32, tmp,
                                       codecCtx->pix_fmt, width, height);

                        QImage img(outputbuf, width, height,
                                   QImage::Format_RGB32);

                        if (!img.save(filename, qPrintable(saveFormat)))
                        {
                            LOG(VB_GENERAL, LOG_ERR,
                                QString("grabThumbnail(): Failed to save "
                                        "thumb: '%1'")
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
                                    ret = avcodec_receive_frame(codecCtx, frame);
                                    if (ret == 0)
                                        frameFinished = true;
                                    if (ret == 0 || ret == AVERROR(EAGAIN))
                                        avcodec_send_packet(codecCtx, &pkt);
                                }
                            }
                        }
                    }
                }

                if (thumbCount >= list.count())
                    break;
            }
        }

        av_packet_unref(&pkt);
    }

    delete[] outputbuf;

    // close the codec
    codecmap.FreeCodecContext(inputFC->streams[videostream]);

    return 0;
}

static int64_t getFrameCount(AVFormatContext *inputFC, int vid_id)
{
    int64_t count = 0;

    LOG(VB_JOBQUEUE, LOG_INFO, "Calculating frame count");

    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, "packet allocation failed");
        return 0;
    }
    while (av_read_frame(inputFC, pkt) >= 0)
    {
        if (pkt->stream_index == vid_id)
        {
            count++;
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    return count;
}

static int64_t getCutFrames(const QString &filename, int64_t lastFrame)
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

    if (cutlist.empty())
    {
        delete progInfo;
        return 0;
    }

    for (it = cutlist.begin(); it != cutlist.end();)
    {
        uint64_t start = 0;
        uint64_t end = 0;

        if (it.value() == MARK_CUT_START)
        {
            start = it.key();
            ++it;
            if (it != cutlist.end())
            {
                end = it.key();
                ++it;
            }
            else
            {
                end = lastFrame;
            }
        }
        else if (it.value() == MARK_CUT_END)
        {
            start = 0;
            end = it.key();
            ++it;
        }
        else
        {
            ++it;
            continue;
        }

        frames += end - start;
    }

    delete progInfo;
    return frames;
}

static int64_t getFrameCount(const QString &filename, float fps)
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
                delete progInfo;
                return 0;
            }
        }
    }

    delete progInfo;
    if (posMap.empty())
        return 0; // no position map in recording

    frm_pos_map_t::const_iterator it = posMap.cend();
    --it;
    uint64_t totframes = it.key() * keyframedist;
    return totframes;
}

static int getFileInfo(const QString& inFile, const QString& outFile, int lenMethod)
{
    // Open recording
    LOG(VB_JOBQUEUE , LOG_INFO, QString("getFileInfo(): Opening '%1'")
            .arg(inFile));

    MythCodecMap codecmap;
    ArchiveRemoteAVFormatContext inputFC(inFile);
    if (!inputFC.isOpen())
    {
        LOG(VB_JOBQUEUE, LOG_ERR, "getFileInfo(): Couldn't open input file" +
                                  ENO);
        return 1;
    }

    // Getting stream information
    int ret = avformat_find_stream_info(inputFC, nullptr);

    if (ret < 0)
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            QString("Couldn't get stream info, error #%1").arg(ret));
        return 1;
    }

    // Dump stream information
    av_dump_format(inputFC, 0, qPrintable(inFile), 0);

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
        std::string buf (256,'\0');
        AVCodecContext *avctx = codecmap.GetCodecContext(st);
        AVCodecParameters *par = st->codecpar;

        if (avctx)
            avcodec_string(buf.data(), buf.size(), avctx, static_cast<int>(false));

        switch (st->codecpar->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
            {
                QStringList param = QString::fromStdString(buf).split(',', Qt::SkipEmptyParts);
                QString codec = param[0].remove("Video:", Qt::CaseInsensitive).remove(QChar::Null);
                QDomElement stream = doc.createElement("video");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);
                stream.setAttribute("codec", codec.trimmed());
                stream.setAttribute("width", par->width);
                stream.setAttribute("height", par->height);
                stream.setAttribute("bitrate", (qlonglong)par->bit_rate);

                float fps = NAN;
                if (st->r_frame_rate.den && st->r_frame_rate.num)
                    fps = av_q2d(st->r_frame_rate);
                else
                    fps = 1/av_q2d(st->time_base);

                stream.setAttribute("fps", fps);

                if (par->sample_aspect_ratio.den && par->sample_aspect_ratio.num)
                {
                    float aspect_ratio = av_q2d(par->sample_aspect_ratio);
                    if (QString(inputFC->iformat->name) != "nuv")
                        aspect_ratio = ((float)par->width
                        / par->height) * aspect_ratio;

                    stream.setAttribute("aspectratio", aspect_ratio);
                }
                else
                {
                    stream.setAttribute("aspectratio", "N/A");
                }

                stream.setAttribute("id", st->id);

                if (st->start_time != (int) AV_NOPTS_VALUE)
                {
                    int secs = st->start_time / AV_TIME_BASE;
                    int us = st->start_time % AV_TIME_BASE;
                    stream.setAttribute("start_time", QString("%1.%2")
                            .arg(secs).arg(av_rescale(us, 1000000, AV_TIME_BASE)));
                }
                else
                {
                    stream.setAttribute("start_time", 0);
                }

                streams.appendChild(stream);

                // TODO: probably should add a better way to choose which
                // video stream we use to calc the duration
                if (duration == 0)
                {
                    int64_t frameCount = 0;

                    switch (lenMethod)
                    {
                        case 0:
                        {
                            // use duration guess from avformat
                            if (inputFC->duration != (uint) AV_NOPTS_VALUE)
                            {
                                duration = (uint) (inputFC->duration / AV_TIME_BASE);
                                root.setAttribute("duration", duration);
                                LOG(VB_JOBQUEUE, LOG_INFO,
                                    QString("duration = %1") .arg(duration));
                                frameCount = (int64_t)(duration * fps);
                            }
                            else
                            {
                                root.setAttribute("duration", "N/A");
                            }
                            break;
                        }
                        case 1:
                        {
                            // calc duration of the file by counting the video frames
                            frameCount = getFrameCount(inputFC, i);
                            LOG(VB_JOBQUEUE, LOG_INFO,
                                QString("frames = %1").arg(frameCount));
                            duration = (uint)(frameCount / fps);
                            LOG(VB_JOBQUEUE, LOG_INFO,
                                QString("duration = %1").arg(duration));
                            root.setAttribute("duration", duration);
                            break;
                        }
                        case 2:
                        {
                            // use info from pos map in db
                            // (only useful if the file is a myth recording)
                            frameCount = getFrameCount(inFile, fps);
                            if (frameCount)
                            {
                                LOG(VB_JOBQUEUE, LOG_INFO,
                                    QString("frames = %1").arg(frameCount));
                                duration = (uint)(frameCount / fps);
                                LOG(VB_JOBQUEUE, LOG_INFO,
                                    QString("duration = %1").arg(duration));
                                root.setAttribute("duration", duration);
                            }
                            else if (inputFC->duration != (uint) AV_NOPTS_VALUE)
                            {
                                duration = (uint) (inputFC->duration / AV_TIME_BASE);
                                root.setAttribute("duration", duration);
                                LOG(VB_JOBQUEUE, LOG_INFO,
                                    QString("duration = %1").arg(duration));
                                frameCount = (int64_t)(duration * fps);
                            }
                            else
                            {
                                root.setAttribute("duration", "N/A");
                            }
                            break;
                        }
                        default:
                            root.setAttribute("duration", "N/A");
                            LOG(VB_JOBQUEUE, LOG_ERR,
                                QString("Unknown lenMethod (%1)")
                                    .arg(lenMethod));
                    }

                    // add duration after all cuts are removed
                    int64_t cutFrames = getCutFrames(inFile, frameCount);
                    LOG(VB_JOBQUEUE, LOG_INFO,
                        QString("cutframes = %1").arg(cutFrames));
                    int cutduration = (int)(cutFrames / fps);
                    LOG(VB_JOBQUEUE, LOG_INFO,
                        QString("cutduration = %1").arg(cutduration));
                    root.setAttribute("cutduration", duration - cutduration);
                }

                break;
            }

            case AVMEDIA_TYPE_AUDIO:
            {
                QStringList param = QString::fromStdString(buf).split(',', Qt::SkipEmptyParts);
                QString codec = param[0].remove("Audio:", Qt::CaseInsensitive).remove(QChar::Null);

                QDomElement stream = doc.createElement("audio");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);

                // change any streams identified as "liba52" to "AC3" which is what
                // the mythburn.py script expects to get.
                if (codec.trimmed().toLower() == "liba52")
                    stream.setAttribute("codec", "AC3");
                else
                    stream.setAttribute("codec", codec.trimmed());

                stream.setAttribute("channels", par->ch_layout.nb_channels);

                AVDictionaryEntry *metatag =
                    av_dict_get(st->metadata, "language", nullptr, 0);
                if (metatag)
                    stream.setAttribute("language", metatag->value);
                else
                    stream.setAttribute("language", "N/A");

                stream.setAttribute("id", st->id);

                stream.setAttribute("samplerate", par->sample_rate);
                stream.setAttribute("bitrate", (qlonglong)par->bit_rate);

                if (st->start_time != (int) AV_NOPTS_VALUE)
                {
                    int secs = st->start_time / AV_TIME_BASE;
                    int us = st->start_time % AV_TIME_BASE;
                    stream.setAttribute("start_time", QString("%1.%2")
                            .arg(secs).arg(av_rescale(us, 1000000, AV_TIME_BASE)));
                }
                else
                {
                    stream.setAttribute("start_time", 0);
                }

                streams.appendChild(stream);

                break;
            }

            case AVMEDIA_TYPE_SUBTITLE:
            {
                QStringList param = QString::fromStdString(buf).split(',', Qt::SkipEmptyParts);
                QString codec = param[0].remove("Subtitle:", Qt::CaseInsensitive).remove(QChar::Null);

                QDomElement stream = doc.createElement("subtitle");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);
                stream.setAttribute("codec", codec.trimmed());

                AVDictionaryEntry *metatag =
                    av_dict_get(st->metadata, "language", nullptr, 0);
                if (metatag)
                    stream.setAttribute("language", metatag->value);
                else
                    stream.setAttribute("language", "N/A");

                stream.setAttribute("id", st->id);

                streams.appendChild(stream);

                break;
            }

            case AVMEDIA_TYPE_DATA:
            {
                QDomElement stream = doc.createElement("data");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("codec", QString::fromStdString(buf).remove(QChar::Null));
                streams.appendChild(stream);

                break;
            }

            default:
                LOG(VB_JOBQUEUE, LOG_ERR,
                    QString("Skipping unsupported codec %1 on stream %2")
                        .arg(inputFC->streams[i]->codecpar->codec_type).arg(i));
                break;
        }
        codecmap.FreeCodecContext(st);
    }

    // finally save the xml to the file
    QFile f(outFile);
    if (!f.open(QIODevice::WriteOnly))
    {
        LOG(VB_JOBQUEUE, LOG_ERR,
            "Failed to open file for writing - " + outFile);
        return 1;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();

    return 0;
}

static int getDBParamters(const QString& outFile)
{
    DatabaseParams params = GetMythDB()->GetDatabaseParams();

    // save the db paramters to the file
    QFile f(outFile);
    if (!f.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythArchiveHelper: Failed to open file for writing - %1")
                .arg(outFile));
        return 1;
    }

    QTextStream t(&f);
    t << params.m_dbHostName << Qt::endl;
    t << params.m_dbUserName << Qt::endl;
    t << params.m_dbPassword << Qt::endl;
    t << params.m_dbName << Qt::endl;
    t << gCoreContext->GetHostName() << Qt::endl;
    t << GetInstallPrefix() << Qt::endl;
    f.close();

    return 0;
}

static int isRemote(const QString& filename)
{
    if (filename.startsWith("myth://"))
        return 3;

    // check if the file exists
    if (!QFile::exists(filename))
        return 0;

    if (!FileSystemInfo(QString(), filename).isLocal())
        return 2;

    return 1;
}

class MPLUGIN_PUBLIC MythArchiveHelperCommandLineParser : public MythCommandLineParser
{
  public:
    MythArchiveHelperCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
};

MythArchiveHelperCommandLineParser::MythArchiveHelperCommandLineParser() :
    MythCommandLineParser("mytharchivehelper")
{ MythArchiveHelperCommandLineParser::LoadArguments(); }

void MythArchiveHelperCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addLogging();

    add(QStringList{"-t", "--createthumbnail"},
            "createthumbnail", false,
            "Create one or more thumbnails\n"
            "Requires: --infile, --thumblist, --outfile\n"
            "Optional: --framecount", "");
    add("--infile", "infile", "",
            "Input file name\n"
            "Used with: --createthumbnail, --getfileinfo, --isremote, "
            "--sup2dast, --importarchive", "");
    add("--outfile", "outfile", "",
            "Output file name\n"
            "Used with: --createthumbnail, --getfileinfo, --getdbparameters, "
            "--nativearchive\n"
            "When used with --createthumbnail: eg 'thumb%1-%2.jpg'\n"
            "  %1 will be replaced with the no. of the thumb\n"
            "  %2 will be replaced with the frame no.", "");
    add("--thumblist", "thumblist", "",
            "Comma-separated list of required thumbs (in seconds)\n"
            "Used with: --createthumbnail","");
    add("--framecount", "framecount", 1,
            "Number of frames to grab (default 1)\n"
            "Used with: --createthumbnail", "");

    add(QStringList{"-i", "--getfileinfo"},
            "getfileinfo", false,
            "Write file info about infile to outfile\n"
            "Requires: --infile, --outfile, --method", "");
    add("--method", "method", 0,
            "Method of file duration calculation\n"
            "Used with: --getfileinfo\n"
            "  0 = use av_estimate_timings() (quick but not very accurate - "
            "default)\n"
            "  1 = read all frames (most accurate but slow)\n"
            "  2 = use position map in DB (quick, only works for MythTV "
            "recordings)", "");

    add(QStringList{"-p", "--getdbparameters"},
            "getdbparameters", false,
            "Write the mysql database parameters to outfile\n"
            "Requires: --outfile", "");

    add(QStringList{"-n", "--nativearchive"},
            "nativearchive", false,
            "Archive files to a native archive format\n"
            "Requires: --outfile", "");

    add(QStringList{"-f", "--importarchive"},
            "importarchive", false,
            "Import an archived file\n"
            "Requires: --infile, --chanid", "");
    add("--chanid", "chanid", -1,
            "Channel ID to use when inserting records in DB\n"
            "Used with: --importarchive", "");

    add(QStringList{"-r", "--isremote"},
            "isremote", false,
            "Check if infile is on a remote filesystem\n"
            "Requires: --infile\n"
            "Returns:   0 on error or file not found\n"
            "         - 1 file is on a local filesystem\n"
            "         - 2 file is on a remote filesystem", "");

    add(QStringList{"-b", "--burndvd"},
            "burndvd", false,
            "Burn a created DVD to a blank disc\n"
            "Optional: --mediatype, --erasedvdrw, --nativeformat", "");
    add("--mediatype", "mediatype", 0,
            "Type of media to burn\n"
            "Used with: --burndvd\n"
            "  0 = single layer DVD (default)\n"
            "  1 = dual layer DVD\n"
            "  2 = rewritable DVD", "");
    add("--erasedvdrw", "erasedvdrw", false,
            "Force an erase of DVD-R/W Media\n"
            "Used with: --burndvd (optional)", "");
    add("--nativeformat", "nativeformat", false,
            "Archive is a native archive format\n"
            "Used with: --burndvd (optional)", "");

    add(QStringList{"-s", "--sup2dast"},
            "sup2dast", false,
            "Convert projectX subtitles to DVD subtitles\n"
            "Requires: --infile, --ifofile, --delay", "");
    add("--ifofile", "ifofile", "",
            "Filename of ifo file\n"
            "Used with: --sup2dast", "");
    add("--delay", "delay", 0,
            "Delay in ms to add to subtitles (default 0)\n"
            "Used with: --sup2dast", "");
}



static int main_local(int argc, char **argv)
{
    MythArchiveHelperCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        MythArchiveHelperCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("mytharchivehelper");

    // by default we only output our messages
    QString mask("jobqueue");
    int retval = cmdline.ConfigureLogging(mask);
    if (retval != GENERIC_EXIT_OK)
        return retval;

    ///////////////////////////////////////////////////////////////////////
    // Don't listen to console input
    close(0);

    MythContext context {MYTH_BINARY_VERSION};
    if (!context.Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    int res = 0;
    bool bGrabThumbnail   = cmdline.toBool("createthumbnail");
    bool bGetDBParameters = cmdline.toBool("getdbparameters");
    bool bNativeArchive   = cmdline.toBool("nativearchive");
    bool bImportArchive   = cmdline.toBool("importarchive");
    bool bGetFileInfo     = cmdline.toBool("getfileinfo");
    bool bIsRemote        = cmdline.toBool("isremote");
    bool bDoBurn          = cmdline.toBool("burndvd");
    bool bEraseDVDRW      = cmdline.toBool("erasedvdrw");
    bool bNativeFormat    = cmdline.toBool("nativeformat");;
    bool bSup2Dast        = cmdline.toBool("sup2dast");

    QString thumbList     = cmdline.toString("thumblist");
    QString inFile        = cmdline.toString("infile");
    QString outFile       = cmdline.toString("outfile");
    QString ifoFile       = cmdline.toString("ifofile");

    int mediaType         = cmdline.toUInt("mediatype");
    int lenMethod         = cmdline.toUInt("method");
    int chanID            = cmdline.toInt("chanid");
    int frameCount        = cmdline.toUInt("framecount");
    int delay             = cmdline.toUInt("delay");

    //  Check command line arguments
    if (bGrabThumbnail)
    {
        if (inFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --infile in -t/--grabthumbnail "
                                     "option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        if (thumbList.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --thumblist in -t/--grabthumbnail"
                                     " option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        if (outFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --outfile in -t/--grabthumbnail "
                                     "option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (bGetDBParameters)
    {
        if (outFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing argument to -p/--getdbparameters "
                                     "option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (bIsRemote)
    {
        if (inFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Missing argument to -r/--isremote option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (bDoBurn)
    {
        if (mediaType < 0 || mediaType > 2)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Invalid mediatype given: %1")
                    .arg(mediaType));
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (bNativeArchive)
    {
        if (outFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing argument to -n/--nativearchive "
                                     "option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (bImportArchive)
    {
        if (inFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --infile argument to "
                                     "-f/--importarchive option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (bGetFileInfo)
    {
        if (inFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --infile in -i/--getfileinfo "
                                     "option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        if (outFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --outfile in -i/--getfileinfo "
                                     "option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (bSup2Dast)
    {
        if (inFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Missing --infile in -s/--sup2dast option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        if (ifoFile.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Missing --ifofile in -s/--sup2dast option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (bGrabThumbnail)
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
    {
        cmdline.PrintHelp();
    }

    exit(res);
}

int main(int argc, char **argv)
{
    int result = main_local(argc, argv);
    logStop();
    return result;
}
