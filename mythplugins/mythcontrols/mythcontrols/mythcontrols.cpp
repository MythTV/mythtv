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

#ifndef MYTHCONTROLS_CPP
#define MYTHCONTROLS_CPP

/* QT includes */
#include <qnamespace.h>
#include <qstringlist.h>
#include <qapplication.h>
#include <qbuttongroup.h>

/* MythTV includes */
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/themedmenu.h>

using namespace std;

#include "mythcontrols.h"
#include "keygrabber.h"



/* comments in header */
MythControls::MythControls (MythMainWindow *parent, bool& ui_ok)
    :MythThemedDialog(parent, "controls", "controls-", "controls")
{
    /* Nullify keybindings so the deconstructor knows not to delete it */
    this->key_bindings = NULL;

    /* delete the contents when we're done */
    m_contexts.setAutoDelete(true);
    
    /* load up the ui components */
    if ((ui_ok = loadUI()))
    {
        /* for starters, load this host */
        loadHost(gContext->GetHostName());

        /* update the information */
        refreshKeyInformation();

        /* capture the signals we want */
        connect(ContextList, SIGNAL(itemSelected(UIListBtnTypeItem*)),
                this, SLOT(contextSelected(UIListBtnTypeItem*)));
        connect(ActionList, SIGNAL(itemSelected(UIListBtnTypeItem*)),
                this, SLOT(actionSelected(UIListBtnTypeItem*)));

    }
}



/* comments in header */
MythControls::~MythControls() { delete this->key_bindings; }



/* comments in header */
bool MythControls::loadUI()
{
    /* the return value of the method */
    bool retval = true;

    /* Get the UI widgets that we need to work with */
    if ((description = getUITextType("description")) == NULL)
    {
        VERBOSE(VB_ALL, "MythControls: Unable to load action_description");
        retval = false;
    }

    if ((container = getContainer("controls")) == NULL) {
        VERBOSE(VB_ALL, "MythControls:  No controls container in theme");
        retval = false;
    }
    else if ((ContextList = getUIListBtnType("contextlist")) == NULL) {
        VERBOSE(VB_ALL, "MythControls:  No context_list in theme");
        retval = false;
    }
    else if ((ActionList = getUIListBtnType("actionlist")) == NULL) {
        VERBOSE(VB_ALL, "MythControls:  No ActionList in theme");
        retval = false;
    }
    else {
        /* focus the context list by default */
        focused = ContextList;
        ContextList->calculateScreenArea();
        ContextList->SetActive(true);
        ActionList->calculateScreenArea();
        ActionList->SetActive(false);
    }

    /* Check that all the buttons are there */
    if ((ActionButtons[0] = getUITextButtonType("action_one")) == NULL)
    {
        VERBOSE(VB_ALL, "MythControls: Unable to load first action button");
        retval = false;
    }
    else if ((ActionButtons[1] = getUITextButtonType("action_two")) == NULL)
    {
        VERBOSE(VB_ALL, "MythControls: Unable to load second action button");
        retval = false;
    }
    else if ((ActionButtons[2] = getUITextButtonType("action_three")) == NULL)
    {
        VERBOSE(VB_ALL, "MythControls: Unable to load thrid action button");
        retval = false;
    }
    else if ((ActionButtons[3] = getUITextButtonType("action_four")) == NULL)
    {
        VERBOSE(VB_ALL, "MythControls: Unable to load fourth action button");
        retval = false;
    }

    return retval;
}



/* comments in header */
size_t MythControls::focusedButton(void) const
{
    for (size_t i = 0; i < Action::MAX_KEYS; i++)
        if (focused == ActionButtons[i]) return i;

    return Action::MAX_KEYS;
}


void MythControls::focusButton(int direction)
{
    if (direction == 0)
    {
        focused = ActionButtons[0];
        ActionButtons[0]->takeFocus();
        ActionList->looseFocus();
        ActionList->SetActive(false);
        repaintButtonLists();
    }
    else
    {

        /* change focus by at most one in either direction */
        if (direction > 0) direction = 1;
        else direction = -1;

        /* figure out which button is focused */
        int current = 0;
        if (focused == ActionButtons[1]) current=1;
        else if (focused == ActionButtons[2]) current = 2;
        else if (focused == ActionButtons[3]) current = 3;

        /* determine the new focused button index */
        int newb = current + direction;

        /* focus an existing button */
        if ((newb >= 0) && (newb < Action::MAX_KEYS))
        {
            focused->looseFocus();
            focused = ActionButtons[newb];
            focused->takeFocus();
        }
    }
}



