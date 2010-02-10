#include <cstdlib> // for llabs

#include "mythconfig.h"
#if CONFIG_DARWIN || defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#elif __linux__
#include <sys/vfs.h>
#endif

#include <QStringList>
#include <QMutex>
#include <QDir>
#include <QMap>

#include "backendutil.h"
#include "encoderlink.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythdbcon.h"
#include "util.h"
#include "decodeencode.h"
#include "compat.h"
#include "storagegroup.h"

static QMap<QString, int> fsID_cache;
static QMutex cache_lock;

/// gets stable fsIDs based of the dirID
static int GetfsID(vector<FileSystemInfo>::iterator fsInfo)
{
    QString fskey = fsInfo->hostname + ":" + fsInfo->directory;
    QMutexLocker lock(&cache_lock);
    if (!fsID_cache.contains(fskey))
        fsID_cache[fskey] = fsID_cache.count();

    return fsID_cache[fskey];
}

static size_t GetCurrentMaxBitrate(QMap<int, EncoderLink *> *encoderList)
{
    size_t totalKBperMin = 0;

    QMap<int, EncoderLink*>::iterator it = encoderList->begin();
    for (; it != encoderList->end(); ++it)
    {
        EncoderLink *enc = *it;

        if (!enc->IsConnected() || !enc->IsBusy())
            continue;

        long long maxBitrate = enc->GetMaxBitrate();
        if (maxBitrate<=0)
            maxBitrate = 19500000LL;
        long long thisKBperMin = (((size_t)maxBitrate)*((size_t)15))>>11;
        totalKBperMin += thisKBperMin;
        VERBOSE(VB_FILE, QString("Cardid %1: max bitrate %2 KB/min")
                .arg(enc->GetCardID()).arg(thisKBperMin));
    }

    VERBOSE(VB_FILE, QString("Maximal bitrate of busy encoders is %1 KB/min")
            .arg(totalKBperMin));

    return totalKBperMin;
}

