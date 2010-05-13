
#include "mythcorecontext.h"
#include "mythverbose.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuitext.h"
#include "mythuitextedit.h"
#include "rawsettingseditor.h"
#include "remoteutil.h"

/** \fn RawSettingsEditor::RawSettingsEditor(MythScreenStack *parent,
                                             const char name)
  * \brief Raw Settings Editor constructor
  *
  * Initializes necessary variables.
  *
  * \param parent Parent screen stack for this window
  * \param name   Name of this window
  */
RawSettingsEditor::RawSettingsEditor(MythScreenStack *parent, const char *name)
  : MythScreenType(parent, name),
    m_title(tr("Settings Editor")),
    // Settings widgets
    m_settingsList(NULL),      m_settingValue(NULL),
    // Action buttons
    m_saveButton(NULL),        m_cancelButton(NULL),
    // Labels
    m_textLabel(NULL)
{
}

/** \fn RawSettingsEditor::~RawSettingsEditor()
 *  \brief Raw Settings Editor destructor
 */
RawSettingsEditor::~RawSettingsEditor()
{
}

/** \fn RawSettingsEditor::Create(void)
 *  \brief Creates the UI screen.
 */
bool RawSettingsEditor::Create(void)
{
    if (!LoadWindowFromXML("settings-ui.xml", "rawsettingseditor", this))
        return false;

    m_settingsList = dynamic_cast<MythUIButtonList *> (GetChild("settings"));

    m_saveButton = dynamic_cast<MythUIButton *> (GetChild("save"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));
    m_textLabel = dynamic_cast<MythUIText *> (GetChild("label-text"));

    if (!m_settingsList || !m_textLabel || !m_saveButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    BuildFocusList();

    MythUIText *text = dynamic_cast<MythUIText *> (GetChild("heading"));
    if (text)
        text->SetText(m_title);

    MythUIShape *shape = NULL;

    for (int i = -8; i <= 8; i++)
    {
        text = dynamic_cast<MythUIText *>
                (GetChild(QString("value%1%2").arg(i >= 0? "+" : "").arg(i)));
        if (text)
            m_prevNextTexts[i] = text;

        shape = dynamic_cast<MythUIShape *>
                (GetChild(QString("shape%1%2").arg(i >= 0? "+" : "").arg(i)));
        if (shape)
            m_prevNextShapes[i] = shape;
    }

    m_settingValue = dynamic_cast<MythUITextEdit *> (GetChild("settingvalue"));

    connect(m_settingsList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(selectionChanged(MythUIButtonListItem*)));
    connect(m_settingValue, SIGNAL(LosingFocus()), SLOT(valueChanged()));

    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(Save()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    LoadInBackground();

    return true;
}

/** \fn RawSettingsEditor::Load(void)
 *  \brief Loads the current values for the specified settings list
 */
void RawSettingsEditor::Load(void)
{
    QList<QString>settingsList = m_settings.keys();
    QList<QString>::iterator it = settingsList.begin();

    // FIXME, optimize this using gCoreContext->GetSettings()
    // QMap<QString,QString> kv;

    while (it != settingsList.end())
    {
        QString value = gCoreContext->GetSetting(*it);
        m_settingValues[*it] = value;
        m_origValues[*it] = value;

        ++it;
    }
    m_settingValues.detach();
    m_origValues.detach();
}

/** \fn RawSettingsEditor::Init(void)
 *  \brief Initialize the settings screen with the loaded data
 */
void RawSettingsEditor::Init(void)
{
    QList<QString>settingsList = m_settings.keys();
    QList<QString>::iterator it = settingsList.begin();

    while (it != settingsList.end())
    {
        MythUIButtonListItem *item = new MythUIButtonListItem(m_settingsList,
                                                    "", qVariantFromValue(*it));

        if (m_settings[*it].isEmpty())
            item->SetText(*it);
        else
            item->SetText(m_settings[*it]);

        ++it;
    }

    m_settingsList->SetItemCurrent(0);
    m_textLabel->SetText(m_settingsList->GetItemFirst()->GetText());
    updatePrevNextTexts();
}

/** \fn RawSettingsEditor::Save(void)
 *  \brief Save editted values and clear settings cache if necessary
 */
void RawSettingsEditor::Save(void)
{
    bool changed = false;

    QHash <QString, QString>::const_iterator it = m_settingValues.constBegin();
    while (it != m_settingValues.constEnd())
    {
        if ((!it.value().isEmpty()) ||
            ((m_origValues.contains(it.key())) &&
             (!m_origValues.value(it.key()).isEmpty())))
        {
            gCoreContext->SaveSetting(it.key(), it.value());
            changed = true;
        }

        ++it;
    }

    if (changed && (!gCoreContext->IsMasterHost() || gCoreContext->BackendIsRunning()))
        RemoteSendMessage("CLEAR_SETTINGS_CACHE");

    Close();
}

/** \fn RawSettingsEditor::selectionChanged(MythUIButtonListItem *item)
 *  \brief Slot handler for buttonlist current item changes
 *
 *  Updates the text edit area with the current value of the setting that
 *  is currently selected in the button list.
 *
 *  \param item The currently selected item in the buttonlist
 */
void RawSettingsEditor::selectionChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    m_settingValue->SetText(m_settingValues[item->GetData().toString()]);
    m_textLabel->SetText(item->GetText());

    updatePrevNextTexts();
}

/** \fn RawSettingsEditor::updatePrevNextTexts(void)
 *  \brief Updates previous and next text areas
 *
 *  Updates previous and next text areas to show values of other non-current
 *  settings in the button list.
 */
void RawSettingsEditor::updatePrevNextTexts(void)
{
    MythUIButtonListItem *tmpitem;
    int curPos = m_settingsList->GetCurrentPos();
    int recs   = m_settingsList->GetCount();

    if (!recs)
        return;

    for (int i = -8; i <= 8; i++)
    {
        if (m_prevNextTexts.contains(i))
        {
            if (((i < 0) && ((curPos + i) >= 0)) ||
                ((i > 0) && (((recs-1) - i) >= curPos)))
            {
                if (m_prevNextShapes.contains(i))
                    m_prevNextShapes[i]->Show();

                tmpitem = m_settingsList->GetItemAt(curPos + i);
                m_prevNextTexts[i]->SetText(
                    m_settingValues[tmpitem->GetData().toString()]);
            }
            else
            {
                if (m_prevNextShapes.contains(i))
                    m_prevNextShapes[i]->Hide();

                m_prevNextTexts[i]->SetText(QString());
            }
        }
    }
}

/** \fn RawSettingsEditor::valueChanged(void)
 *  \brief Tracks current value for a setting when the value is editted
 *
 *  Updates the local in-memory settings value cache when the value for a
 *  setting in the list is updated.
 */
void RawSettingsEditor::valueChanged(void)
{
    m_settingValues[m_settingsList->GetItemCurrent()->GetData().toString()] =
        m_settingValue->GetText();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
