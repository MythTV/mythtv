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
#include "actionid.h"
#include "actionset.h"

/**
 * @class KeyBindings.
 * @brief Information about the current keybindings.
 *
 * This class can retrieve, set, and modify the keybindings used by
 * mythtv.  It is a layer between the user interface, and the
 * keybindings themeslfs.
 */
class KeyBindings {

 public:

  /**
   * @brief Levels of conflict 
   * @li None: Does not conflict
   * @li Potential: May conflict.
   * @li Fatal: Frontend will become inoperable.
   */
  enum ConflictLevels { Warning, Error};

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
  inline QStringList getContexts() const {return actionset.contextStrings();}

  /**
   * @brief Get a list of the actions in a context.
   * @param context The name of the context.
   * @return A list of action (names) for the target context.
   */
  inline QStringList getActions(const QString & context) {
    return actionset.actionStrings(context);
  }

  /**
   * @brief Get an action's keys.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @return The keys bound to the specified context's action
   */
  inline QStringList getActionKeys(const QString & context_name,
				   const QString & action_name) const {
      return actionset.getKeys(ActionID(context_name, action_name));
  }

  /**
   * @brief Get an action's description.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @return The description of the specified context's action
   */
  inline QString getActionDescription(const QString & context_name,
				      const QString & action_name) const {
    return actionset.getDescription(ActionID(context_name, action_name));
  }

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
  inline void addActionKey(const QString & context_name,
			   const QString & action_name,
			   const QString & key) {
    this->actionset.add(ActionID(context_name, action_name), key);
  }

  /**
   * @brief Determine if adding a key would cause a conflict.
   * @param context_name The name of the context.
   * @param key The key to add.
   * @param level The level of conflict.  Check this if the return
   * value is not NULL
   * @return true if adding the key would cause a conflict.
   *
   * Conflicts occur if:
   * @li the key is a jump point, but is bound elsewhere
   * @li the key is already bound to a jumppoint.
   * @li the key is bound to something in the global context.
   * @li the key is bound to something else in the specified context
   *
   * If the method does not return NULL, check the value given to
   * level.  Warnings can be ignored (at the users disgression), but
   * errors should be prevented no matter what.  If they dont like it,
   * they can learn SQL.
   */
  ActionID * conflicts(const QString & context_name,
		       const QString & key, int &level) const;

  /**
   * @brief Replace a key in an action.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @param newkey The key to add.
   * @param oldkey The index of the key to be replaced
   * @see commitChanges
   *
   * This does not take effect until the commitChanges function is
   * called.
   */
  inline void replaceActionKey(const QString & context_name,
			       const QString & action_name,
			       const QString & newkey,
			       const QString & oldkey) {
      this->actionset.replace(ActionID(context_name, action_name),
			     newkey, oldkey);
  }

  /**
   * @brief Unbind a key from an action.
   * @param context_name The name of the context.
   * @param action_name The name of the action.
   * @param key The key to remove.
   * @return true if the key was removed, or false if it was not.
   *
   * Unless the action is manditory there is only one key in the
   * action, this method should return true.
   */
  bool removeActionKey(const QString & context_name,
		       const QString & action_name,
		       const QString & key);

  /**
   * @brief Set the manditory bindings to their defaults.
   */
  void defaultManditoryBindings(void);

  /**
   * @brief Determine if there are changes to the keybindings.
   * @return True if keys have been changed, otherwise, false.
   */
  inline bool hasChanges(void) const { return actionset.hasModified(); }

  /**
   * @brief Commit all changes made to the keybindings.
   *
   * This method will write the changes to the database, unbind myth's
   * current bindings for those actions that changed, and setup the
   * new bindings.
   */
  void commitChanges(void);

  /**
   * @brief Get a list of the manditory key bindings.
   * @return A list of the manditory bindings.
   */
  inline const ActionList& getManditoryBindings(void) const {
      return this->_manditory_bindings;
  }

  /**
   * @brief Determine if all manditory bindings are satisfied.
   * @return true if all manditory bindings are satisfied, otherwise, false.
   */
  bool hasManditoryBindings(void) const;

 protected:

  /**
   * @brief Get a reference to the hostname.
   * @return A reference to the hostname.
   */
  inline QString & hostname() { return this->_hostname; }

  /**
   * @brief Commit a jumppoint to the database.
   *
   * This (currently) does not reload the jumppoint.
   */
  void commitJumppoint(const ActionID &id);

  /**
   * @brief Commit an action to the database, and reload its keybindings.
   */
  void commitAction(const ActionID & id);

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
   * @brief Load the manditory bindings.
   */
  void loadManditoryBindings(void);

  /**
   * @brief Get a reference to the list of the manditory key bindings.
   * @return A reference to the list of the manditory bindings.
   */
  inline ActionList & manditoryBindings(void) { return _manditory_bindings; }

  /**
   * @brief Get the default keys.
   * @return A list of the default keys.
   */
  inline QStringList & defaultKeys() { return _default_keys; }

 private:

  QString _hostname;
  ActionList _manditory_bindings;
  QStringList _default_keys;
  ActionSet actionset;
};

#endif /* KEYBINDINGS_H */
