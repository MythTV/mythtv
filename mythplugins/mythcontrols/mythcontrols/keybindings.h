/**
 * @file keybindings.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main header for keybinding classes
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
#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H

#include <mythtv/mythdbcon.h>

#include "action.h"
#include "actionidentifier.h"
#include "keyconflict.h"

#define JUMP_CONTEXT "JumpPoints"
#define GLOBAL_CONTEXT "Global"


/**
 * @brief Contexts are dictionaries of actions.
 */
typedef QDict<Action> Context;



/**
 * @class KeyBindings.
 * @brief Information about the current keybindings.
 *
 * This class can retrieve, set, and modify the keybindings used by mythtv.
 */
class KeyBindings {

 public:

  /**
   * @brief Create a new KeyBindings instance.
   * @param hostname The host for which to create the key bindings.
   */
  KeyBindings(const QString & hostname);

  /**
   * @brief Get the hostname these keybindings are from.
   * @return The hostname these keybindings are from.
   */
  inline const QString & getHostname() const { return this->_hostname; }

  /**
   * @brief Get a list of the context names.
   * @return A list of the context names.
   */
  QStringList getContexts() const;

  /**
   * @brief Get a list of the actions in a context.
   * @param context The name of the context.
   * @return A list of action (names) for the target context.
   */
  QStringList getActions(const QString & context);

  /**
   * @brief Get an action's keys.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @return The keys bound to the specified context's action
   */
  QString getActionKeys(const QString & context_name,
			const QString & action_name) const;

  /**
   * @brief Get an action's description.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @return The description of the specified context's action
   */
  QString getActionDescription(const QString & context_name,
			       const QString & action_name) const;

  /**
   * @brief Add a key to an action.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @param key The key to add.
   * @see commitChanges
   *
   * This does not take effect until the commitChanges function is
   * called.
   */
  void addActionKey(const QString & context_name,
		    const QString & action_name,
		    const QString & key);

  /**
   * @brief Unbind a key from an action.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @param key The key to remove.
   */
  void removeActionKey(const QString & context_name,
		       const QString & action_name,
		       const QString & key);

  /**
   * @brief Set the keys for an action.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @param key The key to add.
   * @see commitChanges
   *
   * This does not take effect until the commitChanges function is
   * called.
   */
  void setActionKeys(const QString & context_name,
		     const QString & action_name,
		     const QString & keys);

  /**
   * @brief Get a conflicting key.
   * @return The first identified conflicting key.  If there are no
   * conflicts NULL is returned.
   */
  KeyConflict * getKeyConflict(void) const;

  /**
   * @brief Determine if the manditory bindings are set.
   * @return true if the manditory bindings are set, otherwise false.
   */
  bool hasManditoryBindings(void) const;

  /**
   * @brief Set the manditory bindings to their defaults.
   */
  void defaultManditoryBindings(void);

  /**
   * @brief Commit all changes made to the keybindings.
   *
   * This method will write the changes to the database, unbind myth's
   * current bindings for those actions that changed, and setup the
   * new bindings.
   */
  void commitChanges(void);

  /**
   * @brief Get the appropriate container of modified actions based on
   * context.
   * @return The appropriate container of modified actions.
   */
  inline const QValueList<ActionIdentifier> & getModified(const QString & context_name) const
  {
    return (context_name == JUMP_CONTEXT) ? _modified_jumppoints :
      _modified_actions;
  }


  /**
   * @brief Get a list of the manditory key bindings.
   * @return A list of the manditory bindings.
   */
  inline const QValueList<ActionIdentifier>& getManditoryBindings(void) const
  {
      return this->_manditory_bindings;
  }
  

 protected:

  /**
   * @brief Get a reference to the hostname.
   * @return A reference to the hostname.
   */
  inline QString & hostname() { return this->_hostname; }

  /**
   * @brief Get a reference to the contexts.
   * @return A reference to the contexts.
   */
  inline QDict<Context> & contexts() { return this->_contexts; }

