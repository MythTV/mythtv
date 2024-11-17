#include <QDir>
#include <QFile>
#include <QUrl>

#include "storagegroup.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "filesysteminfo.h"
#include "mythdirs.h"

#define LOC QString("SG(%1): ").arg(m_groupname)

const char *StorageGroup::kDefaultStorageDir = "/mnt/store";

QMutex                 StorageGroup::m_staticInitLock;
bool                   StorageGroup::m_staticInitDone = false;
QMap<QString, QString> StorageGroup::m_builtinGroups;
QMutex                 StorageGroup::s_groupToUseLock;
QHash<QString,QString> StorageGroup::s_groupToUseCache;

const QStringList StorageGroup::kSpecialGroups = QStringList()
    << QT_TRANSLATE_NOOP("(StorageGroups)", "LiveTV")
//    << "Thumbnails"
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "DB Backups")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "Videos")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "Trailers")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "Coverart")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "Fanart")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "Screenshots")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "Banners")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "Photographs")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "Music")
    <<  QT_TRANSLATE_NOOP("(StorageGroups)", "MusicArt")
    ;

/****************************************************************************/

/** \brief StorageGroup constructor.
 *  \param group    storage group to search, blank will search all groups.
 *  \param hostname hostname where to search, blank will search all hosts'
 *                  directories, but only in local directory structure.
 *                  This is parameter is ignored if group is an empty string.
 *  \param allowFallback allow the storage group search code to fall back if
 *                  no dirs exist for the specified group/host
 */
StorageGroup::StorageGroup(QString group, QString hostname,
                           bool allowFallback) :
    m_groupname(std::move(group)), m_hostname(std::move(hostname)),
    m_allowFallback(allowFallback)
{
    m_dirlist.clear();

    if (qEnvironmentVariableIsSet("MYTHTV_NOSGFALLBACK"))
        m_allowFallback = false;

    Init(m_groupname, m_hostname, m_allowFallback);
}

void StorageGroup::StaticInit(void)
{
    QMutexLocker locker(&m_staticInitLock);

    if (m_staticInitDone)
        return;

    m_staticInitDone = true;

    m_builtinGroups["ChannelIcons"] = GetConfDir() + "/channels";
    m_builtinGroups["Themes"] = GetConfDir() + "/themes";
    m_builtinGroups["Temp"] = GetConfDir() + "/tmp";
    m_builtinGroups["Streaming"] = GetConfDir() + "/tmp/hls";
    m_builtinGroups["3rdParty"] = GetConfDir() + "/3rdParty";

    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_builtinGroups.begin(); it != m_builtinGroups.end(); ++it)
    {
        QDir qdir(it.value());
        if (!qdir.exists())
            qdir.mkpath(it.value());

        if (!qdir.exists())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("SG() Error: Could not create builtin"
                        "Storage Group directory '%1' for '%2'")
                    .arg(it.value(), it.key()));
        }
    }
}

/**
 *  \brief Initilizes the groupname, hostname, and dirlist
 *
 *   First attempts to find the Storage Group defined with the specified name
 *   for the given host.  If not found, checks for the named Storage Group, as
 *   defined across all hosts.  If not found, tries the "Default" Storage Group
 *   for the given host.  If not found, tries the "Default" Storage Group, as
 *   defined across all hosts.
 *
 *  \param group    The name of the Storage Group
 *  \param hostname The host whose Storage Group definition is desired
 *  \param allowFallback allow the storage group search code to fall back if
 *                  no dirs exist for the specified group/host
 */
