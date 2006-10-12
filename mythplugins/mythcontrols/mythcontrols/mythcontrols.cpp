/* -*- myth -*- */
/**
 * @file mythcontrols.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main header for mythcontrols.
 *
 * Note that the keybindings are fetched all at once, and cached for
 * this host.  This avoids pelting the database everytime the user
 * changes their selection.  This makes a HUGE difference in delay.
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 */

// Qt headers
#include <qnamespace.h>
#include <qstringlist.h>
#include <qapplication.h>
#include <qbuttongroup.h>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>

// MythControls headers
#include "mythcontrols.h"
#include "keygrabber.h"

static QString key_to_display(const QString &key);
static QString display_to_key(const QString &key);

#define LOC QString("MythControls: ")
#define LOC_ERR QString("MythControls, Error: ")

/** \fn MythControls::MythControls(MythMainWindow*, bool&)
 *  \brief Creates a new MythControls wizard
 *  \param parent The main myth window.
 *  \param ui_ok Set if UI was correctly loaded, cleared otherwise.
 */
MythControls::MythControls(MythMainWindow *parent, bool &ok)
    : MythThemedDialog(parent, "controls", "controls-", "controls"),
      m_focusedUIElement(NULL),
      m_leftList(NULL),        m_rightList(NULL),
      m_description(NULL),
      m_leftDescription(NULL), m_rightDescription(NULL),
      m_bindings(NULL),     m_container(NULL)
{
    bzero(m_actionButtons, sizeof(void*) * Action::kMaximumNumberOfBindings);
    m_contexts.setAutoDelete(true);
    
    // Load up the ui components
    ok = LoadUI();
    if (!ok)
        return;

    m_leftListType  = kContextList;
    m_rightListType = kActionList;

    LoadData(gContext->GetHostName());
    RefreshKeyInformation();

    connect(m_leftList,  SIGNAL(itemSelected( UIListBtnTypeItem*)),
            this,        SLOT(  LeftSelected( UIListBtnTypeItem*)));
    connect(m_rightList, SIGNAL(itemSelected( UIListBtnTypeItem*)),
            this,        SLOT(  RightSelected(UIListBtnTypeItem*)));
}

MythControls::~MythControls()
{
    if (m_bindings)
    {
        delete m_bindings;
        m_bindings = NULL;
    }
}

/** \fn MythControls::LoadUI(void)
 *  \brief Loads UI elements from theme
 *
 *   This method grabs all of the UI elements that are needed by
 *   mythcontrols. If this method returns false the plugin must
 *   exit, otherwise the application will crash.
 *
 *  \return true iff all UI elements load successfully.
 */
bool MythControls::LoadUI(void)
{
    void *err_test[4];
    err_test[0] = m_description = getUITextType("description");
    err_test[1] = m_container   = getContainer("controls");
    err_test[2] = m_leftList    = getUIListBtnType("leftlist");
    err_test[3] = m_rightList   = getUIListBtnType("rightlist");

    QString err_msg[4] =
    {
        "Unable to load action_description",
        "No controls container in theme",
        "No leftlist in theme",
        "No rightList in theme",
    };

    bool ok = true;
    for (uint i = 0; i < 4; i++)
    {
        if (!err_test[i])
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + err_msg[i]);
            ok = false;
        }
    }

    if (ok)
    {
        m_leftDescription  = getUITextType("leftdesc");
        m_rightDescription = getUITextType("rightdesc");
        m_focusedUIElement   = m_leftList;

        m_leftList->calculateScreenArea();
        m_leftList->SetActive(true);

        m_rightList->calculateScreenArea();
        m_rightList->SetActive(false);
    }

    static const QString numstr[Action::kMaximumNumberOfBindings] =
        { "one", "two", "three", "four", };

    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
    {
        m_actionButtons[i] = getUITextButtonType(
            QString("action_%1").arg(numstr[i]));

        if (!m_actionButtons[i])
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Unable to load action button #%1").arg(i));

            ok = false;
        }
    }

    return ok;
}