void BackendQueryDiskSpace(QStringList &strlist,
                           QMap <int, EncoderLink *> *encoderList,
                           bool consolidated, bool allHosts)
{
    QString allHostList = gContext->GetHostName();
    long long totalKB = -1, usedKB = -1;
    QMap <QString, bool>foundDirs;
    QString driveKey;
    QString localStr = "1";
    struct statfs statbuf;
    QStringList groups(StorageGroup::kSpecialGroups);
    groups.removeAll("LiveTV");
    QString specialGroups = groups.join("', '");
    QString sql = QString("SELECT MIN(id),dirname "
                            "FROM storagegroup "
                           "WHERE hostname = :HOSTNAME "
                             "AND groupname NOT IN ( '%1' ) "
                           "GROUP BY dirname;").arg(specialGroups);
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(sql);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (query.exec() && query.isActive())
    {
        // If we don't have any dirs of our own, fallback to list of Default
        // dirs since that is what StorageGroup::Init() does.
        if (!query.size())
        {
            query.prepare("SELECT MIN(id),dirname "
                          "FROM storagegroup "
                          "WHERE groupname = :GROUP "
                          "GROUP BY dirname;");
            query.bindValue(":GROUP", "Default");
            if (!query.exec())
                MythDB::DBError("BackendQueryDiskSpace", query);
        }

        QDir checkDir("");
        QString dirID;
        QString currentDir;
        int bSize;
        while (query.next())
        {
            dirID = query.value(0).toString();
            currentDir = query.value(1).toString();
            if (currentDir.right(1) == "/")
                currentDir.remove(currentDir.length() - 1, 1);

            checkDir.setPath(currentDir);
            if (!foundDirs.contains(currentDir))
            {
                if (checkDir.exists())
                {
                    QByteArray cdir = currentDir.toAscii();
                    getDiskSpace(cdir.constData(), totalKB, usedKB);
                    bzero(&statbuf, sizeof(statbuf));
                    localStr = "1"; // Assume local
                    bSize = 0;

                    if (!statfs(currentDir.toLocal8Bit().constData(), &statbuf))
                    {
#if CONFIG_DARWIN
                        char *fstypename = statbuf.f_fstypename;
                        if ((!strcmp(fstypename, "nfs")) ||   // NFS|FTP
                            (!strcmp(fstypename, "afpfs")) || // ApplShr
                            (!strcmp(fstypename, "smbfs")))   // SMB
                            localStr = "0";
#elif __linux__
                        long fstype = statbuf.f_type;
                        if ((fstype == 0x6969) ||             // NFS
                            (fstype == 0x517B) ||             // SMB
                            (fstype == (long)0xFF534D42))     // CIFS
                            localStr = "0";
#endif
                        bSize = statbuf.f_bsize;
                    }

                    strlist << gContext->GetHostName();
                    strlist << currentDir;
                    strlist << localStr;
                    strlist << "-1"; // Ignore fsID
                    strlist << dirID;
                    strlist << QString::number(bSize);
                    encodeLongLong(strlist, totalKB);
                    encodeLongLong(strlist, usedKB);

                    foundDirs[currentDir] = true;
                }
                else
                    foundDirs[currentDir] = false;
            }
        }
    }

    if (allHosts)
    {
        QMap <QString, bool> backendsCounted;
        QString encoderHost;
        QMap<int, EncoderLink *>::Iterator eit = encoderList->begin();
        while (eit != encoderList->end())
        {
            encoderHost = (*eit)->GetHostName();
            if ((*eit)->IsConnected() &&
                !(*eit)->IsLocal() &&
                !backendsCounted.contains(encoderHost))
            {
                backendsCounted[encoderHost] = true;

                (*eit)->GetDiskSpace(strlist);

                allHostList += "," + encoderHost;
            }
            ++eit;
        }
    }

    if (!consolidated)
        return;

    FileSystemInfo fsInfo;
    vector<FileSystemInfo> fsInfos;

    QStringList::const_iterator it = strlist.begin();
    while (it != strlist.end())
    {
        fsInfo.hostname = *(it++);
        fsInfo.directory = *(it++);
        fsInfo.isLocal = (*(it++)).toInt();
        fsInfo.fsID = (*(it++)).toInt();
        fsInfo.dirID = (*(it++)).toInt();
        fsInfo.blocksize = (*(it++)).toInt();
        fsInfo.totalSpaceKB = decodeLongLong(strlist, it);
        fsInfo.usedSpaceKB = decodeLongLong(strlist, it);
        fsInfo.freeSpaceKB = fsInfo.totalSpaceKB - fsInfo.usedSpaceKB;
        fsInfos.push_back(fsInfo);
    }
    strlist.clear();

    // Consolidate hosts sharing storage
    size_t maxWriteFiveSec = GetCurrentMaxBitrate(encoderList)/12 /*5 seconds*/;
    maxWriteFiveSec = max((size_t)2048, maxWriteFiveSec); // safety for NFS mounted dirs
    vector<FileSystemInfo>::iterator it1, it2;
    int bSize = 32;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); it1++)
    {
        if (it1->fsID == -1)
        {
            it1->fsID = GetfsID(it1);
            it1->directory =
                it1->hostname.section(".", 0, 0) + ":" + it1->directory;
        }

        it2 = it1;
        for (it2++; it2 != fsInfos.end(); it2++)
        {
            // our fuzzy comparison uses the maximum of the two block sizes
            // or 32, whichever is greater
            bSize = max(32, max(it1->blocksize, it2->blocksize) / 1024);
            if (it2->fsID == -1 &&
                (absLongLong(it1->totalSpaceKB - it2->totalSpaceKB) <= bSize) &&
                ((size_t)absLongLong(it1->usedSpaceKB - it2->usedSpaceKB)
                 <= maxWriteFiveSec))
            {
                if (!it1->hostname.contains(it2->hostname))
                    it1->hostname = it1->hostname + "," + it2->hostname;
                it1->directory = it1->directory + "," +
                    it2->hostname.section(".", 0, 0) + ":" + it2->directory;
                fsInfos.erase(it2);
                it2 = it1;
            }
        }
    }

    // Passed the cleaned list back
    totalKB = 0;
    usedKB  = 0;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); it1++)
    {
        strlist << it1->hostname;
        strlist << it1->directory;
        strlist << QString::number(it1->isLocal);
        strlist << QString::number(it1->fsID);
        strlist << QString::number(it1->dirID);
        strlist << QString::number(it1->blocksize);
        encodeLongLong(strlist, it1->totalSpaceKB);
        encodeLongLong(strlist, it1->usedSpaceKB);

        totalKB += it1->totalSpaceKB;
        usedKB  += it1->usedSpaceKB;
    }

    if (allHosts)
    {
        strlist << allHostList;
        strlist << "TotalDiskSpace";
        strlist << "0";
        strlist << "-2";
        strlist << "-2";
        strlist << "0";
        encodeLongLong(strlist, totalKB);
        encodeLongLong(strlist, usedKB);
    }
}

