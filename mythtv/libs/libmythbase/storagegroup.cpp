#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QUrl>

#include "storagegroup.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mythcoreutil.h"
#include "mythdirs.h"

#define LOC QString("SG(%1): ").arg(m_groupname)

const char *StorageGroup::kDefaultStorageDir = "/mnt/store";

QMutex                 StorageGroup::m_staticInitLock;
bool                   StorageGroup::m_staticInitDone = false;
QMap<QString, QString> StorageGroup::m_builtinGroups;
QMutex                 StorageGroup::s_groupToUseLock;
QHash<QString,QString> StorageGroup::s_groupToUseCache;

const QStringList StorageGroup::kSpecialGroups = QStringList()
    << "LiveTV"
//    << "Thumbnails"
    << "DB Backups"
    << "Videos"
    << "Trailers"
    << "Coverart"
    << "Fanart"
    << "Screenshots"
    << "Banners"
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
StorageGroup::StorageGroup(const QString group, const QString hostname,
                           bool allowFallback) :
    m_groupname(group), m_hostname(hostname), m_allowFallback(allowFallback)
{
    m_groupname.detach();
    m_hostname.detach();
    m_dirlist.clear();

    if (getenv("MYTHTV_NOSGFALLBACK"))
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

    QMap<QString, QString>::iterator it = m_builtinGroups.begin();
    for (; it != m_builtinGroups.end(); ++it)
    {
        QDir qdir(it.value());
        if (!qdir.exists())
            qdir.mkpath(it.value());

        if (!qdir.exists())
            LOG(VB_GENERAL, LOG_ERR,
                QString("SG() Error: Could not create builtin"
                        "Storage Group directory '%1' for '%2'").arg(it.value())
                    .arg(it.key()));
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
 */
void StorageGroup::Init(const QString group, const QString hostname,
                        const bool allowFallback)
{
    bool found = false;
    m_groupname = group;    m_groupname.detach();
    m_hostname  = hostname; m_hostname.detach();
    m_allowFallback = allowFallback;
    m_dirlist.clear();

    StaticInit();

    found = FindDirs(m_groupname, m_hostname, &m_dirlist);

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
                    "all hosts!").arg(group).arg(hostname));
        found = FindDirs(m_groupname, "", &m_dirlist);
        if (found)
        {
            m_hostname = "";
            m_hostname.detach();
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
            m_groupname.detach();
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
                m_groupname.detach();
                m_hostname.detach();
            }
        }
    }

    if (allowFallback && !m_dirlist.size())
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
        return QString();

    QString tmp = m_dirlist[0];
    tmp.detach();

    if (appendSlash)
        tmp += "/";

    return tmp;
}

QStringList StorageGroup::GetDirFileList(QString dir, QString base,
                                         bool recursive)
{
    QStringList files;
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

        for (QStringList::iterator p = list.begin(); p != list.end(); ++p)
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("GetDirFileList: Dir: %1/%2").arg(base).arg(*p));
            files << GetDirFileList(dir + "/" + *p, base + *p, true);
        }
    }

    QStringList list = d.entryList(QDir::Files|QDir::Readable);
    for (QStringList::iterator p = list.begin(); p != list.end(); ++p)
    {
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("GetDirFileList: File: %1%2").arg(base).arg(*p));
        if (recursive)
            files.append(base + *p);
        else
            files.append(*p);
    }

    return files;
}

QStringList StorageGroup::GetFileList(QString Path, bool recursive)
{
    QStringList files;
    QString tmpDir;
    QDir d;

    for (QStringList::Iterator it = m_dirlist.begin(); it != m_dirlist.end(); ++it)
    {
        tmpDir = *it + Path;

        d.setPath(tmpDir);
        if (d.exists())
            files << GetDirFileList(tmpDir, Path, recursive);
    }

    return files;
}

