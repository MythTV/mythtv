// -*- Mode: c++ -*-
/**
 * @file mythcontrols.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main mythcontrols class.
 *
 * Note that the keybindings are fetched all at once, and cached for
 * this host.  This avoids pelting the database every time the user
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

#include "mythcontrols.h"

// Qt headers
#include <QStringList>
#include <QCoreApplication>

// MythTV headers
#include "mythcorecontext.h"
#include "mythmainwindow.h"

// MythUI headers
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythdialogbox.h"

// MythControls headers
#include "keygrabber.h"

#define LOC QString("MythControls: ")
#define LOC_ERR QString("MythControls, Error: ")

/**
 *  \brief Creates a new MythControls wizard
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythControls::MythControls(MythScreenStack *parent, const char *name)
    : MythScreenType (parent, name)
{
    m_currentView = kActionsByContext;
    m_leftList = m_rightList = NULL;
    m_description = m_leftDescription = m_rightDescription = NULL;
    m_bindings = NULL;

    m_leftListType  = kContextList;
    m_rightListType = kActionList;

    m_menuPopup = NULL;
}

MythControls::~MythControls()
{
    Teardown();
}

void MythControls::Teardown(void)
{
    if (m_bindings)
    {
        delete m_bindings;
        m_bindings = NULL;
    }

    m_contexts.clear();
}

/**
 *  \brief Loads UI elements from theme
 *
 *  \return true if all UI elements load successfully.
 */
bool MythControls::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("controls-ui.xml", "controls", this);

    if (!foundtheme)
        return false;

    m_description = dynamic_cast<MythUIText *>(GetChild("description"));
    m_leftList = dynamic_cast<MythUIButtonList *>(GetChild("leftlist"));
    m_rightList = dynamic_cast<MythUIButtonList *>(GetChild("rightlist"));
    m_leftDescription = dynamic_cast<MythUIText *>(GetChild("leftdesc"));
    m_rightDescription = dynamic_cast<MythUIText *>(GetChild("rightdesc"));

    if (!m_description || !m_leftList || !m_rightList ||
            !m_leftDescription || !m_rightDescription)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_leftList,  SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(LeftSelected(MythUIButtonListItem*)));
    connect(m_leftList,  SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(LeftPressed(MythUIButtonListItem*)));

    connect(m_rightList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(RightSelected(MythUIButtonListItem*)));
    connect(m_rightList,  SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(RightPressed(MythUIButtonListItem*)));
    connect(m_rightList, SIGNAL(TakingFocus()),
            SLOT(RefreshKeyInformation()));

    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
    {
        MythUIButton *button = dynamic_cast<MythUIButton *>
                (GetChild(QString("action_%1").arg(i)));

        if (!button)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Unable to load action button action_%1").arg(i));

            return false;
        }

        connect(button, SIGNAL(Clicked()), SLOT(ActionButtonPressed()));

        m_actionButtons.append(button);
    }

    BuildFocusList();

    LoadData(gCoreContext->GetHostName());

    /* start off with the actions by contexts view */
    m_currentView = kActionsByContext;
    SetListContents(m_leftList, m_bindings->GetContexts(), true);
    UpdateRightList();

    return true;
}

/**
 *  \brief Change button focus in a particular direction
 *  \param direction +1 moves focus to the right, -1 moves to the left,
 *                   and 0 changes the focus to the first button.
 */
void MythControls::ChangeButtonFocus(int direction)
{
    if ((m_leftListType != kContextList) || (m_rightListType != kActionList))
        return;

    if (direction == 0)
        SetFocusWidget(m_actionButtons.at(0));
}

/**
 *  \brief Slot handling a button being pressed in the left list
 */
void MythControls::LeftPressed(MythUIButtonListItem *item)
{
    (void) item;
    NextPrevWidgetFocus(true);
}

/**
 *  \brief Slot handling a button being pressed in the left list
 */
void MythControls::RightPressed(MythUIButtonListItem *item)
{
    (void) item;
    if (m_currentView == kActionsByContext)
        ChangeButtonFocus(0);
}

/**
 *  \brief Slot handling a button being pressed in the left list
 */
void MythControls::ActionButtonPressed()
{
    QString key = GetCurrentKey();
    if (!key.isEmpty())
    {
        QString label = tr("Modify Action");

        MythScreenStack *popupStack =
                                GetMythMainWindow()->GetStack("popup stack");

        m_menuPopup =
                new MythDialogBox(label, popupStack, "actionmenu");

        if (m_menuPopup->Create())
            popupStack->AddScreen(m_menuPopup);

        m_menuPopup->SetReturnEvent(this, "action");

        m_menuPopup->AddButton(tr("Set Binding"));
        m_menuPopup->AddButton(tr("Remove Binding"));
    }
    else // for blank keys, no reason to ask what to do
        GrabKey();
}

