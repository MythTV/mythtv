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

enum TreeKeys {
    KEY_THEATER = 1,    
    KEY_THEATER_MASTER, 
    KEY_MOVIE,          
    KEY_MOVIE_MASTER,   
    KEY_TIME,
    KEY_DATE
};    



class MMTDBItem  : public UIListGenericTree
{
    public:
        MMTDBItem(UIListGenericTree *parent, const QString &text, 
                  TreeKeys key, int id);
                  
        virtual ~MMTDBItem() { }

        int getKey(void) { return Key; }
        int getID(void) { return ID; }
        virtual void populateTree(UIListGenericTree*) {return;}
        
    protected:
        int ID;
        int Key;
};

// MMTMovieMasterItem encapsulates the top level list of movies
// It's used to handle the "I want to watch movie XXX, when is it on?" question
class MMTMovieMasterItem : public MMTDBItem
{
    public:
        MMTMovieMasterItem(UIListGenericTree* parent, const QString &text);
        virtual void populateTree(UIListGenericTree* Tree);
};

class MMTMovieItem : public MMTDBItem
{
    public:
        MMTMovieItem(UIListGenericTree* parent, const QString &text, int filmID,
                     const QString& theaterID = "");
        
        virtual void populateTree(UIListGenericTree* tree) {
            if (!TheaterID.isEmpty())
                populateTreeWithDates(tree);
            else
                populateTreeWithTheaters(tree);
        }
                        
    protected:
        void populateTreeWithDates(UIListGenericTree* tree);
        void populateTreeWithTheaters(UIListGenericTree* tree);
        
        QString TheaterID;
};

// MMTTheaterMasterItem encapsulates the top level list of movies
// It's used to handle the "I want to watch movie at theater XXX, what's showing?" question
class MMTTheaterMasterItem : public MMTDBItem
{
    public:
        MMTTheaterMasterItem(UIListGenericTree* parent, const QString &text);
        virtual void populateTree(UIListGenericTree* Tree);
};

class MMTTheaterItem : public MMTDBItem
{
    public:
        MMTTheaterItem(UIListGenericTree* parent, const QString &text, 
                       const QString& theaterID, int filmID = 0, int showingID = 0);
        virtual void populateTree(UIListGenericTree* Tree);
    
    protected:
        void populateTreeWithMovies(UIListGenericTree* Tree);
        void populateTreeWithDates(UIListGenericTree* Tree);
        
        QString TheaterID;
        int FilmID;
        int ShowingID;
};


#endif