void StorageGroup::Init(const QString &group, const QString &hostname,
                        const bool allowFallback)
{
    m_groupname = group;
    m_hostname  = hostname;
    m_allowFallback = allowFallback;
    m_dirlist.clear();

    StaticInit();

    bool found = FindDirs(m_groupname, m_hostname, &m_dirlist);

    if (!found && m_builtinGroups.contains(group))
    {
        QDir testdir(m_builtinGroups[group]);
        if (!testdir.exists())
            testdir.mkpath(m_builtinGroups[group]);

        if (testdir.exists())
        {
            m_dirlist.prepend(testdir.absolutePath());
            found = true;
        }
    }

    if ((!found) && m_allowFallback && (m_groupname != "LiveTV") &&
        (!hostname.isEmpty()))
    {
        LOG(VB_FILE, LOG_NOTICE, LOC +
            QString("Unable to find any directories for the local "
                    "storage group '%1' on '%2', trying directories on "
                    "all hosts!").arg(group, hostname));
        found = FindDirs(m_groupname, "", &m_dirlist);
        if (found)
        {
            m_hostname = "";
        }
    }
    if ((!found) && m_allowFallback && (group != "Default"))
    {
        LOG(VB_FILE, LOG_NOTICE, LOC +
            QString("Unable to find storage group '%1', trying "
                    "'Default' group!").arg(group));
        found = FindDirs("Default", m_hostname, &m_dirlist);
        if(found)
        {
            m_groupname = "Default";
        }
        else if (!hostname.isEmpty())
        {
            LOG(VB_FILE, LOG_NOTICE, LOC +
                QString("Unable to find any directories for the local "
                        "Default storage group on '%1', trying directories "
                        "in all Default groups!").arg(hostname));
            found = FindDirs("Default", "", &m_dirlist);
            if(found)
            {
                m_groupname = "Default";
                m_hostname = "";
            }
        }
    }

    if (allowFallback && m_dirlist.empty())
    {
        QString msg = "Unable to find any Storage Group Directories.  ";
        QString tmpDir = gCoreContext->GetSetting("RecordFilePrefix");
        if (tmpDir != "")
        {
            msg += QString("Using old 'RecordFilePrefix' value of '%1'")
                           .arg(tmpDir);
        }
        else
        {
            tmpDir = kDefaultStorageDir;
            msg += QString("Using hardcoded default value of '%1'")
                           .arg(kDefaultStorageDir);
        }
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);
        m_dirlist << tmpDir;
    }
}

QString StorageGroup::GetFirstDir(bool appendSlash) const
{
    if (m_dirlist.isEmpty())
        return {};

    QString tmp = m_dirlist[0];

    if (appendSlash)
        tmp += "/";

    return tmp;
}

QStringList StorageGroup::GetDirFileList(const QString &dir,
                                         const QString &lbase,
                                         bool recursive, bool onlyDirs)
{
    QStringList files;
    QString base = lbase;
    QDir d(dir);

    if (!d.exists())
        return files;

    if (base.split("/").size() > 20)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetDirFileList(), 20 levels deep, "
                                       "possible directory loop detected.");
        return files;
    }

    if (!base.isEmpty())
        base += "/";

    if (recursive)
    {
        QStringList list =
            d.entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::Readable);

        for (const auto& p : std::as_const(list))
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("GetDirFileList: Dir: %1/%2").arg(base, p));

            if (onlyDirs)
                files.append(base + p);

            files << GetDirFileList(dir + "/" + p, base + p, true, onlyDirs);
        }
    }

    if (!onlyDirs)
    {
        QStringList list = d.entryList(QDir::Files|QDir::Readable);
        for (const auto& p : std::as_const(list))
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("GetDirFileList: File: %1%2").arg(base, p));
            if (recursive)
                files.append(base + p);
            else
                files.append(p);
        }
    }
    return files;
}

QStringList StorageGroup::GetDirList(const QString &Path, bool recursive)
{
    QStringList files;
    QString tmpDir;
    QDir d;
    for (const auto& dir : std::as_const(m_dirlist))
    {
        tmpDir = dir + Path;
        d.setPath(tmpDir);
        if (d.exists())
            files << GetDirFileList(tmpDir, Path, recursive, true);
    }
    return files;
}