void MythControls::switchListFocus(UIListBtnType *focus, UIListBtnType *unfocus)
{
    /* unfocus a list (setactive(false)) or a button (focused->looseFocus) */
    if (unfocus)
        unfocus->SetActive(false);

    focused->looseFocus();
    focused = focus;
    focus->SetActive(true);
    focus->takeFocus();
    repaintButtonLists();
    refreshKeyInformation();
}



void MythControls::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Controls", e, actions);

    for (size_t i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU" || action == "INFO")
        {
            OptionsMenu popup(gContext->GetMainWindow());
            if (popup.getOption() == OptionsMenu::SAVE) save();
        }
        else if (action == "SELECT")
        {
            if (focused == ContextList)
                switchListFocus(ActionList, ContextList);
            else if (focused == ActionList)
                focusButton(0);
            else {
                ActionMenu popup(gContext->GetMainWindow());
                int result = popup.getOption();
                if (result == ActionMenu::SET) addKeyToAction();
                else if (result == ActionMenu::REMOVE) deleteKey();
            }
        }
        else if (action == "ESCAPE")
        {
            if (focused == ContextList)
            {
                handled = false;
                if (key_bindings->hasChanges())
                {
                    /* prompt user to save changes */
                    UnsavedMenu popup(gContext->GetMainWindow());
                    if (popup.getOption() == UnsavedMenu::SAVE)
                    {
                        save();
                    }
                }
            }
            else if (focused == ActionList)
                switchListFocus(ContextList, ActionList);
            else
                switchListFocus(ActionList, NULL);
        }
        else if (action == "UP")
        {
            if (focused == ContextList)
                ContextList->MoveUp();
            else if (focused == ActionList)
                ActionList->MoveUp();
        }
        else if (action == "DOWN")
        {
            if (focused == ContextList)
                ContextList->MoveDown();
            else if (focused == ActionList)
                ActionList->MoveDown();
        }
        else if (action == "LEFT")
        {
            if (focused==ActionList)
                switchListFocus(ContextList, ActionList);
            else if (focused != ContextList)
                focusButton(-1);
        }
        else if (action == "RIGHT")
        {
            if (focused == ContextList)
                switchListFocus(ActionList, ContextList);
            else if (focused != ActionList)
                focusButton(1);
        }
        else if (action == "PAGEUP")
        {
            if (focused == ContextList)
                ContextList->MoveUp(UIListBtnType::MovePage);
            else if (focused == ActionList)
                ActionList->MoveUp(UIListBtnType::MovePage);
        }
        else if (action == "PAGEDOWN")
        {
            if (focused == ContextList)
                ContextList->MoveDown(UIListBtnType::MovePage);
            else if (focused == ActionList)
                ActionList->MoveDown(UIListBtnType::MovePage);
        }
        else handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void MythControls::contextSelected(UIListBtnTypeItem *item)
{
    ActionList->blockSignals(true);
    refreshActionList(getCurrentContext());
    repaintButtonLists();
    ActionList->blockSignals(false);
}

void MythControls::actionSelected(UIListBtnTypeItem *item)
{
    repaintButtonLists();
    refreshKeyInformation();
}


/* method description in header */
void MythControls::repaintButtonLists(void)
{
    /* why am i getting funky colour ? */
    QPixmap pix(container->GetAreaRect().size());
    pix.fill(this, container->GetAreaRect().topLeft());
    QPainter p(&pix);
    if (container)
    {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, container->GetAreaRect().left(),
           container->GetAreaRect().top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}



/* method description in header */
void MythControls::refreshActionList(const QString & context)
{
    ActionList->Reset();

    /* add all of the actions to the context list */
    QStringList *actions = m_contexts[getCurrentContext()];
    UIListBtnTypeItem *item;
    for (size_t i = 0; i < actions->size(); i++)
        item = new UIListBtnTypeItem(ActionList, (*actions)[i]);
}



/* comments in header */
void MythControls::refreshKeyInformation()
{
    /* get the description of the current action */
    QString desc;

    if (focused == ContextList)
    {
        /* blank all keys on the context */
        for (size_t i = 0; i < Action::MAX_KEYS; i++)
            ActionButtons[i]->setText("");
    }
    else
    {
        /* set the description */
        desc = key_bindings->getActionDescription(getCurrentContext(),
                                                  getCurrentAction());

        /* get the bindings of the current action */
        QStringList keys = key_bindings->getActionKeys(getCurrentContext(),
                                                       getCurrentAction());

        size_t i;

        /* fill existing keys */
        for (i = 0; i < keys.count(); i++)
            ActionButtons[i]->setText(keys[i]);

        /* blank the other ones */
        for (; i < Action::MAX_KEYS; i++)
            ActionButtons[i]->setText("");
    }

    /* set the information */
    description->SetText(desc);
}



/* comments in header */
QString MythControls::getCurrentContext(void) const
{
    return ContextList->GetItemCurrent()->text();
}



/* comments in header */
QString MythControls::getCurrentAction(void) const
{
    return (focused != ContextList) ? ActionList->GetItemCurrent()->text() : "";
}



/* comments in header */
void MythControls::loadHost(const QString & hostname) {

    /* create the key bindings and the tree */
    key_bindings = new KeyBindings(hostname);
    QStringList * context_names = key_bindings->getContexts();

    /* Alphabetic order, but jump and global at the top  */
    context_names->sort();
    context_names->remove(JUMP_CONTEXT);
    context_names->remove(GLOBAL_CONTEXT);
    context_names->insert(context_names->begin(), 1, GLOBAL_CONTEXT);
    context_names->insert(context_names->begin(), 1, JUMP_CONTEXT);

    UIListBtnTypeItem *item;
    QStringList *actions;
    for (size_t i = 0; i < context_names->size(); i++)
    {
        item = new UIListBtnTypeItem(ContextList, (*context_names)[i]);
        item->setDrawArrow(true);
        actions = key_bindings->getActions((*context_names)[i]);
        actions->sort();
        m_contexts.insert((*context_names)[i], actions);
    }

    refreshActionList((*context_names)[0]);
    free(context_names);
}



/* comments in header */
void MythControls::deleteKey()
{
    size_t b = focusedButton();
    QString action = getCurrentAction(), context = getCurrentContext();
    QStringList keys = key_bindings->getActionKeys(context, action);
    if (b < keys.count())
    {
        if (!key_bindings->removeActionKey(context, action, keys[b]))
        {
            InvalidBindingPopup popup(gContext->GetMainWindow());
            popup.getOption();
        }
        else refreshKeyInformation();
    }
}



/* method description in header */
bool MythControls::resolveConflict(ActionID *conflict, int level)
{
    MythMainWindow *window = gContext->GetMainWindow();

    /* prevent a fatal binding */
    if (level == KeyBindings::Error)
    {
        InvalidBindingPopup popup(gContext->GetMainWindow(),
                                  conflict->action(),
                                  conflict->context());
        popup.getOption();
        return false;
    }
    else
    {
        /* warn the user that this could conflict */
        QString message = "This kebinding may conflict with ";
        message += conflict->action() + " in the " + conflict->context();
        message += " context.  Do you want to bind it anyways?";

        if (MythPopupBox::show2ButtonPopup(window, "Conflict Warning",
                                           message,"Bind Key","Cancel",0))
            return false;
    }

    return true;
}



/* method description in header */
void MythControls::addKeyToAction(void)
{
    /* grab a key from the user */
    KeyGrabPopupBox *kg = new KeyGrabPopupBox(gContext->GetMainWindow());
    int result = kg->ExecPopup(kg,SLOT(cancel()));
    QString key = kg->getCapturedKey();
    delete kg;

    /* go no further if canceled */
    if (result == 0) return;

    /* get the keys for the selected action */
    size_t b = focusedButton();
    QString action = getCurrentAction(), context = getCurrentContext();
    QStringList keys = key_bindings->getActionKeys(context, action);

    /* dont bother rebinding the same key */
    if (keys[b] == key) return;

    bool bind = true;
    int level;

    /* get the potential conflict */
    ActionID *conflict = NULL;
    if ((conflict = key_bindings->conflicts(context, key, level)))
        bind = resolveConflict(conflict, level);

    delete conflict;

    /* dont bind if we shouldn't bind */
    if (!bind) return;

    /* finally bind or rebind a key to the action */
    if (b < keys.count())
        key_bindings->replaceActionKey(context,action, key, keys[b]);
    else
        key_bindings->addActionKey(context, action, key);

    refreshKeyInformation();
}

#endif /* MYTHCONTROLS_CPP */
