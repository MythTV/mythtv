/* ============================================================
 * File  : mtdbitem.h
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Abstract: This file includes the definitions for the various 
 *           tree items used in the MovieTime main window.
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
#ifndef _MMT_DBITEM
#define _MMT_DBITEM

#include <mythtv/uilistbtntype.h>

extern const char* KEY_THEATER;
extern const char* KEY_MOVIE;
extern const char* KEY_TIME;
extern const char* KEY_DATE;



class MMTDBItem  : public UIListGenericTree
{
    public:
        MMTDBItem(UIListGenericTree *parent, const QString &text, 
                  const char *key, int id);
                  
        virtual ~MMTDBItem() { }

        QString getKey(void) { return Key; }
        int getID(void) { return ID; }
    
    protected:
        int ID;
        QString Key;
};


#endif