/** \fn MythControls::GetCurrentButton(void) const
 *  \brief Returns the focused button, or 
 *         Action::kMaximumNumberOfBindings if no buttons are focued.
 */
uint MythControls::GetCurrentButton(void) const
{
    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
    {
        if (m_focusedUIElement == m_actionButtons[i])
            return i;
    }

    return Action::kMaximumNumberOfBindings;
}

/** \fn MythControls::ChangeButtonFocus(int)
 *  \brief Change button focus in a particular direction
 *  \param direction +1 moves focus to the right, -1 moves to the left,
 *                   and 0 changes the focus to the first button.
 */
void MythControls::ChangeButtonFocus(int direction)
{
    if ((m_leftListType != kContextList) || (m_rightListType != kActionList))
        return;

    if (direction == 0)
    {
        m_focusedUIElement = m_actionButtons[0];
        m_actionButtons[0]->takeFocus();
        m_rightList->looseFocus();
        m_rightList->SetActive(false);
        return;
    }

    direction = (direction > 0) ? 1 : -1;

    int newb = (GetCurrentButton() + direction);
    newb %= Action::kMaximumNumberOfBindings;

    m_focusedUIElement->looseFocus();
    m_focusedUIElement = m_actionButtons[newb];
    m_focusedUIElement->takeFocus();
}

/** \fn MythControls::ChangeListFocus(UIListBtnType*,UIListBtnType*)
 *  \brief Change list focus from "unfocus" list to "focus" list.
 *  \param focus The list to gain focus
 *  \param unfocus The list to loose focus
 */
void MythControls::ChangeListFocus(UIListBtnType *focus,
                                   UIListBtnType *unfocus)
{
    /* unfocus a list (setactive(false)) or a button (focused->looseFocus) */
    if (unfocus)
        unfocus->SetActive(false);

    m_focusedUIElement->looseFocus();
    m_focusedUIElement = focus;

    focus->SetActive(true);
    focus->takeFocus();

    RefreshKeyInformation();
}

