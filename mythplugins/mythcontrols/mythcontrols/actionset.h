/* -*- myth -*- */
/**
 * @file actionset.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main header for the action set class.
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
#ifndef ACTIONSET_H
#define ACTIONSET_H

#include <qdict.h>

/**
 * @brief The statically assigned context for jump point actions.
 */
#define JUMP_CONTEXT "JumpPoints"

/**
 * @brief The name of global actions.
 */
#define GLOBAL_CONTEXT "Global"


typedef QDict<Action> Context;
typedef QValueList<ActionID> ActionList;


/**
 * @class ActionSet
 * @brief Maintains consistancy between actions and keybindings.
 *
 * This class handles adding a removing bindings to keys.
 */
class ActionSet
{

public:

    /**
     * @brief Create a new, empty set of action bindings.
     */
    inline ActionSet() {};

    /**
     * @brief Add an action.
     * @param id The action identifier.
     * @param description The action description.
     * @param keys The keys for this action.
     * @return true if the action was added, otherwise, false.
     *
     * If the action has already been added, it will not be added
     * again.  There are no duplicate actions in the action set.
     */
    bool addAction(const ActionID &id, const QString & description,
                   const QString &keys);

    /**
     * @brief Add a binding.
     * @param id The action identifier.
     * @param key The symbolic name of the key.
     * @return true if the key was added otherwise, false.
     */
    bool add(const ActionID &id, const QString &key);

    /**
     * @brief Remove a key from an action identifier.
     * @param id The action identifier to remove from.
     * @param key The key to remove.
     */
    bool remove(const ActionID &id, const QString &key);

    /**
     * @brief Replace a specific key in a specific action.
     * @param id The action identifier.
     * @param newkey The new key.
     * @param oldkey The key to be replaced.
     */
    bool replace(const ActionID &id, const QString &newkey,
                 const QString &oldkey);

    /**
     * @brief Get a list of all contexts in the action set.
     * @return A list of all contexts.
     */
    QStringList * contextStrings(void) const;

    /**
     * @brief Get a list of all action in the action set.
     * @return A list of all actions in the action set.
     */
    QStringList * actionStrings(const QString & context_name) const;

    /**
     * @brief Get a string containing all the keys in bound to an
     * action.
     * @param id The action's identifier.
     * @return null if there is no such action, otherwise, the keys.
     */
    QString keyString(const ActionID &id) const;

    /**
     * @brief Get the keys bound to an action by it's identifier.
     * @param id The action identifier.
     * @return The keys used by the action.
     */
    QStringList getKeys(const ActionID &id) const;

    /**
     * @brief Get the description of an action.
     * @param id The action identifier.
     * @return The action description.
     */
    const QString & getDescription(const ActionID &id) const;

    /**
     * @brief Get the actions bound to the specified key.
     * @param key The key.
     * @return The list of actions, which are bound to the key.
     */
    const ActionList & getActions(const QString &key) const;

    /**
     * @brief Get the appropriate container of modified actions based on
     * context.
     * @return The appropriate container of modified actions.
     */
    inline const ActionList & getModified(void) const { return _modified; }

    /**
     * @brief Determine if the action set has been modified.
     * @return True if changes have been made, otherwise, false.
     */
    inline bool hasModified(void) const { return !(_modified.isEmpty()); }

    /**
     * @brief Mark an action as unmodified.
     * @return true if the action was modified, and is set to unmodified.
     */
    inline bool unmodify(const ActionID & id)
    {
        return (_modified.remove(id) > 0);
    }

    /**
     * @brief Determine if an action has been modified.
     * @param id The action identifier.
     * @return true if the action is modified, otherwise, false.
     */
    inline bool isModified(const ActionID &id)const { return !unmodified(id); }

    /**
     * @brief Determine if an action is still unmodified.
     * @param id The action identifier.
     * @return true if the action has not been modified, otherwise, false.
     */
    inline bool unmodified(const ActionID &id) const
    {
        return (getModified().contains(id) == 0);
    }

protected:

    /**
     * @brief Mark an action as modified.
     * @param id The actio identifier.
     */
    inline void modify(const ActionID &id)
    {
        if (unmodified(id)) _modified.push_back(id);
    }

    /**
     * @brief Get an action by its identifier.
     * @return A pointer to the action.
     */
    inline Action * action(const ActionID &id) const
    {
        return context(id.context()) ?
            (*context(id.context()))[id.action()] : NULL;
    }

    /**
     * @brief Get a context by its identifier.
     * @return A pointer to the context.
     */
    inline Context * context(const QString & name) const
    {
        return this->_contexts[name];
    }

private:

    QMap<QString, ActionList> _keymap;
    QDict<Context> _contexts;
    ActionList _modified;
};

#endif /* ACTIONSET_H */
