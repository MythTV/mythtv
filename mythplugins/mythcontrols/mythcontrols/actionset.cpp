#ifndef ACTIONSET_CPP
#define ACTIONSET_CPP

#include <iostream>

#include <qmap.h>
#include <qdict.h>
#include <qvaluelist.h>

#include "action.h"
#include "actionid.h"

using namespace std;

#include "actionset.h"



/* method description in header */
bool ActionSet::add(const ActionID &id, const QString &key)
{
    Action *a;

    /* get the action; make sure it exists before doing anything */
    if ((a = this->action(id)))
    {

	/* if the key was added, mark this action as modified, and
	   update the key map */
	if (a->addKey(key))
	{
	    ActionList &ids = _keymap[key];
	    ids.push_back(id);
	    this->modify(id);	
	    return true;
	} else cerr << "Key not added" << endl;
    } else cerr << "No action" << endl;

    return false;
}



/* method description in header */
bool ActionSet::remove(const ActionID &id, const QString & key)
{

    Action *a;

    /* make sure an action exists before removing */
    if ((a = this->action(id)))
    {
	/* if the key was removed from the action, then remove it from
	 * the key list, and mark the action id as modified */
	if (a->removeKey(key))
	{
	    /* remove the action from the key map */
	    this->_keymap[key].remove(id);

	    /* mark the action as modified */
	    this->modify(id);

	    return true;	    
	}
    }

    /* if we got this far, it wasn't removed */
    return false;
}



/* method description in header */
bool ActionSet::replace(const ActionID &id, const QString &newkey,
			const QString &oldkey)
{
    Action *a;

    /* make sure an action exists before replacing */
    if ((a = this->action(id)))
    {

	/* make sure the key was actually replaced before modifying */
	if (a->replaceKey(newkey, oldkey))
	{
	    /* remove the action from the old key */
	    this->_keymap[oldkey].remove(id);

	    /* add the action to the new key */
	    this->_keymap[newkey].push_back(id);

	    /* mark the action as modified */
	    this->modify(id);

	    /* the job is done, return true */
	    return true;	    
	}
    }

    /* if we made it this far, nothing was replaced */
    return false;
}



/* method description in header */
QStringList ActionSet::contextStrings(void) const
{
    QStringList context_strings;
    QDictIterator<Context> it(_contexts);

    for (; it.current(); ++it)
	context_strings.append(it.currentKey());
    
    return context_strings;
}



/* method description in header */
QStringList ActionSet::actionStrings(const QString &context_name) const
{
    QStringList action_strings;
    QDictIterator<Action> it(*(_contexts[context_name]));

    for (; it.current(); ++it)
	action_strings.append(it.currentKey());
    
    return action_strings;
}



/* method description in header */
bool ActionSet::addAction(const ActionID &id, const QString &description,
			  const QString &keys)
{
    if (!_contexts[id.context()]) _contexts.insert(id.context(),
						   new Context());

    /* return false if the action already exists */
    if ((*_contexts[id.context()])[id.action()]) return false;
    else
    {
	/* create the action */
	Action *a = new Action(description, keys);

	/* add the action into the dict */
	_contexts[id.context()]->insert(id.action(), a);

	/* get the actions keys */
	const QStringList &keys = a->getKeys();
	for (size_t i = 0; i < keys.count(); i++)
	{
	    /* get the action list (of actions bound to keys[i])
	       if there is no key, keys[i], in the map, it will be added now */
	    ActionList &ids = _keymap[keys[i]];

	    /* add this action id to the list of actions bound to this key */
	    ids.push_back(id);
	}

	return true;
    }

    return false;
}



QString ActionSet::keyString(const ActionID &id) const
{
    Context *c;
    if ((c = _contexts[id.context()]))
    {
	Action *a;
	if ((a = (*c)[id.action()]))
	{
	    return a->keyString();
	}
    }
    return QString::null;
}



QStringList ActionSet::getKeys(const ActionID &id) const
{
    Context *c;
    if ((c = _contexts[id.context()]))
    {
	Action *a;
	if ((a = (*c)[id.action()]))
	{
	    return a->getKeys();
	}
    }

    return QStringList();
}



const QString & ActionSet::getDescription(const ActionID &id) const
{
    Context *c;
    if ((c = _contexts[id.context()]))
    {
	Action *a;
	if ((a = (*c)[id.action()]))
	{
	    return a->getDescription();
	}
    }

    return QString::null;
}



const ActionList & ActionSet::getActions(const QString &key) const
{
    return _keymap[key];
}


#endif /* ACTIONSET_CPP */