void MythControls::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    bool escape = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Controls", e, actions);

    for (uint i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU" || action == "INFO")
        {
            m_focusedUIElement->looseFocus();

            OptionsMenu popup(gContext->GetMainWindow());
            if (OptionsMenu::kSave == popup.GetOption())
                Save();

            m_focusedUIElement->takeFocus();
        }
        else if (action == "SELECT")
        {
            if (m_focusedUIElement == m_leftList)
                ChangeListFocus(m_rightList, m_leftList);
            else if (m_focusedUIElement == m_rightList)
                ChangeButtonFocus(0);
            else
            {
                QString key = GetCurrentKey();
                if (!key.isEmpty())
                {
                    ActionMenu popup(gContext->GetMainWindow());
                    int result = popup.GetOption();
                    if (result == ActionMenu::kSet)
                        AddKeyToAction();
                    else if (result == ActionMenu::kRemove)
                        DeleteKey();
                }
                else // for blank keys, no reason to ask what to do
                    AddKeyToAction();
            }
            
        }
        else if (action == "ESCAPE")
        {
            escape = true;
            if (m_focusedUIElement == m_leftList)
            {
                handled = false;
                if (m_bindings->HasChanges())
                {
                    /* prompt user to save changes */
                    UnsavedMenu popup(gContext->GetMainWindow());
                    if (popup.GetOption() == UnsavedMenu::kSave)
                        Save();
                }
            }
            else if (m_focusedUIElement == m_rightList)
                ChangeListFocus(m_leftList, m_rightList);
            else
                ChangeListFocus(m_rightList, NULL);
        }
        else if (action == "UP")
        {
            if (m_focusedUIElement == m_leftList)
                m_leftList->MoveUp();
            else if (m_focusedUIElement == m_rightList)
                m_rightList->MoveUp();
        }
        else if (action == "DOWN")
        {
            if (m_focusedUIElement == m_leftList)
                m_leftList->MoveDown();
            else if (m_focusedUIElement == m_rightList)
                m_rightList->MoveDown();
        }
        else if (action == "LEFT")
        {
            if (m_focusedUIElement==m_rightList)
                ChangeListFocus(m_leftList, m_rightList);
            else if (m_focusedUIElement != m_leftList)
                ChangeButtonFocus(-1);
        }
        else if (action == "RIGHT")
        {
            if (m_focusedUIElement == m_leftList)
                ChangeListFocus(m_rightList, m_leftList);
            else if (m_focusedUIElement != m_rightList)
                ChangeButtonFocus(1);
        }
        else if (action == "PAGEUP")
        {
            if (m_focusedUIElement == m_leftList)
                m_leftList->MoveUp(UIListBtnType::MovePage);
            else if (m_focusedUIElement == m_rightList)
                m_rightList->MoveUp(UIListBtnType::MovePage);
        }
        else if (action == "PAGEDOWN")
        {
            if (m_focusedUIElement == m_leftList)
                m_leftList->MoveDown(UIListBtnType::MovePage);
            else if (m_focusedUIElement == m_rightList)
                m_rightList->MoveDown(UIListBtnType::MovePage);
        }
        else if (action == "1")
        {
            if ((m_leftListType  != kContextList) ||
                (m_rightListType != kActionList))
            {
                m_leftListType  = kContextList;
                m_rightListType = kActionList;
                UpdateLists();

                if (m_focusedUIElement != m_leftList)
                {
                    ChangeListFocus(m_leftList,
                                    (m_focusedUIElement == m_rightList) ?
                                    m_rightList : NULL);
                }
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "2")
        {
            if ((m_leftListType  != kContextList) ||
                (m_rightListType != kKeyList))
            {
                m_leftListType  = kContextList;
                m_rightListType = kKeyList;
                UpdateLists();

                if (m_focusedUIElement != m_leftList)
                {
                    ChangeListFocus(m_leftList,
                                    (m_focusedUIElement == m_rightList) ?
                                    m_rightList : NULL);
                }
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "3")
        {
            if ((m_leftListType  != kKeyList) ||
                (m_rightListType != kContextList))
            {
                m_leftListType  = kKeyList;
                m_rightListType = kContextList;
                UpdateLists();

                if (m_focusedUIElement != m_leftList)
                {
                    ChangeListFocus(m_leftList,
                                    (m_focusedUIElement == m_rightList) ?
                                    m_rightList : NULL);
                }
            }
            else
            {
                handled = false;
            }
        }
        else
        {
            handled = false;
        }
    }

    if (!handled)
    {
        handled |= (!escape && JumpTo(e));
        if (!handled)
            MythThemedDialog::keyPressEvent(e);
    }
}

/** \fn MythControls::JumpTo(QKeyEvent*)
 *  \brief Jump to a particular key binding
 *  \param e key event to use as jump
 */
bool MythControls::JumpTo(QKeyEvent *e)
{
    UIListBtnType *list = NULL;

    list = ((m_focusedUIElement == m_leftList) &&
            (m_leftListType     == kKeyList)) ? m_leftList  : list;
    list = ((m_focusedUIElement == m_rightList) &&
            (m_rightListType    == kKeyList)) ? m_rightList : list;

    if (!list)
        return false;

    QString key = e->text();
    if (key.left(6) == "remote")
    {
        key = key_to_display(key);
    }
    else
    {
        key = QString(QKeySequence(e->key()));

        if (key.isEmpty())
            return false;

        QString modifiers = "";

        if (e->state() & Qt::ShiftButton)
            modifiers += "Shift+";
        if (e->state() & Qt::ControlButton)
            modifiers += "Ctrl+";
        if (e->state() & Qt::AltButton)
            modifiers += "Alt+";
        if (e->state() & Qt::MetaButton)
            modifiers += "Meta+";

        key = modifiers + key;
    }

    uint len = 1024; // infinity
    if (list == m_rightList)
    {
        key = key + " ";
        len = key.length();
    }

    UIListBtnTypeItem *b;
    for (b = list->GetItemFirst(); b; b = list->GetItemNext(b))
    {
        if (b->text().left(len) == key)
            break;
    }

    if (!b)
        return false;

    int curpos = list->GetItemPos(list->GetItemCurrent());
    int newpos = list->GetItemPos(b);

    if (newpos > curpos)
        list->MoveDown(newpos - curpos);
    else if (newpos < curpos)
        list->MoveUp(curpos - newpos);

    return true;
}

/** \fn MythControls::LeftSelected(UIListBtnTypeItem*)
 *  \brief Refreshes the right list when an item in the
 *         left list is selected
 */
void MythControls::LeftSelected(UIListBtnTypeItem*)
{
    m_leftList->refresh();
    m_rightList->blockSignals(true);
    RefreshRightList();
    m_rightList->blockSignals(false);
    m_rightList->refresh();
}

/** \fn MythControls::RightSelected(UIListBtnTypeItem*)
 *  \brief Refreshes key information when an item in the
 *         right list is selected
 */
void MythControls::RightSelected(UIListBtnTypeItem*)
{
    m_rightList->refresh();
    RefreshKeyInformation();
}

/** \fn MythControls::RefreshRightList(void)
 *  \brief Load the appropriate actions into the action list
 */
void MythControls::RefreshRightList(void)
{
    m_rightList->Reset();

    if (!m_leftList->GetItemCurrent())
        return;

    if (m_leftListType == kContextList)
    {
        if (m_rightListType == kActionList)
        {
            /* add all of the actions to the context list */
            QString context = m_leftList->GetItemCurrent()->text();
            QStringList *actions = m_contexts[context];
            if (!actions || actions->empty())
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + 
                        QString("Unable to find actions for context %1")
                        .arg(context));

                return;
            }

            for (uint i = 0; i < actions->size(); i++)
                new UIListBtnTypeItem(m_rightList, (*actions)[i]);
        }
        else if (m_rightListType == kKeyList)
        {
            /* add all of the actions to the context list */
            QString context = m_leftList->GetItemCurrent()->text();
            const BindingList *list = m_contextToBindingsMap[context];
            if (!list)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR+ 
                        QString("Unable to find keys for context %1")
                        .arg(context));

                return;
            }

            BindingList::const_iterator it = list->begin();
            for (; it != list->end(); ++it)
            {
                new UIListBtnTypeItem(
                    m_rightList,
                    key_to_display((*it)->key) + " => " + (*it)->action);
            }
        }
    }
    else if ((m_leftListType == kKeyList) && (m_rightListType == kContextList))
    {
        QString key = display_to_key(m_leftList->GetItemCurrent()->text());
        const BindingList *list = m_keyToBindingsMap[key];
        if (!list)
        {
            VERBOSE(VB_IMPORTANT, LOC + QString(
                        "Unable to find actions for key %1").arg(key));
            return;
        }

        BindingList::const_iterator it = list->begin();
        const Binding *b = *it;
        for (uint i = 0; i < m_sortedContexts.size(); i++)
        {
            const QString context = m_sortedContexts[i];
            QString action  = "<none>";

            if (b && b->context == context)
            {
                action = b->action;
                ++it;
                b = (it != list->end()) ? *it : NULL;
            }

            new UIListBtnTypeItem(m_rightList, context + " => " + action);
        }
    }
}

