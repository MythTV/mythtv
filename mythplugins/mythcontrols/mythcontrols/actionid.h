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

#include <qstring.h>


/**
 * @class ActionID
 * @brief A class that uniquely identifies an action.
 *
 * Actions are identified based on their action name and context.
 */
class ActionID
{

public:

    /**
     * @brief Create an empty action
     */
    inline ActionID() {}

    /**
     * @brief Create a new action identifier.
     * @param context The action's context name.
     * @param action The action's name
     */
    inline ActionID(const QString & context, const QString & action)
        : _context(context), _action(action) {};

    /**
     * @brief Get the context name.
     * @return The context name.
     */
    inline QString context(void) const { return this->_context; }

    /**
     * @brief Get the action name
     * @return The action name.
     */
    inline QString action(void) const { return this->_action; }

    /**
     * @brief Determine if two actions identifiers have the same
     * context.
     * @return True if the actions have a common context.
     */
    inline static bool sameContext(const ActionID &a, const ActionID &b)
    {
        return (a.context() == b.context());
    }

    /**
     * @brief Determine if two actions have the same action name.
     * @return True if the actions have the same action name.
     */
    inline static bool sameAction(const ActionID &a, const ActionID &b)
    {
        return (a.action() == b.action());
    }

    /**
     * @brief Determine if the action identifiers are equal
     * @return true if the operators are equal, otherwise, false is
     * returned.
     */
    inline bool operator == (const ActionID &that) const
    {
        return (sameAction(*this,that) && sameContext(*this,that));
    }

private:
    QString _context;
    QString _action;
};




#endif /* ACTIONID_H */
