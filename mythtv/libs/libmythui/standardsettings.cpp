#include "standardsettings.h"
#include <QApplication>
#include <QCoreApplication>
#include <QThread>
#include <utility>

#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuifilebrowser.h"
#include "libmythui/mythuispinbox.h"
#include "libmythui/mythuitext.h"

void MythUIButtonListItemSetting::ShouldUpdate(StandardSetting *setting)
{
    setting->updateButton(this);
}

StandardSetting::~StandardSetting()
{
    QList<StandardSetting *>::const_iterator i;
    for (i = m_children.constBegin(); i != m_children.constEnd(); ++i)
        delete *i;
    m_children.clear();

    QMap<QString, QList<StandardSetting *> >::const_iterator iMap;
    for (iMap = m_targets.constBegin(); iMap != m_targets.constEnd(); ++iMap)
    {
        for (i = (*iMap).constBegin(); i != (*iMap).constEnd(); ++i)
            delete *i;
    }
    m_targets.clear();
}

MythUIButtonListItem * StandardSetting::createButton(MythUIButtonList * list)
{
    auto *item = new MythUIButtonListItemSetting(list, m_label);
    item->SetData(QVariant::fromValue(this));
    connect(this, &StandardSetting::ShouldRedraw,
            item, &MythUIButtonListItemSetting::ShouldUpdate);
    updateButton(item);
    return item;
}

void StandardSetting::setEnabled(bool enabled)
{
    m_enabled = enabled;
    emit ShouldRedraw(this);
}

void StandardSetting::setReadOnly(bool readonly)
{
    m_readonly = readonly;
    emit ShouldRedraw(this);
}

void StandardSetting::setVisible(bool visible)
{
    m_visible = visible;
    emit settingsChanged(this);
}

void StandardSetting::setParent(StandardSetting *parent)
{
    m_parent = parent;
}

void StandardSetting::addChild(StandardSetting *child)
{
    if (!child)
        return;

    m_children.append(child);
    child->setParent(this);
}

void StandardSetting::removeChild(StandardSetting *child)
{
    m_children.removeAll(child);
    emit settingsChanged(this);
}

bool StandardSetting::keyPressEvent(QKeyEvent * /*e*/)
{
    return false;
}

/**
 * This method is called whenever the UI need to reflect a change
 * Reimplement this If you widget need a custom look
 * \param item is the associated MythUIButtonListItem to be updated
 */
void StandardSetting::updateButton(MythUIButtonListItem *item)
{
    item->setVisible(m_visible);
    item->DisplayState("standard", "widgettype");
    item->setEnabled(isEnabled());
    item->SetText(m_label);
    item->SetText(m_settingValue, "value");
    item->SetText(getHelpText(), "description");
    item->setDrawArrow(haveSubSettings());
}

void StandardSetting::addTargetedChildren(const QString &value,
                              std::initializer_list<StandardSetting *> settings)
{
    m_targets[value].reserve(settings.size());
    for (auto *setting : std::as_const(settings))
    {
        m_targets[value].append(setting);
        setting->setParent(this);
    }
}
void StandardSetting::addTargetedChild(const QString &value,
                                       StandardSetting * setting)
{
    m_targets[value].append(setting);
    setting->setParent(this);
}

void StandardSetting::removeTargetedChild(const QString &value,
                                          StandardSetting *child)
{
    if (m_targets.contains(value))
    {
        m_targets[value].removeAll(child);
        delete child;
    }
}

void StandardSetting::clearTargetedSettings(const QString &value)
{
    if (m_targets.contains(value))
    {
        for (auto *setting : std::as_const(m_targets[value]))
        {
            delete setting;
        }
        m_targets[value].clear();
    }
}

QList<StandardSetting *> *StandardSetting::getSubSettings()
{
    if (m_targets.contains(m_settingValue) &&
        !m_targets[m_settingValue].empty())
        return &m_targets[m_settingValue];
    return &m_children;
}

bool StandardSetting::haveSubSettings()
{
    QList<StandardSetting *> *subSettings = getSubSettings();
    return subSettings && !subSettings->empty();
}