QStringList StorageGroup::GetFileList(const QString &Path, bool recursive)
{
    QStringList files;
    QString tmpDir;
    QDir d;

    for (const auto& dir : std::as_const(m_dirlist))
    {
        tmpDir = dir + Path;

        d.setPath(tmpDir);
        if (d.exists())
            files << GetDirFileList(tmpDir, Path, recursive, false);
    }

    return files;
}

QStringList StorageGroup::GetFileInfoList(const QString &Path)
{
    QStringList files;
    QString relPath;
    bool badPath = true;

    if (Path.isEmpty() || Path == "/")
    {
        for (const auto& dir : std::as_const(m_dirlist))
            files << QString("sgdir::%1").arg(dir);

        return files;
    }

    for (const auto& dir : std::as_const(m_dirlist))
    {
        if (Path.startsWith(dir))
        {
            relPath = Path;
            relPath.replace(dir,"");
            if (relPath.startsWith("/"))
                relPath.replace(0,1,"");
            badPath = false;
        }
    }

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("GetFileInfoList: Reading '%1'").arg(Path));

    if (badPath)
        return files;

    QDir d(Path);
    if (!d.exists())
        return files;

    d.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
        return files;

    for (const auto& entry : std::as_const(list))
    {
        if (entry.fileName() == "Thumbs.db")
            continue;

        QString tmp;

        if (entry.isDir())
            tmp = QString("dir::%1::0").arg(entry.fileName());
        else
        {
            tmp = QString("file::%1::%2::%3%4").arg(entry.fileName())
                          .arg(entry.size())
                          .arg(relPath, entry.fileName());

        }
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("GetFileInfoList: (%1)").arg(tmp));
        files.append(tmp);
    }

    return files;
}

bool StorageGroup::FileExists(const QString &filename)
{
    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("FileExist: Testing for '%1'").arg(filename));
    bool badPath = true;

    if (filename.isEmpty())
        return false;

    for (const auto & dir : std::as_const(m_dirlist))
    {
        if (filename.startsWith(dir))
        {
            badPath = false;
        }
    }

    if (badPath)
        return false;

    return QFile::exists(filename);
}


// Returns a string list of details about the file
// in the order FILENAME, DATE, SIZE
QStringList StorageGroup::GetFileInfo(const QString &lfilename)
{
    QString filename = lfilename;
    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("GetFileInfo: For '%1'") .arg(filename));

    QStringList details;
    bool searched = false;

    if (!FileExists(filename))
    {
        searched = true;
        filename = FindFile(filename);
    }

    if ((searched && !filename.isEmpty()) ||
        (FileExists(filename)))
    {
        QFileInfo fInfo(filename);

        details << filename;
        if (fInfo.lastModified().isValid()) {
            details << QString("%1").arg(fInfo.lastModified().toSecsSinceEpoch());
        } else {
            details << QString::number(UINT_MAX);
        }
        details << QString("%1").arg(fInfo.size());
    }

    return details;
}

/**
 *  \brief Returns the relative pathname of a file by comparing the filename
 *         against all Storage Group directories (and MythVideo's startupdir)
 *
 *  \param filename The full pathname of the file to use
 *  \return         The relative path if it can be determined, otherwise the
 *                  full input filename is returned back to the caller.
 */
