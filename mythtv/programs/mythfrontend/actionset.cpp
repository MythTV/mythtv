/* -*- myth -*- */
/**
 * @file actionset.cpp
 * @brief Implementation of the action set.
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
 */

// MythTV headers
#include "mythlogging.h"

// MythContext headers
#include "action.h"
#include "actionset.h"

const QString ActionSet::kJumpContext   = "JumpPoints";
const QString ActionSet::kGlobalContext = "Global";

ActionSet::~ActionSet()
{
    while (!m_contexts.empty())
    {
        Context &ctx = *m_contexts.begin();
        while (!ctx.empty())
        {
            delete *ctx.begin();
            ctx.erase(ctx.begin());
        }
        m_contexts.erase(m_contexts.begin());
    }
}

/** \fn ActionSet::Add(const ActionID&, const QString&)
 *  \brief Add a binding.
 *
 *   If the key is added, we mark this action as modified,
 *   and update the key map.
 *
 *  \param id The action identifier.
 *  \param key The symbolic name of the key.
 *  \return true if the key was added otherwise, false.
 */
bool ActionSet::Add(const ActionID &id, const QString &key)
{
    Action *a = GetAction(id);

    if (!a)
        return false;

    if (!a->AddKey(key))
    {
        LOG(VB_GENERAL, LOG_ERR, "ActionSet::AddKey() failed");
        return false;
    }

    ActionList &ids = m_keyToActionMap[key];
    ids.push_back(id);
    SetModifiedFlag(id, true);

    return true;
}

/** \fn ActionSet::Remove(const ActionID&, const QString&)
 *  \brief Remove a key from an action identifier.
 *
 *   If the key is removed from the action, then we remove
 *   it from the key list, and mark the action id as modified.
 *
 *  \param id The action identifier to remove from.
 *  \param key The key to remove.
 *
 *  \todo Remove the actionlist from the m_keyToActionMap if the key
 *        is no longer bound to any actions.
 */
bool ActionSet::Remove(const ActionID &id, const QString &key)
{
    Action *a = GetAction(id);

    if (!a)
        return false;

    if (!a->RemoveKey(key))
        return false;

    m_keyToActionMap[key].removeAll(id);

    // remove the key if there isn't anything bound to it.
    if (m_keyToActionMap[key].isEmpty())
        m_keyToActionMap.remove(key);

    SetModifiedFlag(id, true);

    return true;
}

/** \fn ActionSet::Replace(const ActionID&,const QString&,const QString&)
 *  \brief Replace a specific key in a specific action.
 *
 *   If the key is replaced, then we remove the old key from the
 *   key list and add the new key and mark the action as modified.
 *
 *  \param id The action identifier.
 *  \param newkey The new key.
 *  \param oldkey The key to be replaced.
 */
bool ActionSet::Replace(const ActionID &id,
                        const QString &newkey,
                        const QString &oldkey)
{
    Action *a = GetAction(id);

    if (!a)
        return false;

    if (!a->ReplaceKey(newkey, oldkey))
        return false;

    m_keyToActionMap[oldkey].removeAll(id);
    m_keyToActionMap[newkey].push_back(id);
    SetModifiedFlag(id, true);

    return true;
}

/** \fn ActionSet::SetModifiedFlag(const ActionID&, bool)
 *  \brief Mark an action as modified or unmodified by its identifier.
 *  \return true if the action was modified, and is set to unmodified.
 */
bool ActionSet::SetModifiedFlag(const ActionID &id, bool modified)
{
    if (!modified)
        return m_modified.removeAll(id);

    if (!IsModified(id))
    {
        m_modified.push_back(id);
        return true;
    }

    return false;
}

/** \fn ActionSet::GetContextStrings(void) const
 *  \brief Returns a list of all contexts in the action set.
 */
QStringList ActionSet::GetContextStrings(void) const
{
    QStringList context_strings;

    ContextMap::const_iterator it = m_contexts.begin();
    for (; it != m_contexts.end(); ++it)
        context_strings.append(it.key());

    context_strings.detach();
    return context_strings;
}

/** \fn ActionSet::GetActionStrings(const QString&) const
 *  \brief Returns a list of all action in the action set.
 */
QStringList ActionSet::GetActionStrings(const QString &context_name) const
{
    QStringList action_strings;

    ContextMap::const_iterator cit = m_contexts.find(context_name);
    if (cit == m_contexts.end())
        return action_strings;

    Context::const_iterator it = (*cit).begin();
    for (; it != (*cit).end(); ++it)
        action_strings.append(it.key());

    action_strings.detach();
    return action_strings;
}