QStringList StorageGroup::GetFileInfoList(QString Path)
{
    QStringList files;
    QString relPath;
    bool badPath = true;

    if (Path.isEmpty() || Path == "/")
    {
        for (QStringList::Iterator it = m_dirlist.begin(); it != m_dirlist.end(); ++it)
            files << QString("sgdir::%1").arg(*it);

        return files;
    }

    for (QStringList::Iterator it = m_dirlist.begin(); it != m_dirlist.end(); ++it)
    {
        if (Path.startsWith(*it))
        {
            relPath = Path;
            relPath.replace(*it,"");
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

    for (QFileInfoList::iterator p = list.begin(); p != list.end(); ++p)
    {
        if (p->fileName() == "Thumbs.db")
            continue;

        QString tmp;

        if (p->isDir())
            tmp = QString("dir::%1::0").arg(p->fileName());
        else
            tmp = QString("file::%1::%2::%3%4").arg(p->fileName()).arg(p->size())
                          .arg(relPath).arg(p->fileName());

        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("GetFileInfoList: (%1)").arg(tmp));
        files.append(tmp);
    }

    return files;
}

bool StorageGroup::FileExists(QString filename)
{
    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("FileExist: Testing for '%1'").arg(filename));
    bool badPath = true;

    if (filename.isEmpty())
        return false;

    for (QStringList::Iterator it = m_dirlist.begin(); it != m_dirlist.end(); ++it)
    {
        if (filename.startsWith(*it))
        {
            badPath = false;
        }
    }

    if (badPath)
        return false;

    bool result = false;

    QFile checkFile(filename);
    if (checkFile.exists(filename))
        result = true;

    return result;
}


// Returns a string list of details about the file
// in the order EXISTS, DATE, SIZE
QStringList StorageGroup::GetFileInfo(QString filename)
{
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
        details << QString("%1").arg(fInfo.lastModified().toTime_t());
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
             * uses QString::fromAscii() for toString(). Explicitly convert the
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
                        .arg(filename).arg(result));
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
                                            QString::SkipEmptyParts);
            QString directory;
            for (QStringList::Iterator it = videodirs.begin();
                                       it != videodirs.end(); ++it)
            {
                directory = *it;
                if (filename.startsWith(directory))
                {
                    result = filename;
                    result.replace(0, directory.length(), "");
                    if (result.startsWith("/"))
                        result.replace(0, 1, "");

                    LOG(VB_FILE, LOG_DEBUG,
                        QString("StorageGroup::GetRelativePathname(%1) = '%2'")
                            .arg(filename).arg(result));
                    return result;
                }
            }
        }
    }

    QMap<QString, QString>::iterator it = m_builtinGroups.begin();
    for (; it != m_builtinGroups.end(); ++it)
    {
        QDir qdir(it.value());
        if (!qdir.exists())
            qdir.mkpath(it.value());

        QString directory = it.value();
        if (filename.startsWith(directory))
        {
            result = filename;
            result.replace(0, directory.length(), "");
            if (result.startsWith("/"))
                result.replace(0, 1, "");

            LOG(VB_FILE, LOG_DEBUG,
                QString("StorageGroup::GetRelativePathname(%1) = '%2'")
                    .arg(filename).arg(result));
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
bool StorageGroup::FindDirs(const QString group, const QString hostname,
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
    else if (query.next())
    {
        do
        {
            /* The storagegroup.dirname column uses utf8_bin collation, so Qt
             * uses QString::fromAscii() for toString(). Explicitly convert the
             * value using QString::fromUtf8() to prevent corruption. */
            dirname = QString::fromUtf8(query.value(0)
                                        .toByteArray().constData());
            dirname.replace(QRegExp("^\\s*"), "");
            dirname.replace(QRegExp("\\s*$"), "");
            if (dirname.right(1) == "/")
                dirname.remove(dirname.length() - 1, 1);

            if (dirlist)
                (*dirlist) << dirname;
            else
                return true;
        }
        while (query.next());
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

QString StorageGroup::FindFile(QString filename)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("FindFile: Searching for '%1'")
                                      .arg(filename));

    QString recDir = FindFileDir(filename);
    QString result = "";

    if (!recDir.isEmpty())
    {
        result = recDir + "/" + filename;
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("FindFile: Found '%1'") .arg(result));
    }
    else
    {
        LOG(VB_FILE, LOG_ERR, LOC +
            QString("FindFile: Unable to find '%1'!") .arg(filename));
    }

    return result;
}

QString StorageGroup::FindFileDir(QString filename)
{
    QString result = "";
    QFileInfo checkFile("");

    int curDir = 0;
    while (curDir < m_dirlist.size())
    {
        QString testFile = m_dirlist[curDir] + "/" + filename;
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("FindFileDir: Checking '%1' for '%2'")
                .arg(m_dirlist[curDir]).arg(testFile));
        checkFile.setFile(testFile);
        if (checkFile.exists() || checkFile.isSymLink())
        {
            QString tmp = m_dirlist[curDir];
            tmp.detach();
            return tmp;
        }

        curDir++;
    }

    if (m_groupname.isEmpty() || (m_allowFallback == false))
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

    result.detach();
    return result;
}

