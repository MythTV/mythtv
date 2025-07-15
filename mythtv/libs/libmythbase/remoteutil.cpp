#include "remoteutil.h"

#include <unistd.h>

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QList>

#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsocket.h"
#include "libmythbase/storagegroup.h"

#include "programinfo.h"

std::vector<ProgramInfo *> *RemoteGetRecordedList(int sort)
{
    QString str = "QUERY_RECORDINGS ";
    if (sort < 0)
        str += "Descending";
    else if (sort > 0)
        str += "Ascending";
    else
        str += "Unsorted";

    QStringList strlist(str);

    auto *info = new std::vector<ProgramInfo *>;

    if (!RemoteGetRecordingList(*info, strlist))
    {
        delete info;
        return nullptr;
    }

    return info;
}

bool RemoteGetLoad(system_load_array& load)
{
    QStringList strlist(QString("QUERY_LOAD"));

    if (gCoreContext->SendReceiveStringList(strlist) && strlist.size() >= 3)
    {
        load[0] = strlist[0].toDouble();
        load[1] = strlist[1].toDouble();
        load[2] = strlist[2].toDouble();
        return true;
    }

    return false;
}

bool RemoteGetUptime(std::chrono::seconds &uptime)
{
    QStringList strlist(QString("QUERY_UPTIME"));

    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.isEmpty())
        return false;

    if (strlist[0].isEmpty() || !strlist[0].at(0).isNumber())
        return false;

    if (sizeof(std::chrono::seconds::rep) == sizeof(long long))
        uptime = std::chrono::seconds(strlist[0].toLongLong());
    else if (sizeof(std::chrono::seconds::rep) == sizeof(long))
        uptime = std::chrono::seconds(strlist[0].toLong());
    else if (sizeof(std::chrono::seconds::rep) == sizeof(int))
        uptime = std::chrono::seconds(strlist[0].toInt());

    return true;
}

bool RemoteGetMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM)
{
    QStringList strlist(QString("QUERY_MEMSTATS"));

    if (gCoreContext->SendReceiveStringList(strlist) && strlist.size() >= 4)
    {
        totalMB = strlist[0].toInt();
        freeMB  = strlist[1].toInt();
        totalVM = strlist[2].toInt();
        freeVM  = strlist[3].toInt();
        return true;
    }

    return false;
}

bool RemoteCheckFile(ProgramInfo *pginfo, bool checkSlaves)
{
    QStringList strlist("QUERY_CHECKFILE");
    strlist << QString::number((int)checkSlaves);
    pginfo->ToStringList(strlist);

    if (!gCoreContext->SendReceiveStringList(strlist) ||
        (strlist.size() < 2) || !strlist[0].toInt())
        return false;

    // Only modify the pathname if the recording file is available locally on
    // this host
    QString localpath = strlist[1];
    QFile checkFile(localpath);
    if (checkFile.exists())
        pginfo->SetPathname(localpath);

    return true;
}

bool RemoteDeleteRecording(uint recordingID, bool forceMetadataDelete,
    bool forgetHistory)
{
     // FIXME: Remove when DELETE_RECORDING has been updated to use recording id
    ProgramInfo recInfo(recordingID);
    bool result = true;
    QString cmd =
        QString("DELETE_RECORDING %1 %2 %3 %4")
        .arg(QString::number(recInfo.GetChanID()),
             recInfo.GetRecordingStartTime().toString(Qt::ISODate),
             forceMetadataDelete ? "FORCE" : "NO_FORCE",
             forgetHistory ? "FORGET" : "NO_FORGET");
    QStringList strlist(cmd);

    if ((!gCoreContext->SendReceiveStringList(strlist) || strlist.isEmpty()) ||
        (strlist[0].toInt() == -2))
        result = false;

    if (!result)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 QString("Failed to delete recording %1:%2")
                     .arg(recInfo.GetChanID())
                     .arg(recInfo.GetRecordingStartTime().toString(Qt::ISODate)));
    }

    return result;
}

bool RemoteUndeleteRecording(uint recordingID)
{
    // FIXME: Remove when UNDELETE_RECORDING has been updated to use recording id
    ProgramInfo recInfo(recordingID);
    bool result = false;

#if 0
    if (!gCoreContext->GetNumSetting("AutoExpireInsteadOfDelete", 0))
        return result;
#endif

    QStringList strlist(QString("UNDELETE_RECORDING"));
    strlist.push_back(QString::number(recInfo.GetChanID()));
    strlist.push_back(recInfo.GetRecordingStartTime().toString(Qt::ISODate));

    gCoreContext->SendReceiveStringList(strlist);

    if (!strlist.isEmpty() && strlist[0].toInt() == 0)
        result = true;

    return result;
}

