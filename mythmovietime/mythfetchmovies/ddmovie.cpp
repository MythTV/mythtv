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


#include "mythtv/mythcontext.h"
#include <mythtv/mythdbcon.h>
#include "ddmovie.h"
 

FILE* TMSMovieDirect::fetchData(const QString& user, const QString& pass, 
                                const QString& postalCode, double radius, int days)
{
    FILE* ret = NULL;
    
    QString ddMoviesURL = gContext->GetSetting("movietime-ddurl",
                                               "http://webservices.stage.tms.tribune.com/moviedata/xmd");
    QString ddCustID = gContext->GetSetting("movietime-ddcustomer", "TNV01");
                    
    QString url = QString("%1?customer=%2\\&postalCode=%3\\&radius=%4\\&numberOfDays=%5")
                          .arg(ddMoviesURL)
                          .arg(ddCustID)
                          .arg(postalCode)
                          .arg(radius)
                          .arg(days);
    

    QString command = QString("wget --http-user='%1' --http-passwd='%2' "
                              "--header='Accept-Encoding:gzip'"
                              " %3 --output-document=- | gzip -df")
                             .arg(user)
                             .arg(pass)
                             .arg(url);



    
    
    ret = popen(command.ascii(), "r");
    return ret;
}



bool TMSMovieDirect::store()
{
    MSqlQuery query(MSqlQuery::InitCon());    
    query.exec( "TRUNCATE TABLE movietime_theaters");
    query.exec( "TRUNCATE TABLE movietime_timesmessages");
    query.exec( "TRUNCATE TABLE movietime_showtimes");
    query.exec( "TRUNCATE TABLE movietime_movies");
    query.exec( "TRUNCATE TABLE movietime_moviemessages");
    query.exec( "TRUNCATE TABLE movietime_films");
    query.exec( "TRUNCATE TABLE movietime_crew");
    query.exec( "TRUNCATE TABLE movietime_advisories");
    
    // Store the films
    for ( FilmList::iterator it = Films.begin(); it != Films.end(); ++it )
    {
        if( !(*it).store() )
            return false;
    }
    
    // Store the theaters
    for ( TheaterList::iterator it = Theaters.begin(); it != Theaters.end(); ++it )
    {
        if( !(*it).store() )
            return false;
    }

    return true;
}

bool TMSMovieDirect::importFile(const QString& filename)
{
    QFile f(filename);
    
    
    if (!filename || !f.open(IO_ReadOnly))
    {
        // This shouldn't ever happen but let's be anal.
        VERBOSE(VB_IMPORTANT, QString("TMSMovieDirect::importFile: Can't open %1.").arg(filename));
        return false;
    }
    
    return importFile(f);
}


bool TMSMovieDirect::importData(const QString& user, const QString& pass, const QString& postalCode, double radius, int days)
{
    FILE* fp = fetchData(user, pass, postalCode, radius, days);
    if (!fp)
        return false;
   
    QFile f;

    if (!f.open(IO_ReadOnly, fp)) 
    {
       VERBOSE(VB_GENERAL,"Error opening DataDirect file");
       return false;
    }
    
    return importFile(f);
}

bool TMSMovieDirect::importFile(QFile& f)
{   
    QDomDocument doc;     
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
    
    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, QString("TMSMovieDirect::importFile: Error, parsing %1\n"
                                       "at line: %2  column: %3 msg: %4").
                                       arg(f.name()).arg(errorLine).arg(errorColumn).arg(errorMsg));
        f.close();
        return false;        
    }
    
    // Ok now that the sanity checks are over with let's do something with this beast.
    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
        
    SchemaVersion = docElem.attribute( "schemaVersion" );
    PostalCode = docElem.attribute( "postalCode" );
    Radius = docElem.attribute( "radius", "5.0" ).toDouble();
    
    NumDays = docElem.attribute( "numberOfDays", "1" ).toInt();
    NumTheaters = docElem.attribute( "numberOfTheaters", "0" ).toInt();
    
    //cerr << "schemaVersion: " << SchemaVersion << endl;
    //cerr << "postalCode: " << PostalCode << endl;
    
    
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        
        
        
        if ( e.tagName() == "theatres" )
        {
            parseTheaters( e );
        }
        else if ( e.tagName() == "films" )
        {
            parseFilms( e );
        }
        else
        {
            VERBOSE( VB_IMPORTANT, QString("TMSMovieDirect::importFile "
                                           "unrecognized element %1. Ignoring it.").
                                           arg(e.tagName()) );
        }
            
        n = n.nextSibling();    
    }
    
    return true;
}

