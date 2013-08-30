/* -*- myth -*- */
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H

// MythControls headers
#include "action.h"
#include "actionset.h"

/** @class KeyBindings.
 *  @brief Encapsulates information about the current keybindings.
 *
 *   This class can retrieve, set, and modify the keybindings used by %MythTV
 *
 */
class KeyBindings
{
  public:
    /// \brief Levels of conflict
    enum ConflictLevels { kKeyBindingWarning, kKeyBindingError, };

    KeyBindings(const QString &hostname);

    // Commands
    bool        AddActionKey(const QString &context_name,
                             const QString &action_name,
                             const QString &key);
    ActionID   *GetConflict(const QString &context_name,
                            const QString &key, int &level) const;
    void        ReplaceActionKey(const QString &context_name,
                                 const QString &action_name,
                                 const QString &newkey,
                                 const QString &oldkey);
    bool        RemoveActionKey(const QString &context_name,
                                const QString &action_name,
                                const QString &key);
    void        CommitChanges(void);

    // Gets
    QStringList GetKeys(void) const;
    QStringList GetContexts(void) const;
    QStringList GetActions(const QString &context) const;
    void        GetKeyActions(const QString &key, ActionList &list) const;
    QStringList GetActionKeys(const QString &context_name,
                              const QString &action_name) const;
    QStringList GetContextKeys(const QString &context) const;
    QStringList GetKeyContexts(const QString &key) const;
    QString     GetActionDescription(const QString &context_name,
                                     const QString &action_name) const;
    bool        HasMandatoryBindings(void) const;
    bool        HasChanges(void) const { return m_actionSet.HasModified(); }

  protected:
    void CommitJumppoint(const ActionID &id);
    void CommitAction(const ActionID &id);

    void LoadMandatoryBindings(void);
    void LoadContexts(void);
    void LoadJumppoints(void);

  private:
    QString     m_hostname;
    ActionList  m_mandatoryBindings;
    QStringList m_defaultKeys;
    ActionSet   m_actionSet;
};

#endif /* KEYBINDINGS_H */
