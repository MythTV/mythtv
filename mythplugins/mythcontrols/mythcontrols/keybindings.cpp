/**
 * @file keybindings.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Keybinding classes
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
 *
 * @todo Make jump points take effect immediately.  This requires
 * changes to myth.
 *
 * @todo Prompt with a dialog when unbound manditory keys are going to
 * be bound to their defaults.
 */

/* QT */
#include <qstring.h>

/* MythTV */
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>

using namespace std;

#include "keybindings.h"

enum query_indicies {CTXT, ACTN, DESC, KEYS};

#define JUMP_DESC "For now, mythtv must be restarted for new jump point key bindings to take effect"



KeyBindings::KeyBindings(const QString & hostname) {
    this->hostname() = hostname;
    loadManditoryBindings();

    retrieveContexts();
    retrieveJumppoints();
}



QStringList KeyBindings::getContexts() const
{
    QStringList context_strings;
    QDictIterator<Context> it(this->_contexts);

    for (; it.current(); ++it)
	context_strings.append(it.currentKey());
    
    return context_strings;
}


QStringList KeyBindings::getActions(const QString & context)
{
    QStringList action_strings;
    QDictIterator<Action> it(*getContext(context));

    for (; it.current(); ++it)
	action_strings.append(it.currentKey());
    
    return action_strings;
}



QString KeyBindings::getActionKeys(const QString & context_name,
				   const QString & action_name) const
{
  Action *action = getAction(context_name, action_name);

  return action ? action->getKeys() : "";
}



QString KeyBindings::getActionDescription(const QString & context_name,
					  const QString & action_name) const
{
    Action *action = this->getAction(context_name, action_name);

    return action ? action->getDescription() : "";
}



void KeyBindings::addActionKey(const QString & context_name,
			       const QString & action_name,
			       const QString & key)
{
    Action *action = this->getAction(context_name, action_name);
    if (action)
    {
	action->addKey(key);
	this->modify(context_name,action_name);
    }
}

void KeyBindings::removeActionKey(const QString & context_name,
				  const QString & action_name,
				  const QString & key)
{
    Action *action = this->getAction(context_name, action_name);

    if (action)
    {
	action->removeKey(key);
	this->modify(context_name,action_name);
    }
}



void KeyBindings::setActionKeys(const QString & context_name,
				const QString & action_name,
				const QString & keys)
{
    Action *action = this->getAction(context_name, action_name);
    if (action)
    {
	action->setKeys(keys);
	this->modify(context_name, action_name);
    }
}



bool KeyBindings::isModified(const QString & context_name,
			     const QString & action_name) const
{

    QValueList<ActionIdentifier> modlist=this->getModified(context_name);
    QValueList<ActionIdentifier>::iterator it;

    for (it= modlist.begin(); it != modlist.end(); ++it)
    {
	if (((*it).first == context_name) && ((*it).second == action_name))
	{
	    return true;
	}
    }
    return false;
}


void KeyBindings::modify(const QString & context_name,
			 const QString & action_name)
{
    if (isModified(context_name, action_name)) return;

    modified(context_name).push_back(ActionIdentifier(context_name,
						      action_name));
}



void KeyBindings::commitAction(const ActionIdentifier & id)
{

    Action *action = getAction(id.first,id.second);
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE keybindings SET keylist = :KEYLIST "
		  "WHERE hostname = :HOSTNAME "
		  "AND action = :ACTION "
		  "AND context = :CONTEXT ;");

    if (query.isConnected())
    {
	query.bindValue(":HOSTNAME", this->getHostname());
	query.bindValue(":CONTEXT", id.first);
	query.bindValue(":ACTION", id.second);
	query.bindValue(":KEYLIST", action->getKeys());

	if (query.exec() && query.isActive())
	{
	    /* remove the current bindings */
	    gContext->GetMainWindow()->ClearKeylist(id.first,id.second);

	    /* make the settings take effect */
	    gContext->GetMainWindow()->RegisterKey(id.first,
						   id.second,
						   action->getDescription(),
						   action->getKeys());
	}
    }
}



void KeyBindings::commitJumppoint(const ActionIdentifier &id)
{

    Action *action = getAction(id.first,id.second);
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE jumppoints "
		  "SET keylist = :KEYLIST "
		  "WHERE hostname = :HOSTNAME "
		  "AND destination = :DESTINATION ;");

    if (query.isConnected())
    {
	query.bindValue(":HOSTNAME", this->getHostname());
	query.bindValue(":DESTINATION", id.second);
	query.bindValue(":KEYLIST", action->getKeys());

	/* tell the user to restart for settings to take effect */
	if (query.exec() && query.isActive())
	    VERBOSE(VB_ALL, "Restart myth to for jumppoint to take effect");

	/* reminds me of when I used windoze ;) */
    }
    
}



