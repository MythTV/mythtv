#include <QDir>
#include <QFile>
#include <QRegExp>

#include "storagegroup.h"
#include "mythcontext.h"
#include "libmythdb/mythdb.h"
#include "libmythdb/mythverbose.h"
#include "util.h"

#define LOC QString("SG(%1): ").arg(m_groupname)
#define LOC_WARN QString("SG(%1) Warning: ").arg(m_groupname)
#define LOC_ERR QString("SG(%1) Error: ").arg(m_groupname)

const char *StorageGroup::kDefaultStorageDir = "/mnt/store";

const QStringList StorageGroup::kSpecialGroups = QStringList()
    << "LiveTV"
//    << "Thumbnails"
    << "DB Backups"
    ;

/****************************************************************************/

/** \brief StorageGroup constructor.
 *  \param group    storage group to search, blank will search all groups.
 *  \param hostname hostname where to search, blank will search all hosts'
 *                  directories, but only in local directory structure.
 *                  This is parameter is ignored if group is an empty string.
 */
StorageGroup::StorageGroup(const QString group, const QString hostname) :
    m_groupname(group), m_hostname(hostname)
{
    m_groupname.detach();
    m_hostname.detach();
    m_dirlist.clear();

    Init(m_groupname, m_hostname);
}

void StorageGroup::Init(const QString group, const QString hostname)
{
    QString dirname;
    MSqlQuery query(MSqlQuery::InitCon());

    m_groupname = group;    m_groupname.detach();
    m_hostname  = hostname; m_hostname.detach();
    m_dirlist.clear();

    QString sql = "SELECT DISTINCT dirname "
                  "FROM storagegroup ";

    if (!m_groupname.isEmpty())
    {
        sql.append("WHERE groupname = :GROUP");
        if (!m_hostname.isEmpty())
            sql.append(" AND hostname = :HOSTNAME");
    }

    query.prepare(sql);
    if (!m_groupname.isEmpty())
    {
        query.bindValue(":GROUP", m_groupname);
        if (!m_hostname.isEmpty())
            query.bindValue(":HOSTNAME", m_hostname);
    }

    if (!query.exec() || !query.isActive())
        MythDB::DBError("StorageGroup::StorageGroup()", query);
    else if (!query.next())
    {
        if (group != "Default")
        {
            VERBOSE(VB_FILE, LOC +
                    QString("Unable to find storage group '%1', trying "
                            "'Default' group!").arg(m_groupname));
            Init("Default", m_hostname);
            return;
        }
        else if (!m_hostname.isEmpty())
        {
            VERBOSE(VB_FILE, LOC +
                    QString("Unable to find any directories for the local "
                            "Default storage group, trying directories in all "
                            "Default groups!").arg(m_groupname));
            Init("Default", "");
            return;
        }
    }
    else
    {
        do
        {
            dirname = query.value(0).toString();
            dirname.replace(QRegExp("^\\s*"), "");
            dirname.replace(QRegExp("\\s*$"), "");
            if (dirname.right(1) == "/")
                dirname.remove(dirname.length() - 1, 1);
            m_dirlist << dirname;
        }
        while (query.next());
    }

    if (!m_dirlist.size())
    {
        QString msg = "Directory value for Default Storage Group is empty.  ";
        QString tmpDir = gContext->GetSetting("RecordFilePrefix");
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
    QFile checkFile("");

    int curDir = 0;
    while (curDir < m_dirlist.size())
    {
        QString testFile = m_dirlist[curDir] + "/" + filename;
        VERBOSE(VB_FILE, LOC + QString("FindRecordingDir: Checking '%1'")
                .arg(m_dirlist[curDir]));
        checkFile.setFileName(testFile);
        if (checkFile.exists())
        {
            QString tmp = m_dirlist[curDir];
            tmp.detach();
            return tmp;
        }

        curDir++;
    }

    if (m_groupname.isEmpty())
    {
        // Not found in any dir, so try RecordFilePrefix if it exists
        QString tmpFile =
            gContext->GetSetting("RecordFilePrefix") + "/" + filename;
        checkFile.setFileName(tmpFile);
        if (checkFile.exists())
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
    QString nextDir = kDefaultStorageDir;
    long long nextDirFree = 0;
    long long thisDirTotal;
    long long thisDirUsed;
    long long thisDirFree;

    VERBOSE(VB_FILE, LOC + QString("FindNextDirMostFree: Starting'"));

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
                            .arg(nextDir));
            curDir++;
            continue;
        }

        thisDirFree = getDiskSpace(m_dirlist[curDir], thisDirTotal,
                                   thisDirUsed);
        VERBOSE(VB_FILE, LOC +
                QString("FindNextDirMostFree: '%1' has %2 KiB free")
                        .arg(m_dirlist[curDir])
                        .arg(longLongToString(thisDirFree)));

        if (thisDirFree > nextDirFree)
        {
            nextDir     = m_dirlist[curDir];
            nextDirFree = thisDirFree;
        }
        curDir++;
    }

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
    query.bindValue(":HOSTNAME", gContext->GetHostName());
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

    if (gContext->GetSetting("MasterServerIP","master") ==
            gContext->GetSetting("BackendServerIP","me"))
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
            gContext->GetMainWindow(), 
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
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("StorageGroupEditor::open", query);
        else
            lastValue = name;
    } else {
        SGPopupResult result = StorageGroupPopup::showPopup(
            gContext->GetMainWindow(), 
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
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("StorageGroupEditor::open", query);

        query.prepare("INSERT INTO storagegroup (groupname, hostname, dirname) "
                      "VALUES (:NAME, :HOSTNAME, :DIRNAME);");
        query.bindValue(":NAME", m_group);
        query.bindValue(":DIRNAME", name);
        query.bindValue(":HOSTNAME", gContext->GetHostName());
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
        gContext->GetMainWindow(), "", message,
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
        query.bindValue(":HOSTNAME", gContext->GetHostName());
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
    query.bindValue(":HOSTNAME", gContext->GetHostName());
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
    if (gContext->GetSetting("MasterServerIP","master") ==
            gContext->GetSetting("BackendServerIP","me"))
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
                gContext->GetMainWindow(), 
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

    if ((gContext->GetSetting("MasterServerIP","master") ==
         gContext->GetSetting("BackendServerIP","me")) &&
        (name == "Default"))
        return;

    QString dispGroup = name;
    if (name == "Default")
        dispGroup = QObject::tr("Default");
    else if (StorageGroup::kSpecialGroups.contains(name))
        dispGroup = QObject::tr(name.toLatin1().constData());

    QString message = tr("Delete '%1' Storage Group?").arg(dispGroup);

    DialogCode value = MythPopupBox::Show2ButtonPopup(
        gContext->GetMainWindow(),
        "", message,
        tr("Yes, delete group"),
        tr("No, Don't delete group"), kDialogCodeButton1);

    if (kDialogCodeButton0 == value)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM storagegroup "
                      "WHERE groupname = :NAME AND hostname = :HOSTNAME;");
        query.bindValue(":NAME", name);
        query.bindValue(":HOSTNAME", gContext->GetHostName());
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
    bool isMaster = (gContext->GetSetting("MasterServerIP","master") ==
                     gContext->GetSetting("BackendServerIP","me"));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT distinct groupname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME "
                  "ORDER BY groupname;");
    query.bindValue(":HOSTNAME", gContext->GetHostName());
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

/* vim: set expandtab tabstop=4 shiftwidth=4: */
