/* ============================================================
 * File  : ddmovie.h
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Abstract: This file includes the definitions for the various objects 
 *           for parsing movie data from TMSDataDirect::Movies.
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


#ifndef DATADIRECT_H_
#define DATADIRECT_H_

#include <qstringlist.h>
#include <qdom.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qfile.h>

class TMSCrewmember
{
    public:
        TMSCrewmember() { clearValues(); }
        TMSCrewmember(const QDomElement& elem) { 
                clearValues(); 
                parse(elem); 
        }
        
        bool parse(const QDomElement& elem);
        
        void clearValues() {
        }
        QString Role;
        QString Givenname;
        QString Surname;
};


// TMSFilm contains all the data about a specific film.
class TMSFilm
{
    public:
        TMSFilm() { clearValues(); }
        TMSFilm(const QDomElement& elem) { 
                clearValues(); 
                parse(elem); 
        }
        
        bool parse(const QDomElement& elem);
        bool store();
        
        void clearValues() {
            FilmID = 0;
            ReleaseDate = "1/1/1800";
        }
        
        int FilmID; // TMS unique ID.
        QString Title;              // 60 max
        QString Description;        // 2500 max
        QString Language;           // 120 max
        QString Runtime;        
        QString ReleaseDate;
        QString ColorCode;          // 20 max
        QString SoundTrack;  // 255 max
        QString SoundMix;           // 255 max
        QString Genre;              // 255 max?
        QString MPAARating;         // 255 max?
        QString StarRating;       // 5 max
        QString Certification;      // 2500 max
        QStringList Advisories;
        
        QString Audience;           // 255 max
        QString ImageURL;           // 255 max
        int ImageWidth;
        int ImageHeight;
        QString OfficialURL;         // 255 max
        
        typedef  QValueList<TMSCrewmember> CrewList;
        CrewList Crew;
};



// TMSTime represents a point in time for a showing.  Since this also
// includes extra data we can't just wrap this up as a string.
//
// All time values are expressed in 12 hour format and are usually
// assumed to be in the afternoon.  TMS doesn't have data in 24 hour
// format so we'll have to make due.
class TMSTime
{
    public:
        TMSTime() { clearValues(); }
        
        TMSTime(QString _Time, bool _Bargin) { Time = _Time; Bargin = _Bargin; }
        
        void clearValues() {
            Time = "";
            Bargin = false;
        }
        
        QString Time;
        bool Bargin;
};

// A TMSTimes object represents a collection of TMSTime objects.
class TMSTimes {
    public:
        TMSTimes() { clearValues(); }
        TMSTimes(const QDomElement& elem) { 
                clearValues(); 
                parse(elem); 
        }
        
        bool store(int movieRowID);
        
        bool parse(const QDomElement& elem);
        
        void clearValues() {
            Title = "Showings";
            TicketURL = "";
            Messages.clear();
        }
        
        QString Title;
        QString TicketURL;
        QStringList Messages;
        
        typedef  QValueList<TMSTime> TimeList;
        TimeList Times;
};

// A TMSMovie represents a film (TMSFilm) at a theater (TMSTheater)
// on a particular date.
class TMSMovie
{
    public:
        TMSMovie() { clearValues(); }
        TMSMovie(const QString& _Date, const QDomElement& elem) { 
            clearValues(); 
            Date = _Date; 
            parse(elem); 
        }
        
        bool store(const QString& theaterID);
        
        void clearValues() {
            Times.clearValues();
            FilmID = 0;
            MovieTitle = "";
            TicketURL = "";
            Messages.clear();
            Date = "";
        }
        
        bool parse(const QDomElement& elem);
        
        int FilmID;
        TMSTimes Times;
        QString MovieTitle;
        QString TicketURL;
        QStringList Messages;
        QString Date;
};


// TMSTheater contains all the details about a particular theater as well as
// the show times of the various films.
class TMSTheater
{
    public:
        TMSTheater() { clearValues(); }
        TMSTheater(const QDomElement& elem) { clearValues(); parse(elem); }
        
        
        void clearValues() {
            Distance = 0xDEADBEAF; 
            TheaterID = "00000";
            Name = "";
            Chain = "";
            City = "";
            State = "";
            PostalCode = "";
            Phone = "";
        }
        
        bool parse(const QDomElement& elem);
        bool parseDetails(const QDomElement& elem);
        bool parseShowTimes(const QDomElement& elem);
        bool store();
        
        double Distance;
        QString TheaterID;
        QString Name;
        QString Chain;
        
        // Note: The tms:address type contains more fields but these are 
        //       the only ones we're going to care about.
        QString StreetAddress;
        QString City;
        QString State;
        QString PostalCode;
        QString Phone;
        
        typedef  QValueList<TMSMovie> ShowList;
        ShowList Shows;
};


// The root node of a TMSDataDirect::Movies stream.  
class TMSMovieDirect
{
    public:
        TMSMovieDirect() { clearValues(); }
        
        bool importFile(const QString& filename);
        bool importFile(QFile& f);
        bool importData(const QString& user, const QString& pass, 
                        const QString& postalCode, 
                        double radius, int days = 7);
            
        bool store();
        
        bool parseTheaters(const QDomElement& elem);
        bool parseFilms(const QDomElement& elem);
                
        void clearValues() {
            Theaters.clear();
            
            // Default values based on the scheme from TMS.
            PostalCode = "";
            SchemaVersion = "";
            Radius = 5.0;
            WithFilmDetails = true;
            NumDays = -1;       // This is an optional attribute we'll use -1 to
                                // indicate max available.
            
            NumTheaters = -1;   // This is an optional attribute we'll use 
                                // -1 to indicate max available.
        }
        
        
        QString PostalCode;
        QString SchemaVersion;
        double Radius;
        bool WithFilmDetails;
        int NumDays;
        int NumTheaters;
        typedef  QValueList<TMSTheater> TheaterList;
        TheaterList Theaters;
        
        typedef  QValueList<TMSFilm> FilmList;
        FilmList Films;
    
    protected:
        FILE* fetchData(const QString& user, const QString& pass, 
                        const QString& postalCode, double radius, int days = 7);

};

#endif