QString StorageGroup::GetRelativePathname(const QString &filename)
{
    QString result = filename;
    MSqlQuery query(MSqlQuery::InitCon());

    LOG(VB_FILE, LOG_DEBUG,
        QString("StorageGroup::GetRelativePathname(%1)").arg(filename));

    StaticInit();

    if (filename.startsWith("myth://"))
    {
        QUrl qurl(filename);

        if (qurl.hasFragment())
            result = qurl.path() + "#" + qurl.fragment();
        else
            result = qurl.path();

        if (result.startsWith("/"))
            result.replace(0, 1, "");

        return result;
    }

    query.prepare("SELECT DISTINCT dirname FROM storagegroup "
                      "ORDER BY dirname DESC;");
    if (query.exec())
    {
        QString dirname;
        while (query.next())
        {
            /* The storagegroup.dirname column uses utf8_bin collation, so Qt
             * uses QString::fromLatin1() for toString(). Explicitly convert the
             * value using QString::fromUtf8() to prevent corruption. */
            dirname = QString::fromUtf8(query.value(0)
                                        .toByteArray().constData());
            if (filename.startsWith(dirname))
            {
                result = filename;
                result.replace(0, dirname.length(), "");
                if (result.startsWith("/"))
                    result.replace(0, 1, "");

                LOG(VB_FILE, LOG_DEBUG,
                    QString("StorageGroup::GetRelativePathname(%1) = '%2'")
                        .arg(filename, result));
                return result;
            }
        }
    }

    query.prepare("SELECT DISTINCT data FROM settings WHERE "
                  "value = 'VideoStartupDir';");
    if (query.exec())
    {
        while (query.next())
        {
            QString videostartupdir = query.value(0).toString();
            QStringList videodirs = videostartupdir.split(':',
                                            Qt::SkipEmptyParts);
            for (const auto& directory : std::as_const(videodirs))
            {
                if (filename.startsWith(directory))
                {
                    result = filename;
                    result.replace(0, directory.length(), "");
                    if (result.startsWith("/"))
                        result.replace(0, 1, "");

                    LOG(VB_FILE, LOG_DEBUG,
                        QString("StorageGroup::GetRelativePathname(%1) = '%2'")
                            .arg(filename, result));
                    return result;
                }
            }
        }
    }

    for (const auto& group : std::as_const(m_builtinGroups))
    {
        QDir qdir(group);
        if (!qdir.exists())
            qdir.mkpath(group);

        const QString& directory = group;
        if (filename.startsWith(directory))
        {
            result = filename;
            result.replace(0, directory.length(), "");
            if (result.startsWith("/"))
                result.replace(0, 1, "");

            LOG(VB_FILE, LOG_DEBUG,
                QString("StorageGroup::GetRelativePathname(%1) = '%2'")
                    .arg(filename, result));
            return result;
        }
    }

    return result;
}

/**
 *  \brief Finds and and optionally initialize a directory list
 *         associated with a Storage Group
 *
 *  \param group    The name of the Storage Group
 *  \param hostname The host whose directory list should be checked, first
 *  \param dirlist  Optional pointer to a QStringList to hold found dir list
 *  \return         true if directories were found
 */
bool StorageGroup::FindDirs(const QString &group, const QString &hostname,
                            QStringList *dirlist)
{
    bool found = false;
    QString dirname;
    MSqlQuery query(MSqlQuery::InitCon());

    StaticInit();

    QString sql = "SELECT DISTINCT dirname "
                  "FROM storagegroup ";

    if (!group.isEmpty())
    {
        sql.append("WHERE groupname = :GROUP");
        if (!hostname.isEmpty())
            sql.append(" AND hostname = :HOSTNAME");
    }

    query.prepare(sql);
    if (!group.isEmpty())
    {
        query.bindValue(":GROUP", group);
        if (!hostname.isEmpty())
            query.bindValue(":HOSTNAME", hostname);
    }

    if (!query.exec() || !query.isActive())
        MythDB::DBError("StorageGroup::StorageGroup()", query);

    while (query.next())
    {
        /* The storagegroup.dirname column uses utf8_bin collation, so Qt
         * uses QString::fromLatin1() for toString(). Explicitly convert the
         * value using QString::fromUtf8() to prevent corruption. */
        dirname = QString::fromUtf8(query.value(0)
                                    .toByteArray().constData());
        dirname = dirname.trimmed();
        if (dirname.endsWith("/"))
            dirname.remove(dirname.length() - 1, 1);

        if (nullptr == dirlist)
            return true;
        (*dirlist) << dirname;
        found = true;
    }

    if (m_builtinGroups.contains(group))
    {
        QDir testdir(m_builtinGroups[group]);
        if (testdir.exists())
        {
            if (dirlist && !dirlist->contains(testdir.absolutePath()))
                (*dirlist) << testdir.absolutePath();
            found = true;
        }
    }

    return found;
}