void StandardSetting::clearSettings()
{
    m_children.clear();
}

void StandardSetting::setValue(int newValue)
{
    setValue(QString::number(newValue));
}

void StandardSetting::setValue(const QString &newValue)
{
    if (m_settingValue != newValue)
    {
        m_settingValue = newValue;
        m_haveChanged = true;

        emit valueChanged(newValue);
        emit valueChanged(this);
    }
    emit ShouldRedraw(this);
}

/**
 * Return true if the setting have changed or any of its children
 */
bool StandardSetting::haveChanged()
{
    if (m_haveChanged)
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Setting '%1' changed to %2").arg(getLabel(), getValue()));
        return true;
    }

    //we check only the relevant children
    QList<StandardSetting *> *children = getSubSettings();
    if (!children)
        return false;

    QList<StandardSetting *>::const_iterator i;
    bool haveChanged = false;
    for (i = children->constBegin(); !haveChanged && i != children->constEnd();
         ++i)
        haveChanged = (*i)->haveChanged();

    return haveChanged;
}

void StandardSetting::setChanged(bool changed)
{
    m_haveChanged = changed;
}

void StandardSetting::Load(void)
{
    if (m_storage)
        m_storage->Load();

    m_haveChanged = false;

    QList<StandardSetting *>::const_iterator i;
    for (i = m_children.constBegin(); i != m_children.constEnd(); ++i)
        (*i)->Load();

    QMap<QString, QList<StandardSetting *> >::const_iterator iMap;
    for (iMap = m_targets.constBegin(); iMap != m_targets.constEnd(); ++iMap)
    {
        for (i = (*iMap).constBegin(); i != (*iMap).constEnd(); ++i)
            (*i)->Load();
    }
}

void StandardSetting::Save(void)
{
    if (m_storage)
        m_storage->Save();

    //we save only the relevant children
    QList<StandardSetting *> *children = getSubSettings();
    if (children)
    {
        for (auto i = children->constBegin(); i != children->constEnd(); ++i)
            (*i)->Save();
    }

    if (!m_haveChanged)
        return;

    m_haveChanged = false;
    emit ChangeSaved();
}

void StandardSetting::setName(const QString &name)
{
    m_name = name;
    if (m_label.isEmpty())
        setLabel(name);
}

StandardSetting* StandardSetting::byName(const QString &name)
{
    if (name == m_name)
        return this;

    for (auto *setting : std::as_const(*getSubSettings()))
    {
        StandardSetting *s = setting->byName(name);
        if (s)
            return s;
    }
    return nullptr;
}

void StandardSetting::MoveToThread(QThread *thread)
{
    moveToThread(thread);

    QList<StandardSetting *>::const_iterator i;
    for (i = m_children.constBegin(); i != m_children.constEnd(); ++i)
        (*i)->MoveToThread(thread);

    QMap<QString, QList<StandardSetting *> >::const_iterator iMap;
    for (iMap = m_targets.constBegin(); iMap != m_targets.constEnd(); ++iMap)
    {
        for (i = (*iMap).constBegin(); i != (*iMap).constEnd(); ++i)
            (*i)->MoveToThread(thread);
    }
}

/******************************************************************************
                            Group Setting
*******************************************************************************/
void GroupSetting::edit(MythScreenType *screen)
{
    if (!isEnabled())
        return;

    if (isReadOnly())
        return;

    auto *dce = new DialogCompletionEvent("leveldown", 0, "", "");
    QCoreApplication::postEvent(screen, dce);
}

void GroupSetting::updateButton(MythUIButtonListItem *item)
{
    item->DisplayState("group", "widgettype");
    item->setEnabled(isEnabled());
    item->SetText(m_label);
    item->SetText(m_settingValue, "value");
    item->SetText(getHelpText(), "description");
    item->setDrawArrow(haveSubSettings());
}

ButtonStandardSetting::ButtonStandardSetting(const QString &label)
{
    setLabel(label);
}

void ButtonStandardSetting::edit(MythScreenType */*screen*/)
{
    emit clicked();
}

