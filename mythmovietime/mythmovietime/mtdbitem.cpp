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

#include <qsqldatabase.h>
#include <qsqlquery.h>

#include <mythtv/mythdbcon.h>
#include <mythtv/mythcontext.h>

#include "mtdbitem.h"

QString DateFormat;
QString TimeFormat;

const QString& getTimeFormat()
{
    if( TimeFormat.isEmpty() )
    {
        TimeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    }
    
    return TimeFormat;
}

const QString& getDateFormat()
{
    if( DateFormat.isEmpty() )
    {
        DateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    }
    
    return DateFormat;
}

static bool pixmapsSet = false;
static QPixmap *pixBargain = NULL;

static void setupPixmaps(void)
{
    pixmapsSet = true;
    
    pixBargain = gContext->LoadScalePixmap("bargain_ticket.png");
}

static QPixmap *getPixmap(const QString &level)
{
    if (!pixmapsSet)
        setupPixmaps();

    if (level == "bargain")
        return pixBargain;
    else
        return NULL;

}



 
MMTDBItem::MMTDBItem(UIListGenericTree *parent, const QString &text, 
                     int filmID, const QString& theaterID, 
                     int showingID, int timeID, const QString& officialURL)
         : UIListGenericTree(parent, text, "MMTDBItem")
{
    FilmID = filmID;
    TheaterID = theaterID;
    ShowingID = showingID;
    TimeID = timeID;
    OfficialURL = officialURL;
    
}


void MMTDBItem::toMap(QMap<QString, QString> &map)
{
    bool haveData = false;
    MSqlQuery query(MSqlQuery::InitCon());
    
    // Grab the theater info
    if (!TheaterID.isEmpty() && (TheaterID != "ALL"))
    {
        query.prepare("SELECT Distance, Name, Chain, city, State, "
                      "street, Postalcode, phone "
                      "FROM movietime_theaters "
                      "WHERE theaterID = :THEATERID" );
        
        query.bindValue(":THEATERID", TheaterID);
        
        if (query.exec() && query.isActive() && query.size() > 0)
        {
            haveData = true;
            query.next();
            map["theater_ID"]         = TheaterID;
            map["theater_distance"]   = query.value(0).toString();
            map["theater_name"]       = query.value(1).toString();
            map["theater_chain"]      = query.value(2).toString();
            map["theater_city"]       = query.value(3).toString();
            map["theater_state"]      = query.value(4).toString();
            map["theater_street"]     = query.value(5).toString();
            map["theater_postalcode"] = query.value(6).toString();
            map["theater_phone"]      = query.value(7).toString();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "MMTDBItem::toMap failed to fetch theater info" );
            VERBOSE(VB_IMPORTANT, query.executedQuery());
            VERBOSE(VB_IMPORTANT, query.lastError().text());
        }

    }
    
    if( haveData == false)
    {
        map["theater_id"]         = "";
        map["theater_distance"]   = "";
        map["theater_name"]       = "";
        map["theater_chain"]      = "";
        map["theater_city"]       = "";
        map["theater_state"]      = "";
        map["theater_street"]     = "";
        map["theater_postalcode"] = "";
        map["theater_phone"]      = "";
    }
    
    // Grab the movie info
    haveData = false;
    
    if (FilmID > 0 )
    {
        query.prepare("SELECT Title, Description, Runtime, ReleaseDate, colorCode, "
                      "soundtrack, Soundmix, genre, MPAARating, StarRating,  "
                      "Certification, Audience, ImageURL, OfficialURL, Language "
                      "FROM movietime_films "
                      "WHERE FilmID = :FILMID" );
        
        query.bindValue(":FILMID", FilmID);
        
        if (query.exec() && query.isActive() && query.size() > 0)
        {
            haveData = true;
            query.next();
            map["film_id"]              = FilmID;
            map["film_title"]           = query.value(0).toString();
            map["film_description"]     = query.value(1).toString();
            map["film_runtime"]         = query.value(2).toString();
            map["film_releasedate"]     = query.value(3).toString();
            map["film_colorcode"]       = query.value(4).toString();
            map["film_soundtrack"]      = query.value(5).toString();
            map["film_soundmix"]        = query.value(6).toString();
            map["film_genre"]           = query.value(7).toString();
            map["film_mpaarating"]      = query.value(8).toString();
            if (!query.value(8).toString().isEmpty() )
            {
                map["film_disp_mpaarating"] = QString("%1 %2" )
                                                      .arg(QObject::tr("Rated"))
                                                      .arg(query.value(8).toString());
            }                                     
            map["film_starrating"]      = query.value(9).toString();
            if(!map["film_starrating"].isEmpty())
            {
                map["film_longtitle"] = map["film_title"] + " (" + map["film_starrating"] + ")";
            }
            else
            {
                map["film_longtitle"] = map["film_title"];
            }
            
            map["film_certification"]   = query.value(10).toString();
            map["film_audience"]        = query.value(11).toString();
            map["film_imageurl"]        = query.value(12).toString();
            map["film_officialurl"]     = query.value(13).toString();
            map["film_language"]        = query.value(14).toString();
        }
    }
    
    if (haveData == false)
    {
        map["film_id"]              = "";
        map["film_title"]           = "";
        map["film_description"]     = "";
        map["film_runtime"]         = "";
        map["film_releasedate"]     = "";
        map["film_colorcode"]       = "";
        map["film_soundtrack"]      = "";
        map["film_soundmix"]        = "";
        map["film_genre"]           = "";
        map["film_mpaarating"]      = "";
        map["film_disp_mpaarating"] = "";
        map["film_starrating"]      = "";
        map["film_certification"]   = "";
        map["film_audience"]        = "";
        map["film_imageurl"]        = "";
        map["film_officialurl"]     = "";
        map["film_language"]        = "";
    }    

    // Clear out fields implemented by derrived classes
    map["ticket_time"] = "";
    map["ticket_date"] = "";
    map["ticket_date_time"] = "";
}


