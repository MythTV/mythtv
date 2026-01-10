#include "programinforemoteutil.h"

#include <thread>

#include <QFileInfo>
#include <QFile>
#include <QDir>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "programinfo.h"

static uint RemoteGetRecordingList(std::vector<ProgramInfo *> &reclist, QStringList &strList)
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

std::vector<ProgramInfo *> *RemoteGetConflictList(const ProgramInfo *pginfo)
{
    QString cmd = QString("QUERY_GETCONFLICTING");
    QStringList strlist( cmd );
    pginfo->ToStringList(strlist);

    auto *retlist = new std::vector<ProgramInfo *>;

    RemoteGetRecordingList(*retlist, strlist);
    return retlist;
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
            std::this_thread::sleep_for(50ms);
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

/* vim: set expandtab tabstop=4 shiftwidth=4: */