void AutoIncrementSetting::Save(void)
{
    if (getValue() == "0")
    {
        // Generate a new, unique ID
        QString querystr = "INSERT INTO " + m_table +
                                   " (" + m_column + ") VALUES (0);";

        MSqlQuery query(MSqlQuery::InitCon());

        if (!query.exec(querystr))
        {
            MythDB::DBError("inserting row", query);
            return;
        }
        // XXX -- HACK BEGIN:
        // lastInsertID fails with "QSqlQuery::value: not positioned on a valid
        // record" if we get a invalid QVariant we workaround the problem by
        // taking advantage of mysql always incrementing the auto increment
        // pointer this breaks if someone modifies the auto increment pointer
        //setValue(query.lastInsertId().toInt());

        QVariant var = query.lastInsertId();

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        auto id = static_cast<QMetaType::Type>(var.type());
#else
        auto id = var.typeId();
#endif
        if (id != QMetaType::UnknownType)
            setValue(var.toInt());
        else
        {
            querystr = "SELECT MAX(" + m_column + ") FROM " +
                               m_table + ";";
            if (query.exec(querystr) && query.next())
            {
                int lii = query.value(0).toInt();
                lii = lii ? lii : 1;
                setValue(lii);
            }
            else
            {
                LOG(VB_GENERAL, LOG_EMERG,
                    "Can't determine the Id of the last insert "
                    "QSqlQuery.lastInsertId() failed, the workaround "
                    "failed too!");
            }
        }
        // XXX -- HACK END:
    }
}

AutoIncrementSetting::AutoIncrementSetting(QString _table, QString _column) :
    m_table(std::move(_table)), m_column(std::move(_column))
{
    setValue("0");
}

/******************************************************************************
                            Text Setting
*******************************************************************************/

void MythUITextEditSetting::SetPasswordEcho(bool b)
{
    m_passwordEcho = b;
}

