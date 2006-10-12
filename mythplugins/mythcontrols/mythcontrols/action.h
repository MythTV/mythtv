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

// Qt headers
#include <qstringlist.h>
#include <qdict.h>

/** \class Action
 *  \brief An action (for this plugin) consists of a description,
 *         and a set of key sequences.
 *
 *   On its own, the action cannot actually identify a particular action.
 *   This is a class to make the keybinding class easier to manage.
 */
class Action
{
  public:
    /// \brief Create a new empty action.
    Action(const QString &description) : m_description(description) {}
    Action(const QString &description, const QString &keys);

    // Commands
    bool AddKey(const QString &key);
    bool ReplaceKey(const QString &newkey, const QString &oldkey);
    /// \brief Remove a key from this action.
    /// \return true on success, false otherwise.
    bool RemoveKey(const QString &key) { return m_keys.remove(key)>0; }

    // Gets
    /// \brief Returns the action description. (note: not threadsafe)
    QString     GetDescription(void) const { return m_description; }
    /// \brief Returns the key sequence(s) that trigger this action.
    ///        (note: not threadsafe)
    QStringList GetKeys(void)        const { return m_keys; }
    /// \brief Returns comma delimited string of key bindings
    QString     GetKeyString(void)   const { return m_keys.join(","); }
    /// \brief Returns true iff the action has no keys
    bool        IsEmpty(void)        const { return m_keys.empty(); }
    bool        HasKey(const QString &key) const;

  public:
    /// \brief The maximum number of keys that can be bound to an action.
    static const unsigned int kMaximumNumberOfBindings = 4;

  private:
    QString     m_description; ///< The actions description.
    QStringList m_keys;        ///< The keys bound to the action.
};
typedef QDict<Action> Context;

#endif /* ACTION_H */