QString MythControls::RefreshKeyInformationKeyList(void)
{
    if ((m_leftListType != kKeyList) && (m_rightListType != kKeyList))
        return "";

    QString action  = GetCurrentAction();
    QString context = GetCurrentContext();

    if (action.isEmpty())
        return "";

    QString desc = m_bindings->GetActionDescription(context, action);
    
    BindingList *list = NULL;
    if (m_leftListType == kKeyList && m_rightListType == kContextList)
    {
        list = m_keyToBindingsMap[display_to_key(GetCurrentKey())];
    }
    else if (m_leftListType == kContextList && m_rightListType == kKeyList)
    {
        list = m_contextToBindingsMap[context];
    }

    if (!list)
        return desc;

    QString searchKey = QString::null;
    if (m_rightListType == kContextList)
    {
        searchKey = context;
    }
    else if (m_rightListType == kActionList)
    {
        searchKey = action;
    }
    else if (m_rightListType == kKeyList)
    {
        searchKey = display_to_key(GetCurrentKey());
    }

    const Binding *binding = NULL;
    for (BindingList::const_iterator it = list->begin();
         it != list->end(); ++it)
    {
        switch (m_rightListType)
        {
            case kContextList:
                if ((*it)->context == searchKey)
                    binding = *it;
                break;
            case kActionList:
                if ((*it)->action == searchKey)
                    binding = *it;
                break;
            case kKeyList:
                if ((*it)->key == searchKey)
                    binding = *it;
                break;
        }

        if (binding)
            break;
    }
    
    if (!binding)
        return desc;


    if (desc.isEmpty() && (context != binding->contextFrom))
    {
        desc = m_bindings->GetActionDescription(
            binding->contextFrom, action);
    }

    desc += "\n" + tr("Binding comes from %1 context")
        .arg(binding->contextFrom);

    return desc;
}

