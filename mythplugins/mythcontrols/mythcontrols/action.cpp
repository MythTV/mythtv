/* -*- myth -*- */
/**
 * @file action.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Implementation of the action class.
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
#ifndef ACTION_CPP
#define ACTION_CPP

// Qt headers
#include <qkeysequence.h>

// MythControls headers
#include "action.h"

/** \fn Action::Action(const QString&, const QString&)
 *  \brief Create a new action.
 *
 *   The keys are parased by a QKeySequence, so they should be in ","
 *   or ", " delimeted format.  Also, there should be no more than
 *   four keys in the string.
 *
 *  \param description The description of the action.
 *  \param keys The key sequence (strings) that trigger the action.
 */
Action::Action(const QString &description, const QString &keys)
    : m_description(description),
      m_keys(QStringList::split(", ", QString(QKeySequence(keys))))
{
}

/** \fn Action::HasKey(const QString&) const
 *  \brief Determine if the action already has a key.
 *  \param key The key to check for.
 *  \return true if the key is already bound in this action,
 *          otherwise, false is returned.
 */
bool Action::HasKey(const QString &key) const
{
    for (size_t i = 0; i < GetKeys().count(); i++)
    {
        if (GetKeys()[i] == key)
            return true;
    }

    return false;
}

/** \fn Action::AddKey(const QString&)
 *  \brief Add a key sequence to this action.
 *
 *   We don't add empty keys nor duplicates, and can not
 *   add more than kMaximumNumberOfBindings. If any of
 *   these restrictions are a problem we return false and
 *   do not add the binding.
 *
 *  \param key The key to add to the action.
 *  \return true if the key was added otherwise, false.
 */
bool Action::AddKey(const QString &key)
{
    if (key.isEmpty() ||
        (GetKeys().size() >= kMaximumNumberOfBindings) ||
        (GetKeys().contains(key)))
    {
        return false;
    }

    m_keys.push_back(key);
    return true;
}

/** \fn Action::ReplaceKey(const QString&, const QString&)
 *  \brief Replace a key.
 *  \param newkey The new key.
 *  \param oldkey The old key, which is being replaced.
 */
bool Action::ReplaceKey(const QString &newkey, const QString &oldkey)
{
    // make sure that the key list doesn't already have the new key
    if (GetKeys().contains(newkey) != 0)
        return false;

    for (size_t i = 0; i < GetKeys().size(); i++)
    {
        if (GetKeys()[i] == oldkey)
        {
            m_keys[i] = newkey;
            return true;
        }
    }

    return false;
}

#endif /* ACTION_CPP */
