/* ============================================================
 * File  : dbcheck.cpp
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Copyright 2005 by J. Donavan Stanley

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
#include <qstring.h>
#include <qdir.h>

#include <iostream>
using namespace std;

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

 
const QString currentDatabaseVersion = "1001";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.exec("DELETE FROM settings WHERE value='MovieTimeDBSchemaVer';");
    query.exec(QString("INSERT INTO settings (value, data, hostname) "
                   "VALUES ('MovieTimeDBSchemaVer', %1, NULL);")
                       .arg(newnumber));
}



static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    VERBOSE(VB_ALL, QString("Upgrading to MythMusic schema version ") + 
            version);

    MSqlQuery query(MSqlQuery::InitCon());

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        query.exec(thequery);
        counter++;
        thequery = updates[counter];
    }

    UpdateDBVersionNumber(version);
    dbver = version;
}

void UpgradeMovieTimeDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("MovieTimeDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        VERBOSE(VB_ALL, "Inserting MythMovieTime initial database information.");

        const QString updates[] = {
            "CREATE TABLE movietime_advisories ("
            "rowid int(11) NOT NULL auto_increment,"
            "filmid int(11) NOT NULL default '0',"
            "advisory varchar(255) NOT NULL default '',"
            "PRIMARY KEY  (rowid),"
            "KEY filmid (filmid));",
            
            "CREATE TABLE movietime_crew ("
            "RowID int(11) NOT NULL auto_increment,"
            "FilmID int(11) NOT NULL default '0',"
            "Role varchar(30) NOT NULL default '',"
            "Name varchar(62) NOT NULL default '',"
            "PRIMARY KEY  (RowID),"
            "KEY FilmID (FilmID));",
                
            "CREATE TABLE movietime_films ("
            "FilmID int(11) NOT NULL default '0',"
            "Title varchar(60) NOT NULL default '',"
            "Description longtext,"
            "Runtime varchar(20) default '',"
            "ReleaseDate date default '0000-00-00',"
            "ColorCode varchar(20) default '',"
            "SoundTrack varchar(255) default '',"
            "SoundMix varchar(255) default '',"
            "Genre varchar(255) default '',"
            "MPAARating varchar(255) default '',"
            "StarRating varchar(10) default '',"
            "Certification longtext,"
            "Audience varchar(255) default '',"
            "ImageURL varchar(255) default '',"
            "ImageWidth int(11) default '0',"
            "ImageHeight int(11) default '0',"
            "OfficialURL varchar(255) default '',"
            "Language varchar(120) default '',"
            "PRIMARY KEY  (FilmID));",
              
            "CREATE TABLE movietime_moviemessages ("
            "rowid int(11) NOT NULL auto_increment,"
            "movie_rowid int(11) NOT NULL default '0',"
            "message varchar(255) NOT NULL default '',"
            "PRIMARY KEY  (rowid),"
            "KEY idxMovieRow (movie_rowid));",
            
            "CREATE TABLE movietime_movies ("
            "RowID int(11) NOT NULL auto_increment,"
            "FilmID int(11) NOT NULL default '0',"
            "MovieTitle varchar(60) default '',"
            "TicketURL varchar(255) default '',"
            "TimesTicketURL varchar(255) default '',"
            "TimesTitle varchar(255) default '',"
            "Date date NOT NULL default '0000-00-00',"
            "TheaterID varchar(5) NOT NULL default '',"
            "PRIMARY KEY  (RowID),"
            "KEY idxFilmID (FilmID),"
            "KEY idxTheaterID (TheaterID));",
              
            "CREATE TABLE movietime_showtimes ("
            "RowID int(11) NOT NULL auto_increment,"
            "TimesRowID int(11) NOT NULL default '0',"
            "Time varchar(100) NOT NULL default '',"
            "Bargin tinyint(1) NOT NULL default '0',"
            "PRIMARY KEY  (RowID));",
            
            "CREATE TABLE movietime_theaters ("
            "TheaterID varchar(5) NOT NULL default '',"
            "Distance double NOT NULL default '0',"
            "Name varchar(255) NOT NULL default '',"
            "Chain varchar(255) NOT NULL default '',"
            "City varchar(255) NOT NULL default '',"
            "State char(2) NOT NULL default '',"
            "Street varchar(255) NOT NULL default '',"
            "PostalCode varchar(10) NOT NULL default '',"
            "Phone varchar(255) NOT NULL default '',"
            "PRIMARY KEY  (TheaterID),"
            "KEY idxTheaterName (Name));",
            
            "CREATE TABLE movietime_timesmessages ("
            "RowID varchar(100) NOT NULL default '',"
            "TimesRowID int(11) NOT NULL default '0',"
            "Message varchar(100) NOT NULL default '',"
            "PRIMARY KEY  (RowID),"
            "KEY idxTimesRow (TimesRowID));",
            
            ""
        };
        
        performActualUpdate(updates, "1000", dbver);
    }
    
    if (dbver == "1000")
    {
        //VERBOSE(VB_ALL, "Inserting MythMovieTime database schema to 1001.");        
        const QString updates[] = {
            "ALTER TABLE movietime_showtimes CHANGE Time Time TIME NOT NULL;",
            ""
        };
        
        performActualUpdate(updates, "1001", dbver);
    }
}
