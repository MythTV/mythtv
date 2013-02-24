#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QUrl>
#include <QVector>

#include "storagegroupeditor.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mythdate.h"

#define LOC QString("SGE(%1): ").arg(m_groupname)

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
    MythPopupBox *popup = new MythPopupBox(parent, title.toLatin1().constData());
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

    if (gCoreContext->IsMasterHost())
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
                lastValue = dirname;
                first = false;
            }
            listbox->addSelection(dirname);
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
    if (gCoreContext->IsMasterHost())
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

    bool is_master_host = gCoreContext->IsMasterHost();

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
            message = tr("Delete '%1' Storage Group?\n(from remote hosts)").arg(dispGroup);
        }
        else
        {
            message = tr("Delete '%1' Storage Group?\n(from all hosts)").arg(dispGroup);
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
    QVector< bool > createAddSpecialGroupButton( StorageGroup::kSpecialGroups.size() );
    bool isMaster = gCoreContext->IsMasterHost();

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

/* vim: set expandtab tabstop=4 shiftwidth=4: */
