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
    this->keys() = keys;
}



bool Action::hasKey(const QString & key) const
{

    /* of course we dont silly */
    if (this->getKeys().isEmpty() || this->getKeys().isNull()) return false;

    /* Using the key sequence means we dont have to worry about
       incorrect or malformed separators ("," vs ", ").*/
    QKeySequence keyseq(this->getKeys());
    QKeySequence newkey(key);

    for (size_t i = 0; i < keyseq.count(); i++)
    {
	if (newkey[0] == keyseq[i]) return true;
    }

    /* the key was not found, so return false */
    return false;
}



void Action::addKey(const QString & key)
{

    /* just add the key if there are none */
    if ((this->getKeys().isEmpty()) || (this->getKeys().isNull()))
    {
	this->keys() = key;
    }
    else if (!hasKey(key))
    {
	QStringList keylist;
	QKeySequence keyseq(key + "," + this->getKeys());

	/* add each key to the list */
	for (size_t i = 0; i < keyseq.count(); i++)
	    keylist.push_back(QString(QKeySequence(keyseq[i])));

	/* join the darkside */
	this->keys() = keylist.join(",");
    }
}



void Action::removeKey(const QString & key)
{

    if (hasKey(key))
    {
	QStringList keylist;
	QKeySequence keyseq(this->getKeys());
	QKeySequence newkey(key);

	for (size_t i = 0; i < keyseq.count(); i++)
	{
	    if (newkey[0] != keyseq[i])
	    {
		keylist.push_back(QString(QKeySequence(keyseq[i])));
	    }
	}

	this->keys() = keylist.join(",");
    }
}

#endif /* ACTION_CPP */
