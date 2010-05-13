#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QUrl>

#include "storagegroup.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "util.h"

#define LOC QString("SG(%1): ").arg(m_groupname)
#define LOC_WARN QString("SG(%1) Warning: ").arg(m_groupname)
#define LOC_ERR QString("SG(%1) Error: ").arg(m_groupname)

const char *StorageGroup::kDefaultStorageDir = "/mnt/store";

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

    Init(m_groupname, m_hostname, m_allowFallback);
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

    found = FindDirs(m_groupname, m_hostname, &m_dirlist);
    if ((!found) && m_allowFallback && (m_groupname != "LiveTV") &&
        (!hostname.isEmpty()))
    {
        VERBOSE(VB_FILE, LOC +
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
        VERBOSE(VB_FILE, LOC +
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
            VERBOSE(VB_FILE, LOC +
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
        m_dirlist << tmpDir;
    }
}

QStringList StorageGroup::GetFileList(QString Path)
{
    QStringList files;
    QString tmpDir;
    QDir d;

    for (QStringList::Iterator it = m_dirlist.begin(); it != m_dirlist.end(); ++it)
    {
        tmpDir = *it + Path;

        d.setPath(tmpDir);
        if (d.exists())
        {
            VERBOSE(VB_FILE, LOC + QString("GetFileList: Reading '%1'").arg(tmpDir));
            QStringList list = d.entryList(QDir::Files|QDir::Readable);
            for (QStringList::iterator p = list.begin(); p != list.end(); ++p)
            {
                VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("GetFileList: (%1)").arg(*p));
                files.append(*p);
            }
        }
    }

    return files;
}

QStringList StorageGroup::GetFileInfoList(QString Path)
{
    QStringList files;
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
            badPath = false;
        }
    }

    VERBOSE(VB_FILE, LOC + QString("GetFileInfoList: Reading '%1'").arg(Path));

    if (badPath)
        return files;

    QDir d(Path);
    if (!d.exists())
        return files;

    QFileInfoList list = d.entryInfoList();
    if (!list.size())
        return files;

    for (QFileInfoList::iterator p = list.begin(); p != list.end(); ++p)
    {
        if (p->fileName() == "." ||
            p->fileName() == ".." ||
            p->fileName() == "Thumbs.db")
        {
            continue;
        }

        QString tmp;

        if (p->isDir())
            tmp = QString("dir::%1::0").arg(p->fileName());
        else
            tmp = QString("file::%1::%2").arg(p->fileName()).arg(p->size());

        VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("GetFileInfoList: (%1)").arg(tmp));
        files.append(tmp);
    }

    return files;
}

bool StorageGroup::FileExists(QString filename)
{
    VERBOSE(VB_FILE, LOC + QString("FileExist: Testing for '%1'")
                                               .arg(filename));
    bool badPath = true;

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
    VERBOSE(VB_FILE, LOC + QString("GetFileInfo: For '%1'")
                                               .arg(filename));

    QStringList details;

    if (FileExists(filename))
    {
        QFileInfo fInfo(filename);

        details << filename;
        details << QString("%1").arg(fInfo.lastModified().toTime_t());
        details << QString("%1").arg(fInfo.size());
    }

    return details;
}