/**
 *  \brief Change the view.
 */
void MythControls::ChangeView(void)
{
    QString label = tr("Change View");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup =
            new MythDialogBox(label, popupStack, "mcviewmenu");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "view");

    m_menuPopup->AddButton(tr("Actions By Context"));
    m_menuPopup->AddButton(tr("Contexts By Key"));
    m_menuPopup->AddButton(tr("Keys By Context"));

}

void MythControls::ShowMenu()
{
    QString label = tr("Options");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup =
            new MythDialogBox(label, popupStack, "optionmenu");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "option");

    m_menuPopup->AddButton(tr("Save"));
    m_menuPopup->AddButton(tr("Change View"));
    m_menuPopup->AddButton(tr("Reset All Keys to Defaults"));
}

void MythControls::Close()
{
    if (m_bindings && m_bindings->HasChanges())
    {
        /* prompt user to save changes */
        QString label = tr("Save changes?");

        MythScreenStack *popupStack =
                                GetMythMainWindow()->GetStack("popup stack");

        MythConfirmationDialog *confirmPopup
                    = new MythConfirmationDialog(popupStack, label, true);

        if (confirmPopup->Create())
            popupStack->AddScreen(confirmPopup);

        confirmPopup->SetReturnEvent(this, "exit");
    }
    else
        MythScreenType::Close();
}

/**
 *  \brief Refreshes the right list when an item in the
 *         left list is selected
 */
void MythControls::LeftSelected(MythUIButtonListItem*)
{
    UpdateRightList();
}

/**
 *  \brief Refreshes key information when an item in the
 *         right list is selected
 */
void MythControls::RightSelected(MythUIButtonListItem*)
{
    RefreshKeyInformation();
}


/**
 *  \brief Set the contents of a list.
 *  \param uilist The list being changed.
 *  \param contents The contents of the list.
 *  \param arrows True to draw with arrows, otherwise arrows are not drawn.
 */
void MythControls::SetListContents(
    MythUIButtonList *uilist, const QStringList &contents, bool arrows)
{
    // remove all strings from the current list
    uilist->Reset();

    // add each new string
    QStringList::const_iterator it = contents.begin();
    for (; it != contents.end(); ++it)
    {
        QString tmp = *it; tmp.detach();
        MythUIButtonListItem *item = new MythUIButtonListItem(uilist, tmp);
        item->setDrawArrow(arrows);
    }
}

/**
 *  \brief Update the right list.
 */
void MythControls::UpdateRightList(void)
{
    // get the selected item in the right list.
    MythUIButtonListItem *item = m_leftList->GetItemCurrent();

    if (!item)
        return;

    QString rtstr = item->GetText();

    switch(m_currentView)
    {
    case kActionsByContext:
        SetListContents(m_rightList, m_contexts[rtstr]);
        break;
    case kKeysByContext:
        SetListContents(m_rightList, m_bindings->GetContextKeys(rtstr));
        break;
    case kContextsByKey:
        SetListContents(m_rightList, m_bindings->GetKeyContexts(rtstr));
        break;
    }
}

/**
 *  \brief Updates the list of keys that are shown and the
 *         description of the action.
 */
void MythControls::RefreshKeyInformation(void)
{
    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
        m_actionButtons.at(i)->SetText("");

    if (GetFocusWidget() == m_leftList)
    {
        m_description->Reset();
        return;
    }

    const QString context = GetCurrentContext();
    const QString action  = GetCurrentAction();

    QString desc = m_bindings->GetActionDescription(context, action);
    m_description->SetText(tr(desc.toLatin1().constData()));

    QStringList keys = m_bindings->GetActionKeys(context, action);
    for (int i = 0; (i < keys.count()) &&
             (i < (int)Action::kMaximumNumberOfBindings); i++)
    {
        m_actionButtons.at(i)->SetText(keys[i]);
    }
}


/**
 * \brief Get the currently selected context string.
 *
 *   If no context is selected, an empty string is returned.
 *
 *  \return The currently selected context string.
 */