MMTGenreItem::MMTGenreItem(UIListGenericTree* parent, const QString &text,
                           int filmID, const QString& theaterID, int showingID, const QString& officialURL)
            : MMTDBItem(parent, text, filmID, theaterID, showingID, 0, officialURL)
{
    Genre = text;
}


void MMTGenreItem::populateTree(UIListGenericTree*)
{
    // We're going to create a bunch of children.  One for each movie.
    MMTMovieItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    // Logic would dictate that we just grab all the movies listed in the
    // movietime_films table however that table can contain films that are
    // not actually being shown. So we're forced to do an inner join.
    query.prepare( "SELECT DISTINCT movie.FilmID, film.title, film.officialurl "
                   "FROM movietime_movies movie "
                   "INNER JOIN movietime_films film ON film.filmID = movie.filmID "
                   "WHERE film.Genre = :GENRE "
                   "ORDER BY title" );
    
    query.bindValue(":GENRE", Genre);    
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTMovieItem(this, query.value(1).toString(), 
                                        query.value(0).toInt(), TheaterID, ShowingID,
                                        query.value(2).toString());
         
        }
    }
    
}



MMTGenreMasterItem::MMTGenreMasterItem(UIListGenericTree* parent, const QString &text,
                                       int filmID, const QString& theaterID, int showingID)
                  : MMTDBItem(parent, text, filmID, theaterID, showingID)
{
    populateTree(parent);    
}

 
void MMTGenreMasterItem::populateTree(UIListGenericTree*)
{
    // We're going to create a bunch of children.  One for each movie.
    MMTGenreItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    // Logic would dictate that we just grab all the movies listed in the
    // movietime_films table however that table can contain films that are
    // not actually being shown. So we're forced to do an inner join.
    query.prepare( "SELECT DISTINCT film.genre "
                   "FROM movietime_movies movie "
                   "INNER JOIN movietime_films film ON film.filmID = movie.filmID "
                   "ORDER BY genre" );
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTGenreItem(this, query.value(0).toString(), FilmID,
                                        TheaterID, ShowingID);
        }
    }
    
}



MMTMovieMasterItem::MMTMovieMasterItem(UIListGenericTree* parent, const QString &text)
                  : MMTDBItem(parent, text)
{
    populateTree(parent);    
}




void MMTMovieMasterItem::populateTree(UIListGenericTree*)
{
    // We're going to create a bunch of children.  One for each movie.
    MMTMovieItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    // Logic would dictate that we just grab all the movies listed in the
    // movietime_films table however that table can contain films that are
    // not actually being shown. So we're forced to do an inner join.
    query.prepare( "SELECT DISTINCT movie.FilmID, film.title, film.officialurl "
                   "FROM movietime_movies movie "
                   "INNER JOIN movietime_films film ON film.filmID = movie.filmID "
                   "ORDER BY title" );
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTMovieItem(this, query.value(1).toString(), 
                                        query.value(0).toInt(), TheaterID, ShowingID,
                                        query.value(2).toString());
         
        }
    }
    
}