bool TMSMovieDirect::parseTheaters(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        
        if (e.tagName() == "theatre" )
        {
            Theaters.append(TMSTheater(e));
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("TMSMovieDirect::parseTheaters "
                                          "unrecognized element %1. Ignoring it.").
                                          arg(e.tagName()) );
        }        
        
        n = n.nextSibling();    
    }
    
    return true;
}

bool TMSMovieDirect::parseFilms(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        
        if (e.tagName() == "film" )
        {
            Films.append(TMSFilm(e));
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("TMSMovieDirect::parseFilms "
                                          "unrecognized element %1. Ignoring it.").
                                          arg(e.tagName()) );
        }        
        
        n = n.nextSibling();    
    }
    
    return true;
}

bool TMSTheater::store()
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("INSERT INTO movietime_theaters (theaterid, distance, name, "
                  "chain, street, city, state, postalcode, phone) "
                  "VALUES (:THEATERID, :DIST, :NAME, :CHAIN, "
                  ":STREET, :CITY, :STATE, :ZIP, :PHONE)");
    
                  
    query.bindValue(":THEATERID",   TheaterID);
    query.bindValue(":DIST",        Distance);
    query.bindValue(":NAME",        Name);
    query.bindValue(":CHAIN",       Chain);
    query.bindValue(":STREET",      StreetAddress);
    query.bindValue(":CITY",        City);
    query.bindValue(":STATE",       State);
    query.bindValue(":ZIP",         PostalCode);
    query.bindValue(":PHONE",       Phone);
    
    if (!query.exec())
    {
        VERBOSE(VB_IMPORTANT, "TMSTheater::store failed to store theaters" );
        VERBOSE(VB_IMPORTANT, query.executedQuery());
        VERBOSE(VB_IMPORTANT, query.lastError().text());
        return false;
    }

    // Store the showtimes
    for ( ShowList::iterator it = Shows.begin(); it != Shows.end(); ++it )
    {
        if( !(*it).store(TheaterID) )
            return false;
    }

    
    return true;
}

bool TMSTheater::parse(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        
        if (e.tagName() == "theatreDetails" )
        {
            parseDetails(e);
        }
        else if( e.tagName() == "distance" )
        {
            Distance = e.text().toDouble();
        }
        else if( e.tagName() == "showTimes" )
        {
            parseShowTimes(e);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("TMSTheater::parseTheaters "
                                          "unrecognized element %1. Ignoring it.").
                                          arg(e.tagName()) );
        }

        n = n.nextSibling();    
    }
    
    return true;
}

bool TMSTheater::parseDetails(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        
        if (e.tagName() == "theatreId" )
        {
            TheaterID = e.text();
        }
        else if( e.tagName() == "name" )
        {
            Name = e.text();
        }
        else if( e.tagName() == "chain" )
        {
            Chain = e.text();
        }
        else if( e.tagName() == "address" )
        {
            QDomNode addrNode = e.firstChild();
            while (!addrNode.isNull())
            {
                QDomElement addrElem = addrNode.toElement();
                
                if (addrElem.tagName() == "streetAddress")
                {
                    StreetAddress = addrElem.text();
                }
                else if (addrElem.tagName() == "city")
                {
                    City = addrElem.text();
                }
                else if (addrElem.tagName() == "state")
                {
                    State = addrElem.text();
                }
                else if (addrElem.tagName() == "postalCode")
                {
                    PostalCode = addrElem.text();
                }
                else if (addrElem.tagName() == "telephone")
                {
                    Phone = addrElem.text();
                }
                // Note: Don't bitch about unknown elements because we don't handle them all
                
                addrNode = addrNode.nextSibling();                    
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("TMSTheater::parseDetails "
                                          "unrecognized element %1. Ignoring it.").
                                          arg(e.tagName()) );
        }
        
        
        n = n.nextSibling();    
    }
    
    return true;
}

