/* -*- myth -*- */
/**
 * @file actionid.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Header for the action identifier.
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
#ifndef ACTIONID_H
#define ACTIONID_H

// Qt headers
#include <QString>
#include <QList>

/** \class ActionID
 *  \brief A class that uniquely identifies an action.
 *
 *  Actions are identified based on their action name and context.
 */
class ActionID
{
  public:
    /// \brief Create an empty action
    ActionID() : m_context(QString::null), m_action(QString::null) { }

    /** \brief Create a new action identifier
     *  \param context The action's context
     *  \param action The action's name
     */
    ActionID(const QString &context, const QString &action)
        : m_context(context), m_action(action)
    {
        m_context.detach();
        m_action.detach();
    }

    ActionID(const ActionID &other)
        : m_context(other.m_context), m_action(other.m_action)
    {
        m_context.detach();
        m_action.detach();
    }

    /// \brief Returns the context name.
    QString GetContext(void) const
    {
        QString tmp = m_context;
        tmp.detach();
        return tmp;
    }

    /// \brief Returns the action name.
    QString GetAction(void)  const
    {
        QString tmp = m_action;
        tmp.detach();
        return tmp;
    }

    bool operator==(const ActionID &other) const
    {
        return ((m_action  == other.m_action) &&
                (m_context == other.m_context));
    }

  private:
    QString m_context;
    QString m_action;
};
typedef QList<ActionID> ActionList;

#endif /* ACTIONID_H */
