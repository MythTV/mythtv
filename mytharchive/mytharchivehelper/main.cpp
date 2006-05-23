#include <iostream>
#include <stdint.h>
using namespace std;


// Qt headers
#include <qapplication.h>
#include <qfile.h>
#include <qdir.h>

// MythTV headers
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "mythcontext.h"
#include "programinfo.h"
#include "util.h"
#include "mythcontext.h"
#include "exitcodes.h"
#include "mythdbcon.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

// mytharchive headers
#include "../mytharchive/mytharchivewizard.h"

class NativeArchive
{
  public:
      NativeArchive(void);
      ~NativeArchive(void);

      bool doNativeArchive(QString &jobFile);
      bool copyFile(const QString &source, const QString &destination);
  private:
      QString formatSize(long long size);
};

NativeArchive::NativeArchive(void)
{
    // create the lock file so the UI knows we're running
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");
    system("echo Lock > " + tempDir + "/logs/mythburn.lck");
}

NativeArchive::~NativeArchive(void)
{
    // remove lock file
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");
    if (QFile::exists(tempDir + "/logs/mythburn.lck"))
        QFile::remove(tempDir + "/logs/mythburn.lck");
}

QString NativeArchive::formatSize(long long sizeKB)
{
    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QString("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:2);
    }
    else if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QString("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:2);
    }
    else if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QString("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:2);
    }
    // Kilobytes
    return QString("%1 KB").arg(sizeKB);
}

bool NativeArchive::copyFile(const QString &source, const QString &destination)
{
    QFile srcFile(source), destFile(destination);

    VERBOSE(VB_JOBQUEUE, QString("copying from %1 to %2")
            .arg(source).arg(destination));

    if (!srcFile.open(IO_ReadOnly))
    { 
        VERBOSE(VB_JOBQUEUE, "Unable to open source file");
        return false;
    }

    if (!destFile.open(IO_WriteOnly))
    { 
        VERBOSE(VB_JOBQUEUE, "Unable to open destination file");
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
        VERBOSE(VB_JOBQUEUE, QString("Not enough free space available on destination filesystem. %1 %2")
                .arg(freeSpace).arg(totalSize));
        destFile.close();
        srcFile.close();
        return false;
    }

    while ((srcLen = srcFile.readBlock(buffer, sizeof(buffer))) > 0)
    {
        destLen = destFile.writeBlock(buffer, srcLen);

        if (destLen == -1 || srcLen != destLen)
        {
            VERBOSE(VB_JOBQUEUE, "Error while trying to write to destination file.");
            srcFile.close();
            destFile.close();
            return false;
        }
        wroteSize += destLen;
        percent = (int) ((100.0 * wroteSize) / totalSize);
        if (percent % 5 == 0  && percent != lastPercent)
        {
            VERBOSE(VB_JOBQUEUE, QString("%1 out of %2 (%3%) completed")
                    .arg(formatSize(wroteSize/1024)).arg(formatSize(totalSize/1024)).arg(percent));
            lastPercent = percent;
        }
    }

    srcFile.close();
    destFile.close();
    if (srcFile.size() != destFile.size())
    {
        VERBOSE(VB_JOBQUEUE, "Copy not completed OK - Source and destination file sizes do not match!!");
        VERBOSE(VB_JOBQUEUE, QString("Source is %1 bytes, Destination is %2 bytes")
                .arg(srcFile.size()).arg(destFile.size()));
        return false;
    }
    else
        VERBOSE(VB_JOBQUEUE, "Copy completed OK");

    return true;
}

