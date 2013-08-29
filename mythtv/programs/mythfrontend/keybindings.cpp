/* -*- myth -*- */
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * This class is responsible for storing and loading the controls.  It
 * acts as a layer between the actual key/action bindings (actionset)
 * and the UI.
 */

// MythTV headers
#include "mythdb.h"
#include "mythmainwindow.h"

// MythControls headers
#include "keybindings.h"

/** \fn KeyBindings::KeyBindings(const QString&)
 *  \brief Create a new KeyBindings instance.
 *  \param hostname The host for which to create the key bindings.
 */
KeyBindings::KeyBindings(const QString &hostname)
    : m_hostname(hostname)
{
    m_hostname.detach();
    LoadMandatoryBindings();
    LoadContexts();
    LoadJumppoints();
}

/** \brief Returns a list of all keys bound to an action. */
QStringList KeyBindings::GetKeys(void) const
{
    return m_actionSet.GetAllKeys();
}

/** \fn KeyBindings::GetContexts(void) const
 *  \brief Returns a list of the context names.
 *  \note The returned list is a copy and can be modified without side-effects.
 */
QStringList KeyBindings::GetContexts(void) const
{
    QStringList ctxts = m_actionSet.GetContextStrings();
    ctxts.sort();
    return ctxts;
}

/** \fn KeyBindings::GetActions(const QString&) const
 *  \brief Get a list of the actions in a context.
 *  \param context The name of the context.
 *  \return A list of action (names) for the target context.
 *  \note Store this instead of calling repeatedly.  Every time you
 *        do, ActionSet has to iterate over all contexts and actions.
 */
QStringList KeyBindings::GetActions(const QString &context) const
{
    return m_actionSet.GetActionStrings(context);
}

/** \fn KeyBindings::GetKeyActions(const QString&, ActionList&) const
 *  \brief Get a list of the actions in a context.
 *  \param key The name of the context.
 *  \return A list of action (names) for the target context.
 *  \note Store this instead of calling repeatedly.  Every time you
 *        do, ActionSet has to iterate over all contexts and actions.
 */
void KeyBindings::GetKeyActions(const QString &key, ActionList &list) const
{
    list = m_actionSet.GetActions(key);
}

/** \fn KeyBindings::GetActionKeys(const QString&,const QString&) const
 *  \brief Get an action's keys.
 *  \param context_name The name of the context.
 *  \param action_name The name of the action.
 *  \return The keys bound to the specified context's action
 */
QStringList KeyBindings::GetActionKeys(const QString &context_name,
                                       const QString &action_name) const
{
    return m_actionSet.GetKeys(ActionID(context_name, action_name));
}

/** \fn KeyBindings::GetContextKeys(const QString &) const
 *  \brief Get the keys within a context.
 *  \param context The context name.
 *  \return A list of the keys in the context.
 */
QStringList KeyBindings::GetContextKeys(const QString &context) const
{
    return m_actionSet.GetContextKeys(context);
}

/** \fn KeyBindings::GetKeyContexts(const QString &) const
 *  \brief Get the context names in which a key is bound.
 *  \return A list of context names in which a key is bound.
 */
QStringList KeyBindings::GetKeyContexts(const QString &key) const
{
    ActionList actions = m_actionSet.GetActions(key);
    QStringList contexts;

    for (int i = 0; i < actions.size(); i++)
    {
        QString context = actions[i].GetContext();
        if (!contexts.contains(context))
            contexts.push_back(context);
    }

    return contexts;
}

/** \fn KeyBindings::GetActionDescription(const QString&,const QString&) const
 *  \brief Get an action's description.
 *  \param context_name The name of the context.
 *  \param action_name The name of the action.
 *  \return The description of the specified context's action
 */
QString KeyBindings::GetActionDescription(const QString &context_name,
                                          const QString &action_name) const
{
    ActionID id(context_name, action_name);
    return m_actionSet.GetDescription(id);
}

/** \fn KeyBindings::AddActionKey(const QString&,const QString&,const QString&)
 *  \brief Add a key to an action.
 *
 *   This does not take effect until CommitChanges() is called.
 *
 *  \param context_name The name of the context.
 *  \param action_name The name of the action.
 *  \param key The key to add.
 */
bool KeyBindings::AddActionKey(const QString &context_name,
                               const QString &action_name,
                               const QString &key)
{
    return m_actionSet.Add(ActionID(context_name, action_name), key);
}

