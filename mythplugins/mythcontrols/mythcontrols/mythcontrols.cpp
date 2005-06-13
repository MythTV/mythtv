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
#include <mythtv/uitypes.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/dialogbox.h>
#include <mythtv/xmlparse.h>
#include <mythtv/themedmenu.h>
#include <mythtv/mythdbcon.h>

using namespace std;

#include "mythcontrols.h"
#include "keygrabber.h"

/**
 * @class ConflictResolver
 * @brief A MythDialog that notifies the user of conflicts. 
 */
class ConflictResolver : public MythPopupBox
{

public:
    ConflictResolver(MythMainWindow *window, ActionID *conflict)
	: MythPopupBox(window, "conflictresolver")
    {
	QString warning = "The key you have chosen is already bound to "
	    + conflict->action() + " in the " + conflict->context()
	    + " context; please rebind it before binding this key.";
	
	addLabel("Action Conflict", Large, false);
	addLabel(warning, Small, true);
    }


    ConflictResolver(MythMainWindow *window)
	: MythPopupBox(window,"conflictresolver")
    {

	QString warning;
	warning = "This action is manditory and needs at least one key bound ";
	warning += "to it.  Instead, try rebinding with another key.";
	    
	addLabel("Manditory Action", Large, false);
	addLabel(warning, Small, true);
    }
};


/* comments in header */
MythControls::MythControls (MythMainWindow *parent, const char *name) 
    :MythThemedDialog(parent, "controls", "controls-", name)
{
    /* load up the ui components */
    loadUI();

    /* for starters, load this host */
    loadHost(gContext->GetHostName());

    /* update the information */
    refreshKeyInformation();
}



/* comments in header */
MythControls::~MythControls() { delete(this->key_bindings); }