/** \fn StorageGroup::GetRelativePathname(const Qstring&)
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

    VERBOSE(VB_FILE+VB_EXTRA,
            QString("StorageGroup::GetRelativePathname(%1)").arg(filename));

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

    query.prepare("SELECT DISTINCT dirname FROM storagegroup;");
    if (query.exec())
    {
        while (query.next())
        {
            if (filename.startsWith(query.value(0).toString()))
            {
                result = filename;
                result.replace(0, query.value(0).toString().length(), "");
                if (result.startsWith("/"))
                    result.replace(0, 1, "");

                VERBOSE(VB_FILE+VB_EXTRA,
                        QString("StorageGroup::GetRelativePathname(%1) = '%2'")
                                .arg(filename).arg(result));
                return result;
            }
        }
    }

    query.prepare("SELECT DISTINCT data FROM settings WHERE value = 'VideoStartupDir';");
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

                    VERBOSE(VB_FILE+VB_EXTRA,
                            QString("StorageGroup::GetRelativePathname(%1) "
                                    "= '%2'").arg(filename).arg(result));
                    return result;
                }
            }
        }
    }

    return result;
}

/** \fn StorageGroup::FindDirs(const QString, const QString)
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
            dirname = query.value(0).toString();
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

    return found;
}

QString StorageGroup::FindRecordingFile(QString filename)
{
    VERBOSE(VB_FILE, LOC + QString("FindRecordingFile: Searching for '%1'")
                                   .arg(filename));

    QString recDir = FindRecordingDir(filename);
    QString result = "";
    
    if (!recDir.isEmpty())
    {
        result = recDir + "/" + filename;
        VERBOSE(VB_FILE, LOC + QString("FindRecordingFile: Found '%1'")
                                       .arg(result));
    }
    else
    {
        VERBOSE(VB_FILE, LOC_ERR +
                QString("FindRecordingFile: Unable to find '%1'!")
                        .arg(filename));
    }

    return result;
}

QString StorageGroup::FindRecordingDir(QString filename)
{
    QString result = "";
    QFileInfo checkFile("");

    int curDir = 0;
    while (curDir < m_dirlist.size())
    {
        QString testFile = m_dirlist[curDir] + "/" + filename;
        VERBOSE(VB_FILE, LOC + QString("FindRecordingDir: Checking '%1' for '%2'")
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
        QString tmpFile = sgroup.FindRecordingDir(filename);
        result = (tmpFile.isEmpty()) ? result : tmpFile;
    }
    else
    {
        // Not found in Default so try any dir
        StorageGroup sgroup;
        QString tmpFile = sgroup.FindRecordingDir(filename);
        result = (tmpFile.isEmpty()) ? result : tmpFile;
    }

    result.detach();
    return result;
}

QString StorageGroup::FindNextDirMostFree(void)
{
    QString nextDir;
    long long nextDirFree = 0;
    long long thisDirTotal;
    long long thisDirUsed;
    long long thisDirFree;

    VERBOSE(VB_FILE, LOC + QString("FindNextDirMostFree: Starting"));

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
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("FindNextDirMostFree: '%1' does not exist!")
                            .arg(m_dirlist[curDir]));
            curDir++;
            continue;
        }

        thisDirFree = getDiskSpace(m_dirlist[curDir], thisDirTotal,
                                   thisDirUsed);
        VERBOSE(VB_FILE, LOC +
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
        VERBOSE(VB_FILE, LOC + QString("FindNextDirMostFree: Unable to find "
                                       "any directories to use."));
    else
        VERBOSE(VB_FILE, LOC + QString("FindNextDirMostFree: Using '%1'")
                                       .arg(nextDir));

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

    VERBOSE(VB_FILE, LOC + "CheckAllStorageGroupDirs(): Checking All Storage "
            "Group directories");

    QFile testFile("");
    QDir testDir("");
    while (query.next())
    {
        m_groupname = query.value(0).toString();
        dirname = query.value(1).toString();

        dirname.replace(QRegExp("^\\s*"), "");
        dirname.replace(QRegExp("\\s*$"), "");

        VERBOSE(VB_FILE, LOC +
                QString("Checking directory '%1' in group '%2'.")
                        .arg(dirname).arg(m_groupname));

        testDir.setPath(dirname);
        if (!testDir.exists())
        {
            VERBOSE(VB_FILE, LOC_WARN + 
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
                VERBOSE(VB_IMPORTANT,
                        LOC_ERR +
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
        while (query.next())
            groups += QString("myth://%1@%2%3").arg(groupname)
                                       .arg(query.value(1).toString())
                                       .arg(query.value(0).toString());

    groups.sort();
    groups.detach();

    return groups;
}


/****************************************************************************/
typedef enum {
    SGPopup_OK = 0,
    SGPopup_CANCEL,
    SGPopup_DELETE
} SGPopupResult;

class StorageGroupPopup
{
  public:
    static SGPopupResult showPopup(MythMainWindow *parent, QString title,
                                   QString message, QString& text);
};