/** \fn MythControls::RefreshKeyInformation(void)
 *  \brief Updates the list of keys that are shown and the
 *         description of the action.
 */
void MythControls::RefreshKeyInformation(void)
{
    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
        m_actionButtons[i]->setText("");

    if (m_focusedUIElement == m_leftList)
    {
        m_description->SetText("");
        return;
    }

    if ((m_leftListType == kKeyList) || (m_rightListType == kKeyList))
    {
        m_description->SetText(RefreshKeyInformationKeyList());
        return;
    }

    const QString context = GetCurrentContext();
    const QString action  = GetCurrentAction();

    QString desc = m_bindings->GetActionDescription(context, action);
    m_description->SetText(desc);

    QStringList keys = m_bindings->GetActionKeys(context, action);
    for (uint i = 0; (i < keys.count()) &&
             (i < Action::kMaximumNumberOfBindings); i++)
    {
        m_actionButtons[i]->setText(key_to_display(keys[i]));
    }
}


/** \fn MythControls::GetCurrentContext(void) const
 *  \brief Get the currently selected context string.
 *
 *   If no context is selected, an empty string is returned.
 *
 *  \return The currently selected context string.
 */
QString MythControls::GetCurrentContext(void) const
{
    if (m_leftListType == kContextList)
        return m_leftList->GetItemCurrent()->text();

    if (m_focusedUIElement == m_leftList)
        return QString::null;

    QString desc = m_rightList->GetItemCurrent()->text();
    int loc = desc.find(" => ");
    if (loc == -1)
        return QString::null; // Should not happen

    if (m_rightListType == kContextList)
        return desc.left(loc);

    return desc.mid(loc + 4);
}

/** \fn MythControls::GetCurrentAction(void) const
 *  \brief Get the currently selected action string.
 *
 *   If no action is selected, an empty string is returned.
 *
 *  \return The currently selected action string.
 */
QString MythControls::GetCurrentAction(void) const
{
    if (m_leftListType == kActionList)
        return m_leftList->GetItemCurrent()->text();

    if (m_focusedUIElement == m_leftList)
        return QString::null;

    QString desc = m_rightList->GetItemCurrent()->text();
    if (m_leftListType == kContextList && m_rightListType == kActionList)
        return desc;

    int loc = desc.find(" => ");
    if (loc == -1)
        return QString::null; // should not happen..

    if (m_rightListType == kActionList)
        return desc.left(loc);

    QString rv = desc.mid(loc+4);
    if (rv == "<none>")
        return QString::null;

    return rv;
}

/** \fn MythControls::GetCurrentKey(void) const
 *  \brief Get the currently selected key string
 *
 *   If no key is selected, an empty string is returned.
 *
 *  \return The currently selected key string
 */
QString MythControls::GetCurrentKey(void) const
{
    if (m_leftListType == kKeyList)
        return m_leftList->GetItemCurrent()->text();

    if (m_focusedUIElement == m_leftList)
        return QString::null;

    if ((m_leftListType == kContextList) && (m_rightListType == kActionList))
    {
        QString context = GetCurrentContext();
        QString action = GetCurrentAction();
        uint b = GetCurrentButton();
        QStringList keys = m_bindings->GetActionKeys(context, action);

        if (b < keys.count())
            return keys[b];

        return QString::null;
    }

    QString desc = m_rightList->GetItemCurrent()->text();
    int loc = desc.find(" => ");
    if (loc == -1)
        return QString::null; // Should not happen


    if (m_rightListType == kKeyList)
        return desc.left(loc);

    return desc.mid(loc + 4);
}