bool extractDetailsFromFilename(const QString &inFile,
                                QString &chanID, QString &startTime)
{
    VERBOSE(VB_JOBQUEUE, "Extracting details from: " + inFile);
    QString filename = inFile;

    chanID = "";
    startTime = "";
    bool res = false;

    // remove any path
    int pathStart = filename.findRev('/');
    if (pathStart > 0)
        filename = filename.mid(pathStart + 1);

    int sep = filename.find('_');
    if (sep != -1)
    {
        chanID = filename.left(sep);
        startTime = filename.mid(sep + 1, filename.length() - sep - 5);
        if (startTime.length() == 14)
        {
            QString year, month, day, hour, minute, second;
            year = startTime.mid(0, 4);
            month = startTime.mid(4, 2);
            day = startTime.mid(6, 2);
            hour = startTime.mid(8, 2);
            minute = startTime.mid(10, 2);
            second = startTime.mid(12, 2);
            startTime = QString("%1-%2-%3T%4:%5:%6").arg(year).arg(month).arg(day)
                    .arg(hour).arg(minute).arg(second);
            res = true;
        }
    }
    VERBOSE(VB_JOBQUEUE, QString("chanid: %1 starttime:%2 ").arg(chanID).arg(startTime));
    return res;
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

void createISOImage(QString &sourceDirectory)
{
    VERBOSE(VB_JOBQUEUE, "Creating ISO image");

    QString tempDirectory = gContext->GetSetting("MythArchiveTempDir", "");
    if (!tempDirectory.endsWith("/"))
        tempDirectory += "/";

    tempDirectory += "work/";

    QString mkisofs = gContext->GetSetting("MythArchiveMkisofsCmd", "mkisofs");
    QString command = mkisofs + " -R -J -V 'MythTV Archive' -o ";
    command += tempDirectory + "mythburn.iso " + sourceDirectory;

    int res = system(command);
    cout << "createISOImage result: " << res << endl;
    VERBOSE(VB_JOBQUEUE, "Finished creating ISO image");
}

void burnISOImage(int mediaType, bool bEraseDVDRW)
{
    QString dvdDrive = gContext->GetSetting(" MythArchiveDVDLocation", "/dev/dvd");
    VERBOSE(VB_JOBQUEUE, "Burning ISO image to " + dvdDrive);

    QString tempDirectory = gContext->GetSetting("MythArchiveTempDir", "");
    if (!tempDirectory.endsWith("/"))
        tempDirectory += "/";

    tempDirectory += "work/";

    QString growisofs = gContext->GetSetting("MythArchiveGrowisofsCmd", "growisofs");
    QString command;
    if (mediaType == AD_DVD_RW && bEraseDVDRW == true)
    {
        command = growisofs + " -use-the-force-luke -Z " + dvdDrive;
        command += " -V 'MythTV Archive' -R -J " + tempDirectory;
    }
    else
    {
        command = growisofs + " -Z " + dvdDrive; 
        command += " -V 'MythTV Archive' -R -J " + tempDirectory;
    }

    int res = system(command);
    cout << "burnISOImage result: " << res << endl;
    VERBOSE(VB_JOBQUEUE, "Finished burning ISO image");
}

bool NativeArchive::doNativeArchive(QString &jobFile)
{
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");

    QDomDocument doc("archivejob");
    QFile file(jobFile);
    if (!file.open(IO_ReadOnly))
    {
        VERBOSE(VB_ALL, "Could not open job file: " + jobFile); 
        return false;
    }

    if (!doc.setContent(&file))
    {
        VERBOSE(VB_ALL, "Could not load job file: " + jobFile); 
        file.close();
        return false;
    }

    file.close();

    // get options from job file
    bool bCreateISO = false;
    bool bEraseDVDRW = false;
    bool bDoBurn = false;
    QString saveDirectory;
    int mediaType;

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
        VERBOSE(VB_ALL, QString("Found %1 options nodes - should be 1").arg(nodeList.count()));
        return false;
    }
    VERBOSE(VB_JOBQUEUE, QString("Options - createiso: %1, doburn: %2, mediatype: %3, "
            "erasedvdrw: %4").arg(bCreateISO).arg(bDoBurn).arg(mediaType).arg(bEraseDVDRW));
    VERBOSE(VB_JOBQUEUE, QString("savedirectory: %1").arg(saveDirectory));

    // figure out where to save files
    if (mediaType != AD_FILE)
    {
        saveDirectory = tempDir;
        if (!saveDirectory.endsWith("/"))
            saveDirectory += "/";

        saveDirectory += "work/";
        system("rm -fr " + saveDirectory + "*");
    }

    VERBOSE(VB_JOBQUEUE, QString("Saving files to : %1").arg(saveDirectory));

    // get list of file nodes from the job file
    nodeList = doc.elementsByTagName("file");
    if (nodeList.count() < 1)
    {
        VERBOSE(VB_ALL, "Cannot find any file nodes?");
        return false;
    }

    QString dbVersion = gContext->GetSetting("DBSchemaVer", "");

    // loop though file nodes and archive each file
    QDomNode node;
    QDomElement elem;
    QString chanID, startTime, type = "", title = "", filename = "";
    bool doDelete = false;

    for (uint x = 0; x < nodeList.count(); x++)
    {
        node = nodeList.item(x);
        elem = node.toElement();
        if (!elem.isNull())
        {
            type = elem.attribute("type");
            if (type.lower() != "recording")
            {
                VERBOSE(VB_ALL, QString("Don't know how to archive items of type '%1'").arg(type.lower()));
                continue;
            }
            title = elem.attribute("title");
            filename = elem.attribute("filename");
            doDelete = (elem.attribute("delete", "0") = "0");
            VERBOSE(VB_JOBQUEUE, QString("Archiving %1 (%2), do delete: %3")
                    .arg(title).arg(filename).arg(doDelete));
        }

        if (title == "" || filename == "")
        {
            VERBOSE(VB_ALL, "Bad title or filename");
            continue;
        }

        if (!extractDetailsFromFilename(filename, chanID, startTime))
        {
            VERBOSE(VB_ALL, QString("Failed to extract chanID and startTime from '%1'").arg(filename));
            continue;
        }

        // create the directory to hold this items files
        QDir dir(saveDirectory + title);
        if (!dir.exists())
            dir.mkdir(saveDirectory + title);

        VERBOSE(VB_JOBQUEUE, "Creating xml file for " + title);
        QDomDocument doc("MYTHARCHIVEITEM");

        QDomElement root = doc.createElement("item");
        doc.appendChild(root);
        root.setAttribute("type", type.lower() );
        root.setAttribute("databaseversion", dbVersion);

        QDomElement recorded = doc.createElement("recorded");
        root.appendChild(recorded);

        // get details from recorded
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid, starttime, endtime, title, subtitle, description, "
                "category, hostname, bookmark, editing, cutlist, autoexpire, commflagged, "
                "recgroup, recordid, seriesid, programid, lastmodified, filesize, stars, "
                "previouslyshown, originalairdate, preserve, findid, deletepending, "
                "transcoder, timestretch, recpriority, basename, progstart, progend, "
                "playgroup, profile, duplicate, transcoded FROM recorded "
                "WHERE chanid = :CHANID and starttime = :STARTTIME;");
        query.bindValue(":CHANID", chanID);
        query.bindValue(":STARTTIME", startTime);

        query.exec();
        if (query.isActive() && query.numRowsAffected())
        {
            query.first();
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

        query.exec();
        if (query.isActive() && query.numRowsAffected())
        {
            query.first();
            QDomElement channel = doc.createElement("channel");
            channel.setAttribute("chanid", query.value(0).toString());
            channel.setAttribute("channum", query.value(1).toString());
            channel.setAttribute("callsign", query.value(2).toString());
            channel.setAttribute("name", query.value(3).toString());
            root.appendChild(channel);
            VERBOSE(VB_JOBQUEUE, "Created channel element for " + title);
        }

        // add any credits
        query.prepare("SELECT credits.person, role, people.name "
                "FROM recordedcredits AS credits "
                "LEFT JOIN people ON credits.person = people.person "
                "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
        query.bindValue(":CHANID", chanID);
        query.bindValue(":STARTTIME", startTime);

        query.exec();
        if (query.isActive() && query.numRowsAffected())
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

        query.exec();
        if (query.isActive() && query.numRowsAffected())
        {
            query.first();
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
        query.exec();
        if (query.isActive() && query.numRowsAffected())
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

        // finally save the xml to the file
        QString xmlFile = saveDirectory + title + "/" + filename + ".xml";
        QFile f(xmlFile);
        if (!f.open(IO_WriteOnly))
        {
            VERBOSE(VB_ALL, "MythNativeWizard: Failed to open file for writing - " + xmlFile);
            return false;
        }

        QTextStream t(&f);
        t << doc.toString(4);
        f.close();

        // copy the file
        QString prefix = gContext->GetSettingOnHost("RecordFilePrefix", gContext->GetHostName());
        bool res = copyFile(prefix + "/" + filename, saveDirectory + title + "/" + filename);
        if (!res)
        {
            return false;
        }
    }

    // create an iso image if needed
    if (mediaType == AD_FILE && bCreateISO)
        createISOImage(saveDirectory);

    // burn the iso if needed
    if (mediaType != AD_FILE && bDoBurn)
        burnISOImage(mediaType, bEraseDVDRW);

    // make sure the files we created are read/writable by all 
    system("chmod -R a+rw-x+X " + saveDirectory);

    VERBOSE(VB_JOBQUEUE, "Finished processing job file");

    return true;
}

bool doNativeArchive(QString &jobFile)
{
    NativeArchive ra;
    return ra.doNativeArchive(jobFile);
}

bool grabThumbnail(QString inFile, QString thumbList, QString outFile)
{
    // try to extract chanID and starttime from inFile
    QString chanID = "";
    QString startTime = "";
    QStringList list = QStringList::split(",", thumbList);
    RingBuffer *rbuf = NULL;
    NuppelVideoPlayer *nvp = NULL;

    ProgramInfo *programInfo = getProgramInfoForFile(inFile);

    VERBOSE(VB_IMPORTANT, "programInfo->pathname: " + programInfo->pathname);

    programInfo->MarkAsInUse(true, "preview_generator");

    bool bCreatedMarkupMap = false;
    bool bNeedMarkupMap = true;

    // if we are only grabbing the first frame we don't neet a position map
    if (list.count() == 1 && list[0].stripWhiteSpace().toInt() == 0)
        bNeedMarkupMap = false;

    // if this is not a myth recording we may have to build a position map
    if (bNeedMarkupMap && programInfo->isVideo && !programInfo->CheckMarkupFlag(MARK_GOP_START))
    {
        rbuf = new RingBuffer(programInfo->pathname, false, false, 0);
        if (!rbuf->IsOpen())
        {
            VERBOSE(VB_IMPORTANT, "GetScreenGrab: could not open file: " +
                    QString("'%1'").arg(programInfo->pathname));
            delete rbuf;
            return false;
        }

        nvp = new NuppelVideoPlayer("GetScreenGrab", programInfo);
        nvp->SetRingBuffer(rbuf);

        VERBOSE(VB_IMPORTANT, QString("Creating markup map for: %1").arg(programInfo->pathname));
        nvp->RebuildSeekTable(true, NULL, NULL);

        delete nvp;
        delete rbuf;

        bCreatedMarkupMap = true;
    }

    for (uint x = 0; x < list.count(); x++)
    {
        QString filename = outFile;

        if (outFile.contains("%1"))
            filename = outFile.arg(x + 1);

        float video_aspect = 0;
        int len, width, height, sz;
        int pos = list[x].stripWhiteSpace().toInt();
        len = width = height = sz = 0;

        rbuf = new RingBuffer(programInfo->pathname, false, false, 0);
        if (!rbuf->IsOpen())
        {
            VERBOSE(VB_IMPORTANT, "GetScreenGrab: could not open file: " +
                    QString("'%1'").arg(programInfo->pathname));
            delete rbuf;
            return false;
        }

        nvp = new NuppelVideoPlayer("GetScreenGrab", programInfo);
        nvp->SetRingBuffer(rbuf);

        VERBOSE(VB_IMPORTANT, QString("Grabbing thumb: %1 from: %2").arg(filename).arg(pos));
        unsigned char *data = (unsigned char*)
                nvp->GetScreenGrab(pos, len, width, height, video_aspect);

        if (!data || !width || !height)
        {
            VERBOSE(VB_IMPORTANT, "Failed to create thumb: " + filename);
            continue;
        }

        QImage img((unsigned char*) data,
                              width, height, 32, NULL, 65536 * 65536,
                              QImage::LittleEndian);

        if (!img.save(filename.ascii(), "JPEG"))
        {
            VERBOSE(VB_IMPORTANT, "Failed to save thumb: " + filename);
        }

        if (data)
            delete[] data;

        delete nvp;
        delete rbuf;
    }

    programInfo->MarkAsInUse(false);

    // delete position map if we created one
    if (programInfo->isVideo && bCreatedMarkupMap)
    {
        if (!MSqlQuery::testDBConnection())
        {
            VERBOSE(VB_IMPORTANT, "GetScreenGrab: could not connect to DB.");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Deleting markup map for: " + programInfo->pathname);

            MSqlQuery query(MSqlQuery::InitCon());

            query.prepare("DELETE FROM filemarkup WHERE filename = :FILENAME;");
            query.bindValue(":FILENAME", programInfo->pathname);
            query.exec();
        }
    }

    delete programInfo;

    return true;
}

int getFrameCount(AVFormatContext *inputFC, int vid_id)
{
    AVPacket pkt;
    int count = 0;

    VERBOSE(VB_JOBQUEUE, "Calculating frame count");

    av_init_packet(&pkt);

    while (av_read_frame(inputFC, &pkt) >= 0)
    {
        if (pkt.stream_index == vid_id)
        {
            count++;
        }
    }

    return count;
}

bool getFileInfo(QString inFile, QString outFile)
{
    const char *type = NULL;
    int ret;

    av_register_all();

    AVFormatContext *inputFC = NULL;
    AVInputFormat *fmt = NULL;

    if (type)
        fmt = av_find_input_format(type);

    // Open recording
    VERBOSE(VB_JOBQUEUE, QString("Opening %1").arg(inFile));

    ret = av_open_input_file(&inputFC, inFile.ascii(), fmt, 0, NULL);

    if (ret != 0)
    {
        VERBOSE(VB_JOBQUEUE,
            QString("Couldn't open input file, error #%1").arg(ret));
        return 0;
    }

    // Getting stream information
    ret = av_find_stream_info(inputFC);

    if (ret < 0)
    {
        VERBOSE(VB_JOBQUEUE,
            QString("Couldn't get stream info, error #%1").arg(ret));
        av_close_input_file(inputFC);
        inputFC = NULL;
        return 0;
    }

    VERBOSE(VB_JOBQUEUE, "av_estimate_timings - Start");
    av_estimate_timings(inputFC);
    VERBOSE(VB_JOBQUEUE, "av_estimate_timings - Stop");

    // Dump stream information
    dump_format(inputFC, 0, inFile.ascii(), 0);

    QDomDocument doc("FILEINFO");

    QDomElement root = doc.createElement("file");
    doc.appendChild(root);
    root.setAttribute("type", inputFC->iformat->name);
    root.setAttribute("filename", inFile);

    QDomElement streams = doc.createElement("streams");

    root.appendChild(streams);
    streams.setAttribute("count", inputFC->nb_streams);
    int ffmpegIndex = 0;

    for (int i = 0; i < inputFC->nb_streams; i++)
    {
        AVStream *st = inputFC->streams[i];
        char buf[256];

        avcodec_string(buf, sizeof(buf), st->codec, false);

        switch (inputFC->streams[i]->codec->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                QStringList param = QStringList::split(',', QString(buf));
                QString codec = param[0].remove("Video:", false);
                QDomElement stream = doc.createElement("video");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);
                stream.setAttribute("codec", codec.stripWhiteSpace());
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
                    float sample_aspect_ratio = av_q2d(st->codec->sample_aspect_ratio);
                    float aspect_ratio = ((float)st->codec->width / st->codec->height) * sample_aspect_ratio;
                    stream.setAttribute("aspectratio", aspect_ratio);
                }
                else
                    stream.setAttribute("aspectratio", "N/A");

                stream.setAttribute("id", st->id);

                streams.appendChild(stream);

                // calc duration of the file by counting the video frames
                int duration = getFrameCount(inputFC, i);
                VERBOSE(VB_JOBQUEUE, QString("frames = %1").arg(duration));
                duration = (int)(duration / fps);
                VERBOSE(VB_JOBQUEUE, QString("duration = %1").arg(duration));
                root.setAttribute("duration", duration);

                break;
            }

            case CODEC_TYPE_AUDIO:
            {
                QStringList param = QStringList::split(',', QString(buf));
                QString codec = param[0].remove("Audio:", false);

                QDomElement stream = doc.createElement("audio");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);
                stream.setAttribute("codec", codec.stripWhiteSpace());
                stream.setAttribute("channels", st->codec->channels);
                if (strlen(st->language) > 0)
                    stream.setAttribute("language", st->language);
                else
                    stream.setAttribute("language", "N/A");

                stream.setAttribute("id", st->id);

                stream.setAttribute("samplerate", st->codec->sample_rate);
                stream.setAttribute("bitrate", st->codec->bit_rate);

                streams.appendChild(stream);

                break;
            }

            case CODEC_TYPE_SUBTITLE:
            {
                QStringList param = QStringList::split(',', QString(buf));
                QString codec = param[0].remove("Subtitle:", false);

                QDomElement stream = doc.createElement("subtitle");
                stream.setAttribute("streamindex", i);
                stream.setAttribute("ffmpegindex", ffmpegIndex++);
                stream.setAttribute("codec", codec.stripWhiteSpace());
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
    if (!f.open(IO_WriteOnly))
    {
        VERBOSE(VB_JOBQUEUE, "Failed to open file for writing - " + outFile);
        return false;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();

    // Close input file
    av_close_input_file(inputFC);
    inputFC = NULL;

    return true;
}

bool getDBParamters(QString outFile)
{
    DatabaseParams params = gContext->GetDatabaseParams();

    // save the db paramters to the file
    QFile f(outFile);
    if (!f.open(IO_WriteOnly))
    {
        cout << "MythArchiveHelper: Failed to open file for writing - "
                << outFile << endl;
        return false;
    }

    QTextStream t(&f);
    t << params.dbHostName << endl;
    t << params.dbUserName << endl;
    t << params.dbPassword << endl;
    t << params.dbName << endl;
    t << gContext->GetHostName() << endl;
    f.close();

    return true;
}

void showUsage()
{
    cout << "Usage of mytharchivehelper\n"; 
    cout << "-t/--createthumbnail infile thumblist outfile\n";
    cout << "                          (create one or more thumbnails\n";
    cout << "       infile    - filename of recording to grab thumbs from\n";
    cout << "       thumblist - comma seperated list of required thumbs in seconds\n";
    cout << "       outfile   - name of file to save thumbs to eg 'thumb-%1.jpg'\n";
    cout << "                   %1 will be replaced with the no. of the thumb\n";
    cout << "-i/--getfileinfo infile outfile\n";
    cout << "                          (write some file information to outfile for infile)\n";
    cout << "-p/--getdbparameters outfile\n";
    cout << "                          (write the mysql database parameters to outfile)\n";
    cout << "-n/--nativearchive        (archive files to a native archive format)\n";
    cout << "       jobfile    - filename of archive job file\n";
    cout << "-l/--log logfile          (name of log file to send messages)\n";
    cout << "-v/--verbose debug-level  (Use '-v help' for level info)\n";
    cout << "-h/--help                 (shows this usage)\n";
}

int main(int argc, char **argv)
{
    // by default we only output our messages
    print_verbose_messages = VB_JOBQUEUE;

    QApplication a(argc, argv, false);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        cout << "mytharchivehelper: Could not initialize myth context. "
                "Exiting." << endl;;
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    int res = 0;
    bool bShowUsage = false;
    bool bGrabThumbnail = false;
    bool bGetDBParameters = false;
    bool bNativeArchive = false;
    bool bGetFileInfo = false;
    QString thumbList;
    QString inFile;
    QString outFile;
    QString logFile;

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
        res = grabThumbnail(inFile, thumbList, outFile);
    else if (bGetDBParameters)
        res = getDBParamters(outFile);
    else if (bNativeArchive)
        res = doNativeArchive(outFile);
    else if (bGetFileInfo)
        res = getFileInfo(inFile, outFile);
    else 
        showUsage();

    return 0;
}