SGPopupResult StorageGroupPopup::showPopup(MythMainWindow *parent,
                                 QString title, QString message, QString& text)
{
    MythPopupBox *popup = new MythPopupBox(parent, title.toAscii().constData());
    popup->addLabel(message);

    MythLineEdit *textEdit = new MythLineEdit(popup, "chooseEdit");
    textEdit->setText(text);
    popup->addWidget(textEdit);

    popup->addButton(QObject::tr("OK"),     popup, SLOT(accept()));
    popup->addButton(QObject::tr("Cancel"), popup, SLOT(reject()));

    textEdit->setFocus();

    bool ok = (MythDialog::Accepted == popup->ExecPopup());
    if (ok)
    {
        text = textEdit->text();
        text.detach();
    }

    popup->hide();
    popup->deleteLater();

    return (ok) ? SGPopup_OK : SGPopup_CANCEL;
}

/****************************************************************************/

StorageGroupEditor::StorageGroupEditor(QString group) :
    m_group(group), listbox(new ListBoxSetting(this)), lastValue("")
{
    QString dispGroup = group;

    if (group == "Default")
        dispGroup = QObject::tr("Default");
    else if (StorageGroup::kSpecialGroups.contains(group))
        dispGroup = QObject::tr(group.toLatin1().constData());

    if (gCoreContext->GetSetting("MasterServerIP","master") ==
            gCoreContext->GetSetting("BackendServerIP","me"))
    {
        listbox->setLabel(tr("'%1' Storage Group Directories").arg(dispGroup));
    }
    else
    {
        listbox->setLabel(tr("Local '%1' Storage Group Directories")
                             .arg(dispGroup));
    }

    addChild(listbox);
}

void StorageGroupEditor::open(QString name) 
{
    lastValue = name;

    if (name == "__CREATE_NEW_STORAGE_DIRECTORY__")
    {
        name = "";
        SGPopupResult result = StorageGroupPopup::showPopup(
            GetMythMainWindow(), 
            tr("Add Storage Group Directory"),
            tr("Enter directory name or press SELECT to enter text via the "
               "On Screen Keyboard"), name);
        if (result == SGPopup_CANCEL)
            return;

        if (name.isEmpty())
            return;

        if (name.right(1) != "/")
            name.append("/");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO storagegroup (groupname, hostname, dirname) "
                      "VALUES (:NAME, :HOSTNAME, :DIRNAME);");
        query.bindValue(":NAME", m_group);
        query.bindValue(":DIRNAME", name);
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("StorageGroupEditor::open", query);
        else
            lastValue = name;
    } else {
        SGPopupResult result = StorageGroupPopup::showPopup(
            GetMythMainWindow(), 
            tr("Edit Storage Group Directory"),
            tr("Enter directory name or press SELECT to enter text via the "
               "On Screen Keyboard"), name);
        if (result == SGPopup_CANCEL)
            return;

        if (name.right(1) != "/")
            name.append("/");

        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("DELETE FROM storagegroup "
                      "WHERE groupname = :NAME "
                          "AND dirname = :DIRNAME "
                          "AND hostname = :HOSTNAME;");
        query.bindValue(":NAME", m_group);
        query.bindValue(":DIRNAME", lastValue);
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("StorageGroupEditor::open", query);

        query.prepare("INSERT INTO storagegroup (groupname, hostname, dirname) "
                      "VALUES (:NAME, :HOSTNAME, :DIRNAME);");
        query.bindValue(":NAME", m_group);
        query.bindValue(":DIRNAME", name);
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("StorageGroupEditor::open", query);
        else
            lastValue = name;
    }
};

void StorageGroupEditor::doDelete(void) 
{
    QString name = listbox->getValue();
    if (name == "__CREATE_NEW_STORAGE_DIRECTORY__")
        return;

    QString message =
        tr("Remove '%1'\nDirectory From Storage Group?").arg(name);

    DialogCode value = MythPopupBox::Show2ButtonPopup(
        GetMythMainWindow(), "", message,
        tr("Yes, remove directory"),
        tr("No, Don't remove directory"),
        kDialogCodeButton1);

    if (kDialogCodeButton0 == value)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM storagegroup "
                      "WHERE groupname = :NAME "
                          "AND dirname = :DIRNAME "
                          "AND hostname = :HOSTNAME;");
        query.bindValue(":NAME", m_group);
        query.bindValue(":DIRNAME", name);
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("StorageGroupEditor::doDelete", query);

        int lastIndex = listbox->getValueIndex(name);
        lastValue = "";
        Load();
        listbox->setValue(lastIndex);
    }

    listbox->setFocus();
}

