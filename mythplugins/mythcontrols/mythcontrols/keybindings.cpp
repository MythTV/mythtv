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
 * This class is responsible for storing and loading the controls.  It
 * acts as a layer between the actual key/action bindings (actionset)
 * and the UI.
 *
 * @todo Unify the mapping stuff into separate function calls.  EG:
 * remove, add
 *
 * @todo New data struct which holds the dicts and map, and maintains
 * them both automatically.
 */

/* QT */
#include <qstring.h>

/* MythTV */
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>

using namespace std;

#include "keybindings.h"

enum query_indicies {CTXT, ACTN, DESC, KEYS};


KeyBindings::KeyBindings(const QString & hostname) {
    this->hostname() = hostname;
    loadManditoryBindings();

    retrieveContexts();
    retrieveJumppoints();
}



ActionID * KeyBindings::conflicts(const QString & context_name,
				  const QString & key) const
{
    const QValueList<ActionID> &ids = actionset.getActions(key);

    /* trying to bind a jumppoint to an already bound key */
    if ((context_name == JUMP_CONTEXT) && (ids.count() > 0))
	return new ActionID(ids[0]);

    /* check the contexts of the other bindings */
    for (size_t i = 0; i < ids.count(); i++)
    {
	if (ids[i].context() == JUMP_CONTEXT) return new ActionID(ids[i]);
	else if (ids[i].context() == context_name) return new ActionID(ids[i]);
    }

    /* no conflicts */
    return NULL;
}



void KeyBindings::commitAction(const ActionID & id)
{

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE keybindings SET keylist = :KEYLIST "
		  "WHERE hostname = :HOSTNAME "
		  "AND action = :ACTION "
		  "AND context = :CONTEXT ;");

    if (query.isConnected())
    {
	QString keys = actionset.keyString(id);
	query.bindValue(":HOSTNAME", this->getHostname());
	query.bindValue(":CONTEXT", id.context());
	query.bindValue(":ACTION", id.action());
	query.bindValue(":KEYLIST", keys);

	if (query.exec() && query.isActive())
	{
	    /* remove the current bindings */
	    gContext->GetMainWindow()->ClearKey(id.context(),id.action());

	    /* make the settings take effect */
	    gContext->GetMainWindow()->BindKey(id.context(), id.action(),keys);
	}
    }
}



void KeyBindings::commitJumppoint(const ActionID &id)
{

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE jumppoints "
		  "SET keylist = :KEYLIST "
		  "WHERE hostname = :HOSTNAME "
		  "AND destination = :DESTINATION ;");

    if (query.isConnected())
    {
	QString keys = actionset.keyString(id);
	query.bindValue(":HOSTNAME", this->getHostname());
	query.bindValue(":DESTINATION", id.action());
	query.bindValue(":KEYLIST", keys);

	/* tell the user to restart for settings to take effect */
	if (query.exec() && query.isActive()) {

	    /* clear bindings to the jumppoint */
	    gContext->GetMainWindow()->ClearJump(id.action());

	    /* make the bindings take effect */
	    gContext->GetMainWindow()->BindJump(id.action(),keys);
	}
    }
}



void KeyBindings::commitChanges(void)
{
    ActionList modified = actionset.getModified();

    while (modified.size() > 0)
    {
	ActionID id = modified.front();

	/* commit either a jumppoint or an action */
	if (id.context() == JUMP_CONTEXT) commitJumppoint(id);
	else commitAction(id);

	/* tell the action set that the action is no longer modified */
	actionset.unmodify(id);

	modified.pop_front();
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
    /*contexts().insert(JUMP_CONTEXT, new Context());
      Context *jumppoints = getContext(JUMP_CONTEXT);*/

    for (query.next(); query.isValid(); query.next())
    {
	ActionID id(JUMP_CONTEXT, query.value(0).toString());
	this->actionset.addAction(id,query.value(1).toString(),
				  query.value(2).toString());
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
	ActionID id(query.value(CTXT).toString(),query.value(ACTN).toString());
	this->actionset.addAction(id,query.value(DESC).toString(),
				  query.value(KEYS).toString());

	/*if (!getContext(id.context()))
	{
	    contexts().insert(id.context(), new Context());
	    }*/

	/*Action *action = new Action(query.value(DESC).toString(),
	  query.value(KEYS).toString());

	  const QStringList &keys = action->getKeys();
	  for (size_t i = 0; i < keys.count(); i++)
	  {
	  
	  QValueList<ActionID> &ids = key_map[keys[i]];
	  ids.push_back(id);
	}

	getContext(id.context())->insert(id.action(),action);*/
    }
}



/* comments in header */
/*bool KeyBindings::actionsCanConflict(const ActionID & first,
  const ActionID & second) const
  {
  return  (first.context() == JUMP_CONTEXT ||
  second.context() == JUMP_CONTEXT ||
  first.context() == second.context());
  }*/



void KeyBindings::loadManditoryBindings(void)
{
    if (getManditoryBindings().empty())
    {
	manditoryBindings().append(ActionID(GLOBAL_CONTEXT, "DOWN"));
	defaultKeys().append("Down");

	manditoryBindings().append(ActionID(GLOBAL_CONTEXT, "UP"));
	defaultKeys().append("Up");

	manditoryBindings().append(ActionID(GLOBAL_CONTEXT, "LEFT"));
	defaultKeys().append("Left");

	manditoryBindings().append(ActionID(GLOBAL_CONTEXT, "RIGHT"));
	defaultKeys().append("Right");

	manditoryBindings().append(ActionID(GLOBAL_CONTEXT, "ESCAPE"));
	defaultKeys().append("Esc");

	manditoryBindings().append(ActionID(GLOBAL_CONTEXT, "SELECT"));
	defaultKeys().append("Return,Enter,Space");
    }
}



bool KeyBindings::hasManditoryBindings(void) const
{
    QValueList<ActionID> manlist = getManditoryBindings();
    for (size_t i = 0; i < manlist.count(); i++)
    {
	if (actionset.getKeys(manlist[i]).isEmpty())
	    return false;
    }

    /* none are empty, return true */
    return true;
}
