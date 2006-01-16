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


static QMap<int,QString> FindContexts(const QString &context)
{
    QMap<int,QString> retval;
    retval.clear();
    if (context != JUMP_CONTEXT) retval[-1] = JUMP_CONTEXT;
    retval[0] = context;
    if (context != JUMP_CONTEXT && context != GLOBAL_CONTEXT)
    {
        if (context == "TV Editting")
            retval[1] = "TV Playback";
        retval[2] = GLOBAL_CONTEXT;
        if (context != "qt")
            retval[3] = "qt";
    }
    return retval;
}

static const QString KeyToDisplay(const QString key)
{
    if (key.left(6) == "remote")
        return "[" + key.mid(6) + "]";
    else
        return key;
}

static const QString DisplayToKey(const QString key)
{
    if (key.left(1) == "[" && key != "[")
        return "remote" + key.mid(1,key.length()-2);
    else
        return key;
}

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
        leftType = kContextList;
        rightType = kActionList;

        /* for starters, load this host */
        loadHost(gContext->GetHostName());

        /* update the information */
        refreshKeyInformation();

        /* capture the signals we want */
        connect(LeftList, SIGNAL(itemSelected(UIListBtnTypeItem*)),
                this, SLOT(leftSelected(UIListBtnTypeItem*)));
        connect(RightList, SIGNAL(itemSelected(UIListBtnTypeItem*)),
                this, SLOT(rightSelected(UIListBtnTypeItem*)));

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
        VERBOSE(VB_IMPORTANT, "MythControls: Unable to load action_description");
        retval = false;
    }

    if ((container = getContainer("controls")) == NULL) {
        VERBOSE(VB_IMPORTANT, "MythControls:  No controls container in theme");
        retval = false;
    }
    else if ((LeftList = getUIListBtnType("leftlist")) == NULL) {
        VERBOSE(VB_IMPORTANT, "MythControls:  No leftlist in theme");
        retval = false;
    }
    else if ((RightList = getUIListBtnType("rightlist")) == NULL) {
        VERBOSE(VB_IMPORTANT, "MythControls:  No rightList in theme");
        retval = false;
    }
    else {
        LeftDesc = getUITextType("leftdesc");
        RightDesc = getUITextType("rightdesc");
        /* focus the context list by default */
        focused = LeftList;
        LeftList->calculateScreenArea();
        LeftList->SetActive(true);
        RightList->calculateScreenArea();
        RightList->SetActive(false);
    }

    /* Check that all the buttons are there */
    if ((ActionButtons[0] = getUITextButtonType("action_one")) == NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythControls: Unable to load first action button");
        retval = false;
    }
    else if ((ActionButtons[1] = getUITextButtonType("action_two")) == NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythControls: Unable to load second action button");
        retval = false;
    }
    else if ((ActionButtons[2] = getUITextButtonType("action_three")) == NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythControls: Unable to load thrid action button");
        retval = false;
    }
    else if ((ActionButtons[3] = getUITextButtonType("action_four")) == NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythControls: Unable to load fourth action button");
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
    if (leftType != kContextList || rightType != kActionList)
        return;
    if (direction == 0)
    {
        focused = ActionButtons[0];
        ActionButtons[0]->takeFocus();
        RightList->looseFocus();
        RightList->SetActive(false);
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
    refreshKeyInformation();
}

void MythControls::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    bool escape = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Controls", e, actions);

    for (size_t i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU" || action == "INFO")
        {
            focused->looseFocus();
            OptionsMenu popup(gContext->GetMainWindow());
            int a = (int)popup.getOption();
            switch (a) {
                case (int)OptionsMenu::SAVE:
                    save();
                    break;
            }
            focused->takeFocus();
//            if (popup.getOption() == OptionsMenu::SAVE) save();
        }
        else if (action == "SELECT")
        {
            if (focused == LeftList)
                switchListFocus(RightList, LeftList);
            else if (focused == RightList)
                focusButton(0);
            else {
                QString key = getCurrentKey();
                if (!key.isEmpty())
                {
                    ActionMenu popup(gContext->GetMainWindow());
                    int result = popup.getOption();
                    if (result == ActionMenu::SET) addKeyToAction();
                    else if (result == ActionMenu::REMOVE) deleteKey();
                } else // for blank keys, no reason to ask what to do
                    addKeyToAction();
            }
        }
        else if (action == "ESCAPE")
        {
            escape = true;
            if (focused == LeftList)
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
            else if (focused == RightList)
                switchListFocus(LeftList, RightList);
            else
                switchListFocus(RightList, NULL);
        }
        else if (action == "UP")
        {
            if (focused == LeftList)
                LeftList->MoveUp();
            else if (focused == RightList)
                RightList->MoveUp();
        }
        else if (action == "DOWN")
        {
            if (focused == LeftList)
                LeftList->MoveDown();
            else if (focused == RightList)
                RightList->MoveDown();
        }
        else if (action == "LEFT")
        {
            if (focused==RightList)
                switchListFocus(LeftList, RightList);
            else if (focused != LeftList)
                focusButton(-1);
        }
        else if (action == "RIGHT")
        {
            if (focused == LeftList)
                switchListFocus(RightList, LeftList);
            else if (focused != RightList)
                focusButton(1);
        }
        else if (action == "PAGEUP")
        {
            if (focused == LeftList)
                LeftList->MoveUp(UIListBtnType::MovePage);
            else if (focused == RightList)
                RightList->MoveUp(UIListBtnType::MovePage);
        }
        else if (action == "PAGEDOWN")
        {
            if (focused == LeftList)
                LeftList->MoveDown(UIListBtnType::MovePage);
            else if (focused == RightList)
                RightList->MoveDown(UIListBtnType::MovePage);
        }
        else if (action == "1")
        {
            if (leftType != kContextList || rightType != kActionList)
            {
                leftType = kContextList;
                rightType = kActionList;
                updateLists();
                if (focused != LeftList)
                    switchListFocus(LeftList,
                                    (focused == RightList) ? RightList : NULL);
            } else handled = false;
        }
        else if (action == "2")
        {
            if (leftType != kContextList || rightType != kKeyList)
            {
                leftType = kContextList;
                rightType = kKeyList;
                updateLists();
                if (focused != LeftList)
                    switchListFocus(LeftList,
                                    (focused == RightList) ? RightList : NULL);
            } else handled = false;
        }
        else if (action == "3")
        {
            if (leftType != kKeyList || rightType != kContextList)
            {
                leftType = kKeyList;
                rightType = kContextList;
                updateLists();
                if (focused != LeftList)
                    switchListFocus(LeftList,
                                    (focused == RightList) ? RightList : NULL);
            } else handled = false;
        }
        else handled = false;
    }

    if (handled) return;

    if (!escape && JumpTo(e)) handled = true;

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

bool MythControls::JumpTo(QKeyEvent *e)
{
    UIListBtnType *list = NULL;
    if (focused == LeftList && leftType == kKeyList) list = LeftList;
    if (focused == RightList && rightType == kKeyList) list = RightList;
    if (!list) return false;

    QString key = e->text();
    if (key.left(6) == "remote") {
        key = KeyToDisplay(key);
    } else {
        key = QString(QKeySequence(e->key()));
        if (key.isEmpty()) return false;
        QString modifiers = "";
        if (e->state()&Qt::ShiftButton) modifiers+="Shift+";
        if (e->state()&Qt::ControlButton) modifiers+="Ctrl+";
        if (e->state()&Qt::AltButton) modifiers+="Alt+";
        if (e->state()&Qt::MetaButton) modifiers+="Meta+";
        key = modifiers + key;
    }

    UIListBtnTypeItem *b;
    uint len = 1024; // infinity
    if (list == RightList)
    {
        key = key + " ";
        len = key.length();
    }

    for (b = list->GetItemFirst(); b; b = list->GetItemNext(b))
        if (b->text().left(len) == key) break;
    if (!b) return false;

    int curpos = list->GetItemPos(list->GetItemCurrent());
    int newpos = list->GetItemPos(b);

    if (newpos > curpos)
        list->MoveDown(newpos - curpos);
    else if (newpos < curpos)
        list->MoveUp(curpos - newpos);
    return true;
}


void MythControls::leftSelected(UIListBtnTypeItem*)
{
    LeftList->refresh();
    RightList->blockSignals(true);
    refreshRightList();
    RightList->blockSignals(false);
    RightList->refresh();
}

void MythControls::rightSelected(UIListBtnTypeItem*)
{
    RightList->refresh();
    refreshKeyInformation();
}



/* method description in header */
void MythControls::refreshRightList()
{
    RightList->Reset();

    if (LeftList->GetItemCurrent() == NULL)
        return;

    if (leftType == kContextList)
    {
        if (rightType == kActionList)
        {
            /* add all of the actions to the context list */
            QString context = LeftList->GetItemCurrent()->text();
            QStringList *actions = m_contexts[context];
            if (actions == NULL)
            {
                VERBOSE(VB_IMPORTANT, QString("MythControls: Unable to find actions for context %1").arg(context));
                return;
            }
            UIListBtnTypeItem *item;
            for (size_t i = 0; i < actions->size(); i++)
                item = new UIListBtnTypeItem(RightList, (*actions)[i]);
        }
        else if (rightType == kKeyList)
        {
            /* add all of the actions to the context list */
            QString context = LeftList->GetItemCurrent()->text();
            BindingList *list = contextKeys[context];
            if (list == NULL)
            {
                VERBOSE(VB_IMPORTANT, QString("MythControls: Unable to find keys for context %1").arg(context));
                return;
            }
            UIListBtnTypeItem *item;
            for (BindingList::iterator it = list->begin(); it != list->end(); ++it)
            {
                binding_t *b = *it;
                item = new UIListBtnTypeItem(RightList, KeyToDisplay(b->key) + " => " + b->action);
            }
        }
    } else if (leftType == kKeyList && rightType == kContextList)
    {
        QString key = DisplayToKey(LeftList->GetItemCurrent()->text());
        BindingList *list = keyActions[key];
        if (list == NULL)
        {
            VERBOSE(VB_IMPORTANT, QString("MythControls: Unable to find actions for key %1").arg(key));
            return;
        }
        UIListBtnTypeItem *item;
        BindingList::iterator it = list->begin();
        binding_t *b = *it;
        for (size_t i = 0; i < contexts.size(); i++)
        {
            QString context = contexts[i];
            QString action = "<none>";
            if (b && b->context == context)
            {
                action = b->action;
                ++it;
                if (it != list->end()) b = *it;
                else b = NULL;
            }
            item = new UIListBtnTypeItem(RightList, context + " => " + action);
        }
    }
}


/* comments in header */
void MythControls::refreshKeyInformation()
{
    /* get the description of the current action */
    QString desc;

    if (focused == LeftList)
    {
        /* blank all keys on the context */
        for (size_t i = 0; i < Action::MAX_KEYS; i++)
            ActionButtons[i]->setText("");
    }
    else if (leftType == kKeyList || rightType == kKeyList)
    { // Should show appropriate description 
        QString action = getCurrentAction();
        QString context = getCurrentContext();
        /* blank all keys on the context */
        for (size_t i = 0; i < Action::MAX_KEYS; i++)
            ActionButtons[i]->setText("");
        if (!action.isEmpty())
            {
    
            desc = key_bindings->getActionDescription(context, action);
    
            BindingList *list = NULL;
            if (leftType == kKeyList && rightType == kContextList)
            {
                QString key = getCurrentKey();
                list = keyActions[DisplayToKey(key)];
            }
            else if (leftType == kContextList && rightType == kKeyList)
                list = contextKeys[context];
            if (list)
            {
                QString searchKey;
                if (rightType == kContextList)
                    searchKey = context;
                else if (rightType == kActionList)
                    searchKey = action;
                else if (rightType == kKeyList)
                    searchKey = DisplayToKey(getCurrentKey());
                binding_t *binding = NULL;
                for (BindingList::iterator it = list->begin(); it != list->end(); ++it)
                {
                    binding_t *b = *it;
                    switch (rightType)
                    {
                        case kContextList:
                            if (b->context == searchKey) binding = b;
                            break;
                        case kActionList:
                            if (b->action == searchKey) binding = b;
                            break;
                        case kKeyList:
                            if (b->key == searchKey) binding = b;
                            break;
                    }
                    if (binding) break;
                }
    
                if (binding)
                {
                    if (desc.isEmpty() && context != binding->contextFrom)
                        desc = key_bindings->getActionDescription(binding->contextFrom, action);
                    desc += "\n" + tr("Binding comes from %1 context")
                            .arg(binding->contextFrom);
                }
            }
        }
    } else {
        QString context = getCurrentContext();
        QString action = getCurrentAction();
        /* set the description */
        desc = key_bindings->getActionDescription(getCurrentContext(),
                                                  getCurrentAction());

        /* get the bindings of the current action */
        QStringList keys = key_bindings->getActionKeys(getCurrentContext(),
                                                       getCurrentAction());

        size_t i;

        /* fill existing keys */
        for (i = 0; i < keys.count(); i++)
            ActionButtons[i]->setText(KeyToDisplay(keys[i]));

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
    if (leftType == kContextList)
        return LeftList->GetItemCurrent()->text();
    if (focused == LeftList) return "";

    QString desc = RightList->GetItemCurrent()->text();
    int loc = desc.find(" => ");
    if (loc == -1) return ""; // Should not happen
    if (rightType == kContextList) return desc.left(loc);
    else return desc.mid(loc+4);
}

/* comments in header */
QString MythControls::getCurrentAction(void) const
{
    if (leftType == kActionList)
        return LeftList->GetItemCurrent()->text();
    if (focused == LeftList) return "";

    QString desc = RightList->GetItemCurrent()->text();
    if (leftType == kContextList && rightType == kActionList)
        return desc;
    int loc = desc.find(" => ");
    if (loc == -1) return ""; // Should not happen
    if (rightType == kActionList) return desc.left(loc);
    else
    {
        QString rv = desc.mid(loc+4);
        if (rv == "<none>") return "";
        else return rv;
    }
}

/* comments in header */
QString MythControls::getCurrentKey(void) const
{
    if (leftType == kKeyList)
        return LeftList->GetItemCurrent()->text();
    if (focused == LeftList) return "";

    if (leftType == kContextList && rightType == kActionList)
    {
        QString context = getCurrentContext();
        QString action = getCurrentAction();
        size_t b = focusedButton();
        QStringList keys = key_bindings->getActionKeys(context, action);
        if (b < keys.count()) return keys[b];
        else return "";
    }

    QString desc = RightList->GetItemCurrent()->text();
    int loc = desc.find(" => ");
    if (loc == -1) return ""; // Should not happen
    if (rightType == kKeyList) return desc.left(loc);
    else return desc.mid(loc+4);
}


/* comments in header */
void MythControls::loadHost(const QString & hostname) {

    /* create the key bindings and the tree */
    key_bindings = new KeyBindings(hostname);
    contexts = *key_bindings->getContexts();

    keys.clear();

    /* Alphabetic order, but jump and global at the top  */
    contexts.sort();
    contexts.remove(JUMP_CONTEXT);
    contexts.remove(GLOBAL_CONTEXT);
    contexts.insert(contexts.begin(), 1, GLOBAL_CONTEXT);
    contexts.insert(contexts.begin(), 1, JUMP_CONTEXT);

    QStringList *actions;
    for (size_t i = 0; i < contexts.size(); i++)
    {
        actions = key_bindings->getActions(contexts[i]);
        actions->sort();
        m_contexts.insert(contexts[i], actions);
    }

    refreshKeyBindings();
    updateLists();
}



/* comments in header */
void MythControls::deleteKey()
{
    // This code needs work to support deleteKey in any mode exc. Context/Action
    QString context = getCurrentContext();
    QString key = getCurrentKey();
    QString action = getCurrentAction();
    if (context.isEmpty() || key.isEmpty() || action.isEmpty())
    {
        InvalidBindingPopup popup(gContext->GetMainWindow());
        popup.getOption();
        return;
    }

    BindingList *list = keyActions[key];
    binding_t *binding = NULL;
    for (BindingList::iterator it = list->begin(); it != list->end(); ++it)
    {
        binding_t *b = *it;
        if (b->context == context) binding = b;
    }
    if (!binding)
    {
        InvalidBindingPopup popup(gContext->GetMainWindow());
        popup.getOption();
        return;
    }

    if (binding->contextFrom != context)
    {
        ConfirmMenu popup(gContext->GetMainWindow(), tr("Delete this key binding from context %1?").arg(binding->contextFrom));
        if (popup.getOption() != ConfirmMenu::CONFIRM) return;
    } else {
        ConfirmMenu popup(gContext->GetMainWindow(), tr("Delete this binding?"));
        if (popup.getOption() != ConfirmMenu::CONFIRM) return;
    }

    if (!key_bindings->removeActionKey(binding->contextFrom, action, key))
    {
        InvalidBindingPopup popup(gContext->GetMainWindow());
        popup.getOption();
        return;
    }

    // refreshing everything is overkill.  I tried incrementally updating, but the
    // code was ugly.  Since this is quick in my experience, overkill away!
    refreshKeyBindings();
    refreshKeyInformation();
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
    // This code needs work to support deleteKey in any mode exc. Context/Action
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

    refreshKeyBindings();
    refreshKeyInformation();
}

void MythControls::addBindings(QDict<binding_t> &bindings, const QString &context, const QString &contextParent, int bindlevel)
{
    QStringList *actions = key_bindings->getActions(context);

    for (size_t i = 0; i < actions->size(); i++)
    {
        QString action = (*actions)[i];
        QStringList keys = key_bindings->getActionKeys(context, action);

        for (size_t j = 0; j < keys.size(); j++)
        {
            QString key = keys[j];

            binding_t *b = bindings.find(key);
            if (!b) 
            {
                b = new(binding_t);
                b->key = key;
                b->action = action;
                b->context = contextParent;
                b->contextFrom = context;
                b->bindlevel = bindlevel;
                bindings.insert(key, b);
            }
            else if (b->bindlevel == bindlevel)
            {
                b->action += ", " + action;
            }
        }
    }
}

BindingList *MythControls::getKeyBindings(const QString &context)
{
    QDict<binding_t> bindings;
    bindings.clear();

    QMap<int,QString> contextList = FindContexts(context);
    for (QMap<int,QString>::iterator it = contextList.begin(); it != contextList.end(); ++it)
    {
        int level = it.key();
        QString curcontext = it.data();
        addBindings(bindings, curcontext, context, level);
    }

    QStringList keys;

    for (QDictIterator<binding_t> it(bindings); it.current(); ++it)
    {
        QString key = it.currentKey();
        keys.append(key);
    }

    sortKeyList(keys);

    BindingList *retval = new BindingList;
    retval->clear();

    for (QStringList::Iterator kit = keys.begin(); kit != keys.end(); ++kit)
    {
        QString key = *kit;
        retval->append(bindings[key]);
    }
    retval->setAutoDelete(true);
    return retval;
}

void MythControls::refreshKeyBindings()
{
    contextKeys.clear();
    keyActions.clear();
    for (size_t i = 0; i < contexts.size(); i++)
    {
        QString context = contexts[i];
        BindingList *list = getKeyBindings(context);
        contextKeys.insert(context, list);
        for (BindingList::iterator it = list->begin(); it != list->end(); ++it)
        {
            binding_t *b = *it;
            BindingList *list = keyActions.find(b->key);
            if (!list)
            {
                list = new BindingList;
                list->clear();
                keyActions.insert(b->key, list);
            }
            keys.append(b->key);
            list->append(b);
        }
    }
    contextKeys.setAutoDelete(true);
    keyActions.setAutoDelete(true);

    sortKeyList(keys);
}

void MythControls::sortKeyList(QStringList &keys)
{
    QStringList t;
    t.clear();

    for ( QStringList::Iterator it = keys.begin(); it != keys.end(); ++it )
    {
        QString key = *it;

        QString keydesc = "3 ";
        if (key.left(6) == "remote")
        {
            keydesc = "0 ";
        }
        else if (key.length() == 1)
        {
            switch (key[0].category())
            {
                case QChar::Letter_Uppercase:
                    keydesc = "2 ";
                    break;
                case QChar::Number_DecimalDigit:
                    keydesc = "1 ";
                    break;
                default:
                    keydesc = "5 ";
                    break;
            }
        }
        else if (key.find("+", 1) != -1)
            keydesc = "4 ";

        t.push_back(keydesc + key);
    }
    t.sort();

    QString prev = "";

    keys.clear();
    for (QStringList::Iterator kit = t.begin(); kit != t.end(); ++kit)
    {
        QString cur = (*kit).mid(2);
        if (cur != prev) 
        {
            keys.append(cur);
            prev = cur;
        }
    }
}

QString MythControls::getTypeDesc(ListType type)
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

void MythControls::updateLists()
{
    RightList->blockSignals(true);
    LeftList->blockSignals(true);
    LeftList->Reset();
    if (leftType == kContextList)
    {
        UIListBtnTypeItem *item;
        for (size_t i = 0; i < contexts.size(); i++)
        {
            item = new UIListBtnTypeItem(LeftList, contexts[i]);
            item->setDrawArrow(true);
        }
    } else if (leftType == kKeyList)
    {
        UIListBtnTypeItem *item;
        for (size_t i = 0; i < keys.size(); i++)
        {
            QString key = KeyToDisplay(keys[i]);
            item = new UIListBtnTypeItem(LeftList, key);
            item->setDrawArrow(true);
        }
    }
    refreshRightList();
    RightList->blockSignals(false);
    LeftList->blockSignals(false);
    LeftList->refresh();
    RightList->refresh();

    if (LeftDesc != NULL)
        LeftDesc->SetText(getTypeDesc(leftType));
    if (RightDesc != NULL)
        RightDesc->SetText(getTypeDesc(rightType));
}


#endif /* MYTHCONTROLS_CPP */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