QString StorageGroup::FindFile(const QString &filename)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("FindFile: Searching for '%1'")
                                      .arg(filename));

    QString recDir = FindFileDir(filename);
    QString result = "";

    if (!recDir.isEmpty())
    {
        result = recDir + "/" + filename;
        LOG(VB_FILE, LOG_INFO, LOC +
            QString("FindFile: Found '%1'") .arg(result));
    }
    else
    {
        LOG(VB_FILE, LOG_ERR, LOC +
            QString("FindFile: Unable to find '%1'!") .arg(filename));
    }

    return result;
}

QString StorageGroup::FindFileDir(const QString &filename)
{
    QString result = "";
    QFileInfo checkFile("");

    int curDir = 0;
    while (curDir < m_dirlist.size())
    {
        QString testFile = m_dirlist[curDir] + "/" + filename;
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("FindFileDir: Checking '%1' for '%2'")
                .arg(m_dirlist[curDir], testFile));
        checkFile.setFile(testFile);
        if (checkFile.exists() || checkFile.isSymLink())
            return m_dirlist[curDir];

        curDir++;
    }

    if (m_groupname.isEmpty() || !m_allowFallback)
    {
        // Not found in any dir, so try RecordFilePrefix if it exists
        QString tmpFile =
            gCoreContext->GetSetting("RecordFilePrefix") + "/" + filename;
        checkFile.setFile(tmpFile);
        if (checkFile.exists() || checkFile.isSymLink())
            result = tmpFile;
    }
    else if (m_groupname != "Default")
    {
        // Not found in current group so try Default
        StorageGroup sgroup("Default");
        QString tmpFile = sgroup.FindFileDir(filename);
        result = (tmpFile.isEmpty()) ? result : tmpFile;
    }
    else
    {
        // Not found in Default so try any dir
        StorageGroup sgroup;
        QString tmpFile = sgroup.FindFileDir(filename);
        result = (tmpFile.isEmpty()) ? result : tmpFile;
    }

    return result;
}

QString StorageGroup::FindNextDirMostFree(void)
{
    QString nextDir;
    int64_t nextDirFree = 0;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("FindNextDirMostFree: Starting"));

    if (m_allowFallback)
        nextDir = kDefaultStorageDir;

    for (const auto & dir : m_dirlist)
    {
        if (!QDir(dir).exists())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("FindNextDirMostFree: '%1' does not exist!").arg(dir));
            continue;
        }

        int64_t thisDirFree = FileSystemInfo(QString(), dir).getFreeSpace();
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("FindNextDirMostFree: '%1' has %2 KiB free")
                .arg(dir, QString::number(thisDirFree)));

        if (thisDirFree > nextDirFree)
        {
            nextDir     = dir;
            nextDirFree = thisDirFree;
        }
    }

    if (nextDir.isEmpty())
    {
        LOG(VB_FILE, LOG_ERR, LOC +
            "FindNextDirMostFree: Unable to find any directories to use.");
    }
    else
    {
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("FindNextDirMostFree: Using '%1'").arg(nextDir));
    }

    return nextDir;
}

void StorageGroup::CheckAllStorageGroupDirs(void)
{
    QString m_groupname;
    QString dirname;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT groupname, dirname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("StorageGroup::CheckAllStorageGroupDirs()", query);
        return;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC +
        "CheckAllStorageGroupDirs(): Checking All Storage Group directories");

    QFile testFile("");
    QDir testDir("");
    while (query.next())
    {
        m_groupname = query.value(0).toString();
        /* The storagegroup.dirname column uses utf8_bin collation, so Qt
         * uses QString::fromLatin1() for toString(). Explicitly convert the
         * value using QString::fromUtf8() to prevent corruption. */
        dirname = QString::fromUtf8(query.value(1)
                                    .toByteArray().constData());
        dirname = dirname.trimmed();

        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("Checking directory '%1' in group '%2'.")
                .arg(dirname, m_groupname));

        testDir.setPath(dirname);
        if (!testDir.exists())
        {
            LOG(VB_FILE, LOG_WARNING, LOC +
                QString("Group '%1' references directory '%2' but this "
                        "directory does not exist.  This directory "
                        "will not be used on this server.")
                    .arg(m_groupname, dirname));
        }
        else
        {
            testFile.setFileName(dirname + "/.test");
            if (testFile.open(QIODevice::WriteOnly))
                testFile.remove();
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Group '%1' wants to use directory '%2', but "
                            "this directory is not writeable.")
                        .arg(m_groupname, dirname));
            }
        }
    }
}