/** \fn MythControls::LoadAll(const QString&)
 *  \brief Load the settings for a particular host.
 *  \param hostname The host to load settings for.
 */
void MythControls::LoadData(const QString &hostname)
{
    /* create the key bindings and the tree */
    m_bindings = new KeyBindings(hostname);
    m_sortedContexts = m_bindings->GetContexts();

    m_sortedKeys.clear();

    /* Alphabetic order, but jump and global at the top  */
    m_sortedContexts.sort();
    m_sortedContexts.remove(ActionSet::kJumpContext);
    m_sortedContexts.remove(ActionSet::kGlobalContext);
    m_sortedContexts.insert(m_sortedContexts.begin(), 1,
                            ActionSet::kGlobalContext);
    m_sortedContexts.insert(m_sortedContexts.begin(), 1,
                            ActionSet::kJumpContext);

    QStringList actions;
    for (uint i = 0; i < m_sortedContexts.size(); i++)
    {
        actions = m_bindings->GetActions(m_sortedContexts[i]);
        actions.sort();
        m_contexts.insert(m_sortedContexts[i], new QStringList(actions));
    }

    RefreshKeyBindings();
    UpdateLists();
}

/** \fn MythControls::DeleteKey(void)
 *  \brief Delete the currently active key to action mapping
 *
 *   TODO FIXME This code needs work to support deleteKey
 *              in any mode exc. Context/Action
 */
void MythControls::DeleteKey(void)
{
    QString context = GetCurrentContext();
    QString key     = GetCurrentKey();
    QString action  = GetCurrentAction();

    if (context.isEmpty() || key.isEmpty() || action.isEmpty())
    {
        InvalidBindingPopup popup(gContext->GetMainWindow());
        popup.GetOption();
        return;
    }

    BindingList *list = m_keyToBindingsMap[key];
    Binding *binding = NULL;

    for (BindingList::iterator it = list->begin(); it != list->end(); ++it)
    {
        Binding *b = *it;
        if (b->context == context)
            binding = b;
    }

    if (!binding)
    {
        InvalidBindingPopup popup(gContext->GetMainWindow());
        popup.GetOption();
        return;
    }

    if (binding->contextFrom != context)
    {
        ConfirmMenu popup(gContext->GetMainWindow(),
                          tr("Delete this key binding from context %1?")
                          .arg(binding->contextFrom));

        if (popup.GetOption() != ConfirmMenu::kConfirm)
            return;
    }
    else
    {
        ConfirmMenu popup(gContext->GetMainWindow(),
                          tr("Delete this binding?"));

        if (popup.GetOption() != ConfirmMenu::kConfirm)
            return;
    }

    if (!m_bindings->RemoveActionKey(binding->contextFrom, action, key))
    {
        InvalidBindingPopup popup(gContext->GetMainWindow());
        popup.GetOption();
        return;
    }

    RefreshKeyBindings();
    RefreshKeyInformation();
}

/** \fn ResolveConflict(ActionID*, int)
 *  \brief Resolve a potential conflict
 *  \return true if the conflict should be bound, false otherwise.
 */
bool MythControls::ResolveConflict(ActionID *conflict, int error_level)
{
    if (KeyBindings::kKeyBindingError == error_level)
    {
        InvalidBindingPopup popup(gContext->GetMainWindow(),
                                  conflict->GetAction(),
                                  conflict->GetContext());

        popup.GetOption();

        return false;
    }

    QString msg =
        tr("This kebinding may conflict with %1 in the %2 context. "
           "Do you want to bind it anyways?")
        .arg(conflict->GetAction()).arg(conflict->GetContext());

    if (MythPopupBox::show2ButtonPopup(
            gContext->GetMainWindow(), tr("Conflict Warning"),
            msg, tr("Bind Key"), QObject::tr("Cancel"), 0))
    {
        return false;
    }

    return true;
}