void RemoteGetAllScheduledRecordings(std::vector<ProgramInfo *> &scheduledlist)
{
    QStringList strList(QString("QUERY_GETALLSCHEDULED"));
    RemoteGetRecordingList(scheduledlist, strList);
}

void RemoteGetAllExpiringRecordings(std::vector<ProgramInfo *> &expiringlist)
{
    QStringList strList(QString("QUERY_GETEXPIRING"));
    RemoteGetRecordingList(expiringlist, strList);
}

uint RemoteGetRecordingList(
    std::vector<ProgramInfo *> &reclist, QStringList &strList)
{
    if (!gCoreContext->SendReceiveStringList(strList) || strList.isEmpty())
        return 0;

    int numrecordings = strList[0].toInt();
    if (numrecordings <= 0)
        return 0;

    if ((numrecordings * NUMPROGRAMLINES) + 1 > strList.size())
    {
        LOG(VB_GENERAL, LOG_ERR,
                 "RemoteGetRecordingList() list size appears to be incorrect.");
        return 0;
    }

    uint reclist_initial_size = (uint) reclist.size();
    QStringList::const_iterator it = strList.cbegin() + 1;
    for (int i = 0; i < numrecordings; i++)
    {
        auto *pginfo = new ProgramInfo(it, strList.cend());
        reclist.push_back(pginfo);
    }

    return ((uint) reclist.size()) - reclist_initial_size;
}

std::vector<ProgramInfo *> *RemoteGetConflictList(const ProgramInfo *pginfo)
{
    QString cmd = QString("QUERY_GETCONFLICTING");
    QStringList strlist( cmd );
    pginfo->ToStringList(strlist);

    auto *retlist = new std::vector<ProgramInfo *>;

    RemoteGetRecordingList(*retlist, strlist);
    return retlist;
}

QDateTime RemoteGetPreviewLastModified(const ProgramInfo *pginfo)
{
    QStringList strlist( "QUERY_PIXMAP_LASTMODIFIED" );
    pginfo->ToStringList(strlist);

    if (!gCoreContext->SendReceiveStringList(strlist))
        return {};

    if (!strlist.isEmpty() && !strlist[0].isEmpty() && (strlist[0] != "BAD"))
    {
        qint64 timet = strlist[0].toLongLong();
        return MythDate::fromSecsSinceEpoch(timet);
    }

    return {};
}

/// Download preview & get timestamp if newer than cachefile's
/// last modified time, otherwise just get the timestamp
QDateTime RemoteGetPreviewIfModified(
    const ProgramInfo &pginfo, const QString &cachefile)
{
    QString loc("RemoteGetPreviewIfModified: ");
    QDateTime cacheLastModified;
    QFileInfo cachefileinfo(cachefile);
    if (cachefileinfo.exists())
        cacheLastModified = cachefileinfo.lastModified();

    QStringList strlist("QUERY_PIXMAP_GET_IF_MODIFIED");
    strlist << ((cacheLastModified.isValid()) ? // unix secs, UTC
                QString::number(cacheLastModified.toSecsSinceEpoch()) :
                QString("-1"));
    strlist << QString::number(200 * 1024); // max size of preview file
    pginfo.ToStringList(strlist);

    if (!gCoreContext->SendReceiveStringList(strlist) ||
        strlist.isEmpty() || strlist[0] == "ERROR")
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Remote error" +
            ((strlist.size() >= 2) ? (":\n\t\t\t" + strlist[1]) : ""));

        return {};
    }

    if (strlist[0] == "WARNING")
    {
        LOG(VB_NETWORK, LOG_WARNING, loc + "Remote warning" +
                 ((strlist.size() >= 2) ? (":\n\t\t\t" + strlist[1]) : ""));

        return {};
    }

    QDateTime retdatetime;
    qlonglong timet = strlist[0].toLongLong();
    if (timet >= 0)
    {
        retdatetime = MythDate::fromSecsSinceEpoch(timet);
    }

    if (strlist.size() < 4)
    {
        return retdatetime;
    }

    size_t  length     = strlist[1].toULongLong();
    quint16 checksum16 = strlist[2].toUInt();
    QByteArray data = QByteArray::fromBase64(strlist[3].toLatin1());
    if ((size_t) data.size() < length)
    { // (note data.size() may be up to 3 bytes longer after decoding
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Preview size check failed %1 < %2")
                .arg(data.size()).arg(length));
        return {};
    }
    data.resize(length);

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    quint16 calculated = qChecksum(data.constData(), data.size());
#else
    quint16 calculated = qChecksum(data);