void StorageGroupEditor::Load(void)
{
    listbox->clearSelections();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT dirname, id FROM storagegroup "
                  "WHERE groupname = :NAME AND hostname = :HOSTNAME "
                  "ORDER BY id;");
    query.bindValue(":NAME", m_group);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec() || !query.isActive())
        MythDB::DBError("StorageGroupEditor::doDelete", query);
    else
    {
        bool first = true;
        while (query.next())
        {
            if (first)
            {
                lastValue = query.value(0).toString();
                first = false;
            }
            listbox->addSelection(query.value(0).toString());
        }
    }

    listbox->addSelection(tr("(Add New Directory)"),
        "__CREATE_NEW_STORAGE_DIRECTORY__");

    if (!lastValue.isEmpty())
        listbox->setValue(lastValue);
}

DialogCode StorageGroupEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted)
        open(listbox->getValue());

    return kDialogCodeRejected;
}

MythDialog* StorageGroupEditor::dialogWidget(MythMainWindow* parent,
                                          const char* widgetName)
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(doDelete()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(doDelete()));
    return dialog;
}

/****************************************************************************/

StorageGroupListEditor::StorageGroupListEditor(void) :
    listbox(new ListBoxSetting(this)), lastValue("")
{
    if (gCoreContext->GetSetting("MasterServerIP","master") ==
            gCoreContext->GetSetting("BackendServerIP","me"))
        listbox->setLabel(
            tr("Storage Groups (directories for new recordings)"));
    else
        listbox->setLabel(
            tr("Local Storage Groups (directories for new recordings)"));

    addChild(listbox);
}

void StorageGroupListEditor::open(QString name) 
{
    lastValue = name;

    if (name.left(28) == "__CREATE_NEW_STORAGE_GROUP__")
    {
        if (name.length() > 28)
        {
            name = name.mid(28);
        }
        else
        {
            name = "";
            SGPopupResult result = StorageGroupPopup::showPopup(
                GetMythMainWindow(), 
                tr("Create New Storage Group"),
                tr("Enter group name or press SELECT to enter text via the "
                   "On Screen Keyboard"), name);
            if (result == SGPopup_CANCEL)
                return;
        }
    }

    if (!name.isEmpty())
    {
        StorageGroupEditor sgEditor(name);
        sgEditor.exec();
    }
};

void StorageGroupListEditor::doDelete(void) 
{
    QString name = listbox->getValue();
    if (name.left(28) == "__CREATE_NEW_STORAGE_GROUP__")
        return;

    bool is_master_host = gCoreContext->GetSetting("MasterServerIP","master") ==
                          gCoreContext->GetSetting("BackendServerIP","me");

    QString dispGroup = name;
    if (name == "Default")
        dispGroup = QObject::tr("Default");
    else if (StorageGroup::kSpecialGroups.contains(name))
        dispGroup = QObject::tr(name.toLatin1().constData());

    QString message = tr("Delete '%1' Storage Group?").arg(dispGroup);
    if (is_master_host)
    {
        if (name == "Default")
        {
            message.append("\n" + tr("(from remote hosts)"));
        }
        else
        {
            message.append("\n" + tr("(from all hosts"));
        }
    }

    DialogCode value = MythPopupBox::Show2ButtonPopup(
        GetMythMainWindow(),
        "", message,
        tr("Yes, delete group"),
        tr("No, Don't delete group"), kDialogCodeButton1);

    if (kDialogCodeButton0 == value)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        QString sql = "DELETE FROM storagegroup "
                      "WHERE groupname = :NAME";
        if (is_master_host)
        {
            // From the master host, delete the group completely (versus just
            // local directory list) unless it's the Default group, then just
            // delete remote overrides of the Default group
            if (name == "Default")
                sql.append(" AND hostname != :HOSTNAME");
        }
        else
        {
            // For non-master hosts, delete only the local override of the
            // group directory list
            sql.append(" AND hostname = :HOSTNAME");
        }
        sql.append(';');
        query.prepare(sql);
        query.bindValue(":NAME", name);
        if (!is_master_host || (name == "Default"))
            query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("StorageGroupListEditor::doDelete", query);

        int lastIndex = listbox->getValueIndex(name);
        lastValue = "";
        Load();
        listbox->setValue(lastIndex);
    }

    listbox->setFocus();
}

