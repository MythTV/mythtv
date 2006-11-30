#include <qdir.h>
#include <qfile.h>
#include <qregexp.h>

#include "storagegroup.h"
#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/util.h"

#define LOC QString("SG(%1): ").arg(m_groupname)
#define LOC_WARN QString("SG(%1) Warning: ").arg(m_groupname)
#define LOC_ERR QString("SG(%1) Error: ").arg(m_groupname)

#define DEFAULTSTORAGEDIR "/mnt/store"

/****************************************************************************/

StorageGroup::StorageGroup(const QString group, const QString hostname) :
    m_groupname(group),
    m_hostname(hostname)
{
    m_dirlist.clear();

    Init(m_groupname, m_hostname);
}

void StorageGroup::Init(const QString group, const QString hostname)
{
    QString m_dirname;
    MSqlQuery query(MSqlQuery::InitCon());

    m_groupname = group;
    m_hostname  = hostname;
    m_dirlist.clear();

    QString sql = "SELECT DISTINCT dirname "
                  "FROM storagegroup ";

    if (m_groupname != "")
    {
        sql.append("WHERE groupname = :GROUP");
        if (m_hostname != "")
            sql.append(" AND hostname = :HOSTNAME");
    }

    query.prepare(sql);
    query.bindValue(":GROUP", m_groupname);
    query.bindValue(":HOSTNAME", m_hostname);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("StorageGroup::StorageGroup()", query);
    else if (query.size() < 1)
    {
        if (group != "Default")
        {
            VERBOSE(VB_FILE, LOC +
                    QString("Unable to find storage group '%1', trying "
                            "'Default' group!").arg(m_groupname));
            Init("Default", m_hostname);
            return;
        }
        else if (m_hostname != "")
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
        while(query.next())
        {
            m_dirname = query.value(0).toString();
            m_dirname.replace(QRegExp("^\\s*"), "");
            m_dirname.replace(QRegExp("\\s*$"), "");
            if (m_dirname.right(1) == "/")
                m_dirname.remove(m_dirname.length() - 1, 1);
            m_dirlist << m_dirname;
        }
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
            tmpDir = DEFAULTSTORAGEDIR;
            msg += QString("Using hardcoded default value of '%1'")
                           .arg(DEFAULTSTORAGEDIR);
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
    
    if (recDir != "")
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

    unsigned int curDir = 0;
    while (curDir < m_dirlist.size())
    {
        QString testFile = m_dirlist[curDir] + "/" + filename;
        VERBOSE(VB_FILE, LOC + QString("FindRecordingDir: Checking '%1'")
                                       .arg(m_dirlist[curDir]));
        checkFile.setName(testFile);
        if (checkFile.exists())
            return m_dirlist[curDir];

        curDir++;
    }

    if (result == "")
    {
        if (m_groupname == "")
        {
            // Not found in any dir, so try RecordFilePrefix if it exists
            QString tmpFile =
                gContext->GetSetting("RecordFilePrefix") + "/" + filename;
            checkFile.setName(tmpFile);
            if (checkFile.exists())
                result = tmpFile;
        }
        else if (m_groupname != "Default")
        {
            // Not found in current group so try Default
            StorageGroup sgroup("Default");
            QString tmpFile = sgroup.FindRecordingDir(filename);
            if (tmpFile != "")
                result = tmpFile;
        }
        else
        {
            // Not found in Default so try any dir
            StorageGroup sgroup;
            QString tmpFile = sgroup.FindRecordingDir(filename);
            if (tmpFile != "")
                result = tmpFile;
        }
    }

    return result;
}

QString StorageGroup::FindNextDirMostFree(void)
{
    QString nextDir = DEFAULTSTORAGEDIR;
    long long nextDirFree = 0;
    long long thisDirTotal;
    long long thisDirUsed;
    long long thisDirFree;

    VERBOSE(VB_FILE, LOC + QString("FindNextDirMostFree: Starting'"));

    if (m_dirlist.size())
        nextDir = m_dirlist[0];

    QDir checkDir("");
    unsigned int curDir = 0;
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
    return nextDir;
}

void StorageGroup::CheckAllStorageGroupDirs(void)
{
    QString m_groupname;
    QString m_dirname;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT groupname, dirname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("StorageGroup::CheckAllStorageGroupDirs()", query);
        return;
    }

    VERBOSE(VB_FILE, LOC + "CheckAllStorageGroupDirs(): Checking All Storage "
            "Group directories");

    QFile testFile("");
    QDir testDir("");
    while (query.next())
    {
        m_groupname = query.value(0).toString();
        m_dirname = query.value(1).toString();

        m_dirname.replace(QRegExp("^\\s*"), "");
        m_dirname.replace(QRegExp("\\s*$"), "");

        VERBOSE(VB_FILE, LOC +
                QString("Checking directory '%1' in group '%2'.")
                        .arg(m_dirname).arg(m_groupname));

        testDir.setPath(m_dirname);
        if (!testDir.exists())
        {
            VERBOSE(VB_FILE, LOC_WARN + 
                    QString("Group '%1' references directory '%2' but this "
                            "directory does not exist.  This directory "
                            "will not be used on this server.")
                            .arg(m_groupname).arg(m_dirname));
        }
        else
        {
            testFile.setName(m_dirname + "/.test");
            if (testFile.open(IO_WriteOnly))
                testFile.remove();
            else
                VERBOSE(VB_IMPORTANT,
                        LOC_ERR +
                        QString("Group '%1' wants to use directory '%2', but "
                                "this directory is not writeable.")
                                .arg(m_groupname).arg(m_dirname));
        }
    }
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
    MythPopupBox popup(parent, title);
    popup.addLabel(message);

    MythLineEdit* textEdit = new MythLineEdit(&popup, "chooseEdit");
    textEdit->setText(text);
    popup.addWidget(textEdit);

    popup.addButton(QObject::tr("OK"));
    popup.addButton(QObject::tr("Cancel"));

    textEdit->setFocus();

    if (popup.ExecPopup() == 0)
    {
        text = textEdit->text();
        return SGPopup_OK;
    }

    return SGPopup_CANCEL;
}