/** \fn KeyBindings::GetConflict(const QString&,const QString&,int&) const
 *  \brief Determine if adding a key would cause a conflict.
 *  \param context_name The name of the context.
 *  \param key The key to add.
 *  \param level The level of conflict if this returns an ActionID
 *  \return pointer to an ActionID if adding the key would cause a conflict.
 *
 *  Conflicts occur if:
 *  \li the key is a jump point, but is bound elsewhere
 *  \li the key is already bound to a jumppoint.
 *  \li the key is bound to something in the global context.
 *  \li the key is bound to something else in the specified context
 *
 *   If the method does not return NULL, check the value given to
 *   level.  Warnings can be ignored (at the users disgression), but
 *   errors should be prevented no matter what.
 *
 *   NOTE: If this returns a non-null pointer, the ActionID returned
 *         must be explicitly deleted with C++ "delete".
 */
ActionID *KeyBindings::GetConflict(
    const QString &context_name, const QString &key, int &level) const
{
    const ActionList ids = m_actionSet.GetActions(key);

    // trying to bind a jumppoint to an already bound key
    if ((context_name == ActionSet::kJumpContext) && (ids.count() > 0))
        return new ActionID(ids[0]);

    // check the contexts of the other bindings
    for (int i = 0; i < ids.count(); i++)
    {
        // jump points, then global, then the same context
        if (ids[i].GetContext() == ActionSet::kJumpContext)
        {
            level = KeyBindings::kKeyBindingError;
            return new ActionID(ids[i]);
        }
        else if (ids[i].GetContext() == context_name)
        {
            level = KeyBindings::kKeyBindingError;
            return new ActionID(ids[i]);
        }
        else if (ids[i].GetContext() == ActionSet::kGlobalContext)
        {
            level = KeyBindings::kKeyBindingWarning;
            return new ActionID(ids[i]);
        }
    }

    return NULL; // no conflicts
}

/**
 *  \brief Replace a key in an action.
 *
 *   This does not take effect until CommitChanges() is called.
 *
 *  \param context_name The name of the context.
 *  \param action_name The name of the action.
 *  \param newkey The key to add.
 *  \param oldkey The index of the key to be replaced
 */
void KeyBindings::ReplaceActionKey(const QString &context_name,
                                   const QString &action_name,
                                   const QString &newkey,
                                   const QString &oldkey)
{
    m_actionSet.Replace(ActionID(context_name, action_name), newkey, oldkey);
}

/** \fn KeyBindings::RemoveActionKey(const QString&,const QString&,
                                     const QString&)
 *  \brief Unbind a key from an action.
 *
 *   Unless the action is Mandatory there is only one key in the
 *   action, this method should return true.
 *
 *  \param context_name The name of the context.
 *  \param action_name The name of the action.
 *  \param key The key to remove.
 *  \return true if the key was removed, or false if it was not.
 */
bool KeyBindings::RemoveActionKey(const QString &context_name,
                                  const QString &action_name,
                                  const QString &key)
{
    ActionID id(context_name, action_name);

    // Don't remove the last mandatory binding
    if (m_mandatoryBindings.contains(id) &&
        (m_actionSet.GetKeys(id).count() < 2))
    {
        return false;
    }

    return m_actionSet.Remove(id, key);
}


/** \fn KeyBindings::CommitAction(const ActionID&)
 *  \brief Commit an action to the database, and reload its keybindings.
 */
void KeyBindings::CommitAction(const ActionID &id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE keybindings "
        "SET keylist = :KEYLIST "
        "WHERE hostname = :HOSTNAME AND "
        "      action   = :ACTION   AND "
        "      context  = :CONTEXT");

    QString keys = m_actionSet.GetKeyString(id);
    query.bindValue(":KEYLIST",  keys);
    query.bindValue(":HOSTNAME", m_hostname);
    query.bindValue(":CONTEXT",  id.GetContext());
    query.bindValue(":ACTION",   id.GetAction());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("KeyBindings::CommitAction", query);
        return;
    }

    GetMythMainWindow()->ClearKey(id.GetContext(), id.GetAction());
    GetMythMainWindow()->BindKey(id.GetContext(), id.GetAction(), keys);
}

/** \fn KeyBindings::CommitJumppoint(const ActionID&)
 *  \brief Commit a jumppoint to the database.
 *
 *   TODO FIXME This does not reload the jumppoint.
 */