QString MythControls::GetCurrentContext(void)
{
    if (m_leftListType == kContextList)
        return m_leftList->GetItemCurrent()->GetText();

    if (GetFocusWidget() == m_leftList)
        return QString();

    QString desc = m_rightList->GetItemCurrent()->GetText();
    int loc = desc.indexOf(" => ");
    if (loc == -1)
        return QString(); // Should not happen

    if (m_rightListType == kContextList)
        return desc.left(loc);

    return desc.mid(loc + 4);
}

/**
 *  \brief Get the currently selected action string.
 *
 *   If no action is selected, an empty string is returned.
 *
 *  \return The currently selected action string.
 */
QString MythControls::GetCurrentAction(void)
{
    if (m_leftListType == kActionList)
    {
        if (m_leftList && m_leftList->GetItemCurrent())
        {
            QString tmp = m_leftList->GetItemCurrent()->GetText();
            tmp.detach();
            return tmp;
        }
        return QString();
    }

    if (GetFocusWidget() == m_leftList)
        return QString();

    if (!m_rightList || !m_rightList->GetItemCurrent())
        return QString();

    QString desc = m_rightList->GetItemCurrent()->GetText();
    if (kContextList == m_leftListType &&
        kActionList  == m_rightListType)
    {
        desc.detach();
        return desc;
    }

    int loc = desc.indexOf(" => ");
    if (loc == -1)
        return QString(); // should not happen..

    if (m_rightListType == kActionList)
        return desc.left(loc);

    QString rv = desc.mid(loc+4);
    if (rv == "<none>")
        return QString();

    return rv;
}

/**
 *  \brief Returns the focused button, or
 *         Action::kMaximumNumberOfBindings if no buttons are focued.
 */
uint MythControls::GetCurrentButton(void)
{
    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
    {
        MythUIButton *button = m_actionButtons.at(i);
        MythUIType *uitype = GetFocusWidget();
        if (uitype == button)
            return i;
    }

    return Action::kMaximumNumberOfBindings;
}

/**
 *  \brief Get the currently selected key string
 *
 *   If no key is selected, an empty string is returned.
 *
 *  \return The currently selected key string
 */
QString MythControls::GetCurrentKey(void)
{
    MythUIButtonListItem* currentButton;
    if (m_leftListType == kKeyList &&
        (currentButton = m_leftList->GetItemCurrent()))
    {
        return currentButton->GetText();
    }

    if (GetFocusWidget() == m_leftList)
        return QString();

    if ((m_leftListType == kContextList) && (m_rightListType == kActionList))
    {
        QString context = GetCurrentContext();
        QString action = GetCurrentAction();
        uint b = GetCurrentButton();
        QStringList keys = m_bindings->GetActionKeys(context, action);

        if (b < (uint)keys.count())
            return keys[b];

        return QString();
    }

    currentButton = m_rightList->GetItemCurrent();
    QString desc;
    if (currentButton)
        desc = currentButton->GetText();

    int loc = desc.indexOf(" => ");
    if (loc == -1)
        return QString(); // Should not happen


    if (m_rightListType == kKeyList)
        return desc.left(loc);

    return desc.mid(loc + 4);
}

/**
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
    m_sortedContexts.removeAll(ActionSet::kJumpContext);
    m_sortedContexts.removeAll(ActionSet::kGlobalContext);
    m_sortedContexts.insert(m_sortedContexts.begin(),
                            ActionSet::kGlobalContext);
    m_sortedContexts.insert(m_sortedContexts.begin(),
                            ActionSet::kJumpContext);

    QStringList::const_iterator it = m_sortedContexts.begin();
    for (; it != m_sortedContexts.end(); ++it)
    {
        QString ctx_name = *it;
        ctx_name.detach();
        QStringList actions = m_bindings->GetActions(ctx_name);
        actions.sort();
        m_contexts.insert(ctx_name, actions);
    }
}

/**
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
        LOG(VB_GENERAL, LOG_ERR,
            "Unable to delete binding, missing information");
        return;
    }

    if (m_bindings->RemoveActionKey(context, action, key))
    {
        RefreshKeyInformation();
        return;
    }

    QString label = tr("This action is mandatory and needs at least one key "
                       "bound to it. Instead, try rebinding with another key.");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    MythConfirmationDialog *confirmPopup =
            new MythConfirmationDialog(popupStack, label, false);

    if (confirmPopup->Create())
    {
        confirmPopup->SetReturnEvent(this, "mandatorydelete");
        popupStack->AddScreen(confirmPopup);
    }
    else
        delete confirmPopup;
}

/**
 *  \brief Resolve a potential conflict
 *  \return true if the conflict should be bound, false otherwise.
 */
