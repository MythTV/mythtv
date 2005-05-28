/**
 * @file action.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main header for the action class.
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

#ifndef ACTION_H
#define ACTION_H

#include <qstring.h>
#include <qstringlist.h>

/**
 * @class Action
 * @brief An action (for this plugin) consists of a description, and a
 * set of key sequences.
 *
 * On its own, the action cannot actually identify a particular
 * action.  This is a class to make the keybinding class easier to
 * manage.
 */
class Action {

 public:

  /**
   * @brief Create a new action.
   * @param description The description of the action.
   * @param keys The key sequence (strings) that trigger the action.
   */
  Action(const QString & description, const QString & keys);

  /**
   * @brief Get the action description.
   * @return The action description.
   */
  inline const QString & getDescription() const { return this->_description; }

  /**
   * @brief Get the key sequence(s) that trigger this action.
   * @return The key sequence(s) that trigger this action.
   */
  inline const QString & getKeys() const { return this->_keys; }

  /**
   * @brief Set the key sequence(s) for this action.
   * @param keys The keys sequence(s) for this action.
   */
  inline void setKeys(const QString & keys) { this->_keys = keys; }

  /**
   * @brief Add a key sequence to this action.
   * @param key The key to add to the action.
   */
  void addKey(const QString & key);

  /**
   * @brief Remove a key from this action.
   * @param key The key to remove.
   */
  void removeKey(const QString & key);

  /**
   * @brief Determine if the action already has a key.
   * @param key The key to check for.
   * @return true if the key is already bound in this action,
   * otherwise, false is returned.
   */
  bool hasKey(const QString & key) const;

  /**
   * @brief Determine if the action has any keys at all.
   * @return true if the action has keys, otherwise, false.
   */
  inline bool empty(void) const {
    return (getKeys().isEmpty() || getKeys().isNull());
  }

 protected:

  /**
   * @brief Get a reference to the action description.
   * @return A reference to the action description.
   */
  inline QString & description() { return this->_description; }

  /**
   * @brief Get a reference to actions the key sequences.
   * @return A reference to actions the key sequences.
   */
  inline QString & keys() { return this->_keys; }

 private:

  QString _description;
  QString _keys;
};

#endif /* ACTION_H */
