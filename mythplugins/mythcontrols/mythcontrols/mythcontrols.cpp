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
#include "conflictresolver.h"


/* comments in header */
MythControls::MythControls (MythMainWindow *parent, const char *name) 
    :MythThemedDialog(parent, "controls", "controls-", name)
{
    /* by default, there is no popup */
    popup = NULL;

    /* Get the UI widgets that we need to work with */
    action_description = getUITextType("action_description");
    if (!action_description)
      VERBOSE(VB_ALL, "Unable to load action_description");

    action_keys = getUITextType("action_keys");
    if (!action_keys)
      VERBOSE(VB_ALL, "Unable to load action_keys");

    control_tree_list=getUIManagedTreeListType("controltree");
    if (!control_tree_list)
    {
	VERBOSE(VB_ALL, "Unable to load controltree");
    }
    else {

	/* set some parameters for the tree */
	control_tree_list->showWholeTree(true);
	control_tree_list->colorSelectables(true);

    }

    /* for starters, load this host */
    loadHost(gContext->GetHostName());

    /* update the information */
    refreshKeyInformation();
}



/* comments in header */
MythControls::~MythControls() { delete(this->key_bindings); }



void MythControls::keyPressEvent(QKeyEvent *e) {

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Controls", e, actions);

    for (size_t i = 0; i < actions.size() && !handled; i++)
    {
	QString action = actions[i];
	handled = true;

	if (action == "SELECT" || action == "MENU" || action == "INFO")
	{
	    control_tree_list->select();
	    actionMenu();
	}
	else if (action == "MENU" || action == "INFO")
	{
	}
	else if (action == "UP")
	{
	    control_tree_list->moveUp();
	    refreshKeyInformation();
	}
	else if (action == "DOWN")
	{
	    control_tree_list->moveDown();
	    refreshKeyInformation();
	}
	else if (action == "LEFT")
	{
	    control_tree_list->popUp();
	    refreshKeyInformation();
	}
	else if (action == "RIGHT")
	{
	    control_tree_list->pushDown();
	    refreshKeyInformation();
	}
	else if (action == "PAGEUP")
	{
	    control_tree_list->pageUp();
	    refreshKeyInformation();
	}
	else if (action == "PAGEDOWN")
	{
	    control_tree_list->pageDown();
	    refreshKeyInformation();
	}
	else handled = false;
    }

    if (!handled) MythThemedDialog::keyPressEvent(e);
}



void MythControls::refreshKeyInformation() {

    QString desc = key_bindings->getActionDescription(getCurrentContext(),
						      getCurrentAction());
    QString keys = key_bindings->getActionKeys(getCurrentContext(),
					       getCurrentAction());
    /* set the information */
    action_description->SetText(desc);
    action_keys->SetText(keys);

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

    if (control_tree_list->getActiveBin() == 2)
    {
	popup->addButton(tr("Add Key"),this, SLOT(captureKey()));
	popup->addButton(tr("Clear Key"), this, SLOT(clearActionKeys()));
    }

    popup->addButton(tr("Save Changes"), this, SLOT(save()));
    popup->addButton(tr("Done"),this, SLOT(killPopup()))->setFocus();
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



void MythControls::addKeyToAction()
{
    QString event = ((KeyGrabPopupBox*)popup)->getCapturedKeyEvent();

    key_bindings->addActionKey(getCurrentContext(),
			       getCurrentAction(),
			       event);

    refreshKeyInformation();
    killPopup();
}



void MythControls::clearActionKeys()
{
    key_bindings->setActionKeys(getCurrentContext(), getCurrentAction(), "");
    refreshKeyInformation();
}



void MythControls::save()
{

    /* ensure the manditory keys are bound to something */
    if (!key_bindings->hasManditoryBindings())
    {
	VERBOSE(VB_ALL, "Manditory bindings are not set, setting to defaults");
	key_bindings->defaultManditoryBindings();
	refreshKeyInformation();
    }

    /* check for conflicts */
    KeyConflict *conflict = key_bindings->getKeyConflict();
    if (conflict)
    {
	/* kill any popup that is currently showing */
	killPopup();

	ConflictResolver *cr;
	cr = new ConflictResolver(gContext->GetMainWindow(),conflict);
	this->popup = (MythPopupBox*)cr;
	cr->addButton(cr->getButtonCaption(1), this,
		      SLOT(keepFirst()))->setFocus();

	cr->addButton(cr->getButtonCaption(2), this,
		      SLOT(keepSecond()));

	this->popup->ShowPopup(this,SLOT(killPopup()));
    }
    else key_bindings->commitChanges();
}


void MythControls::resolve(int winner)
{

    ConflictResolver *resolver = (ConflictResolver*)this->popup;
    KeyConflict *conflict =  resolver->key_conflict;


    if (winner == 1)
    {
	key_bindings->removeActionKey(conflict->getSecond().first,
				      conflict->getSecond().second,
				      conflict->getKey());
    }
    else if (winner == 2)
    {
	key_bindings->removeActionKey(conflict->getFirst().first,
				      conflict->getFirst().second,
				      conflict->getKey());
    }
    else
    {
	VERBOSE(VB_ALL, "Help!  Someone is abusing MythControls::resolve");
	VERBOSE(VB_ALL, "I'm a private method, how is this possible");
    }

    this->killPopup();
    this->refreshKeyInformation();
    this->save();
    delete conflict;
}


#endif /* MYTHCONTROLS_CPP */