bool TMSTheater::parseShowTimes(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    QString Date;
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if ( e.tagName() == "shows" )
        {
            Date = e.attribute("day");
            QDomNode showNode = e.firstChild();
            while( !showNode.isNull() )
            {
                QDomElement showElement = showNode.toElement();
                if (showElement.tagName() == "movie" )
                {
                    Shows.append( TMSMovie( Date, showElement ) );
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("TMSTheater::parseShowTimes "
                                                  "unrecognized element %1. Ignoring it.").
                                                  arg(e.tagName()) );
                }
                
                showNode = showNode.nextSibling();    
            }
            
            
        }
        n = n.nextSibling();    
    }
    
    return true;
}

bool TMSMovie::store(const QString& theaterID)
{
    MSqlQuery query(MSqlQuery::InitCon());
    int rowID = 0;
    
    query.prepare("INSERT INTO movietime_movies (filmid, movietitle, ticketurl, "
                  "date, theaterid, timestitle, timesticketurl) "
                  "VALUES (:FILMID, :MOVIET, :TICKET, :DATE, :THEATER, "
                  ":TTITLE, :TURL )");
    
    query.bindValue(":FILMID",   FilmID);                  
    query.bindValue(":MOVIET",   MovieTitle);
    query.bindValue(":TICKET",   TicketURL);
    query.bindValue(":THEATER",  theaterID);
    query.bindValue(":DATE",     Date);
    query.bindValue(":TTILE",    Times.Title);
    query.bindValue(":TURL",     Times.TicketURL);
    
    
    if (!query.exec())
    {
        VERBOSE(VB_IMPORTANT, "TMSMovie::store failed to store movies." );
        VERBOSE(VB_IMPORTANT, query.executedQuery());
        VERBOSE(VB_IMPORTANT, query.lastError().text());
        return false;
    }

    // Grab the rowid of the newly added movie since we're going to need it a lot.
    query.prepare("SELECT rowid FROM movietime_movies WHERE "
                  "filmid = :FILMID AND date = :DATE AND "
                  "theaterid = :THEATER;");
    
    query.bindValue(":FILMID",   FilmID);                  
    query.bindValue(":THEATER",  theaterID);
    query.bindValue(":DATE",     Date);
    
    if (query.exec() && query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        rowID = query.value(0).toInt();
    }
    
    if (rowID == 0)
    {
        VERBOSE(VB_IMPORTANT, "TMSMovie::store failed to retrieve row ID." );
        return false;
    }
    
    // Save off the messages collection
    query.prepare("INSERT INTO movietime_moviemessages (movie_rowid, message) "
                  "VALUES (:MOVIE, :MESSAGE)");
                  
    for ( QStringList::iterator it = Messages.begin(); it != Messages.end(); ++it )
    {
        query.bindValue(":MOVIE", rowID);
        query.bindValue(":MESSAGE", *it);
        if (!query.exec())
        {
            VERBOSE(VB_IMPORTANT, "TMSMovie::store failed to store message for movie." );
            VERBOSE(VB_IMPORTANT, query.executedQuery());
            VERBOSE(VB_IMPORTANT, query.lastError().text());
            return false;
        }   
    }
    
    // Store off the times collection
    Times.store(rowID);
    
    return true;
}