MMTShowingItem::MMTShowingItem(UIListGenericTree* parent, const QDate& dt, 
                               int filmID, const QString& theaterID, int showingID, 
                               int timeID, const QString& officialURL)
              : MMTDBItem(parent, dt.toString(getDateFormat()),
                          filmID, theaterID, showingID, timeID, officialURL)
{
    ShowDate = dt;
}

               
void MMTShowingItem::populateTree(UIListGenericTree*)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString tempURL;
    
    if (TheaterID == "ALL" )
    {
        MMTTheaterItem* tempItem;
        
        
        query.prepare( "SELECT m.rowid, m.theaterid, t.name "
                       "FROM movietime_movies m "
                       "INNER JOIN movietime_theaters t on t.theaterid = m.theaterid "
                       "WHERE m.filmid = :FILMID " 
                       "AND m.date = :DATE");
                   
        query.bindValue(":FILMID", FilmID);
        query.bindValue(":DATE", ShowDate);
        
        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next()) 
            {
               
                tempItem = new MMTTheaterItem(this, query.value(2).toString(), 
                                              FilmID, query.value(1).toString(), 
                                              query.value(0).toInt(), 0, OfficialURL);
         
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, query.executedQuery());
        }
    }        
    else
    {
       query.prepare( "SELECT st.rowid, st.Time, st.Bargin, "
                      "       mt.TicketURL, mt.TimesTicketURL "
                      "FROM movietime_showtimes st "
                      "INNER JOIN movietime_movies mt ON mt.rowid = st.TimesRowId "
                      "WHERE st.TimesRowID = :ROWID " 
                      "ORDER BY st.Time");
                   
        query.bindValue(":ROWID", ShowingID);
        
        MMTShowTimeItem* tempItem;
        
        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next()) 
            {
//                cerr << query.value(3).toString() << endl;

                if ( query.value(3).toString().isEmpty() )
                {
                    tempURL = query.value(2).toString();
                }
                else
                {
                    tempURL = query.value(3).toString();
                }
                
                tempItem = new MMTShowTimeItem(this, ShowDate, query.value(1).toTime(), 
                                               query.value(2).toBool(), FilmID, 
                                               TheaterID, ShowingID, query.value(0).toInt(),
                                               tempURL, OfficialURL );
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "MMTShowingItem::populateTree failed to fetch times" );
            VERBOSE(VB_IMPORTANT, query.executedQuery());
            VERBOSE(VB_IMPORTANT, query.lastError().text());

        }
    }
    
}


MMTShowTimeItem::MMTShowTimeItem(UIListGenericTree* parent, const QDate& dt, const QTime& tm, 
                                 bool bargain, int filmID, const QString& theaterID, int showingID, 
                                 int timeID, const QString& ticketURL, const QString& officialURL)
               : MMTDBItem(parent, tm.toString(getTimeFormat()), 
                           filmID, theaterID, showingID, timeID)
{
    ShowDate = dt;
    TicketTime = tm;
    Bargain = bargain;
    TicketURL = ticketURL;
    OfficialURL = officialURL;
    if ( Bargain )
    {
        setPixmap(getPixmap("bargain"));
    }
    
    
}

void MMTShowingItem::toMap(QMap<QString, QString> &map)
{
    MMTDBItem::toMap(map);
    map["ticket_time"] = "";
    map["ticket_date"] = ShowDate.toString(getDateFormat());
    map["ticket_date_time"] = map["ticket_date"];
}

void MMTShowTimeItem::toMap(QMap<QString, QString> &map)
{
    MMTDBItem::toMap(map);
    
    if (Bargain)
    {
        map["ticket_type"] = QObject::tr("Bargain");
    }
    else
    {
        map["ticket_type"] = QObject::tr("Normal");
    }
    
    map["ticket_time"] = TicketTime.toString(getTimeFormat());
    map["ticket_date"] = ShowDate.toString(getDateFormat());
    map["ticket_date_time"] = QString("%1, %2").arg(map["ticket_date"])
                                               .arg(map["ticket_time"]);
}


MMTMovieItem::MMTMovieItem(UIListGenericTree* parent, const QString &text, 
                           int filmID, const QString& theaterID, int showingID, const QString& officialURL)
            : MMTDBItem(parent, text, filmID, theaterID, showingID, 0, officialURL)
{
}


void MMTMovieItem::populateTree(UIListGenericTree* tree) 
{
    if (!TheaterID.isEmpty())
        populateTreeWithDates(tree);
    else
        populateTreeWithTheaters(tree);
}


