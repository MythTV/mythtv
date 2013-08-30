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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
#ifndef ACTIONSET_H
#define ACTIONSET_H

// Qt headers
#include <QStringList>
#include <QHash>
#include <QMap>

/** \class ActionSet
 *  \brief Maintains consistancy between actions and keybindings.
 *
 *   This class handles adding a removing bindings to keys.
 */
class ActionSet
{
  public:
    /// \brief Create a new, empty set of action bindings.
    ActionSet() {}
    ~ActionSet();

    // Commands
    bool AddAction(const ActionID &id,
                   const QString  &description,
                   const QString  &keys);
    bool Add(const ActionID &id, const QString &key);
    bool Remove(const ActionID &id, const QString &key);
    bool Replace(const ActionID &id,
                 const QString  &newkey,
                 const QString  &oldkey);

    // Sets
    bool        SetModifiedFlag(const ActionID &id, bool modified);

    // Gets
    QStringList GetContextStrings(void) const;
    QStringList GetActionStrings(const QString &context_name) const;
    QString     GetKeyString(const ActionID &id) const;
    QStringList GetKeys(const ActionID &id) const;
    QStringList GetContextKeys(const QString &context_name) const;
    QStringList GetAllKeys(void) const;
    QString     GetDescription(const ActionID &id) const;
    ActionList  GetActions(const QString &key) const;
    ActionList  GetModified(void) const;
    /// \brief Returns true iff changes have been made.
    bool HasModified(void) const { return !m_modified.isEmpty(); }
    /// \brief Returns true iff the action is modified.
    bool IsModified(const ActionID &id) const
        { return m_modified.contains(id); }

  protected:
    Action *GetAction(const ActionID &id);

  public:
    /// \brief The statically assigned context for jump point actions.
    static const QString kJumpContext;
    /// \brief The name of global actions.
    static const QString kGlobalContext;

  private:
    QMap<QString, ActionList> m_keyToActionMap;
    typedef QHash<QString, Context> ContextMap;
    ContextMap                m_contexts;
    ActionList                m_modified;
};

#endif /* ACTIONSET_H */
