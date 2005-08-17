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

#include <iostream>
#include <qkeysequence.h>

using namespace std;

#include "action.h"


Action::Action(const QString & description, const QString & keys)
{
    this->description() = description;
    this->keys() = QStringList::split(", ", QString(QKeySequence(keys)));
}



bool Action::hasKey(const QString & key) const
{
    /* check the keys */
    for (size_t i = 0; i < getKeys().count(); i++)
    {
        if (getKeys()[i] == key) return true;
    }

    /* the key was not found, so return false */
    return false;
}



bool Action::addKey(const QString & key)
{

    /* dont add empty keys and dont add too many, or duplicates  */
    if ((key.length() == 0) ||
        (getKeys().size() >= MAX_KEYS) ||
        (getKeys().contains(key)))
        return false;

    /* add the key */
    this->keys().push_back(key);
    return true;
}



/* comments in header */
bool Action::replaceKey(const QString & newkey, const QString &oldkey)
{
    /* make sure that the key list doesn't already have the new key */
    if (getKeys().contains(newkey) == 0)
    {
        for (size_t i = 0; i < getKeys().size(); i++)
        {
            if (getKeys()[i] == oldkey)
            {
                keys()[i] = newkey;
                return true;
            }
        }
    }

    return false;
}

#endif /* ACTION_CPP */