void KeyBindings::CommitJumppoint(const ActionID &id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE jumppoints "
        "SET keylist = :KEYLIST "
        "WHERE hostname    = :HOSTNAME AND"
        "      destination = :DESTINATION");

    QString keys = m_actionSet.GetKeyString(id);
    query.bindValue(":KEYLIST",     keys);
    query.bindValue(":HOSTNAME",    m_hostname);
    query.bindValue(":DESTINATION", id.GetAction());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("KeyBindings::CommitJumppoint", query);
        return;
    }

    GetMythMainWindow()->ClearJump(id.GetAction());
    GetMythMainWindow()->BindJump(id.GetAction(), keys);
}

/** \fn KeyBindings::CommitChanges(void)
 *  \brief Commit all changes made to the keybindings.
 *
 *   This method will write the changes to the database, unbind %MythTV's
 *   current bindings for those actions that changed, and setup the
 *   new bindings.
 */
void KeyBindings::CommitChanges(void)
{
    ActionList modified = m_actionSet.GetModified();

    while (modified.size() > 0)
    {
        ActionID id = modified.front();

        // commit either a jumppoint or an action
        if (id.GetContext() == ActionSet::kJumpContext)
            CommitJumppoint(id);
        else
            CommitAction(id);

        // tell the action set that the action is no longer modified
        m_actionSet.SetModifiedFlag(id, false);

        modified.pop_front();
    }
}

/**
 *  \brief Load the jumppoints from the database.
 *
 *   This method will load the keybindings for jump points.
 */
void KeyBindings::LoadJumppoints(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT destination, description, keylist "
        "FROM jumppoints "
        "WHERE hostname = :HOSTNAME "
        "ORDER BY destination");
    query.bindValue(":HOSTNAME", m_hostname);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("KeyBindings::LoadJumppoint", query);
        return;
    }

    while (query.next())
    {
        ActionID id(ActionSet::kJumpContext, query.value(0).toString());

        if (query.value(1).toString().isEmpty())
        {
            m_actionSet.AddAction(id, query.value(0).toString(),
                                  query.value(2).toString());
        }
        else
        {
            m_actionSet.AddAction(id, query.value(1).toString(),
                                  query.value(2).toString());
        }
    }
}

/**
 *  \brief Load the keybindings from the database.
 *
 *   This will load the keybindings which apply to the hostname
 *   specified to the constructor.
 */
void KeyBindings::LoadContexts(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT context, action, description, keylist "
        "FROM keybindings "
        "WHERE hostname = :HOSTNAME "
        "ORDER BY context, action");
    query.bindValue(":HOSTNAME", m_hostname);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("KeyBindings::LoadContexts", query);
        return;
    }

    while (query.next())
    {
        ActionID id(query.value(0).toString(), query.value(1).toString());
        m_actionSet.AddAction(id, query.value(2).toString(),
                              query.value(3).toString());
    }
}

/** \fn KeyBindings::LoadMandatoryBindings(void)
 *  \brief Load the mandatory bindings.
 */
void KeyBindings::LoadMandatoryBindings(void)
{
    if (!m_mandatoryBindings.empty())
        return;

    m_mandatoryBindings.append(ActionID(ActionSet::kGlobalContext, "DOWN"));
    m_defaultKeys.append("Down");

    m_mandatoryBindings.append(ActionID(ActionSet::kGlobalContext, "UP"));
    m_defaultKeys.append("Up");

    m_mandatoryBindings.append(ActionID(ActionSet::kGlobalContext, "LEFT"));
    m_defaultKeys.append("Left");

    m_mandatoryBindings.append(ActionID(ActionSet::kGlobalContext, "RIGHT"));
    m_defaultKeys.append("Right");

    m_mandatoryBindings.append(ActionID(ActionSet::kGlobalContext, "ESCAPE"));
    m_defaultKeys.append("Esc");

    m_mandatoryBindings.append(ActionID(ActionSet::kGlobalContext, "SELECT"));
    m_defaultKeys.append("Return, Enter, Space");
}

/** \fn KeyBindings::HasMandatoryBindings(void) const
 *  \brief Returns true iff all mandatory bindings are satisfied.
 */
bool KeyBindings::HasMandatoryBindings(void) const
{
    const ActionList &manlist = m_mandatoryBindings;
    for (int i = 0; i < manlist.count(); i++)
    {
        if (m_actionSet.GetKeys(manlist[i]).isEmpty())
            return false;
    }

    return true; // none are empty...
}
