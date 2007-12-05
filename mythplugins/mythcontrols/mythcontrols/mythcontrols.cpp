// -*- Mode: c++ -*-
/**
 * @file mythcontrols.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main mythcontrols class.
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

#define LOC QString("MythControls: ")
#define LOC_ERR QString("MythControls, Error: ")
#define CAPTION_CONTEXT QString("Contexts")
#define CAPTION_ACTION QString("Actions")
#define CAPTION_KEY QString("Keys")

/** \fn MythControls::MythControls(MythMainWindow*, bool&)
 *  \brief Creates a new MythControls wizard
 *  \param parent The main myth window.
 *  \param ok Set if UI was correctly loaded, cleared otherwise.
 */
MythControls::MythControls(MythMainWindow *parent, bool &ok)
    : MythThemedDialog(parent, "controls", "controls-", "controls"),
      m_currentView(kActionsByContext),
      m_focusedUIElement(NULL),
      m_leftList(NULL),        m_rightList(NULL),
      m_description(NULL),
      m_leftDescription(NULL), m_rightDescription(NULL),
      m_bindings(NULL),     m_container(NULL)
{
    // MS Windows doesn't like bzero()
    memset(m_actionButtons, 0,
           sizeof(void*) * Action::kMaximumNumberOfBindings);

    m_contexts.setAutoDelete(true);
    
    // Load up the ui components
    ok = LoadUI();
    if (!ok)
        return;

    m_leftListType  = kContextList;
    m_rightListType = kActionList;

    LoadData(gContext->GetHostName());

    /* start off with the actions by contexts view */
    m_currentView = kActionsByContext;
    SetListContents(m_leftList, m_bindings->GetContexts(), true);
    UpdateRightList();

    connect(m_leftList,  SIGNAL(itemSelected( UIListBtnTypeItem*)),
            this,        SLOT(  LeftSelected( UIListBtnTypeItem*)));
    connect(m_rightList, SIGNAL(itemSelected( UIListBtnTypeItem*)),
            this,        SLOT(  RightSelected(UIListBtnTypeItem*)));
}

MythControls::~MythControls()
{
    Teardown();
}

void MythControls::deleteLater(void)
{
    Teardown();
    MythThemedDialog::deleteLater();
}