/****************************************************************************/

StorageGroupEditor::StorageGroupEditor(QString group) :
    m_group(group), listbox(new ListBoxSetting(this)), lastValue("")
{
    QString dispGroup = group;

    if (group == "Default")
        dispGroup = QObject::tr("Default");
    else if (group == "LiveTV")
        dispGroup = QObject::tr("LiveTV");

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

    if (name.isEmpty())
    {
        SGPopupResult result = StorageGroupPopup::showPopup(
            gContext->GetMainWindow(), 
            tr("Add Storage Group Directory"),
            tr("Enter directory name or press SELECT to enter text via the "
               "On Screen Keyboard"), name);
        if (result == SGPopup_CANCEL)
            return;

        if (name.right(1) != "/")
            name.append("/");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO storagegroup (groupname, hostname, dirname) "
                      "VALUES (:NAME, :HOSTNAME, :DIRNAME);");
        query.bindValue(":NAME", m_group.utf8());
        query.bindValue(":DIRNAME", name.utf8());
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        if (!query.exec())
            MythContext::DBError("StorageGroupEditor::open", query);
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
        query.bindValue(":NAME", m_group.utf8());
        query.bindValue(":DIRNAME", lastValue.utf8());
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        if (!query.exec())
            MythContext::DBError("StorageGroupEditor::open", query);

        query.prepare("INSERT INTO storagegroup (groupname, hostname, dirname) "
                      "VALUES (:NAME, :HOSTNAME, :DIRNAME);");
        query.bindValue(":NAME", m_group.utf8());
        query.bindValue(":DIRNAME", name.utf8());
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        if (!query.exec())
            MythContext::DBError("StorageGroupEditor::open", query);
        else
            lastValue = name;
    }
};

void StorageGroupEditor::doDelete(void) 
{
    QString name = listbox->getValue();
    if (name.isEmpty())
        return;

    QString message =
        tr("Remove '%1'\nDirectory From Storage Group?").arg(name);

    int value =
        MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "", message,
                                       tr("Yes, remove directory"),
                                       tr("No, Don't remove directory"), 2);

    if (value == 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM storagegroup "
                      "WHERE groupname = :NAME "
                          "AND dirname = :DIRNAME "
                          "AND hostname = :HOSTNAME;");
        query.bindValue(":NAME", m_group.utf8());
        query.bindValue(":DIRNAME", name.utf8());
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        if (!query.exec())
            MythContext::DBError("StorageGroupEditor::doDelete", query);

        int lastIndex = listbox->getValueIndex(name);
        lastValue = "";
        load();
        listbox->setValue(lastIndex);
    }

    listbox->setFocus();
}

