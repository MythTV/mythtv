// Qt headers
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QVector>
#include <utility>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythui/mythuifilebrowser.h"

#include "storagegroupeditor.h"

#define LOC QString("SGE(%1): ").arg(m_groupname)

/****************************************************************************/

StorageGroupEditor::StorageGroupEditor(QString group) :
    m_group(std::move(group))
{
    SetLabel();
}

void StorageGroupEditor::SetLabel()
{
    QString dispGroup = m_group;

    if (m_group == "Default")
        dispGroup = tr("Default", "Default storage group");
    else if (StorageGroup::kSpecialGroups.contains(m_group))
        dispGroup = QCoreApplication::translate("(StorageGroups)",
                                                m_group.toLatin1().constData());

    if (gCoreContext->IsMasterHost())
    {
        setLabel(tr("'%1' Storage Group Directories").arg(dispGroup));
    }
    else
    {
        setLabel(tr("Local '%1' Storage Group Directories").arg(dispGroup));
    }
}

bool StorageGroupEditor::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled =
        GetMythMainWindow()->TranslateKeyPress("Global", e, actions);
    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];

        if (action == "DELETE")
        {
            handled = true;
            ShowDeleteDialog();
        }
    }
    if (handled)
        return handled;
    return GroupSetting::keyPressEvent(e);
}

bool StorageGroupEditor::canDelete(void)
{
    return true;
}

void StorageGroupEditor::ShowDeleteDialog()
{
    bool is_master_host = gCoreContext->IsMasterHost();

    QString dispGroup = m_group;
    if (m_group == "Default")
        dispGroup = tr("Default", "Default storage group");
    else if (StorageGroup::kSpecialGroups.contains(m_group))
        dispGroup = QCoreApplication::translate("(StorageGroups)",
                                                m_group.toLatin1().constData());

    QString message = tr("Delete '%1' Storage Group?").arg(dispGroup);
    if (is_master_host)
    {
        if (m_group == "Default")
        {
            message = tr("Delete '%1' Storage Group?\n(from remote hosts)").arg(dispGroup);
        }
        else
        {
            message = tr("Delete '%1' Storage Group?\n(from all hosts)").arg(dispGroup);
        }
    }

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *confirmDelete = new MythConfirmationDialog(popupStack, message, true);
    if (confirmDelete->Create())
    {
        connect(confirmDelete, &MythConfirmationDialog::haveResult,
                this, &StorageGroupEditor::DoDeleteSlot);
        popupStack->AddScreen(confirmDelete);
    }
    else
    {
        delete confirmDelete;
    }
}

void StorageGroupEditor::DoDeleteSlot(bool doDelete)
{
    if (doDelete)
    {
        bool is_master_host = gCoreContext->IsMasterHost();
        MSqlQuery query(MSqlQuery::InitCon());
        QString sql = "DELETE FROM storagegroup "
                      "WHERE groupname = :NAME";
        if (is_master_host)
        {
            // From the master host, delete the group completely (versus just
            // local directory list) unless it's the Default group, then just
            // delete remote overrides of the Default group
            if (m_group == "Default")
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
        query.bindValue(":NAME", m_group);
        if (!is_master_host || (m_group == "Default"))
            query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("StorageGroupListEditor::doDelete", query);
    }
}

StorageGroupDirStorage::StorageGroupDirStorage(StorageUser *_user,
                                               int id,
                                               QString group) :
    SimpleDBStorage(_user, "storagegroup", "dirname"),
    m_id(id),
    m_group(std::move(group))
{
}

QString StorageGroupDirStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString dirnameTag(":SETDIRNAME");

    QString query("dirname = " + dirnameTag);

    bindings.insert(dirnameTag, m_user->GetDBValue());

    return query;
}

QString StorageGroupDirStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString hostnameTag(":WHEREHOST");
    QString idTag(":WHEREID");

    QString query("hostname = " + hostnameTag + " AND id = " + idTag);

    bindings.insert(hostnameTag, gCoreContext->GetHostName());
    bindings.insert(idTag, m_id);

    return query;
}