bool TMSMovie::parse(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    
    FilmID = elem.attribute("filmId").toInt();
    
    while (!n.isNull())
    {    
        QDomElement e = n.toElement();
        if (e.tagName() == "ticketUri") 
        {
            TicketURL = e.text();
        }
        else if (e.tagName() == "messages" )
        {
            QDomNode msgNode = e.firstChild();

            while( !msgNode.isNull() )
            {
                QDomElement msgElement = msgNode.toElement();
                if (msgElement.tagName() == "message" )
                {   
                    Messages.append( msgElement.text() );
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("TMSMovie::parse "
                                                  "unrecognized element %1 "
                                                  "while parsing messages. Ignoring it.").
                                                  arg(msgElement.tagName()) );
                }
                
                msgNode = msgNode.nextSibling();    
            }
        }
        else if (e.tagName() == "times" )
        {
            Times.parse(e);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("TMSMovie::parse "
                                          "unrecognized element %1. Ignoring it.").
                                          arg(e.tagName()) );
        }
        
        n = n.nextSibling();    
    }

    return true;
}

bool TMSTimes::store(int rowID)
{

    MSqlQuery query(MSqlQuery::InitCon());
    
//     query.prepare("INSERT INTO movietime_times (rowid, title, ticketurl) "
//                   "VALUES (:ROW, :TITLE, :URL)");
//     
//                   
//     query.bindValue(":ROW",     rowID);
//     query.bindValue(":TITLE",   Title);
//     query.bindValue(":URL",     TicketURL);
//     
//     if (!query.exec())
//     {
//         VERBOSE(VB_IMPORTANT, "TMSTimes::store failed to store." );
//         VERBOSE(VB_IMPORTANT, query.executedQuery());
//         VERBOSE(VB_IMPORTANT, query.lastError().text());
//         return false;
//     }   
//     
    
    // Save off the messages collection
    query.prepare("INSERT INTO movietime_moviemessages (timesrowid, message) "
                  "VALUES (:ROW, :MESSAGE)");
                  
    for ( QStringList::iterator it = Messages.begin(); it != Messages.end(); ++it )
    {
        query.bindValue(":ROW", rowID);
        query.bindValue(":MESSAGE", *it);
        if (!query.exec())
        {
            VERBOSE(VB_IMPORTANT, "TMSTimes::store failed to store message for time collection." );
            VERBOSE(VB_IMPORTANT, query.executedQuery());
            VERBOSE(VB_IMPORTANT, query.lastError().text());
            return false;
        }   
    }
    
    // Save off the show times collectin
    query.prepare("INSERT INTO movietime_showtimes (timesrowid, Time, Bargin) "
                  "VALUES (:ROW, :TIME, :BARG)");
                  
    for ( TimeList::iterator it = Times.begin(); it != Times.end(); ++it )
    {
        query.bindValue(":ROW", rowID);
        query.bindValue(":TIME", (*it).Time);
        query.bindValue(":BARG", (*it).Bargin);
        
        
        if (!query.exec())
        {
            VERBOSE(VB_IMPORTANT, "TMSTimes::store failed to store time for time collection." );
            VERBOSE(VB_IMPORTANT, query.executedQuery());
            VERBOSE(VB_IMPORTANT, query.lastError().text());
            return false;
        }   
    }
    
    return true;
}

bool TMSTimes::parse(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    bool bargin = false;
    
    
    while (!n.isNull())
    {    
        QDomElement e = n.toElement();
        if (e.tagName() == "ticketUri") 
        {
            TicketURL = e.text();
        }
        else if (e.tagName() == "timesTitle" )
        {
            Title = e.text();
        }
        else if (e.tagName() == "messages" )
        {
            Messages = e.text();
        }
        else if (e.tagName() == "time" )
        {
            bargin = (e.attribute( "bargainPrice", "false" ) == "true");
            Times.append( TMSTime(e.text(), bargin) );


        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("TMSTimes::parse "
                                          "unrecognized element %1. Ignoring it.").
                                          arg(e.tagName()) );
        }
        
        n = n.nextSibling();    
    }

    return true;
}