#endif
    if (checksum16 != calculated)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Preview checksum failed");
        return {};
    }

    QString pdir(cachefile.section("/", 0, -2));
    QDir cfd(pdir);
    if (!cfd.exists() && !cfd.mkdir(pdir))
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Unable to create remote cache directory '%1'")
                .arg(pdir));

        return {};
    }

    QFile file(cachefile);
    if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate))
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Unable to open cached preview file for writing '%1'")
                .arg(cachefile));

        return {};
    }

    off_t offset = 0;
    size_t remaining = length;
    uint failure_cnt = 0;
    while ((remaining > 0) && (failure_cnt < 5))
    {
        ssize_t written = file.write(data.data() + offset, remaining);
        if (written < 0)
        {
            failure_cnt++;
            usleep(50000);
            continue;
        }

        failure_cnt  = 0;
        offset      += written;
        remaining   -= written;
    }

    if (remaining)
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Failed to write cached preview file '%1'")
                .arg(cachefile));

        file.resize(0); // in case unlink fails..
        file.remove();  // closes fd
        return {};
    }

    file.close();

    return retdatetime;
}

bool RemoteFillProgramInfo(ProgramInfo &pginfo, const QString &playbackhost)
{
    QStringList strlist( "FILL_PROGRAM_INFO" );
    strlist << playbackhost;
    pginfo.ToStringList(strlist);

    if (gCoreContext->SendReceiveStringList(strlist))
    {
        ProgramInfo tmp(strlist);
        if (tmp.HasPathname() || tmp.GetChanID())
        {
            pginfo = tmp;
            return true;
        }
    }

    return false;
}

QStringList RemoteRecordings(void)
{
    QStringList strlist("QUERY_ISRECORDING");

    if (!gCoreContext->SendReceiveStringList(strlist, false, false))
    {
        QStringList empty;
        empty << "0" << "0";
        return empty;
    }

    return strlist;
}

int RemoteGetRecordingMask(void)
{
    int mask = 0;

    QString cmd = "QUERY_ISRECORDING";

    QStringList strlist( cmd );

    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.isEmpty())
        return mask;

    int recCount = strlist[0].toInt();

    for (int i = 0, j = 0; j < recCount; i++)
    {
        cmd = QString("QUERY_RECORDER %1").arg(i + 1);

        strlist = QStringList( cmd );
        strlist << "IS_RECORDING";

        if (gCoreContext->SendReceiveStringList(strlist) && !strlist.isEmpty())
        {
            if (strlist[0].toInt())
            {
                mask |= 1<<i;
                j++;       // count active recorder
            }
        }
        else
        {
            break;
        }
    }

    return mask;
}

bool RemoteGetFileList(const QString& host, const QString& path, QStringList* list,
                       QString sgroup, bool fileNamesOnly)
{

    // Make sure the list is empty when we get started
    list->clear();

    if (sgroup.isEmpty())
        sgroup = "Videos";

    *list << "QUERY_SG_GETFILELIST";
    *list << host;
    *list << StorageGroup::GetGroupToUse(host, sgroup);
    *list << path;
    *list << QString::number(static_cast<int>(fileNamesOnly));

    bool ok = false;

    if (gCoreContext->IsMasterBackend())
    {
        // since the master backend cannot connect back around to
        // itself, and the libraries do not have access to the list
        // of connected slave backends to query an existing connection
        // start up a new temporary connection directly to the slave
        // backend to query the file list
        QString ann = QString("ANN Playback %1 0")
                        .arg(gCoreContext->GetHostName());
        QString addr = gCoreContext->GetBackendServerIP(host);
        int port = gCoreContext->GetBackendServerPort(host);
        bool mismatch = false;

        MythSocket *sock = gCoreContext->ConnectCommandSocket(
                                            addr, port, ann, &mismatch);
        if (sock)
        {
            ok = sock->SendReceiveStringList(*list);
            sock->DecrRef();
        }
        else
        {
            list->clear();
        }
    }
    else
    {
        ok = gCoreContext->SendReceiveStringList(*list);
    }

// Should the SLAVE UNREACH test be here ?
    return ok;
}

