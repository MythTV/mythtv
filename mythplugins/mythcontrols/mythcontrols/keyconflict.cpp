/**
 * @file keyconflict.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Implementation of the key conflict class.
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
#ifndef KEYCONFLICT_CPP
#define KEYCONFLICT_CPP

using namespace std;

#include "keyconflict.h"

KeyConflict::KeyConflict(const QString & key, const ActionIdentifier & first,
			 const ActionIdentifier & second)
{
    this->key() = key;
    this->first() = first;
    this->second() = second;
}

#endif /* KEYCONFLICT_CPP */