void KeyBindings::commitChanges(void)
{
    while (modifiedActions().size() > 0)
    {	
	commitAction(modifiedActions().front());
	modifiedActions().pop_front();
    }

    while (modifiedJumppoints().size() > 0)
    {
	commitJumppoint(modifiedJumppoints().front());
	modifiedJumppoints().pop_front();
    }
}



void KeyBindings::retrieveJumppoints()
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
	query.prepare("SELECT destination,description,keylist "
		      "FROM jumppoints "
		      "WHERE hostname = :HOSTNAME "
		      "ORDER BY destination ;");
	query.bindValue(":HOSTNAME", this->getHostname());
    }

    query.exec();

    /* add a context for jumppoints */
    contexts().insert(JUMP_CONTEXT, new Context());
    Context *jumppoints = getContext(JUMP_CONTEXT);

    for (query.next(); query.isValid(); query.next())
    {
	Action *action = new Action(JUMP_DESC, query.value(2).toString());

	jumppoints->insert(query.value(0).toString(), action);
	
    }
}



void KeyBindings::retrieveContexts()
{

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
	query.prepare("SELECT context,action,description,keylist "
		      "FROM keybindings "
		      "WHERE hostname = :HOSTNAME "
		      "ORDER BY context,action ;");

	query.bindValue(":HOSTNAME", this->getHostname());
    }

    query.exec();

    for (query.next(); query.isValid(); query.next())
    {
	QString context_name = query.value(CTXT).toString();
	if (!getContext(context_name))
	    contexts().insert(context_name, new Context());

	Action *action = new Action(query.value(DESC).toString(),
				    query.value(KEYS).toString());

	getContext(context_name)->insert(query.value(ACTN).toString(),action);
    }
}



/* comments in header */
bool KeyBindings::actionsCanConflict(const ActionIdentifier & first,
				     const ActionIdentifier & second) const
{

    /* necessasary conditions for a conflict */
    return  (first.first == JUMP_CONTEXT ||
	     second.first == JUMP_CONTEXT ||
	     first.first == second.first);
}



/* comments in header */
KeyConflict * KeyBindings::getKeyConflict(void) const
{

    QMap<QString,QValueList<ActionIdentifier> > key_map;


    /* build a map based on key name */
    for (QDictIterator<Context> cit(this->_contexts); cit.current(); ++cit)
    {
	QString context_string = cit.currentKey();
	QDictIterator<Action> ait(*(cit.current()));
	for (; ait.current(); ++ait)
	{
	    QStringList keys = QStringList::split(",",
						  ait.current()->getKeys(),
						  false);

	    for(size_t i = 0; i < keys.count(); i++)
		key_map[keys[i]].append(ActionIdentifier(cit.currentKey(),
							 ait.currentKey()));
	}
    }

    QValueList<KeyConflict> conflicts;

    QMap<QString,QValueList<ActionIdentifier> >::Iterator it;

    for (it = key_map.begin(); it != key_map.end(); it++)
    {
	QValueList<ActionIdentifier> actions = it.data();

	for (size_t i = 0; i < actions.size(); i++)
	{
	    for (size_t j = i+1; j < actions.size(); j++)
	    {
		if (actionsCanConflict(actions[i], actions[j]))
		{
		    return new KeyConflict(it.key(), actions[i],
					   actions[j]);
		}
	    }
	}
    }

    /* no conflicts, return null */
    return NULL;
}



void KeyBindings::loadManditoryBindings(void)
{
    if (getManditoryBindings().empty())
    {
	manditoryBindings().append(ActionIdentifier(GLOBAL_CONTEXT, "DOWN"));
	defaultKeys().append("Down");

	manditoryBindings().append(ActionIdentifier(GLOBAL_CONTEXT, "UP"));
	defaultKeys().append("Up");

	manditoryBindings().append(ActionIdentifier(GLOBAL_CONTEXT, "LEFT"));
	defaultKeys().append("Left");

	manditoryBindings().append(ActionIdentifier(GLOBAL_CONTEXT, "RIGHT"));
	defaultKeys().append("Right");

	manditoryBindings().append(ActionIdentifier(GLOBAL_CONTEXT, "ESCAPE"));
	defaultKeys().append("Esc");

	manditoryBindings().append(ActionIdentifier(GLOBAL_CONTEXT, "SELECT"));
	defaultKeys().append("Return,Enter,Space");
    }
}



bool KeyBindings::hasManditoryBindings(void) const
{
    QValueList<ActionIdentifier> manlist = getManditoryBindings();
    for (size_t i = 0; i < manlist.count(); i++)
    {
	if (getAction(manlist[i].first, manlist[i].second)->empty())
	    return false;
    }

    /* none are empty, return true */
    return true;
}

void KeyBindings::defaultManditoryBindings(void)
{
    QValueList<ActionIdentifier> manlist = getManditoryBindings();
    for (size_t i = 0; i < manlist.count(); i++)    
    {
	Action *action = getAction(manlist[i].first, manlist[i].second);
	if (action->empty()) action->setKeys(defaultKeys()[i]);
    }
}