void MythUITextEditSetting::edit(MythScreenType * screen)
{
    if (!isEnabled())
        return;

    if (isReadOnly())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *settingdialog =
        new MythTextInputDialog(popupStack, getLabel(), FilterNone,
                                m_passwordEcho, m_settingValue);

    if (settingdialog->Create())
    {
        settingdialog->SetReturnEvent(screen, "editsetting");
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void MythUITextEditSetting::resultEdit(DialogCompletionEvent *dce)
{
    if (m_settingValue != dce->GetResultText())
        setValue(dce->GetResultText());
}


void MythUITextEditSetting::updateButton(MythUIButtonListItem *item)
{
    StandardSetting::updateButton(item);
    item->DisplayState("textedit", "widgettype");
}


/******************************************************************************
                            Directory Setting
*******************************************************************************/

void MythUIFileBrowserSetting::edit(MythScreenType * screen)
{
    if (!isEnabled())
        return;

    if (isReadOnly())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *settingdialog = new MythUIFileBrowser(popupStack, m_settingValue);
    settingdialog->SetTypeFilter(m_typeFilter);
    settingdialog->SetNameFilter(m_nameFilter);

    if (settingdialog->Create())
    {
        settingdialog->SetReturnEvent(screen, "editsetting");
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void MythUIFileBrowserSetting::resultEdit(DialogCompletionEvent *dce)
{
    if (m_settingValue != dce->GetResultText())
        setValue(dce->GetResultText());
}

void MythUIFileBrowserSetting::updateButton(MythUIButtonListItem *item)
{
    StandardSetting::updateButton(item);
    item->DisplayState("filebrowser", "widgettype");
}


/******************************************************************************
                            ComboBoxSetting
*******************************************************************************/
MythUIComboBoxSetting::~MythUIComboBoxSetting()
{
    m_labels.clear();
    m_values.clear();
}

void MythUIComboBoxSetting::setValue(int value)
{
    if (value >= 0 && value < m_values.size())
    {
        StandardSetting::setValue(m_values.at(value));
        m_isSet = true;
    }
}

int MythUIComboBoxSetting::getValueIndex(const QString &value) const
{
    return m_values.indexOf(value);
}

QString MythUIComboBoxSetting::getValueLabel() const
{
    int index = getValueIndex(getValue());
    return (index >= 0) ? m_labels.at(index) : QString("");
}

void MythUIComboBoxSetting::addSelection(const QString &label, QString value,
                                         bool select)
{
    value = value.isEmpty() ? label : value;
    m_labels.push_back(label);
    m_values.push_back(value);

    if (select || !m_isSet)
    {
        StandardSetting::setValue(value);
        if (!m_isSet)
            m_isSet = true;
    }
}

void MythUIComboBoxSetting::clearSelections()
{
    m_isSet = false;
    m_labels.clear();
    m_values.clear();
}

void MythUIComboBoxSetting::updateButton(MythUIButtonListItem *item)
{
    item->DisplayState("combobox", "widgettype");
    item->setEnabled(isEnabled());
    item->SetText(m_label);
    int indexValue = m_values.indexOf(m_settingValue);
    if (indexValue >= 0)
        item->SetText(m_labels.value(indexValue), "value");
    else
        item->SetText(m_settingValue, "value");
    item->SetText(getHelpText(), "description");
    item->setDrawArrow(haveSubSettings());
}

void MythUIComboBoxSetting::edit(MythScreenType * screen)
{
    if (!isEnabled())
        return;

    if (isReadOnly())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(getLabel(), popupStack, "optionmenu");

    if (menuPopup->Create())
    {
        popupStack->AddScreen(menuPopup);

        //connect(menuPopup, &MythDialogBox::haveResult,
        //this, qOverload<const QString&>(&MythUIComboBoxSetting::setValue));

        menuPopup->SetReturnEvent(screen, "editsetting");

        if (m_rewrite)
        {
            menuPopup->AddButtonV(QObject::tr("New entry"),
                                  QString("NEWENTRY"),
                                  false,
                                  m_settingValue == "");
        }
        for (int i = 0; i < m_labels.size() && !m_values.empty(); ++i)
        {
            QString value = m_values.at(i);
            menuPopup->AddButtonV(m_labels.at(i),
                                  value,
                                  false,
                                  value == m_settingValue);
        }
    }
    else
    {
        delete menuPopup;
    }
}

void MythUIComboBoxSetting::setValue(const QString& newValue)
{
    StandardSetting::setValue(newValue);
    m_isSet = true;
}

void MythUIComboBoxSetting::resultEdit(DialogCompletionEvent *dce)
{
    if (dce->GetResult() != -1)
    {
        if (m_rewrite && dce->GetData().toString() == "NEWENTRY")
        {
            MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");

            auto *settingdialog =
                new MythTextInputDialog(popupStack, getLabel(), FilterNone,
                                        false, m_settingValue);

            if (settingdialog->Create())
            {
                connect(settingdialog, &MythTextInputDialog::haveResult,
                        this, qOverload<const QString&>(&MythUIComboBoxSetting::setValue));
                popupStack->AddScreen(settingdialog);
            }
            else
            {
                delete settingdialog;
            }
        }
        else if (m_settingValue != dce->GetData().toString())
        {
            StandardSetting::setValue(dce->GetData().toString());
        }
    }
}

void MythUIComboBoxSetting::fillSelectionsFromDir(const QDir &dir, bool absPath)
{
    QFileInfoList entries = dir.entryInfoList();
    for (const auto& fi : std::as_const(entries))
    {
        if (absPath)
            addSelection( fi.absoluteFilePath() );
        else
            addSelection( fi.fileName() );
    }
}

int MythUIComboBoxSetting::size(void) const
{
    return m_labels.size();
}

/******************************************************************************
                            SpinBox Setting
*******************************************************************************/
MythUISpinBoxSetting::MythUISpinBoxSetting(Storage *_storage, int min, int max,
                                           int step, int pageMultiple,
                                           QString special_value_text)
    : StandardSetting(_storage),
      m_min(min),
      m_max(max),
      m_step(step),
      m_pageMultiple(pageMultiple),
      m_specialValueText(std::move(special_value_text))
{
    // We default to 0 unless 0 is out of range.
    if (m_min > 0 || m_max < 0)
        m_settingValue = QString::number(m_min);

    // The settings pages were coded to assume a parameter true/false
    // meaning allow_single_step. Many pages use this but it was not
    // implemented. It is difficult to implement using the current
    // UI widget design. So I have changed it so you can specify
    // the size of pageup / pagedown increments as an integer instead.
    // For compatibility with callers still using true to indicate
    // allowing single step, the code will set the step size as 1 and
    // the pageup / pagedown as the requested step.

    if (m_pageMultiple == 1)
    {
        m_pageMultiple = step;
        m_step = 1;
    }
    if (m_pageMultiple == 0)
    {
        m_pageMultiple = 5;
    }
}

void MythUISpinBoxSetting::updateButton(MythUIButtonListItem *item)
{
    item->DisplayState("spinbox", "widgettype");
    item->setEnabled(isEnabled());
    item->SetText(m_label);
    if (m_settingValue.toInt() == m_min && !m_specialValueText.isEmpty())
        item->SetText(m_specialValueText, "value");
    else
        item->SetText(m_settingValue, "value");
    item->SetText(getHelpText(), "description");
    item->setDrawArrow(haveSubSettings());
}

int MythUISpinBoxSetting::intValue()
{
    return m_settingValue.toInt();
}

void MythUISpinBoxSetting::edit(MythScreenType * screen)
{
    if (!isEnabled())
        return;

    if (isReadOnly())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *settingdialog = new MythSpinBoxDialog(popupStack, getLabel());

    if (settingdialog->Create())
    {
        settingdialog->SetRange(m_min, m_max, m_step, m_pageMultiple);
        if (!m_specialValueText.isEmpty())
            settingdialog->AddSelection(m_specialValueText, m_min);
        settingdialog->SetValue(m_settingValue);
        settingdialog->SetReturnEvent(screen, "editsetting");
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void MythUISpinBoxSetting::resultEdit(DialogCompletionEvent *dce)
{
    if (m_settingValue != dce->GetResultText())
        setValue(dce->GetResultText());
}

/******************************************************************************
                           MythUICheckBoxSetting
*******************************************************************************/

MythUICheckBoxSetting::MythUICheckBoxSetting(Storage *_storage):
    StandardSetting(_storage)
{
}

void MythUICheckBoxSetting::setValue(const QString &value)
{
    StandardSetting::setValue(value);
    if (haveChanged())
        emit valueChanged(value == "1");
}

void MythUICheckBoxSetting::setValue(bool value)
{
    StandardSetting::setValue(value ? "1" : "0");
    if (haveChanged())
        emit valueChanged(value);
}

void MythUICheckBoxSetting::updateButton(MythUIButtonListItem *item)
{
    StandardSetting::updateButton(item);
    item->DisplayState("checkbox", "widgettype");
    item->setCheckable(true);
    item->SetText("", "value");
    if (m_settingValue == "1")
        item->setChecked(MythUIButtonListItem::FullChecked);
    else
        item->setChecked(MythUIButtonListItem::NotChecked);
}

void MythUICheckBoxSetting::edit(MythScreenType * screen)
{
    if (!isEnabled())
        return;

    if (isReadOnly())
        return;

    auto *dce = new DialogCompletionEvent("editsetting", 0, "", "");
    QCoreApplication::postEvent(screen, dce);
}

void MythUICheckBoxSetting::resultEdit(DialogCompletionEvent */*dce*/)
{
    setValue(!boolValue());
}

/******************************************************************************
                           Standard setting dialog
*******************************************************************************/

StandardSettingDialog::~StandardSettingDialog()
{
    if (m_settingsTree)
        m_settingsTree->deleteLater();
}

bool StandardSettingDialog::Create(void)
{
    if (!LoadWindowFromXML("standardsetting-ui.xml", "settingssetup", this))
        return false;

    bool error = false;
    UIUtilE::Assign(this, m_title, "title", &error);
    UIUtilW::Assign(this, m_groupHelp, "grouphelp", &error);
    UIUtilE::Assign(this, m_buttonList, "settingslist", &error);

    UIUtilW::Assign(this, m_selectedSettingHelp, "selectedsettinghelp");

    if (error)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme elements missing.");
        return false;
    }

    connect(m_buttonList, &MythUIButtonList::itemSelected,
            this, &StandardSettingDialog::settingSelected);
    connect(m_buttonList, &MythUIButtonList::itemClicked,
            this, &StandardSettingDialog::settingClicked);

    BuildFocusList();

    LoadInBackground();

    return true;
}

void StandardSettingDialog::settingSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *setting = item->GetData().value<StandardSetting*>();
    if (setting && m_selectedSettingHelp)
    {
        disconnect(m_selectedSettingHelp);
        m_selectedSettingHelp->SetText(setting->getHelpText());
        connect(setting, &StandardSetting::helpTextChanged, m_selectedSettingHelp, &MythUIText::SetText);
    }
}

void StandardSettingDialog::settingClicked(MythUIButtonListItem *item)
{
    auto* setting = item->GetData().value<StandardSetting*>();
    if (setting)
        setting->edit(this);
}

void StandardSettingDialog::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);
        if (dce == nullptr)
            return;
        QString resultid  = dce->GetId();

        if (resultid == "leveldown")
        {
            //a GroupSetting have been clicked
            LevelDown();
        }
        else if (resultid == "editsetting")
        {
            MythUIButtonListItem * item = m_buttonList->GetItemCurrent();
            if (item)
            {
                auto *ss = item->GetData().value<StandardSetting*>();
                if (ss)
                    ss->resultEdit(dce);
            }
        }
        else if (resultid == "exit")
        {
            int buttonnum = dce->GetResult();
            if (buttonnum == 0)
            {
                Save();
                MythScreenType::Close();
                if (m_settingsTree)
                    m_settingsTree->applyChange();
            }
            else if (buttonnum == 1)
            {
                MythScreenType::Close();
            }
        }
    }
}

void StandardSettingDialog::Load(void)
{
    if (m_settingsTree)
    {
        m_settingsTree->Load();
        m_settingsTree->MoveToThread(QApplication::instance()->thread());
    }
}

void StandardSettingDialog::Init(void)
{
    setCurrentGroupSetting(m_settingsTree);
}

GroupSetting *StandardSettingDialog::GetGroupSettings(void) const
{
    return m_settingsTree;
}

void StandardSettingDialog::setCurrentGroupSetting(
    StandardSetting *groupSettings, StandardSetting *selectedSetting)
{
    if (!groupSettings)
        return;

    if (m_currentGroupSetting)
    {
        disconnect(m_currentGroupSetting,
                   &StandardSetting::settingsChanged, nullptr, nullptr);
        m_currentGroupSetting->Close();
    }

    m_currentGroupSetting = groupSettings;
    m_currentGroupSetting->Open();

    m_title->SetText(m_currentGroupSetting->getLabel());
    if (m_groupHelp)
    {
        m_groupHelp->SetText(m_currentGroupSetting->getHelpText());
    }
    updateSettings(selectedSetting);
    connect(m_currentGroupSetting,
            &StandardSetting::settingsChanged,
            this, &StandardSettingDialog::updateSettings);
}

void StandardSettingDialog::updateSettings(StandardSetting * selectedSetting)
{
    m_buttonList->Reset();
    if (!m_currentGroupSetting->haveSubSettings())
        return;

    QList<StandardSetting *> *settings =
        m_currentGroupSetting->getSubSettings();
    if (!settings)
        return;

    QList<StandardSetting *>::const_iterator i;
    MythUIButtonListItem *selectedItem = nullptr;
    for (i = settings->constBegin(); i != settings->constEnd(); ++i)
    {
        if ((*i)->isVisible())
        {
            if (selectedSetting == (*i))
                selectedItem = (*i)->createButton(m_buttonList);
            else
                (*i)->createButton(m_buttonList);
        }
    }
    if (selectedItem)
        m_buttonList->SetItemCurrent(selectedItem);
    settingSelected(m_buttonList->GetItemCurrent());
}

void StandardSettingDialog::Save()
{
    if (m_settingsTree)
        m_settingsTree->Save();
}

void StandardSettingDialog::LevelUp()
{
    if (!m_currentGroupSetting)
        return;

    if (m_currentGroupSetting->getParent())
    {
        setCurrentGroupSetting(m_currentGroupSetting->getParent(),
                               m_currentGroupSetting);
    }
}

void StandardSettingDialog::LevelDown()
{
    MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
    if (item)
    {
        auto *ss = item->GetData().value<StandardSetting*>();
        if (ss && ss->haveSubSettings() && ss->isEnabled())
            setCurrentGroupSetting(ss);
    }
}

void StandardSettingDialog::Close(void)
{
    if (m_settingsTree->haveChanged())
    {
        QString label = tr("Exit ?");

        MythScreenStack *popupStack =
            GetMythMainWindow()->GetStack("popup stack");

        auto * menuPopup = new MythDialogBox(label, popupStack, "exitmenu");

        if (menuPopup->Create())
        {
            popupStack->AddScreen(menuPopup);

            menuPopup->SetReturnEvent(this, "exit");

            menuPopup->AddButton(tr("Save then Exit"));
            menuPopup->AddButton(tr("Exit without saving changes"));
            menuPopup->AddButton(tr("Cancel"));
        }
        else
        {
            delete menuPopup;
        }
    }
    else
    {
        MythScreenType::Close();
    }
}

static QKeyEvent selectEvent
    (QKeyEvent(QEvent::KeyPress,0,Qt::NoModifier,"SELECT"));
static QKeyEvent deleteEvent
    (QKeyEvent(QEvent::KeyPress,0,Qt::NoModifier,"DELETE"));

bool StandardSettingDialog::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;

    bool handled = m_buttonList->keyPressEvent(e);
    if (handled)
        return true;

    handled = GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    //send the key to the selected Item first
    MythUIButtonListItem * item = m_buttonList->GetItemCurrent();
    if (item)
    {
        auto *ss = item->GetData().value<StandardSetting*>();
        if (ss)
            handled = ss->keyPressEvent(e);
    }
    if (handled)
        return true;

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            if (m_currentGroupSetting &&
                m_currentGroupSetting == m_settingsTree)
                Close();
            else
                LevelUp();
        }
        else if (action == "RIGHT")
        {
            LevelDown();
        }
        else if (action == "EDIT")
        {
            keyPressEvent(&selectEvent);
        }
        else if (action == "DELETE")
        {
            deleteEntry();
        }
        else
        {
            handled = MythScreenType::keyPressEvent(e);
        }
    }

    return handled;
}

