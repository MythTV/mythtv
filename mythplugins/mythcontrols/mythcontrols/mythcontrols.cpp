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
#include <qsqldatabase.h>

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

    
    /* load up the ui components */
    if ((ui_ok = loadUI()))
    {
	/* for starters, load this host */
	loadHost(gContext->GetHostName());

	/* update the information */
	refreshKeyInformation();
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

    if ((control_list=getUIListTreeType("controltree")) == NULL)
    {
	VERBOSE(VB_ALL, "MythControls: Unable to load controltree");
	retval = false;
    }
    else {

	/* set some parameters for the tree */
	focused=control_list;
	control_list->setActive(true);
    }

    /* Check that all the buttons are there */
    if ((action_buttons[0] = getUITextButtonType("action_one")) == NULL)
    {
	VERBOSE(VB_ALL, "MythControls: Unable to load first action button");
	retval = false;
    }
    else if ((action_buttons[1] = getUITextButtonType("action_two")) == NULL)
    {
	VERBOSE(VB_ALL, "MythControls: Unable to load second action button");
	retval = false;
    }
    else if ((action_buttons[2] = getUITextButtonType("action_three")) == NULL)
    {
	VERBOSE(VB_ALL, "MythControls: Unable to load thrid action button");
	retval = false;
    }
    else if ((action_buttons[3] = getUITextButtonType("action_four")) == NULL)
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
	if (focused == action_buttons[i]) return i;

    return Action::MAX_KEYS;
}


void MythControls::focusButton(int direction)
{
    if (direction == 0)
    {
	focused = action_buttons[0];
	action_buttons[0]->takeFocus();
	control_list->looseFocus();
	control_list->setActive(false);
    }
    else
    {

	/* change focus by at most one in either direction */
	if (direction > 0) direction = 1;
	else direction = -1;

	/* figure out which button is focused */
	int current = 0;
	if (focused == action_buttons[1]) current=1;
	else if (focused == action_buttons[2]) current = 2;
	else if (focused == action_buttons[3]) current = 3;

	/* determine the new focused button index */
	int newb = current + direction;

	/* focus an existing button */
	if ((newb >= 0) && (newb < Action::MAX_KEYS))
	{
	    focused->looseFocus();
	    focused = action_buttons[newb];
	    focused->takeFocus();
	}
    }
}


void MythControls::keyPressEvent(QKeyEvent *e) {

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
	    if (focused == control_list)
	    {
		if (control_list->getDepth() == 0) control_list->MoveRight();
		else focusButton(0);
	    }
	    else {
		ActionMenu popup(gContext->GetMainWindow());
		int result = popup.getOption();
		if (result == ActionMenu::SET) addKeyToAction();
		else if (result == ActionMenu::REMOVE) deleteKey();
	    }
	}
	else if (action == "ESCAPE")
	{
	    if (focused == control_list)
	    {
		/* ask the user to save if there are unsaved changes*/
		if (key_bindings->hasChanges())
		{
		    UnsavedMenu popup(gContext->GetMainWindow());
		    if (popup.getOption() == UnsavedMenu::SAVE) save();
		}

		/* let the user exit */
		handled = false;
	    }
	    else
	    {
		/* take focus away from the button */
		focused->looseFocus();
		focused = control_list;
		control_list->takeFocus();
		control_list->setActive(true);
	    }
	}
	else if ((action == "UP") && (focused == control_list))
	{
	    control_list->MoveUp();
	    refreshKeyInformation();
	}
	else if ((action == "DOWN") && (focused == control_list))
	{
	    control_list->MoveDown();
	    refreshKeyInformation();
	}
	else if (action == "LEFT")
	{
	    if (focused==control_list)
	    {
		control_list->MoveLeft();
		refreshKeyInformation();
	    }
	    else focusButton(-1);
	}
	else if (action == "RIGHT")
	{
	    if (focused == control_list)
	    {
		if (control_list->getDepth() == 0)
		{
		    control_list->MoveRight();
		    refreshKeyInformation();
		}
	    }
	    else focusButton(1);
	}
	else if ((action == "PAGEUP") && (focused == control_list))
	{
	    control_list->MoveUp(UIListTreeType::MovePage);
	    refreshKeyInformation();
	}
	else if ((action == "PAGEDOWN") && (focused == control_list))
	{
	    control_list->MoveDown(UIListTreeType::MovePage);
	    refreshKeyInformation();
	}
	else handled = false;
    }

    if (!handled) MythThemedDialog::keyPressEvent(e);
}



/* comments in header */
void MythControls::refreshKeyInformation() {

    /* get the description of the current action */
    QString desc = key_bindings->getActionDescription(getCurrentContext(),
						      getCurrentAction());

    /* get the bindings of the current action */
    QStringList keys = key_bindings->getActionKeys(getCurrentContext(),
						   getCurrentAction());

    size_t i;

    /* fill existing keys */
    for (i = 0; i < keys.count(); i++)
	action_buttons[i]->setText(keys[i]);

    /* blank the other ones */
    for (; i < Action::MAX_KEYS; i++)
	action_buttons[i]->setText("");

    /* set the information */
    description->SetText(desc);
}



/* comments in header */
QString MythControls::getCurrentContext(void) const
{
    UIListGenericTree *current = control_list->GetCurrentPosition();
    if (control_list->getDepth() == 1 )
	return current->getParent()->getString();
    else if (control_list->getDepth() == 0)
	return current->getString();
    else
	return "";
}



/* comments in header */
QString MythControls::getCurrentAction(void) const
{
    if (control_list->getDepth() == 1)
 	return control_list->GetCurrentPosition()->getString();
    else
	return "";
}



/* comments in header */
void MythControls::loadHost(const QString & hostname) {

    /* create the key bindings and the tree */
    key_bindings = new KeyBindings(hostname);
    QStringList context_names = key_bindings->getContexts();

    /* Alphabetic order, but jump and global at the top  */
    context_names.sort();
    context_names.remove(JUMP_CONTEXT);
    context_names.remove(GLOBAL_CONTEXT);
    context_names.insert(context_names.begin(), 1, GLOBAL_CONTEXT);
    context_names.insert(context_names.begin(), 1, JUMP_CONTEXT);

    UIListGenericTree *root = new UIListGenericTree(NULL, "ROOT");

    for (size_t i = 0; i < context_names.size(); i++)
    {
	UIListGenericTree *context_node;
	context_node = new UIListGenericTree(root, context_names[i],
					     context_names[i]);

	QStringList action_names = key_bindings->getActions(context_names[i]);
	action_names.sort();

	/* add all actions to the context */
	for (size_t j = 0; j < action_names.size(); j++)
	{
	    new UIListGenericTree(context_node, action_names[j],
				  action_names[j]);
	}
    }

    control_list->SetTree(root);
    control_list->setActive(true);
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

    bool bind = false;
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