QString StorageGroup::FindNextDirMostFree(void)
{
    QString nextDir;
    int64_t nextDirFree = 0;
    int64_t thisDirTotal;
    int64_t thisDirUsed;
    int64_t thisDirFree;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("FindNextDirMostFree: Starting"));

    if (m_allowFallback)
        nextDir = kDefaultStorageDir;

    if (m_dirlist.size())
        nextDir = m_dirlist[0];

    QDir checkDir("");
    int curDir = 0;
    while (curDir < m_dirlist.size())
    {
        checkDir.setPath(m_dirlist[curDir]);
        if (!checkDir.exists())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("FindNextDirMostFree: '%1' does not exist!")
                    .arg(m_dirlist[curDir]));
            curDir++;
            continue;
        }

        thisDirFree = getDiskSpace(m_dirlist[curDir], thisDirTotal,
                                   thisDirUsed);
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("FindNextDirMostFree: '%1' has %2 KiB free")
                .arg(m_dirlist[curDir])
                .arg(QString::number(thisDirFree)));

        if (thisDirFree > nextDirFree)
        {
            nextDir     = m_dirlist[curDir];
            nextDirFree = thisDirFree;
        }
        curDir++;
    }

    if (nextDir.isEmpty())
        LOG(VB_FILE, LOG_ERR, LOC +
            "FindNextDirMostFree: Unable to find any directories to use.");
    else
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("FindNextDirMostFree: Using '%1'").arg(nextDir));

    nextDir.detach();
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
         * uses QString::fromAscii() for toString(). Explicitly convert the
         * value using QString::fromUtf8() to prevent corruption. */
        dirname = QString::fromUtf8(query.value(1)
                                    .toByteArray().constData());

        dirname.replace(QRegExp("^\\s*"), "");
        dirname.replace(QRegExp("\\s*$"), "");

        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("Checking directory '%1' in group '%2'.")
                .arg(dirname).arg(m_groupname));

        testDir.setPath(dirname);
        if (!testDir.exists())
        {
            LOG(VB_FILE, LOG_WARNING, LOC +
                QString("Group '%1' references directory '%2' but this "
                        "directory does not exist.  This directory "
                        "will not be used on this server.")
                    .arg(m_groupname).arg(dirname));
        }
        else
        {
            testFile.setFileName(dirname + "/.test");
            if (testFile.open(QIODevice::WriteOnly))
                testFile.remove();
            else
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Group '%1' wants to use directory '%2', but "
                            "this directory is not writeable.")
                        .arg(m_groupname).arg(dirname));
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
    for (QStringList::const_iterator it = StorageGroup::kSpecialGroups.begin();
         it != StorageGroup::kSpecialGroups.end(); ++it)
        sql.append(QString(" '%1',").arg(*it));
    sql = sql.left(sql.length() - 1);
    sql.append(" );");

    query.prepare(sql);
    if (query.exec() && query.isActive() && query.size() > 0)
        while (query.next())
            groups += query.value(0).toString();

    groups.sort();
    groups.detach();

    return groups;
}

QStringList StorageGroup::getGroupDirs(QString groupname, QString host)
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

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        QString dirname;
        while (query.next())
        {
            /* The storagegroup.dirname column uses utf8_bin collation, so Qt
             * uses QString::fromAscii() for toString(). Explicitly convert the
             * value using QString::fromUtf8() to prevent corruption. */
            dirname = QString::fromUtf8(query.value(0)
                                        .toByteArray().constData());
            groups += gCoreContext->GenMythURL(query.value(1).toString(),
                                               0,
                                               dirname,
                                               groupname);
        }
    }

    groups.sort();
    groups.detach();

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
    QString groupKey = QString("%1:%2").arg(sgroup).arg(host);

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
                    .arg(host).arg(sgroup));

            tmpGroup = "Videos";
            s_groupToUseCache[groupKey] = tmpGroup;
        }
    }

    return tmpGroup;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