void StandardSettingDialog::ShowMenu()
{
    MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
    if (!item)
        return;

    auto *source = item->GetData().value<GroupSetting*>();
    if (!source)
        return;
    // m_title->GetText() for screen title
    auto *menu = new MythMenu(source->getLabel(), this, "mainmenu");
    menu->AddItem(tr("Edit"), &StandardSettingDialog::editEntry);
    if (source->canDelete())
        menu->AddItem(tr("Delete"), &StandardSettingDialog::deleteSelected);

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(menu, popupStack, "menudialog");
    menuPopup->SetReturnEvent(this, "mainmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

void StandardSettingDialog::editEntry()
{
    keyPressEvent(&selectEvent);
}

void StandardSettingDialog::deleteSelected()
{
    keyPressEvent(&deleteEvent);
}

void StandardSettingDialog::deleteEntry()
{
    MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
    if (!item)
        return;

    auto *source = item->GetData().value<GroupSetting*>();
    if (!source)
        return;

    if (source->canDelete())
    {
        QString message = tr("Do you want to delete the '%1' entry?")
            .arg(source->getLabel());
        ShowOkPopup(message, this, &StandardSettingDialog::deleteEntryConfirmed, true);
    }
}

void StandardSettingDialog::deleteEntryConfirmed(bool ok)
{
    if (ok)
    {
        MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
        if (!item)
            return;
        auto *source = item->GetData().value<GroupSetting*>();
        if (!source)
            return;
        source->deleteEntry();
//        m_settingsTree->removeChild(source);
        source->getParent()->removeChild(source);
        m_buttonList->RemoveItem(item);
    }

}
