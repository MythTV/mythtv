#include <cstdlib> // for llabs

#include "mythconfig.h"
#if defined(CONFIG_DARWIN) || defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#elif __linux__
#include <sys/vfs.h>
#endif

#include <qdir.h>

#include "backendutil.h"
#include "remoteutil.h"

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/util.h"

static size_t GetTotalMaxBitrate(QMap<int, EncoderLink *> *encoderList)
{
    size_t totalKBperMin = 0;

    QMap<int, EncoderLink*>::iterator it = encoderList->begin();
    for (; it != encoderList->end(); ++it)
    {
        EncoderLink *enc = *it;

        if (!enc->IsConnected())
            continue;

        long long maxBitrate = enc->GetMaxBitrate();
        if (maxBitrate<=0)
            maxBitrate = 19500000LL;
        long long thisKBperMin = (((size_t)maxBitrate)*((size_t)15))>>11;
        totalKBperMin += thisKBperMin;
        VERBOSE(VB_FILE, QString("Cardid %1: max bitrate %2 KB/min")
                .arg(enc->GetCardID()).arg(thisKBperMin));
    }

    VERBOSE(VB_FILE, QString("Maximal bitrate of connected encoders is %1 KB/min")
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
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT dirname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (query.exec() && query.isActive())
    {
        // If we don't have any dirs of our own, fallback to list of Default
        // dirs since that is what StorageGroup::Init() does.
        if (!query.size())
        {
            query.prepare("SELECT DISTINCT dirname "
                          "FROM storagegroup "
                          "WHERE groupname = :GROUP;");
            query.bindValue(":GROUP", "Default");
            query.exec();
        }

        QDir checkDir("");
        QString currentDir;
        while (query.next())
        {
            currentDir = query.value(0).toString();
            if (currentDir.right(1) == "/")
                currentDir.remove(currentDir.length() - 1, 1);

            checkDir.setPath(currentDir);
            if (!foundDirs.contains(currentDir))
            {
                if (checkDir.exists())
                {
                    getDiskSpace(currentDir.ascii(), totalKB, usedKB);
                    bzero(&statbuf, sizeof(statbuf));
                    localStr = "1"; // Assume local

#ifdef CONFIG_DARWIN
                    // FIXME, Not sure what these strings should be so this
                    // code is really a placeholder for now.
                    if ((statfs(currentDir, &statbuf) == 0) &&
                        ((!strcmp(statbuf.f_fstypename, "nfs")) ||   // NFS|FTP
                         (!strcmp(statbuf.f_fstypename, "afpfs")) || // ApplShr
                         (!strcmp(statbuf.f_fstypename, "smbfs"))))  // SMB
                        localStr = "0";
#elif __linux__
                    if ((statfs(currentDir, &statbuf) == 0) &&
                        ((statbuf.f_type == 0x6969) ||     // NFS
                         (statbuf.f_type == 0x517B) ||     // SMB
                         (statbuf.f_type == 0xFF534D42)))  // CIFS
                        localStr = "0";
#endif
                    strlist << gContext->GetHostName();
                    strlist << currentDir;
                    strlist << localStr;
                    strlist << "-1"; // Ignore fsID
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
            encoderHost = eit.data()->GetHostName();
            if (eit.data()->IsConnected() &&
                !eit.data()->IsLocal() &&
                !backendsCounted.contains(encoderHost))
            {
                backendsCounted[encoderHost] = true;

                eit.data()->GetDiskSpace(strlist);

                allHostList += "," + encoderHost;
            }
            ++eit;
        }
    }

    if (!consolidated)
        return;

    FileSystemInfo fsInfo;
    vector<FileSystemInfo> fsInfos;

    QStringList::iterator it = strlist.begin();
    while (it != strlist.end())
    {
        fsInfo.hostname = *(it++);
        fsInfo.directory = fsInfo.hostname.section(".", 0, 0) + ":" + *(it++);
        fsInfo.isLocal = (*(it++)).toInt();
        fsInfo.fsID = (*(it++)).toInt();
        fsInfo.totalSpaceKB = decodeLongLong(strlist, it);
        fsInfo.usedSpaceKB = decodeLongLong(strlist, it);
        fsInfo.freeSpaceKB = fsInfo.totalSpaceKB - fsInfo.usedSpaceKB;
        fsInfos.push_back(fsInfo);
    }
    strlist.clear();

    // Consolidate hosts sharing storage
    size_t maxWriteFiveSec = GetTotalMaxBitrate(encoderList)/12 /*5 seconds*/;
    int nextID = 0;
    vector<FileSystemInfo>::iterator it1, it2;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); it1++)
    {
        if (it1->fsID == -1)
            it1->fsID = nextID++;

        it2 = it1;
        for (it2++; it2 != fsInfos.end(); it2++)
        {
            // Sometimes the space reported for an NFS mounted dir is slightly
            // different than when it is locally mounted because of block sizes
            if ((absLongLong(it1->totalSpaceKB - it2->totalSpaceKB) <= 16 ) &&
                (absLongLong(it1->usedSpaceKB - it2->usedSpaceKB)
                 < maxWriteFiveSec))
            {
                if (!it1->hostname.contains(it2->hostname))
                    it1->hostname = it1->hostname + "," + it2->hostname;
                it1->directory = it1->directory + "," + it2->directory;
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

    QStringList::iterator it = strlist.begin();
    while (it != strlist.end())
    {
        fsInfo.hostname = *(it++);
        fsInfo.directory = *(it++);
        fsInfo.isLocal = (*(it++)).toInt();
        fsInfo.fsID = -1;
        it++;
        fsInfo.totalSpaceKB = decodeLongLong(strlist, it);
        fsInfo.liveTVSpaceKB = 0; // FIXME, placeholder
        fsInfo.usedSpaceKB = decodeLongLong(strlist, it);
        fsInfo.freeSpaceKB = fsInfo.totalSpaceKB - fsInfo.usedSpaceKB;
        fsInfo.weight = 0;
        fsInfos.push_back(fsInfo);
    }

    size_t maxWriteFiveSec = GetTotalMaxBitrate(tvList)/12  /*5 seconds*/;
    int nextID = 0;
    vector<FileSystemInfo>::iterator it1, it2;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); it1++)
    {
        if (it1->fsID == -1)
            it1->fsID = nextID++;
        else
            continue;

        it2 = it1;
        for (it2++; it2 != fsInfos.end(); it2++)
        {
            // Sometimes the space reported for an NFS mounted dir is slightly
            // different than when it is locally mounted because of block sizes
            if ((it2->fsID == -1) &&
                ((absLongLong(it1->totalSpaceKB - it2->totalSpaceKB) <= 16 ) &&
                 (absLongLong(it1->usedSpaceKB - it2->usedSpaceKB)
                  < maxWriteFiveSec)))
                it2->fsID = it1->fsID;
        }
    }

    if (print_verbose_messages & VB_FILE)
    {
        cout << "--- GetFilesystemInfos directory list start ---" << endl;
        for (it1 = fsInfos.begin(); it1 != fsInfos.end(); it1++)
        {
            cout << "Dir: " << it1->hostname << ":" << it1->directory << endl;
            cout << "     Location: ";
            if (it1->isLocal)
                cout << "Local";
            else
                cout << "Remote";
            cout << endl;
            cout << "     Drive ID: " << it1->fsID << endl;
            cout << "     TotalKB : " << it1->totalSpaceKB << endl;
            cout << "     UsedKB  : " << it1->usedSpaceKB << endl;
            cout << "     FreeKB  : " << it1->freeSpaceKB << endl;
            cout << endl;
        }
        cout << "--- GetFilesystemInfos directory list end ---" << endl;
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