/** \fn MythControls::AddKeyToAction(void)
 *  \brief Add a key to the currently selected action.
 *
 *  TODO FIXME This code needs work to support deleteKey
 *             in any mode exc. Context/Action
 *  TODO FIXME This code needs work to deal with multiple
 *             binding conflicts.
 */
void MythControls::AddKeyToAction(void)
{
    /* grab a key from the user */
    KeyGrabPopupBox getkey(gContext->GetMainWindow());
    if (0 == getkey.ExecPopup(&getkey, SLOT(Cancel())))
        return; // user hit Cancel button

    QString     key     = getkey.GetCapturedKey();
    QString     action  = GetCurrentAction();
    QString     context = GetCurrentContext();
    QStringList keys    = m_bindings->GetActionKeys(context, action);

    // Don't recreating an existing binding...
    uint binding_index = GetCurrentButton();
    if ((binding_index >= Action::kMaximumNumberOfBindings) ||
        (keys[binding_index] == key))
    {
        return;
    }

    // Check for first of the potential conflicts.
    int err_level;
    ActionID *conflict = m_bindings->GetConflict(context, key, err_level);
    if (conflict)
    {
        bool ok = ResolveConflict(conflict, err_level);

        delete conflict;

        if (!ok)
            return; // abort on unresolved conflicts
    }

    if (binding_index < keys.count())
    {
        VERBOSE(VB_IMPORTANT, "ReplaceActionKey");
        m_bindings->ReplaceActionKey(context, action, key,
                                     keys[binding_index]);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "AddActionKey");
        m_bindings->AddActionKey(context, action, key);
    }

    RefreshKeyBindings();
    RefreshKeyInformation();
}

/** \fn MythControls::AddBindings(QDict<Binding>&, const QString&,
                                  const QString&, int)
 *  \brief Add bindings to QDict<Binding> for specified context
 *  \param bindings the QDict to which to add the bindings
 *  \param context the context to grab keybindings from
 *  \param contextParent the context whose keybindings are being calculated
 *  \param bindlevel the bind level associated with this context
 */
void MythControls::AddBindings(QDict<Binding> &bindings,
                               const QString    &context,
                               const QString    &contextParent,
                               int               bindlevel)
{
    QStringList actions = m_bindings->GetActions(context);

    for (uint i = 0; i < actions.size(); i++)
    {
        QString action = actions[i];
        QStringList keys = m_bindings->GetActionKeys(context, action);

        for (uint j = 0; j < keys.size(); j++)
        {
            Binding *b = bindings.find(keys[j]);

            if (!b) 
            {
                b = new Binding(keys[j], contextParent,
                                context, action, bindlevel);

                bindings.insert(keys[j], b);
            }
            else if (b->bindlevel == bindlevel)
            {
                b->action += ", " + action;
            }
        }
    }
}

/** \fn MythControls::GetKeyBindings(const QString&)
 *  \brief Create a BindingList for the specified context
 *  \param context the context for which a BindingList should be created
 *  \return a BindingList with "auto delete" property enabled.
 */
BindingList *MythControls::GetKeyBindings(const QString &context)
{
    QDict<Binding> bindings;
    for (uint i = 0; i < m_sortedContexts.size(); i++)
        AddBindings(bindings, m_sortedContexts[i], context, i);

    QStringList keys;
    for (QDictIterator<Binding> it(bindings); it.current(); ++it)
        keys.append(it.currentKey());

    SortKeyList(keys);

    BindingList *blist = new BindingList();
    blist->setAutoDelete(true);

    QStringList::const_iterator kit = keys.begin();
    for (; kit != keys.end(); ++kit)
        blist->append(bindings[*kit]);

    return blist;
}

/** \fn MythControls::RefreshKeyBindings(void)
 *  \brief Refresh binding information
 */
