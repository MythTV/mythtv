/* ============================================================
 * File  : mtdbitem.cpp
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Abstract: This file includes the implementation for the 
 *           various tree items used in the MovieTime main window.
 *
 *
 * Copyright 2005 by J. Donavan Stanley
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include "mtdbitem.h"

const char* KEY_THEATER = "KEY_THEATER";
const char* KEY_MOVIE   = "KEY_MOVIE";
const char* KEY_TIME    = "KEY_TIME";
const char* KEY_DATE    = "KEY_DATE";

 
 
MMTDBItem::MMTDBItem(UIListGenericTree *parent, const QString &text, 
                     const char* key, int id)
         : UIListGenericTree(parent, text, "MMTDBItem")
{
    Key = key;
    ID = id;
}