void StorageGroupEditor::load(void) {
    listbox->clearSelections();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT dirname, id FROM storagegroup "
                  "WHERE groupname = :NAME AND hostname = :HOSTNAME "
                  "ORDER BY id;");
    query.bindValue(":NAME", m_group.utf8());
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query.exec() || !query.isActive())
        MythContext::DBError("StorageGroupEditor::doDelete", query);
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

    listbox->addSelection(tr("(Add New Directory)"), "");

    listbox->setValue(lastValue);
}

int StorageGroupEditor::exec() {
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        open(listbox->getValue());

    return QDialog::Rejected;
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

    if (name.isEmpty())
    {
        SGPopupResult result = StorageGroupPopup::showPopup(
            gContext->GetMainWindow(), 
            tr("Create New Storage Group"),
            tr("Enter group name or press SELECT to enter text via the "
               "On Screen Keyboard"), name);
        if (result == SGPopup_CANCEL)
            return;
    }

    if (name != "")
    {
        StorageGroupEditor sgEditor(name);
        sgEditor.exec();
    }
};

void StorageGroupListEditor::doDelete(void) 
{
    QString name = listbox->getValue();
    if (name.isEmpty())
        return;

    if ((gContext->GetSetting("MasterServerIP","master") ==
         gContext->GetSetting("BackendServerIP","me")) &&
        (name == "Default"))
        return;

    QString dispGroup = name;
    if (name == "Default")
        dispGroup = QObject::tr("Default");
    else if (name == "LiveTV")
        dispGroup = QObject::tr("LiveTV");

    QString message = tr("Delete '%1' Storage Group?").arg(dispGroup);

    int value = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                               "", message,
                                               tr("Yes, delete group"),
                                               tr("No, Don't delete group"), 2);

    if (value == 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM storagegroup "
                      "WHERE groupname = :NAME AND hostname = :HOSTNAME;");
        query.bindValue(":NAME", name.utf8());
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        if (!query.exec())
            MythContext::DBError("StorageGroupListEditor::doDelete", query);

        int lastIndex = listbox->getValueIndex(name);
        lastValue = "";
        load();
        listbox->setValue(lastIndex);
    }

    listbox->setFocus();
}

void StorageGroupListEditor::load(void)
{
    QStringList names;
    QStringList masterNames;
    bool createAddDefaultButton = false;
    bool createAddLiveTVButton = false;
    bool isMaster = (gContext->GetSetting("MasterServerIP","master") ==
                     gContext->GetSetting("BackendServerIP","me"));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT distinct groupname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME "
                  "ORDER BY groupname;");
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query.exec())
        MythContext::DBError("StorageGroup::load getting local group names",
                             query);
    else
    {
        while (query.next())
            names << QString::fromUtf8(query.value(0).toString());
    }

    query.prepare("SELECT distinct groupname "
                  "FROM storagegroup "
                  "ORDER BY groupname;");
    if (!query.exec())
        MythContext::DBError("StorageGroup::load getting all group names",
                             query);
    else
    {
        while (query.next())
            masterNames << QString::fromUtf8(query.value(0).toString());
    }

    listbox->clearSelections();

    if (isMaster || names.contains("Default"))
    {
        listbox->addSelection(QObject::tr("Default"), "Default");
        lastValue = "Default";
    }
    else
        createAddDefaultButton = true;

    if (names.contains("LiveTV"))
        listbox->addSelection(QObject::tr("LiveTV"), "LiveTV");
    else
        createAddLiveTVButton = true;

    unsigned int curName = 0;
    while (curName < names.size())
    {
        if ((names[curName] != "Default") &&
            (names[curName] != "LiveTV"))
            listbox->addSelection(names[curName]);
        curName++;
    }

    if (createAddDefaultButton)
    {
        listbox->addSelection(tr("(Create %1 group)").arg("Default"),
                              "Default");
        lastValue = "Default";
    }

    if (createAddLiveTVButton)
        listbox->addSelection(tr("(Create %1 group)").arg("LiveTV"), "LiveTV");

    if (isMaster)
        listbox->addSelection(tr("(Create %1 group)").arg("new"), "");
    else
    {
        curName = 0;
        while (curName < masterNames.size())
        {
            if ((masterNames[curName] != "Default") &&
                (masterNames[curName] != "LiveTV") &&
                (!names.contains(masterNames[curName])))
                listbox->addSelection(tr("(Create %1 group)")
                                         .arg(masterNames[curName]),
                             masterNames[curName]);
            curName++;
        }
    }

    listbox->setValue(lastValue);
}

int StorageGroupListEditor::exec() {
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        open(listbox->getValue());

    return QDialog::Rejected;
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