/** \fn ActionSet::AddAction(const ActionID&, const QString&, const QString&)
 *  \brief Add an action.
 *
 *   If the action has already been added, it will not be added
 *   again. There are no duplicate actions in the action set.
 *
 *  \param id The action identifier.
 *  \param description The action description.
 *  \param keys The keys for this action.
 *  \return true if the action on success, false otherwise.
 */
bool ActionSet::AddAction(const ActionID &id,
                          const QString  &description,
                          const QString  &keys)
{
    ContextMap::iterator cit = m_contexts.find(id.GetContext());
    if (cit == m_contexts.end())
        cit = m_contexts.insert(id.GetContext(), Context());
    else if ((*cit).find(id.GetAction()) != (*cit).end())
        return false;

    Action *a = new Action(description, keys);
    (*cit).insert(id.GetAction(), a);

    const QStringList keylist = a->GetKeys();
    QStringList::const_iterator it = keylist.begin();
    for (; it != keylist.end(); ++it)
        m_keyToActionMap[*it].push_back(id);

    return true;
}

/** \fn ActionSet::GetKeyString(const ActionID&) const
 *  \brief Returns a string containing all the keys in bound to an action
 *         by its identifier.
 */
QString ActionSet::GetKeyString(const ActionID &id) const
{
    ContextMap::const_iterator cit = m_contexts.find(id.GetContext());
    if (cit == m_contexts.end())
        return QString();

    Context::const_iterator it = (*cit).find(id.GetAction());
    if (it != (*cit).end())
        return (*it)->GetKeyString();

    return QString();
}

/** \fn ActionSet::GetKeys(const ActionID&) const
 *  \brief Get the keys bound to an action by its identifier.
 */
QStringList ActionSet::GetKeys(const ActionID &id) const
{
    QStringList keys;

    ContextMap::const_iterator cit = m_contexts.find(id.GetContext());
    if (cit == m_contexts.end())
        return keys;

    Context::const_iterator it = (*cit).find(id.GetAction());
    if (it != (*cit).end())
        keys = (*it)->GetKeys();

    keys.detach();
    return keys;
}

QStringList ActionSet::GetContextKeys(const QString &context_name) const
{
    QStringList keys;
    ContextMap::const_iterator cit = m_contexts.find(context_name);
    if (cit == m_contexts.end())
        return keys;

    Context::const_iterator it = (*cit).begin();
    for (; it != (*cit).end(); ++it)
        keys += (*it)->GetKeys();
    keys.sort();

    keys.detach();
    return keys;
}

/** \brief Get all keys (from every context) to which an action is bound.
 */
QStringList ActionSet::GetAllKeys(void) const
{
    QStringList keys;

    QMap<QString, ActionList>::ConstIterator it;

    for (it = m_keyToActionMap.begin(); it != m_keyToActionMap.end(); ++it)
        keys.push_back(it.key());

    return keys;
}

/** \fn ActionSet::GetDescription(const ActionID&) const
 *  \brief Returns the description of an action by its identifier.
 */
QString ActionSet::GetDescription(const ActionID &id) const
{
    ContextMap::const_iterator cit = m_contexts.find(id.GetContext());
    if (cit == m_contexts.end())
        return QString();

    Context::const_iterator it = (*cit).find(id.GetAction());
    if (it != (*cit).end())
        return (*it)->GetDescription();

    return QString();
}

/** \fn ActionSet::GetActions(const QString &key) const
 *  \brief Returns the actions bound to the specified key.
 */
ActionList ActionSet::GetActions(const QString &key) const
{
    ActionList list = m_keyToActionMap[key];
    list.detach();
    return list;
}

/** \fn ActionSet::GetModified(void) const
 *  \brief Returns the appropriate container of modified actions
 *         based on current context.
 */
ActionList ActionSet::GetModified(void) const
{
    ActionList list = m_modified;
    list.detach();
    return list;
}

/** \fn ActionSet::GetAction(const ActionID&)
 *  \brief Returns a pointer to an action by its identifier.
 *         (note: result not thread-safe)
 */
Action *ActionSet::GetAction(const ActionID &id)
{
    ContextMap::iterator cit = m_contexts.find(id.GetContext());
    if (cit == m_contexts.end())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("GetAction: Did not find context '%1'")
                .arg(id.GetContext()));

        return NULL;
    }

    Context::iterator it = (*cit).find(id.GetAction());

    if (it == (*cit).end())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("GetAction: Did not find action '%1' in context '%1'")
                .arg(id.GetAction()).arg(id.GetContext()));
        return NULL;
    }

    return *it;
}