void MythControls::Teardown(void)
{
    if (m_leftList)
    {
        m_leftList->disconnect();
        m_leftList = NULL;
    }

    if (m_rightList)
    {
        m_rightList->disconnect();
        m_rightList = NULL;
    }

    if (m_bindings)
    {
        delete m_bindings;
        m_bindings = NULL;
    }

    m_contexts.clear();
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

/// \brief Chagne the view.
void MythControls::ChangeView(void)
{
    QStringList buttons;
    buttons += tr("Actions By Context");
    buttons += tr("Contexts By Key");
    buttons += tr("Keys By Context");
    buttons += QObject::tr("Cancel");

    DialogCode code = MythPopupBox::ShowButtonPopup(
        gContext->GetMainWindow(), "mcviewmenu", tr("Change View"),
        buttons, kDialogCodeButton3);

    QStringList contents;
    QString leftcaption, rightcaption;

    switch (code)
    {
        case kDialogCodeButton0:
            leftcaption = tr(CAPTION_CONTEXT);
            rightcaption = tr(CAPTION_ACTION);
            m_currentView = kActionsByContext;
            contents = m_bindings->GetContexts();
            break;
        case kDialogCodeButton1:
            leftcaption = tr(CAPTION_CONTEXT);
            rightcaption = tr(CAPTION_KEY);
            m_currentView = kKeysByContext;
            contents = m_bindings->GetContexts();
            break;
        case kDialogCodeButton2:
            leftcaption = tr(CAPTION_KEY);
            rightcaption = tr(CAPTION_CONTEXT);
            m_currentView = kContextsByKey;
            contents = m_bindings->GetKeys();
            break;
        case kDialogCodeButton3:
        default:
            return;
    }

    m_leftDescription->SetText(leftcaption);
    m_rightDescription->SetText(rightcaption);

    SetListContents(m_leftList, contents, true);
    RefreshKeyInformation();
    UpdateRightList();

    if (m_focusedUIElement != m_leftList)
    {
        m_focusedUIElement->looseFocus();
        m_focusedUIElement = m_leftList;
        m_focusedUIElement->takeFocus();
    }
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

            QStringList buttons;
            buttons += QObject::tr("Save");
            buttons += tr("Change View");
            buttons += QObject::tr("Cancel");

            DialogCode code = MythPopupBox::ShowButtonPopup(
                gContext->GetMainWindow(), "optionmenu", tr("Options"),
                buttons, kDialogCodeButton2);

            switch (code)
            {
                case kDialogCodeButton0:
                    Save();
                    break;
                case kDialogCodeButton1:
                    ChangeView();
                    break;
                case kDialogCodeButton2:
                default:
                    break;
            }

            m_focusedUIElement->takeFocus();
        }
        else if (action == "SELECT")
        {
            if (m_focusedUIElement == m_leftList)
                ChangeListFocus(m_rightList, m_leftList);
            else if (m_focusedUIElement == m_rightList)
            {
                if (m_currentView == kActionsByContext)
                    ChangeButtonFocus(0);
                else
                    handled = false;
            }
            else
            {
                QString key = GetCurrentKey();
                if (!key.isEmpty())
                {
                    QStringList buttons;
                    buttons += tr("Set Binding");
                    buttons += tr("Remove Binding");
                    buttons += QObject::tr("Cancel");

                    DialogCode code = MythPopupBox::ShowButtonPopup(
                        gContext->GetMainWindow(), "actionmenu",
                        tr("Modify Action"), buttons, kDialogCodeButton2);

                    if (kDialogCodeButton0 == code)
                        AddKeyToAction();
                    else if (kDialogCodeButton1 == code)
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
                    QStringList buttons;
                    buttons += tr("Exit without saving changes");
                    buttons += tr("Save then Exit");

                    DialogCode code = MythPopupBox::ShowButtonPopup(
                        gContext->GetMainWindow(), "exitmenu",
                        tr("Exiting, but there are unsaved changes.")+"\n\n"+
                        tr("Which would you prefer?"), buttons,
                        kDialogCodeButton1);

                    if (kDialogCodeButton1 == code)
                        Save();
                    else if (kDialogCodeRejected == code)
                        handled = true;
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
        else
        {
            handled = false;
        }
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

/** \fn MythControls::LeftSelected(UIListBtnTypeItem*)
 *  \brief Refreshes the right list when an item in the
 *         left list is selected
 */
void MythControls::LeftSelected(UIListBtnTypeItem*)
{
    m_leftList->refresh();
    m_rightList->blockSignals(true);
    UpdateRightList();
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


/** \brief Set the contents of a list.
 *  \param uilist The list being changed.
 *  \param contents The contents of the list.
 *  \param arrows True to draw with arrows, otherwise arrows are not drawn.
 */
void MythControls::SetListContents(
    UIListBtnType *uilist, const QStringList &contents, bool arrows)
{
    uilist->blockSignals(true);

    // remove all strings from the current list
    uilist->Reset();

    // add each new string
    for (size_t i = 0; i < contents.size(); i++)
    {
        UIListBtnTypeItem *item = new UIListBtnTypeItem(uilist, contents[i]);
        item->setDrawArrow(arrows);
    }

    uilist->blockSignals(false);
    uilist->refresh();
}

/** \brief Update the right list. */
void MythControls::UpdateRightList(void)
{
    // get the selected item in the right list.
    UIListBtnTypeItem *item = m_leftList->GetItemCurrent();

    if (item != NULL)
    {
        QString rtstr = item->text();

        switch(m_currentView)
        {
        case kActionsByContext:
            SetListContents(m_rightList, *(m_contexts[rtstr]));
            break;
        case kKeysByContext:
            SetListContents(m_rightList, m_bindings->GetContextKeys(rtstr));
            break;
        case kContextsByKey:
            SetListContents(m_rightList, m_bindings->GetKeyContexts(rtstr));
            break;
        }
    }
    else
        VERBOSE(VB_IMPORTANT, QString("Left List Returned Null!"));
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

    const QString context = GetCurrentContext();
    const QString action  = GetCurrentAction();

    QString desc = m_bindings->GetActionDescription(context, action);
    m_description->SetText(desc);

    QStringList keys = m_bindings->GetActionKeys(context, action);
    for (uint i = 0; (i < keys.count()) &&
             (i < Action::kMaximumNumberOfBindings); i++)
    {
        m_actionButtons[i]->setText(keys[i]);
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
    {
        if (m_leftList && m_leftList->GetItemCurrent())
            return QDeepCopy<QString>(m_leftList->GetItemCurrent()->text());
        return QString::null;
    }

    if (m_focusedUIElement == m_leftList)
        return QString::null;

    if (!m_rightList || !m_rightList->GetItemCurrent())
        return QString::null;

    QString desc = m_rightList->GetItemCurrent()->text();
    if (m_leftListType == kContextList && m_rightListType == kActionList)
        return QDeepCopy<QString>(desc);

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

/** \fn MythControls::LoadData(const QString&)
 *  \brief Load the settings for a particular host.
 *  \param hostname The host to load settings for.
 */
void MythControls::LoadData(const QString &hostname)
{
    /* create the key bindings and the tree */
    m_bindings = new KeyBindings(hostname);
    m_sortedContexts = m_bindings->GetContexts();

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
    QString ptitle  = tr("Manditory Action");
    QString pdesc   =
        tr("This action is manditory and needs at least one key "
           "bound to it. Instead, try rebinding with another key.");

    if (context.isEmpty() || key.isEmpty() || action.isEmpty())
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), ptitle, pdesc);
        return;
    }

    bool ok = MythPopupBox::showOkCancelPopup(
        gContext->GetMainWindow(), "confirmdelete",
        tr("Delete this binding?"), true);

    if (!ok)
        return;

    if (!m_bindings->RemoveActionKey(context, action, key))
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), ptitle, pdesc);
        return;
    }

    RefreshKeyInformation();
}

/** \fn ResolveConflict(ActionID*, int)
 *  \brief Resolve a potential conflict
 *  \return true if the conflict should be bound, false otherwise.
 */
bool MythControls::ResolveConflict(ActionID *conflict, int error_level)
{
    if (!conflict)
        return false; // programmer error

    QString msg = tr("This key binding conflicts with %1 in the %2 context.")
        .arg(conflict->GetAction()).arg(conflict->GetContext());

    if (KeyBindings::kKeyBindingError == error_level)
    {
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), tr("Conflicting Binding"), msg);

        return false;
    }

    msg = tr("This key binding may conflict with %1 in the %2 context. "
             "Do you want to bind it anyway?")
        .arg(conflict->GetAction()).arg(conflict->GetContext());

    DialogCode res = MythPopupBox::Show2ButtonPopup(
        gContext->GetMainWindow(), tr("Conflict Warning"),
        msg, tr("Bind Key"), QObject::tr("Cancel"), kDialogCodeButton1);

    return (kDialogCodeButton0 == res);
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
    KeyGrabPopupBox *getkey = new KeyGrabPopupBox(gContext->GetMainWindow());
    DialogCode code = getkey->ExecPopup();
    QString    key  = getkey->GetCapturedKey();
    getkey->deleteLater();
    getkey = NULL;

    if (kDialogCodeRejected == code)
        return; // user hit Cancel button

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

    RefreshKeyInformation();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