  /**
   * @brief Get a context by its identifier.
   * @param context The context identifier.
   * @return The context, or null.
   */
  inline Context * getContext(const QString &context) const
  {
      return _contexts[context];
  }

  /**
   * @brief Get an action by its context and name.
   * @param context The context.
   * @param action The action name.
   * @return A pointer to the action.
   */
  inline Action * getAction(const QString & context,
			    const QString & action) const
  {
    return getContext(context) ? (*getContext(context))[action] : NULL;
  }

  /**
   * @brief Get an action by its action identifier.
   * @param id The action identifier.
   * @return A pointer to the action, or NULL.
   */
  inline Action * getAction (const ActionIdentifier &id) const
  {
      return getAction(id.first, id.second);
  }

  /**
   * @brief Add an action to the list of modified actions.
   * @param context_name The context the action is in.
   * @param action_name The name of the action.
   */
  void modify(const QString & context_name, const QString & action_name);

  /**
   * @brief Determine if an action has been modified.
   * @param context_name The context the action is in.
   * @param action_name The name of the action.
   * @return true if the action is modified, otherwise, false.
   */
  bool isModified(const QString & context_name,
		  const QString & action_name) const;

  /**
   * @brief Get the appropriate container of modified actions based on
   * context.
   * @return The appropriate container of modified actions.
   */
  inline QValueList<ActionIdentifier> & modified(const QString & context_name)
  {
    return (context_name == JUMP_CONTEXT) ? _modified_jumppoints :
      _modified_actions;
  }

  /**
   * @brief Get a reference to the actions which have been modified.
   * @return A value list of Action Identifiers corresponding to the
   * actions that have been modified.
   */
  inline QValueList<ActionIdentifier> & modifiedActions()
  {
    return this->_modified_actions;
  }

  /**
   * @brief Get a reference to the jumppoints which have been modified.
   * @return A value list of Action Identifiers corresponding to the 
   * jumppoints that have been modified.
   */
  inline QValueList<ActionIdentifier> & modifiedJumppoints()
  {
    return this->_modified_jumppoints;
  }

  /**
   * @brief Commit a jumppoint to the database.
   *
   * This (currently) does not reload the jumppoint.
   */
  void commitJumppoint(const ActionIdentifier &id);

  /**
   * @brief Commit an action to the database, and reload its keybindings.
   */
  void commitAction(const ActionIdentifier & id);

  /**
   * @brief Load the jumppoints from the database.
   *
   * This method will load the keybindings for jump points.
   */
  void retrieveJumppoints(void);

  /**
   * @brief Load the keybindings from the database.
   *
   * This will load the keybindings which apply to the hostname
   * specified to the constructor.
   */
  void retrieveContexts(void);

  /**
   * @brief Determine if two actions can conflict.
   * @param first The first action identifier.
   * @param second The second action identifier.
   * @return true if the actions could conflict if bound to the same
   * key.
   */
  bool actionsCanConflict(const ActionIdentifier & first,
			  const ActionIdentifier & second) const;

  /**
   * @brief Load the manditory bindings.
   */
  void loadManditoryBindings(void);


  /**
   * @brief Get a reference to the list of the manditory key bindings.
   * @return A reference to the list of the manditory bindings.
   */
  inline QValueList<ActionIdentifier> & manditoryBindings(void)
  {
      return this->_manditory_bindings;
  }

  /**
   * @brief Get the default keys.
   * @return A list of the default keys.
   */
  inline QStringList & defaultKeys() { return _default_keys; }

 private:

  QString _hostname;
  QDict<Context> _contexts;
  QValueList<ActionIdentifier> _modified_actions;
  QValueList<ActionIdentifier> _modified_jumppoints;
  QValueList<ActionIdentifier> _manditory_bindings;
  QStringList _default_keys;
};

#endif /* KEYBINDINGS_H */