StorageGroupDirSetting::StorageGroupDirSetting(int id, const QString &group) :
    MythUIFileBrowserSetting(new StorageGroupDirStorage(this, id, group)),
    m_id(id), m_group(group)
{
    SetTypeFilter(QDir::AllDirs | QDir::Drives);
}

StorageGroupDirSetting::~StorageGroupDirSetting()
{
    delete GetStorage();
}

bool StorageGroupDirSetting::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled =
        GetMythMainWindow()->TranslateKeyPress("Global", event, actions);
    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];

        if (action == "DELETE")
        {
            handled = true;
            ShowDeleteDialog();
        }
    }
    return handled;
}

void StorageGroupDirSetting::ShowDeleteDialog()
{
    QString message =
        tr("Remove '%1'\nDirectory From Storage Group?").arg(getValue());
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *confirmDelete = new MythConfirmationDialog(popupStack, message, true);

    if (confirmDelete->Create())
    {
        connect(confirmDelete, &MythConfirmationDialog::haveResult,
                this, &StorageGroupDirSetting::DoDeleteSlot);
        popupStack->AddScreen(confirmDelete);
    }
    else
    {
        delete confirmDelete;
    }
}

void StorageGroupDirSetting::DoDeleteSlot(bool doDelete)
{
    if (doDelete)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM storagegroup "
                      "WHERE groupname = :NAME "
                          "AND dirname = :DIRNAME "
                          "AND hostname = :HOSTNAME;");
        query.bindValue(":NAME", m_group);
        query.bindValue(":DIRNAME", getValue());
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
        if (query.exec())
            getParent()->removeChild(this);
        else
            MythDB::DBError("StorageGroupEditor::DoDeleteSlot", query);
    }
}

void StorageGroupEditor::Load(void)
{
    clearSettings();

    auto *button = new ButtonStandardSetting(tr("(Add New Directory)"));
    connect(button, &ButtonStandardSetting::clicked, this, &StorageGroupEditor::ShowFileBrowser);
    addChild(button);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT dirname, id FROM storagegroup "
                      "WHERE groupname = :NAME AND hostname = :HOSTNAME "
                      "ORDER BY id;");
        query.bindValue(":NAME", m_group);
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
        if (!query.exec() || !query.isActive())
            MythDB::DBError("StorageGroupEditor::Load", query);
        else
        {
            bool first = true;
            QString dirname;
            while (query.next())
            {
                /* The storagegroup.dirname column uses utf8_bin collation, so Qt
                 * uses QString::fromAscii() for toString(). Explicitly convert the
                 * value using QString::fromUtf8() to prevent corruption. */
                dirname = QString::fromUtf8(query.value(0)
                                            .toByteArray().constData());
                if (first)
                {
                    first = false;
                }
                addChild(new StorageGroupDirSetting(query.value(1).toInt(),
                                                    m_group));
            }

            if (!first)
            {
                SetLabel();
            }
    }

    StandardSetting::Load();
}

void StorageGroupEditor::ShowFileBrowser()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *settingdialog = new MythUIFileBrowser(popupStack, "");
    settingdialog->SetTypeFilter(QDir::AllDirs | QDir::Drives);

    if (settingdialog->Create())
    {
        settingdialog->SetReturnEvent(this, "editsetting");
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void StorageGroupEditor::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);
        if (dce == nullptr)
            return;
        QString resultid  = dce->GetId();

        if (resultid == "editsetting")
        {
            QString dirname = dce->GetResultText();

            if (dirname.isEmpty())
                return;

            if (!dirname.endsWith("/"))
                dirname.append("/");

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("INSERT INTO storagegroup (groupname, hostname, dirname) "
                          "VALUES (:NAME, :HOSTNAME, :DIRNAME);");
            query.bindValue(":NAME", m_group);
            query.bindValue(":DIRNAME", dirname);
            query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
            if (!query.exec())
                MythDB::DBError("StorageGroupEditor::customEvent", query);
            else
            {
                SetLabel();
                StandardSetting *directory =
                    new StorageGroupDirSetting(query.lastInsertId().toInt(),
                                               m_group);
                directory->setValue(dirname);
                addChild(directory);
                emit settingsChanged(this);
            }
        }
    }
}


/****************************************************************************/