void StorageGroupListEditor::Load(void)
{
    QStringList names;
    QStringList masterNames;
    bool createAddDefaultButton = false;
    bool createAddSpecialGroupButton[StorageGroup::kSpecialGroups.size()];
    bool isMaster = (gCoreContext->GetSetting("MasterServerIP","master") ==
                     gCoreContext->GetSetting("BackendServerIP","me"));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT distinct groupname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME "
                  "ORDER BY groupname;");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec())
        MythDB::DBError("StorageGroup::Load getting local group names",
                             query);
    else
    {
        while (query.next())
            names << query.value(0).toString();
    }

    query.prepare("SELECT distinct groupname "
                  "FROM storagegroup "
                  "ORDER BY groupname;");
    if (!query.exec())
        MythDB::DBError("StorageGroup::Load getting all group names",
                             query);
    else
    {
        while (query.next())
            masterNames << query.value(0).toString();
    }

    listbox->clearSelections();

    if (isMaster || names.contains("Default"))
    {
        listbox->addSelection(QObject::tr("Default"), "Default");
        lastValue = "Default";
    }
    else
        createAddDefaultButton = true;

    int curGroup = 0;
    QString groupName;
    while (curGroup < StorageGroup::kSpecialGroups.size())
    {
        groupName = StorageGroup::kSpecialGroups[curGroup];
        if (names.contains(groupName))
        {
            listbox->addSelection(
                QObject::tr(groupName.toLatin1().constData()), groupName);
            createAddSpecialGroupButton[curGroup] = false;
        }
        else
            createAddSpecialGroupButton[curGroup] = true;
        curGroup++;
    }

    int curName = 0;
    while (curName < names.size())
    {
        if ((names[curName] != "Default") &&
            (!StorageGroup::kSpecialGroups.contains(names[curName])))
            listbox->addSelection(names[curName]);
        curName++;
    }

    if (createAddDefaultButton)
    {
        listbox->addSelection(tr("(Create %1 group)").arg("Default"),
                              "Default");
        lastValue = "Default";
    }

    curGroup = 0;
    while (curGroup < StorageGroup::kSpecialGroups.size())
    {
        groupName = StorageGroup::kSpecialGroups[curGroup];
        if (createAddSpecialGroupButton[curGroup])
            listbox->addSelection(tr("(Create %1 group)").arg(groupName),
                QString("__CREATE_NEW_STORAGE_GROUP__%1").arg(groupName));
        curGroup++;
    }

    if (isMaster)
        listbox->addSelection(tr("(Create %1 group)").arg("new"),
            "__CREATE_NEW_STORAGE_GROUP__");
    else
    {
        curName = 0;
        while (curName < masterNames.size())
        {
            if ((masterNames[curName] != "Default") &&
                (!StorageGroup::kSpecialGroups.contains(masterNames[curName])) &&
                (!names.contains(masterNames[curName])))
                listbox->addSelection(tr("(Create %1 group)")
                                         .arg(masterNames[curName]),
                    "__CREATE_NEW_STORAGE_GROUP__" + masterNames[curName]);
            curName++;
        }
    }

    listbox->setValue(lastValue);
}

DialogCode StorageGroupListEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted)
        open(listbox->getValue());

    return kDialogCodeRejected;
}

MythDialog* StorageGroupListEditor::dialogWidget(MythMainWindow* parent,
                                          const char* widgetName)
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(doDelete()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(doDelete()));
    return dialog;
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
            VERBOSE(VB_FILE+VB_EXTRA,
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