void MythControls::RefreshKeyBindings(void)
{
    m_contextToBindingsMap.clear();
    m_keyToBindingsMap.clear();
    m_contextToBindingsMap.setAutoDelete(true);
    m_keyToBindingsMap.setAutoDelete(true);

    for (uint i = 0; i < m_sortedContexts.size(); i++)
    {
        QString context = m_sortedContexts[i];
        BindingList *list = GetKeyBindings(context);
        m_contextToBindingsMap.insert(context, list);

        BindingList::const_iterator it = list->begin();
        for (; it != list->end(); ++it)
        {
            BindingList *list = m_keyToBindingsMap.find((*it)->key);

            if (!list)
            {
                list = new BindingList();
                m_keyToBindingsMap.insert((*it)->key, list);
            }

            m_sortedKeys.append((*it)->key);
            list->append(*it);
        }
    }

    SortKeyList(m_sortedKeys);
}

/** \fn MythControls::SortKeyList(QStringList&)
 *  \brief Sort a list of keys, removing duplicates
 *  \param keys the list of keys to sort
 */
void MythControls::SortKeyList(QStringList &keys)
{
    QStringList tmp;
    QStringList::const_iterator it = keys.begin();
    for (; it != keys.end(); ++it)
    {
        QString key     = *it;
        QString keydesc = (key.left(6) == "remote") ? "0 " : "3 ";

        keydesc = ((key.length() > 1) && !(key.left(6) == "remote") &&
                   (key.find("+", 1) >= 0)) ? "4 " : keydesc;

        if (key.length() == 1)
        {
            QChar::Category cat = key[0].category();
            keydesc = (QChar::Letter_Uppercase    == cat) ? "2 " : "5 ";
            keydesc = (QChar::Number_DecimalDigit == cat) ? "1 " : keydesc;
        }

        tmp.push_back(keydesc + key);
    }
    tmp.sort();

    keys.clear();

    QString prev = QString::null;
    for (it = tmp.begin(); it != tmp.end(); ++it)
    {
        QString cur = (*it).mid(2);
        if (cur != prev)
        {
            keys.append(cur);
            prev = cur;
        }
    }
}

/// NOTE: This can not be a static method because the QObject::tr()
///       translations do not work reliably in static initializers.
QString MythControls::GetTypeDesc(ListType type) const
{
    switch (type)
    {
        case kContextList:
            return tr("Contexts");
            break;
        case kKeyList:
            return tr("Keys");
            break;
        case kActionList:
            return tr("Actions");
            break;
        default:
            return "";
    }
}

/** \fn MythControls::UpdateLists(void)
 *  \brief Redisplays both the left and right lists and fixes focus issues.
 */
void MythControls::UpdateLists(void)
{
    m_rightList->blockSignals(true);
    m_leftList->blockSignals(true);
    m_leftList->Reset();

    if (m_leftListType == kContextList)
    {
        for (uint i = 0; i < m_sortedContexts.size(); i++)
        {
            UIListBtnTypeItem *item = new UIListBtnTypeItem(
                m_leftList, m_sortedContexts[i]);

            item->setDrawArrow(true);
        }
    }
    else if (m_leftListType == kKeyList)
    {
        for (uint i = 0; i < m_sortedKeys.size(); i++)
        {
            UIListBtnTypeItem *item = new UIListBtnTypeItem(
                m_leftList, key_to_display(m_sortedKeys[i]));

            item->setDrawArrow(true);
        }
    }

    RefreshRightList();

    m_rightList->blockSignals(false);
    m_leftList->blockSignals(false);

    m_leftList->refresh();
    m_rightList->refresh();

    if (m_leftDescription)
        m_leftDescription->SetText(GetTypeDesc(m_leftListType));

    if (m_rightDescription)
        m_rightDescription->SetText(GetTypeDesc(m_rightListType));
}

static QString key_to_display(const QString &key)
{
    if (key.left(6) == "remote")
        return "[" + key.mid(6) + "]";

    return key;
}

static QString display_to_key(const QString &key)
{
    if (key.left(1) == "[" && key != "[")
        return "remote" + key.mid(1, key.length() - 2);

    return key;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