bool TMSFilm::store()
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("INSERT INTO movietime_films (filmid, title, description, "
                  "runtime, releasedate, colorcode, soundtrack, soundmix, "
                  "genre, mpaarating, starrating, certification, audience, "
                  "imageurl, imagewidth, imageheight, officialurl, language)"
                  " VALUES (:FILMID, :TITLE, :DESCR, :RUNTIME, :RELDT, :COLOR,"
                  " :SOUNDT, :SOUNDM, :GENRE, :MPAA, :STAR, :CERT, :AUD, "
                  " :IMGURL, :IMGW, :IMGH, :URL, :LANG)");
    
                  
    query.bindValue(":FILMID",  FilmID);
    query.bindValue(":TITLE",   Title);
    query.bindValue(":DESCR",   Description);
    query.bindValue(":RUNTIME", Runtime);
    query.bindValue(":RELDT",   ReleaseDate);
    query.bindValue(":COLOR",   ColorCode);
    query.bindValue(":SOUNDT",  SoundTrack);
    query.bindValue(":SOUNDM",  SoundMix);
    query.bindValue(":GENRE",   Genre);
    query.bindValue(":MPAA",    MPAARating);
    query.bindValue(":STAR",    StarRating);
    query.bindValue(":CERT",    Certification);
    query.bindValue(":AUD",     Audience);
    query.bindValue(":IMGURL",  ImageURL);
    query.bindValue(":IMGW",    ImageWidth);
    query.bindValue(":IMGH",    ImageHeight);
    query.bindValue(":URL",     OfficialURL);
    query.bindValue(":LANG",    Language);     

    
    if (!query.exec())
    {
        VERBOSE(VB_IMPORTANT, "TMSFilm::store failed to store films." );
        VERBOSE(VB_IMPORTANT, query.executedQuery());
        VERBOSE(VB_IMPORTANT, query.lastError().text());
        return false;
    }
    else
    {
        
        query.prepare("INSERT INTO movietime_crew (filmid, role, name)"
                      " VALUES (:FILMID, :ROLE, :NAME)");
        
        for (CrewList::iterator it = Crew.begin(); it != Crew.end(); ++it)
        {
            if ( ((*it).Role != "Unavailable") && ((*it).Givenname != "Unavailable"))
            {
                query.bindValue(":FILMID",  FilmID);
                query.bindValue(":ROLE",    (*it).Role);
                query.bindValue(":NAME",   QString("%1 %2").arg((*it).Givenname).arg((*it).Surname));     
                if (!query.exec())
                {
                    VERBOSE(VB_IMPORTANT, "TMSFilm::store failed to store cast and crew." );
                    VERBOSE(VB_IMPORTANT, query.executedQuery());
                    VERBOSE(VB_IMPORTANT, query.lastError().text());
                    return false;
                }
            }
        }
       
        
        query.prepare("INSERT INTO movietime_advisories (filmid, advisory)"
                      " VALUES (:FILMID, :ADV)");
        for (QStringList::iterator it = Advisories.begin(); it != Advisories.end(); ++it)
        {
            query.bindValue(":FILMID",  FilmID);
            query.bindValue(":ADV",    *it);
            if (!query.exec())
            {
                VERBOSE(VB_IMPORTANT, "TMSFilm::store failed to store advisories." );
                VERBOSE(VB_IMPORTANT, query.executedQuery());
                VERBOSE(VB_IMPORTANT, query.lastError().text());
                return false;
            }
        }
        
        return true;
    }
    
}