/* comments in header */
bool MythControls::loadUI()
{
    /* the return value of the method */
    bool retval = true;

    /* by default, there is no popup */
    popup = NULL;

    /* Get the UI widgets that we need to work with */
    description = getUITextType("description");
    if (!description) {
	VERBOSE(VB_ALL, "Unable to load action_description");
	retval = false;
    }

    control_tree_list=getUIManagedTreeListType("controltree");
    if (!control_tree_list)
    {
	VERBOSE(VB_ALL, "Unable to load controltree");
	retval = false;
    }
    else {

	/* set some parameters for the tree */
	control_tree_list->showWholeTree(true);
	control_tree_list->colorSelectables(true);
	focused=control_tree_list;
    }

    /* Check that all the buttons are there */
    if ((action_buttons[0] = getUITextButtonType("action_one")) == NULL)
    {
	VERBOSE(VB_ALL, "Unable to load first action button");
	retval = false;
    }
    else if ((action_buttons[1] = getUITextButtonType("action_two")) == NULL)
    {
	VERBOSE(VB_ALL, "Unable to load second action button");
	retval = false;
    }
    else if ((action_buttons[2] = getUITextButtonType("action_three")) == NULL)
    {
	VERBOSE(VB_ALL, "Unable to load thrid action button");
	retval = false;
    }
    else if ((action_buttons[3] = getUITextButtonType("action_four")) == NULL)
    {
	VERBOSE(VB_ALL, "Unable to load fourth action button");
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
	control_tree_list->looseFocus();
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
	    /* bring up the menu */
	    this->actionMenu();
	}
	else if (action == "SELECT")
	{
	    if (focused == control_tree_list)
	    {
		if (control_tree_list->getActiveBin() == 1)
		    control_tree_list->pushDown();
		else
		    focusButton(0);
	    }
	    else {
		popup = new MythPopupBox(gContext->GetMainWindow(),"decision");
		popup->addLabel(tr("Modify Action"),MythPopupBox::Large,false);
		popup->addButton(tr("Set Binding"),this, SLOT(captureKey()));
		popup->addButton(tr("Remove Binding"),this, SLOT(deleteKey()));
		popup->addButton(tr("Cancel"), this,
				 SLOT(killPopup()))->setFocus();
		popup->ShowPopup();
	    }
	}
	else if (action == "ESCAPE")
	{
	    if (focused == control_tree_list)
	    {
		/* ask the user to save if there are unsaved changes*/
		if (key_bindings->hasChanges())
		{
		    popup = new MythPopupBox(gContext->GetMainWindow(),
					     "unsaged");
		    popup->addLabel(tr("Unsaged Changes"),
				    MythPopupBox::Large,
				    false);

		    popup->addLabel(tr("Would you like to save now?"));
		    popup->addButton(tr("Save"), this,
				     SLOT(save()))->setFocus();
		    popup->addButton(tr("Exit"), this, SLOT(killPopup()));
		    popup->ExecPopup(this, SLOT(killPopup()));
		}

		/* let the user exit */
		handled = false;
	    }
	    else
	    {
		/* take focus away from the button */
		focused->looseFocus();
		control_tree_list->takeFocus();
		focused = control_tree_list;
	    }
	}
	else if (action == "UP")
	{
	    if (focused == control_tree_list)
	    {
		control_tree_list->moveUp();
		refreshKeyInformation();
	    }
	    else focusButton(-1);
	}
	else if (action == "DOWN")
	{
	    if (focused == control_tree_list)
	    {
		control_tree_list->moveDown();
		refreshKeyInformation();
	    }
	    else focusButton(1);
	}
	else if (action == "LEFT")
	{
	    if (focused == control_tree_list)
	    {
		control_tree_list->popUp();
		refreshKeyInformation();
	    }
	    else focusButton(-1);
	}
	else if (action == "RIGHT")
	{
	    if (focused == control_tree_list)
	    {
		control_tree_list->pushDown();
		refreshKeyInformation();
	    }
	    else focusButton(1);
	}
	else if (action == "PAGEUP")
	{
	    if (focused == control_tree_list)
	    {
		control_tree_list->pageUp();
		refreshKeyInformation();
	    }
	}
	else if (action == "PAGEDOWN")
	{
	    if (focused == control_tree_list)
	    {
		control_tree_list->pageDown();
		refreshKeyInformation();
	    }
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
    if (control_tree_list->getActiveBin() == 2)
	return control_tree_list->getCurrentNode()->getParent()->getString();
    else if (control_tree_list->getActiveBin() == 1)
	return control_tree_list->getCurrentNode()->getString();
    else
	return "";
}



/* comments in header */
QString MythControls::getCurrentAction(void) const
{
    if (control_tree_list->getActiveBin() == 2)
 	return control_tree_list->getCurrentNode()->getString();
    else
	return "";
}



/* comments in header */
void MythControls::loadHost(const QString & hostname) {

    /* create the key bindings and the tree */
    key_bindings = new KeyBindings(hostname);
    context_tree = new GenericTree(hostname, 0, false);
    QStringList context_names = key_bindings->getContexts();

    for (size_t i = 0; i < context_names.size(); i++) {

	GenericTree *context_node = new GenericTree(context_names[i], i, true);

	/* add the context to the tree of contexts */
	context_tree->addNode(context_node);

	QStringList action_names = key_bindings->getActions(context_names[i]);

	/* add all actions to the context */
	for (size_t j = 0; j < action_names.size(); j++)
	    context_node->addNode(action_names[j], j, true);

	/* put the data in the tree */
	control_tree_list->assignTreeData(context_node);
    }
}



void MythControls::killPopup()
{
    if (this->popup == NULL) return;

    this->popup->hide();
    delete this->popup;
    this->popup = NULL;
    setActiveWindow();
}



/* comments in header */
void MythControls::actionMenu()
{
    if (popup != NULL) return;

    popup = new MythPopupBox(gContext->GetMainWindow(), "editaction");
    popup->addButton(tr("Save Changes"), this, SLOT(save()));
    popup->addButton(tr("Cancel"),this, SLOT(killPopup()))->setFocus();
    popup->ShowPopup(this, SLOT(killPopup()));
}



void MythControls::captureKey()
{
    killPopup();

    KeyGrabPopupBox * kg = new KeyGrabPopupBox(gContext->GetMainWindow());

    kg->addOKButton(kg->addButton(tr("OK"), this, SLOT(addKeyToAction())));
    kg->addButton(tr("Cancel"), this, SLOT(killPopup()));
    this->popup = (MythPopupBox*)kg;
    this->popup->ShowPopup(this,SLOT(killPopup()));
}



void MythControls::deleteKey()
{

    killPopup();

    size_t b = focusedButton();
    QString action = getCurrentAction(), context = getCurrentContext();
    QStringList keys = key_bindings->getActionKeys(context, action);
    if (b < keys.count())
    {
	if (!key_bindings->removeActionKey(context, action, keys[b]))
	{
	    this->popup = new ConflictResolver(gContext->GetMainWindow());
	    this->popup->ExecPopup(this, SLOT(killPopup()));
	}
	else refreshKeyInformation();
    }
}



void MythControls::addKeyToAction(void)
{
    size_t b = focusedButton();
    QString newkey = ((KeyGrabPopupBox*)popup)->getCapturedKeyEvent();
    QString action = getCurrentAction(), context = getCurrentContext();
    QStringList keys = key_bindings->getActionKeys(context, action);
    ActionID *conflict = NULL;

    /* having got the key, kill the key grabber */
    killPopup();

    if (b < keys.count())
    {
	/* dont replace with the same key */
	if (keys[b] != newkey) {
	    if ((conflict = key_bindings->conflicts(context, newkey)))
	    {
		this->popup = new ConflictResolver(gContext->GetMainWindow(),
						   conflict);
		this->popup->ExecPopup(this, SLOT(killPopup()));
	    }
	    else key_bindings->replaceActionKey(context,action,newkey,keys[b]);
	}
    }
    else {
	if ((conflict = key_bindings->conflicts(context, newkey)))
	{
	    this->popup = new ConflictResolver(gContext->GetMainWindow(),
					       conflict);
	    this->popup->ExecPopup(this, SLOT(killPopup()));
	}
	else key_bindings->addActionKey(context, action, newkey);
    }

    /* delete the conflict and hide the popup*/
    if (conflict) {
	delete conflict;
	killPopup();
    }

    refreshKeyInformation();
}

void MythControls::save(void) {
    key_bindings->commitChanges();
    killPopup();
}

#endif /* MYTHCONTROLS_CPP */