void GetFilesystemInfos(QMap<int, EncoderLink*> *tvList,
                        vector <FileSystemInfo> &fsInfos)
{
    QStringList strlist;
    FileSystemInfo fsInfo;

    fsInfos.clear();

    BackendQueryDiskSpace(strlist, tvList, false, true);

    QStringList::const_iterator it = strlist.begin();
    while (it != strlist.end())
    {
        fsInfo.hostname = *(it++);
        fsInfo.directory = *(it++);
        fsInfo.isLocal = (*(it++)).toInt();
        fsInfo.fsID = -1;
        it++;
        fsInfo.dirID = (*(it++)).toInt();
        fsInfo.blocksize = (*(it++)).toInt();
        fsInfo.totalSpaceKB = decodeLongLong(strlist, it);
        fsInfo.usedSpaceKB = decodeLongLong(strlist, it);
        fsInfo.freeSpaceKB = fsInfo.totalSpaceKB - fsInfo.usedSpaceKB;
        fsInfo.weight = 0;
        fsInfos.push_back(fsInfo);
    }

    VERBOSE(VB_SCHEDULE+VB_FILE+VB_EXTRA, "Determining unique filesystems");
    size_t maxWriteFiveSec = GetCurrentMaxBitrate(tvList)/12  /*5 seconds*/;
    maxWriteFiveSec = max((size_t)2048, maxWriteFiveSec); // safety for NFS mounted dirs
    vector<FileSystemInfo>::iterator it1, it2;
    int bSize = 32;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); it1++)
    {
        if (it1->fsID == -1)
            it1->fsID = GetfsID(it1);
        else
            continue;

        VERBOSE(VB_SCHEDULE+VB_FILE+VB_EXTRA,
            QString("%1:%2 (fsID %3, dirID %4) using %5 out of %6 KB, "
                    "looking for matches")
                    .arg(it1->hostname).arg(it1->directory)
                    .arg(it1->fsID).arg(it1->dirID)
                    .arg(it1->usedSpaceKB).arg(it1->totalSpaceKB));
        it2 = it1;
        for (it2++; it2 != fsInfos.end(); it2++)
        {
            // our fuzzy comparison uses the maximum of the two block sizes
            // or 32, whichever is greater
            bSize = max(32, max(it1->blocksize, it2->blocksize) / 1024);
            VERBOSE(VB_SCHEDULE+VB_FILE+VB_EXTRA,
                QString("    Checking %1:%2 (dirID %3) using %4 of %5 KB")
                        .arg(it2->hostname).arg(it2->directory).arg(it2->dirID)
                        .arg(it2->usedSpaceKB).arg(it2->totalSpaceKB));
            VERBOSE(VB_SCHEDULE+VB_FILE+VB_EXTRA,
                QString("        Total KB Diff: %1 (want <= %2)") 
                .arg((long)absLongLong(it1->totalSpaceKB - it2->totalSpaceKB))
                .arg(bSize));
            VERBOSE(VB_SCHEDULE+VB_FILE+VB_EXTRA,
                QString("        Used  KB Diff: %1 (want <= %2)")
                .arg((size_t)absLongLong(it1->usedSpaceKB - it2->usedSpaceKB))
                .arg(maxWriteFiveSec));

            if (it2->fsID == -1 &&
                (absLongLong(it1->totalSpaceKB - it2->totalSpaceKB) <= bSize) &&
                ((size_t)absLongLong(it1->usedSpaceKB - it2->usedSpaceKB)
                 <= maxWriteFiveSec))
            {
                it2->fsID = it1->fsID;

                VERBOSE(VB_SCHEDULE+VB_FILE+VB_EXTRA,
                    QString("    MATCH Found: %1:%2 will use fsID %3")
                            .arg(it2->hostname).arg(it2->directory)
                            .arg(it2->fsID));
            }
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_FILE|VB_SCHEDULE))
    {
        cout << "--- GetFilesystemInfos directory list start ---" << endl;
        for (it1 = fsInfos.begin(); it1 != fsInfos.end(); it1++)
        {
            QString msg = QString("Dir: %1:%2")
                .arg(it1->hostname).arg(it1->directory);
            cout << msg.toLocal8Bit().constData() << endl;
            cout << "     Location: ";
            if (it1->isLocal)
                cout << "Local";
            else
                cout << "Remote";
            cout << endl;
            cout << "     fsID    : " << it1->fsID << endl;
            cout << "     dirID   : " << it1->dirID << endl;
            cout << "     BlkSize : " << it1->blocksize << endl;
            cout << "     TotalKB : " << it1->totalSpaceKB << endl;
            cout << "     UsedKB  : " << it1->usedSpaceKB << endl;
            cout << "     FreeKB  : " << it1->freeSpaceKB << endl;
            cout << endl;
        }
        cout << "--- GetFilesystemInfos directory list end ---" << endl;
    }
}

QMutex recordingPathLock;
QMap <QString, QString> recordingPathCache;

QString GetPlaybackURL(ProgramInfo *pginfo, bool storePath)
{
    QString result = "";
    QMutexLocker locker(&recordingPathLock);
    QString cacheKey = QString("%1:%2").arg(pginfo->chanid)
                               .arg(pginfo->recstartts.toString(Qt::ISODate));
    if ((recordingPathCache.contains(cacheKey)) &&
        (QFile::exists(recordingPathCache[cacheKey])))
    {
        result = recordingPathCache[cacheKey];
        if (!storePath)
            recordingPathCache.remove(cacheKey);
    }
    else
    {
        result = pginfo->GetPlaybackURL(false, true);
        if (storePath && result.left(1) == "/")
            recordingPathCache[cacheKey] = result;
    }

    return result;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
