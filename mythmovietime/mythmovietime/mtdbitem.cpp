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

#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include "mtdbitem.h"

 
 
MMTDBItem::MMTDBItem(UIListGenericTree *parent, const QString &text, 
                     TreeKeys key, int id)
         : UIListGenericTree(parent, text, "MMTDBItem")
{
    Key = key;
    ID = id;
}


MMTMovieMasterItem::MMTMovieMasterItem(UIListGenericTree* parent, const QString &text)
                  : MMTDBItem(parent, text, KEY_MOVIE_MASTER, 0 )
{
    populateTree(parent);    
}

void MMTMovieMasterItem::populateTree(UIListGenericTree*)
{
    // We're going to create a bunch of children.  One for each movie.
    MMTMovieItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare( "SELECT FilmID, Title FROM movietime_films ORDER BY Title" );
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTMovieItem(this, query.value(1).toString(), 
                                        query.value(0).toInt(), false);
         
        }
    }
    
}


MMTMovieItem::MMTMovieItem(UIListGenericTree* parent, const QString &text, 
                           int filmID, const QString& theaterID)
                  : MMTDBItem(parent, text, KEY_MOVIE, filmID )
{
    TheaterID = theaterID;
}

void MMTMovieItem::populateTreeWithDates(UIListGenericTree* tree)
{

    MMTDBItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    if (TheaterID == "ALL" )
    {
        query.prepare( "QUERY SELECT DISTINCT 0 as rowid, Date "
                       "FROM movietime_movies "
                       "WHERE filmid = :FILMID" );
                   
        query.bindValue(":FILMID", ID);
        query.bindValue(":THEATER", TheaterID);
    }        
    else
    {
       query.prepare( "QUERY SELECT DISTINCT rowid, Date "
                       "FROM movietime_movies "
                       "WHERE filmid = :FILMID AND "
                       "theaterid = :THEATER" );
                   
        query.bindValue(":FILMID", ID);
        query.bindValue(":THEATER", TheaterID);
    }
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTDBItem(this, query.value(1).toString(), 
                                     KEY_DATE, query.value(0).toInt());
         
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MMTMovieItem::populateTreeWithDates failed to fetch theaters" );
        VERBOSE(VB_IMPORTANT, query.executedQuery());
        VERBOSE(VB_IMPORTANT, query.lastError().text());
    }
    
}

void MMTMovieItem::populateTreeWithTheaters(UIListGenericTree* tree)
{
    // We're going to create a bunch of children.  
    // One for each theater that's showing this movie.
    MMTTheaterItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare( "SELECT DISTINCT theater.name, film.TheaterID, film.FilmID " 
                   "FROM movietime_movies film "
                   "INNER JOIN movietime_theaters theater on theater.theaterid = film.theaterid "
                   "WHERE film.filmid = :FILMID" );
    
    query.bindValue(":FILMID", ID);
    

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTTheaterItem(this, query.value(0).toString(), 
                                          query.value(1).toString(), ID );
         
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MMTMovieItem::populateTreeWithTheaters failed to fetch theaters" );
        VERBOSE(VB_IMPORTANT, query.executedQuery());
        VERBOSE(VB_IMPORTANT, query.lastError().text());
    }

}



MMTTheaterMasterItem::MMTTheaterMasterItem(UIListGenericTree* parent, const QString &text)
                  : MMTDBItem(parent, text, KEY_THEATER_MASTER, 0 )
{
    populateTree(parent);    
}

void MMTTheaterMasterItem::populateTree(UIListGenericTree*)
{
    // We're going to create a bunch of children.  One for each movie.
    MMTDBItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare( "SELECT TheaterID, Name FROM movietime_theaters ORDER BY Name" );
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTTheaterItem(this, query.value(1).toString(), 
                                          query.value(0).toString());
         
        }
    }
    
}

MMTTheaterItem::MMTTheaterItem(UIListGenericTree* parent, const QString &text, 
                               const QString& theaterID, int filmID, int showingID)
              : MMTDBItem(parent, text, KEY_THEATER, -1)
{
    TheaterID = theaterID;
    FilmID = filmID;
    ShowingID = showingID;
}

void MMTTheaterItem::populateTree(UIListGenericTree* tree)
{
    if ( ShowingID > 0)
    {
       
    }
    else if (FilmID > 0)
    {
        
        populateTreeWithDates( tree );
    }
    else
    {
        populateTreeWithMovies( tree );
    }
}

void MMTTheaterItem::populateTreeWithMovies(UIListGenericTree*)
{
    
}

void MMTTheaterItem::populateTreeWithDates(UIListGenericTree*)
{
    MMTDBItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare( "SELECT rowid,  Date "
                   "FROM movietime_movies "
                   "WHERE filmid = :FILMID AND "
                   "TheaterID = :THEATERID" );

    query.bindValue(":FILMID", FilmID);
    query.bindValue(":THEATERID", TheaterID);
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTDBItem(this, query.value(1).toString(), 
                                     KEY_DATE, query.value(0).toInt());
         
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MMTTheaterItem::populateTreeWithDates failed to fetch dates" );
        VERBOSE(VB_IMPORTANT, query.executedQuery());
        VERBOSE(VB_IMPORTANT, query.lastError().text());
    }
                   
                                      
}

// EOF 
