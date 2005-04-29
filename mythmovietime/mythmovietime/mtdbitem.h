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
 * Public License as published by the Free Software Foundation;
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


class MMTDBItem  : public UIListGenericTree
{
    public:
        MMTDBItem(UIListGenericTree *parent, const QString &text, 
                  int FilmID = 0, const QString& theaterID = "ALL",
                  int ShowingID = 0, int TimeID = 0, const QString& officialURL = "");
                  
        virtual ~MMTDBItem() { }

        int getKey(void) const { return Key; }
        int getFilmID(void) const { return FilmID; }
        int getShowingID(void) const { return ShowingID; }
        
        const QString& getTheaterID(void) const { return TheaterID; }
        const QString& getOfficialURL() const { return OfficialURL; }        
        
        virtual void populateTree(UIListGenericTree*) {return;}
        virtual void toMap(QMap<QString, QString> &map);
        
    protected:
        int Key;
        int FilmID;
        int ShowingID;
        int TimeID;

        QString TheaterID;
        QString OfficialURL;        
};


class MMTShowingItem : public MMTDBItem
{
    public:
        MMTShowingItem(UIListGenericTree* parent, const QDate& dt, int FilmID = 0, 
                       const QString& theaterID = "", int ShowingID = 0, int timeID = 0,
                       const QString& officialURL = "");
        virtual void populateTree(UIListGenericTree* Tree);
        virtual void toMap(QMap<QString, QString> &map);
        
    protected:        
        QDate ShowDate;
};


class MMTShowTimeItem : public MMTDBItem
{
    public:
        MMTShowTimeItem(UIListGenericTree* parent, const QDate& dt, const QTime& time, 
                        bool bargain, int FilmID = 0, const QString& theaterID = "", 
                        int ShowingID = 0, int TimeID = 0, const QString& ticketURL = "",
                        const QString& officialURL = "");
    
        virtual void toMap(QMap<QString, QString> &map);
        const QString& getTicketURL() const { return TicketURL; }
        
    protected:
        bool Bargain;

        QTime TicketTime;
        QDate ShowDate;
        QString TicketURL;
        
};

class MMTGenreMasterItem : public MMTDBItem
{
    public:
        MMTGenreMasterItem(UIListGenericTree* parent, const QString &text, int FilmID = 0, 
                           const QString& theaterID = "", int ShowingID = 0);
        virtual void populateTree(UIListGenericTree* Tree);
};

class MMTGenreItem : public MMTDBItem
{
    public:
        MMTGenreItem(UIListGenericTree* parent, const QString &text, int FilmID = 0, 
                     const QString& theaterID = "", int ShowingID = 0, const QString& officialURL = "");
        
        virtual void populateTree(UIListGenericTree* tree);
    
    protected:
        QString Genre;
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
        MMTMovieItem(UIListGenericTree* parent, const QString &text, int FilmID = 0, 
                     const QString& theaterID = "", int ShowingID = 0, const QString& officialURL = "");
        
        virtual void populateTree(UIListGenericTree* tree);
                        
    protected:
        void populateTreeWithDates(UIListGenericTree* tree);
        void populateTreeWithTheaters(UIListGenericTree* tree);
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
        MMTTheaterItem(UIListGenericTree* parent, const QString &text, int FilmID = 0, 
                       const QString& theaterID = "", int ShowingID = 0, int timeID = 0,
                       const QString& officialURL = "");
        virtual void populateTree(UIListGenericTree* Tree);
    
    protected:
        void populateTreeWithMovies(UIListGenericTree* Tree);
        void populateTreeWithDates(UIListGenericTree* Tree);
        void populateTreeWithTimes(UIListGenericTree* Tree);
};


#endif