StorageGroupListEditor::StorageGroupListEditor(void)
{
    if (gCoreContext->IsMasterHost())
        setLabel(tr("Storage Groups (directories for new recordings)"));
    else
        setLabel(tr("Local Storage Groups (directories for new recordings)"));
}

void StorageGroupListEditor::AddSelection(const QString &label,
                                          const QString &value)
{
    auto *button = new StorageGroupEditor(value);
    button->setLabel(label);
    addChild(button);
}

void StorageGroupListEditor::Load(void)
{
    QStringList names;
    QStringList masterNames;
    bool createAddDefaultButton = false;
    QVector< bool > createAddSpecialGroupButton( StorageGroup::kSpecialGroups.size() );
    bool isMaster = gCoreContext->IsMasterHost();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT distinct groupname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME "
                  "ORDER BY groupname;");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec())
    {
        MythDB::DBError("StorageGroup::Load getting local group names",
                             query);
    }
    else
    {
        while (query.next())
            names << query.value(0).toString();
    }

    query.prepare("SELECT distinct groupname "
                  "FROM storagegroup "
                  "ORDER BY groupname;");
    if (!query.exec())
    {
        MythDB::DBError("StorageGroup::Load getting all group names",
                             query);
    }
    else
    {
        while (query.next())
            masterNames << query.value(0).toString();
    }

    clearSettings();

    if (isMaster || names.contains("Default"))
    {
        AddSelection(tr("Default", "Default storage group"),
                     "Default");
    }
    else
    {
        createAddDefaultButton = true;
    }

    int curGroup = 0;
    QString groupName;
    while (curGroup < StorageGroup::kSpecialGroups.size())
    {
        groupName = StorageGroup::kSpecialGroups[curGroup];
        if (names.contains(groupName))
        {
            addChild(new StorageGroupEditor(groupName));
            createAddSpecialGroupButton[curGroup] = false;
        }
        else
        {
            createAddSpecialGroupButton[curGroup] = true;
        }
        curGroup++;
    }

    int curName = 0;
    while (curName < names.size())
    {
        if ((names[curName] != "Default") &&
            (!StorageGroup::kSpecialGroups.contains(names[curName])))
                addChild(new StorageGroupEditor(names[curName]));
        curName++;
    }

    if (createAddDefaultButton)
    {
        AddSelection(tr("(Create default group)"), "Default");
    }

    curGroup = 0;
    while (curGroup < StorageGroup::kSpecialGroups.size())
    {
        groupName = StorageGroup::kSpecialGroups[curGroup];
        if (createAddSpecialGroupButton[curGroup])
        {
            AddSelection(tr("(Create %1 group)")
                .arg(QCoreApplication::translate("(StorageGroups)",
                    groupName.toLatin1().constData())),
                groupName);
        }
        curGroup++;
    }

    if (isMaster)
    {
        auto *newGroup = new ButtonStandardSetting(tr("(Create new group)"));
        connect(newGroup, &ButtonStandardSetting::clicked, this, &StorageGroupListEditor::ShowNewGroupDialog);
        addChild(newGroup);
    }
    else
    {
        curName = 0;
        while (curName < masterNames.size())
        {
            if ((masterNames[curName] != "Default") &&
                (!StorageGroup::kSpecialGroups.contains(masterNames[curName])) &&
                (!names.contains(masterNames[curName])))
            {
                AddSelection(tr("(Create %1 group)")
                                .arg(masterNames[curName]),
                             masterNames[curName]);
            }
            curName++;
        }
    }

    StandardSetting::Load();
}


void StorageGroupListEditor::ShowNewGroupDialog() const
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *settingdialog = new MythTextInputDialog(popupStack,
                                tr("Enter the name of the new storage group"));

    if (settingdialog->Create())
    {
        connect(settingdialog, &MythTextInputDialog::haveResult,
                this, &StorageGroupListEditor::CreateNewGroup);
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void StorageGroupListEditor::CreateNewGroup(const QString& name)
{
    auto *button = new StorageGroupEditor(name);
    button->setLabel(name + tr(" Storage Group Directories"));
    button->Load();
    addChild(button);
    emit settingsChanged(this);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