/**
 * Get recorder for a programme.
 *
 * \return recordernum if pginfo recording in progress, else 0
 */
int RemoteCheckForRecording(const ProgramInfo *pginfo)
{
    QStringList strlist( QString("CHECK_RECORDING") );
    pginfo->ToStringList(strlist);

    if (gCoreContext->SendReceiveStringList(strlist) && !strlist.isEmpty())
        return strlist[0].toInt();

    return 0;
}

/**
 * Get status of an individual programme (with pre-post roll?).
 *
 * \retval  0  Not Recording
 * \retval  1  Recording
 * \retval  2  Under-Record
 * \retval  3  Over-Record
 */
int RemoteGetRecordingStatus(
    const ProgramInfo *pginfo, int overrecsecs, int underrecsecs)
{
    QDateTime curtime = MythDate::current();

    int retval = 0;

    if (pginfo)
    {
        if (curtime >= pginfo->GetScheduledStartTime().addSecs(-underrecsecs) &&
            curtime < pginfo->GetScheduledEndTime().addSecs(overrecsecs))
        {
            if (curtime >= pginfo->GetScheduledStartTime() &&
                curtime < pginfo->GetScheduledEndTime())
                retval = 1;
            else if (curtime < pginfo->GetScheduledStartTime() &&
                     RemoteCheckForRecording(pginfo) > 0)
                retval = 2;
            else if (curtime > pginfo->GetScheduledEndTime() &&
                     RemoteCheckForRecording(pginfo) > 0)
                retval = 3;
        }
    }

    return retval;
}

/**
 * \brief return list of currently recording shows
 */
std::vector<ProgramInfo *> *RemoteGetCurrentlyRecordingList(void)
{
    QString str = "QUERY_RECORDINGS ";
    str += "Recording";
    QStringList strlist( str );

    auto *reclist = new std::vector<ProgramInfo *>;
    auto *info = new std::vector<ProgramInfo *>;
    if (!RemoteGetRecordingList(*info, strlist))
    {
        delete info;
        return reclist;
    }

    // make sure whatever RemoteGetRecordingList() returned
    // only has RecStatus::Recording shows
    for (auto & p : *info)
    {
        if (p->GetRecordingStatus() == RecStatus::Recording ||
            p->GetRecordingStatus() == RecStatus::Tuning ||
            p->GetRecordingStatus() == RecStatus::Failing ||
            (p->GetRecordingStatus() == RecStatus::Recorded &&
             p->GetRecordingGroup() == "LiveTV"))
        {
            reclist->push_back(new ProgramInfo(*p));
        }
    }

    while (!info->empty())
    {
        delete info->back();
        info->pop_back();
    }
    delete info;

    return reclist;
}

/**
 * \brief return list of backends currently connected to the master
 */
bool RemoteGetActiveBackends(QStringList *list)
{
    list->clear();
    *list << "QUERY_ACTIVE_BACKENDS";

    if (!gCoreContext->SendReceiveStringList(*list))
        return false;

    list->removeFirst();
    return true;
}

static QString downloadRemoteFile(const QString &cmd, const QString &url,
                                  const QString &storageGroup,
                                  const QString &filename)
{
    QStringList strlist(cmd);
    strlist << url;
    strlist << storageGroup;
    strlist << filename;

    bool ok = gCoreContext->SendReceiveStringList(strlist);

    if (!ok || strlist.size() < 2 || strlist[0] != "OK")
    {
        LOG(VB_GENERAL, LOG_ERR,
            "downloadRemoteFile(): " + cmd + " returned ERROR!");
        return {};
    }

    return strlist[1];
}

QString RemoteDownloadFile(const QString &url,
                           const QString &storageGroup,
                           const QString &filename)
{
    return downloadRemoteFile("DOWNLOAD_FILE", url, storageGroup, filename);
}

QString RemoteDownloadFileNow(const QString &url,
                              const QString &storageGroup,
                              const QString &filename)
{
    return downloadRemoteFile("DOWNLOAD_FILE_NOW", url, storageGroup, filename);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