bool TMSFilm::parse(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    
    while (!n.isNull())
    {    
        QDomElement e = n.toElement();
        if (e.tagName() == "filmId") 
        {
            FilmID = e.text().toInt();
        }
        else if (e.tagName() == "title" )
        {
            Title = e.text();
        }
        else if (e.tagName() == "description" )
        {
            Description = e.text();
        }
        else if (e.tagName() == "language" )
        {
            Language = e.text();
        }
        else if (e.tagName() == "runningTime" )
        {
            Runtime = e.text();
        }
        else if (e.tagName() == "colorCode" )
        {
            ColorCode = e.text();
        }
        else if (e.tagName() == "releaseDate" )
        {
            ReleaseDate = e.text();
        }
        else if (e.tagName() == "soundTrack" )
        {
            SoundTrack = e.text();
        }
        else if (e.tagName() == "soundMix" )
        {
            SoundMix = e.text();
        }
        else if (e.tagName() == "genre" )
        {
            Genre = e.text();
        }
        else if (e.tagName() == "ratings" )
        {
            QDomNode rateNode = e.firstChild();
            while( !rateNode.isNull() )
            { 
                QDomElement rateElement = rateNode.toElement();
                if (rateElement.tagName() == "mpaaRating" )
                {
                    MPAARating = rateElement.text();
                }
                else if (rateElement.tagName() == "starRating" )
                {
                    StarRating = rateElement.text();
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("TMSFilm::parse "
                                                  "unrecognized element %1 "
                                                  "while parsing 'ratings' node. "
                                                  "Ignoring it.").
                                                  arg(e.tagName()) );
                }
                
                rateNode = rateNode.nextSibling();    
            }
            
        }
        else if (e.tagName() == "certification" )
        {
            Certification = e.text();
        }
        else if (e.tagName() == "advisories" )
        {
            QDomNode advNode = e.firstChild();
            while( !advNode.isNull() )
            { 
                QDomElement advElement = advNode.toElement();
                if (advElement.tagName() == "advisory" )
                {
                    Advisories.append(advElement.text());
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("TMSFilm::parse "
                                                  "unrecognized element %1 "
                                                  "while parsing 'advisories' node. "
                                                  "Ignoring it.").
                                                  arg(advElement.tagName()) );
                }
                
                advNode = advNode.nextSibling();    
            }
            
        }
        else if (e.tagName() == "audience" )
        {
            Audience = e.text();
        }
        else if (e.tagName() == "image" )
        {
            QDomNode imgNode = e.firstChild();
            while( !imgNode.isNull() )
            { 
                QDomElement imgElement = imgNode.toElement();
                if (imgElement.tagName() == "imageUri" )
                {
                    ImageURL = imgElement.text();
                }
                else if (imgElement.tagName() == "width" )
                {
                    ImageWidth = imgElement.text().toInt();
                }
                else if (imgElement.tagName() == "height" )
                {
                    ImageHeight = imgElement.text().toInt();
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("TMSFilm::parse "
                                                  "unrecognized element %1 "
                                                  "while parsing 'image' node. "
                                                  "Ignoring it.").
                                                  arg(imgElement.tagName()) );
                }
                
                imgNode = imgNode.nextSibling();    
            }

        }
        else if (e.tagName() == "productionCrew" )
        {
            QDomNode crewNode = e.firstChild();
            while( !crewNode.isNull() )
            { 
                QDomElement crewElement = crewNode.toElement();
                if (crewElement.tagName() == "member" )
                {
                    Crew.append(TMSCrewmember(crewElement));
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("TMSFilm::parse "
                                                  "unrecognized element %1 "
                                                  "while parsing 'productionCrew' node. "
                                                  "Ignoring it.").
                                                  arg(crewElement.tagName()) );
                }
                
                crewNode = crewNode.nextSibling();    
            }
            
        }
        else if (e.tagName() == "officialUri" )
        {
            OfficialURL = e.text();
        }
        
        else
        {
            VERBOSE(VB_IMPORTANT, QString("TMSFilm::parse "
                                          "unrecognized element %1. Ignoring it.").
                                          arg(e.tagName()) );
        }
        
        n = n.nextSibling();    
    }

    return true;
}

bool TMSCrewmember::parse(const QDomElement& elem)
{
    QDomNode n = elem.firstChild();
    while (!n.isNull())
    {    
        QDomElement e = n.toElement();
        if (e.tagName() == "role") 
        {
            Role = e.text();
        }
        else if (e.tagName() == "givenname" )
        {
            Givenname = e.text();
        }
        else if (e.tagName() == "surname" )
        {
            Surname = e.text();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("TMSMovie::parse "
                                          "unrecognized element %1. Ignoring it.").
                                          arg(e.tagName()) );
        }
        
        n = n.nextSibling();    
    }

    return true;
}


// EOF