void MMTMovieItem::populateTreeWithDates(UIListGenericTree*)
{

    MMTShowingItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    if (TheaterID == "ALL" )
    {
        query.prepare( "SELECT DISTINCT 0 as rowid, Date "
                       "FROM movietime_movies "
                       "WHERE filmid = :FILMID" );
                   
        query.bindValue(":FILMID", FilmID);
    }        
    else
    {
       query.prepare( "SELECT DISTINCT rowid, Date "
                       "FROM movietime_movies "
                       "WHERE filmid = :FILMID AND "
                       "theaterid = :THEATER" );
                   
        query.bindValue(":FILMID", FilmID);
        query.bindValue(":THEATER", TheaterID);
    }
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTShowingItem(this, query.value(1).toDate(), 
                                          FilmID, TheaterID, query.value(0).toInt(),
                                          0, OfficialURL);
         
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MMTMovieItem::populateTreeWithDates failed to fetch dates" );
        VERBOSE(VB_IMPORTANT, query.executedQuery());
        VERBOSE(VB_IMPORTANT, query.lastError().text());
    }
    
}

void MMTMovieItem::populateTreeWithTheaters(UIListGenericTree*)
{
    // We're going to create a bunch of children.  
    // One for each theater that's showing this movie.
    MMTTheaterItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare( "SELECT DISTINCT theater.name, film.TheaterID, film.FilmID " 
                   "FROM movietime_movies film "
                   "INNER JOIN movietime_theaters theater on theater.theaterid = film.theaterid "
                   "WHERE film.filmid = :FILMID" );
    
    query.bindValue(":FILMID", FilmID);
    

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTTheaterItem(this, query.value(0).toString(), 
                                          FilmID, query.value(1).toString(), ShowingID );
         
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
                  : MMTDBItem(parent, text)
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
                                          0, query.value(0).toString());
         
        }
    }
    
}

MMTTheaterItem::MMTTheaterItem(UIListGenericTree* parent, const QString &text, 
                               int filmID, const QString& theaterID, int showingID, 
                               int timeID, const QString& officialURL)
              : MMTDBItem(parent, text, filmID, theaterID, showingID, timeID, officialURL)
{
}

void MMTTheaterItem::populateTree(UIListGenericTree* tree)
{
    if ( ShowingID > 0)
    {
       populateTreeWithTimes( tree );
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

void MMTTheaterItem::populateTreeWithTimes(UIListGenericTree*)
{
    MMTShowTimeItem* tempItem;
    MSqlQuery query(MSqlQuery::InitCon());
    QString tempURL;
    
    query.prepare( "SELECT st.rowid, st.Time, st.Bargin, mt.date, "
                  "       mt.TicketURL, mt.TimesTicketURL "
                  "FROM movietime_showtimes st "
                  "INNER JOIN movietime_movies mt ON mt.rowid = st.TimesRowId "
                  "WHERE st.TimesRowID = :ROWID " 
                  "ORDER BY st.Time");
               
    query.bindValue(":ROWID", ShowingID);
    
    
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            if ( query.value(5).toString().isEmpty() )
            {
                tempURL = query.value(4).toString();
            }
            else
            {
                tempURL = query.value(5).toString();
            }
            
            tempItem = new MMTShowTimeItem(this, query.value(3).toDate(), query.value(1).toTime(), 
                                           query.value(2).toBool(), FilmID, 
                                           TheaterID, ShowingID, query.value(0).toInt(),
                                           tempURL, OfficialURL );
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MMTTheaterItem::populateTreeWithTimes failed to fetch times" );
        VERBOSE(VB_IMPORTANT, query.executedQuery());
        VERBOSE(VB_IMPORTANT, query.lastError().text());
    }
}

void MMTTheaterItem::populateTreeWithMovies(UIListGenericTree*)
{
    // We're going to create a bunch of children.  One for each movie.
    MMTMovieItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT DISTINCT movie.FilmID, film.Title, film.officialurl "
                  "FROM movietime_movies movie "
                  "INNER JOIN movietime_films film ON film.FilmID = movie.FilmID "
                  "WHERE movie.TheaterID = :THEATERID");
    
    query.bindValue(":THEATERID", TheaterID);                  
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
            tempItem = new MMTMovieItem(this, query.value(1).toString(), 
                                        query.value(0).toInt(), TheaterID, ShowingID, 
                                        query.value(2).toString());
         
        }
    }
}

void MMTTheaterItem::populateTreeWithDates(UIListGenericTree*)
{
    MMTShowingItem* tempItem;
    
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare( "SELECT rowid, Date"
                   "FROM movietime_movies "
                   "WHERE filmid = :FILMID AND "
                   "TheaterID = :THEATERID" );

    query.bindValue(":FILMID", FilmID);
    query.bindValue(":THEATERID", TheaterID);

    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next()) 
        {
        
            tempItem = new MMTShowingItem(this, query.value(1).toDate(), 
                                          FilmID, TheaterID, query.value(0).toInt());
         
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