QStringList StorageGroup::getRecordingsGroups(void)
{
    QStringList groups;

    MSqlQuery query(MSqlQuery::InitCon());

    QString sql = "SELECT DISTINCT groupname "
                  "FROM storagegroup "
                  "WHERE groupname NOT IN (";
    for (const auto& group : std::as_const(StorageGroup::kSpecialGroups))
        sql.append(QString(" '%1',").arg(group));
    sql = sql.left(sql.length() - 1);
    sql.append(" );");

    query.prepare(sql);
    if (query.exec())
    {
        while (query.next())
        {
            groups += query.value(0).toString();
        }
    }

    groups.sort();
    return groups;
}

QStringList StorageGroup::getGroupDirs(const QString &groupname,
                                       const QString &host)
{
    QStringList groups;
    QString addHost;

    MSqlQuery query(MSqlQuery::InitCon());

    if (!host.isEmpty())
        addHost = " AND hostname = :HOSTNAME";
    else
        addHost = "";

    QString sql = QString("SELECT dirname,hostname "
                  "FROM storagegroup "
                  "WHERE groupname = :GROUPNAME %1").arg(addHost);

    query.prepare(sql);
    query.bindValue(":GROUPNAME", groupname);

    if (!host.isEmpty())
        query.bindValue(":HOSTNAME", host);

    if (query.exec())
    {
        QString dirname;
        while (query.next())
        {
            /* The storagegroup.dirname column uses utf8_bin collation, so Qt
             * uses QString::fromLatin1() for toString(). Explicitly convert the
             * value using QString::fromUtf8() to prevent corruption. */
            dirname = QString::fromUtf8(query.value(0)
                                        .toByteArray().constData());
            groups += MythCoreContext::GenMythURL(query.value(1).toString(),
                                                  0,
                                                  dirname,
                                                  groupname);
        }
    }

    groups.sort();
    return groups;
}

void StorageGroup::ClearGroupToUseCache(void)
{
    QMutexLocker locker(&s_groupToUseLock);
    s_groupToUseCache.clear();
}

QString StorageGroup::GetGroupToUse(
    const QString &host, const QString &sgroup)
{
    QString tmpGroup = sgroup;
    QString groupKey = QString("%1:%2").arg(sgroup, host);

    QMutexLocker locker(&s_groupToUseLock);

    if (s_groupToUseCache.contains(groupKey))
    {
        tmpGroup = s_groupToUseCache[groupKey];
    }
    else
    {
        if (StorageGroup::FindDirs(sgroup, host))
        {
            s_groupToUseCache[groupKey] = sgroup;
        }
        else
        {
            LOG(VB_FILE, LOG_DEBUG,
                    QString("GetGroupToUse(): "
                            "falling back to Videos Storage Group for host %1 "
                            "since it does not have a %2 Storage Group.")
                    .arg(host, sgroup));

            tmpGroup = "Videos";
            s_groupToUseCache[groupKey] = tmpGroup;
        }
    }

    return tmpGroup;
}

QString StorageGroup::generate_file_url(const QString &storage_group,
                                        const QString &host,
                                        const QString &path)
{
    return MythCoreContext::GenMythURL(host, gCoreContext->GetBackendServerPort(host),
        path, StorageGroup::GetGroupToUse(host, storage_group));

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