void MythControls::ResolveConflict(ActionID *conflict, int error_level,
                                    const QString &key)
{
    if (!conflict)
        return;

    QString label;

    bool error = (KeyBindings::kKeyBindingError == error_level);

    if (error)
        label = tr("This key binding conflicts with %1 in the %2 context. "
                   "Unable to bind key.")
                    .arg(conflict->GetAction()).arg(conflict->GetContext());
    else
        label = tr("This key binding conflicts with %1 in the %2 context. "
                   "Do you want to bind it anyway?")
                    .arg(conflict->GetAction()).arg(conflict->GetContext());

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    MythConfirmationDialog *confirmPopup =
            new MythConfirmationDialog(popupStack, label, !error);

    if (!error)
    {
        confirmPopup->SetData(qVariantFromValue(key));
        confirmPopup->SetReturnEvent(this, "conflict");
    }

    if (confirmPopup->Create())
        popupStack->AddScreen(confirmPopup);

    delete conflict;
}

void MythControls::GrabKey(void)
{
    /* grab a key from the user */
    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    KeyGrabPopupBox *keyGrabPopup = new KeyGrabPopupBox(popupStack);

    if (keyGrabPopup->Create())
        popupStack->AddScreen(keyGrabPopup, false);

    connect(keyGrabPopup, SIGNAL(HaveResult(QString)),
            SLOT(AddKeyToAction(QString)), Qt::QueuedConnection);
}

/**
 *  \brief Add a key to the currently selected action.
 *
 *  TODO FIXME This code needs work to support deleteKey
 *             in any mode exc. Context/Action
 *  TODO FIXME This code needs work to deal with multiple
 *             binding conflicts.
 */
void MythControls::AddKeyToAction(QString key, bool ignoreconflict)
{
    QString     action  = GetCurrentAction();
    QString     context = GetCurrentContext();
    QStringList keys    = m_bindings->GetActionKeys(context, action);

    // Don't recreating an existing binding...
    int binding_index = GetCurrentButton();
    if ((binding_index >= (int)Action::kMaximumNumberOfBindings) ||
        ((binding_index < keys.size()) && (keys[binding_index] == key)))
    {
        return;
    }

    if (!ignoreconflict)
    {
        // Check for first of the potential conflicts.
        int err_level;
        ActionID *conflict = m_bindings->GetConflict(context, key, err_level);
        if (conflict)
        {
            ResolveConflict(conflict, err_level, key);

            return;
        }
    }

    if (binding_index < keys.count())
        m_bindings->ReplaceActionKey(context, action, key,
                                     keys[binding_index]);
    else
        m_bindings->AddActionKey(context, action, key);

    RefreshKeyInformation();
}

void MythControls::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "action")
        {
            if (buttonnum == 0)
                GrabKey();
            else if (buttonnum == 1)
                DeleteKey();
        }
        else if (resultid == "option")
        {
            if (buttonnum == 0)
                Save();
            else if (buttonnum == 1)
                ChangeView();
            else if (buttonnum == 2)
                GetMythMainWindow()->JumpTo("Reset All Keys");
        }
        else if (resultid == "exit")
        {
            if (buttonnum == 1)
                Save();
            else
                Teardown();

            Close();
        }
        else if (resultid == "view")
        {
            QStringList contents;
            QString leftcaption, rightcaption;

            if (buttonnum == 0)
            {
                leftcaption = tr("Contexts");
                rightcaption = tr("Actions");
                m_currentView = kActionsByContext;
                contents = m_bindings->GetContexts();
            }
            else if (buttonnum == 1)
            {
                leftcaption = tr("Contexts");
                rightcaption = tr("Keys");
                m_currentView = kKeysByContext;
                contents = m_bindings->GetContexts();
            }
            else if (buttonnum == 2)
            {
                leftcaption = tr("Keys");
                rightcaption = tr("Contexts");
                m_currentView = kContextsByKey;
                contents = m_bindings->GetKeys();
            }
            else
                return;

            m_leftDescription->SetText(leftcaption);
            m_rightDescription->SetText(rightcaption);

            SetListContents(m_leftList, contents, true);
            RefreshKeyInformation();
            UpdateRightList();

            if (GetFocusWidget() != m_leftList)
                SetFocusWidget(m_leftList);
        }
        else if (resultid == "conflict")
        {
            if (buttonnum == 1)
            {
                QString key = dce->GetData().toString();
                AddKeyToAction(key, true);
            }
        }

        if (m_menuPopup)
            m_menuPopup = NULL;
    }

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
