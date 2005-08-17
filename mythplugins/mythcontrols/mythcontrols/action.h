/* -*- myth -*- */
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
     * @brief Create a new empty action.
     * @param description The actions description.
     */
    inline Action(const QString & description) : _description(description) {}

    /**
     * @brief Create a new action.
     * @param description The description of the action.
     * @param keys The key sequence (strings) that trigger the action.
     *
     * The keys are parased by a QKeySequence, so they should be in ","
     * or ", " delimeted format.  Also, there should be no more than
     * four keys in the string.
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
    inline const QStringList & getKeys() const { return this->_keys; }

    /**
     * @brief Get the keys as a string.
     * @return QString::null or a comma delimited string of keys.
     */
    inline QString keyString() const { return _keys.join(","); }

    /**
     * @brief Add a key sequence to this action.
     * @param key The key to add to the action.
     * @return true if the key was added otherwise, false.
     */
    bool addKey(const QString & key);

    /**
     * @brief Remove a key from this action.
     * @param key The key to remove.
     * @return true if the key is removed, otherwise, false.
     */
    inline bool removeKey(const QString & key) { return (keys().remove(key)>0); }

    /**
     * @brief Replace a key.
     * @param newkey The new key.
     * @param oldkey The old key, which is being replaced.
     */
    bool replaceKey(const QString & newkey, const QString & oldkey);

    /**
     * @brief Determine if the action already has a key.
     * @param key The key to check for.
     * @return true if the key is already bound in this action,
     * otherwise, false is returned.
     */
    bool hasKey(const QString & key) const;

    /**
     * @brief Get the index of a key.
     * @param key The string containing the key.
     * @return The index of the key, which will be less than zero if the
     * key does not exist.
     */

    /**
     * @brief Determine if the action has any keys at all.
     * @return true if the action has keys, otherwise, false.
     */
    inline bool empty(void) const { return (_keys.size() > 0); }

    /** @brief The maximum number of keys that can be bound to an action. */
    static const unsigned short MAX_KEYS = 4;

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
    inline QStringList & keys() { return this->_keys; }

private:

    QString _description;
    QStringList _keys;
};

#endif /* ACTION_H */
