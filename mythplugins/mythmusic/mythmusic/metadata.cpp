#include <qsqldatabase.h>
#include <qregexp.h>

#include "metadata.h"

bool operator==(const Metadata& a, const Metadata& b)
{
    if (a.Filename() == b.Filename())
        return true;
    return false;
}

bool operator!=(const Metadata& a, const Metadata& b)
{
    if (a.Filename() != b.Filename())
        return true;
    return false;
}

bool Metadata::isInDatabase(QSqlDatabase *db)
{
    bool retval = false;

    QString sqlfilename = filename;
    sqlfilename.replace(QRegExp("\""), QString("\\\""));

    QString thequery = QString("SELECT artist,album,title,genre,year,tracknum,"
                               "length,intid FROM musicmetadata WHERE "
                               "filename = \"%1\";").arg(sqlfilename);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
  
        artist = query.value(0).toString();
        album = query.value(1).toString();
        title = query.value(2).toString();
        genre = query.value(3).toString();
        year = query.value(4).toInt();
        tracknum = query.value(5).toInt();
        length = query.value(6).toInt();
        id = query.value(7).toUInt();

        retval = true;
    }

    return retval;
}

void Metadata::dumpToDatabase(QSqlDatabase *db)
{
    if (artist == "")
        artist = "Unknown";
    if (album == "")
        album = "Unknown";
    if (title == "")
        title = filename;
    if (genre == "")
        genre = "Unknown";

    title.replace(QRegExp("\""), QString("\\\""));
    artist.replace(QRegExp("\""), QString("\\\""));
    album.replace(QRegExp("\""), QString("\\\""));
    genre.replace(QRegExp("\""), QString("\\\""));

    QString sqlfilename = filename;
    sqlfilename.replace(QRegExp("\""), QString("\\\""));

    QString thequery = QString("INSERT INTO musicmetadata (artist,album,title,"
                               "genre,year,tracknum,length,filename) VALUES "
                               "(\"%1\",\"%2\",\"%3\",\"%4\",%5,%6,%7,\"%8\");")
                              .arg(artist.latin1()).arg(album.latin1())
                              .arg(title.latin1()).arg(genre).arg(year)
                              .arg(tracknum).arg(length).arg(sqlfilename);
    db->exec(thequery);

    // easiest way to ensure we've got 'id' filled.
    fillData(db);
}

void Metadata::setField(QString field, QString data)
{
    if (field == "artist")
        artist = data;
    else if (field == "album")
        album = data;
    else if (field == "title")
        title = data;
    else if (field == "genre")
        genre = data;
    else if (field == "filename")
        filename = data;
    else if (field == "year")
        year = data.toInt();
    else if (field == "tracknum")
        tracknum = data.toInt();
    else if (field == "length")
        length = data.toInt();
}

void Metadata::fillData(QSqlDatabase *db)
{
    if (title == "")
        return;

    QString sqltitle = title;
    sqltitle.replace(QRegExp("\""), QString("\\\""));

    QString thequery = "SELECT artist,album,title,genre,year,tracknum,length,"
                       "filename,intid FROM musicmetadata WHERE title=\"" + 
                       sqltitle + "\"";

    if (album != "")
    {
        QString temp = album;
        temp.replace(QRegExp("\""), QString("\\\""));
        thequery += " AND album=\"" + temp + "\"";
    }
    if (artist != "")
    {
        QString temp = artist;
        temp.replace(QRegExp("\""), QString("\\\""));
        thequery += " AND artist=\"" + temp + "\"";
    }

    thequery += ";";

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        artist = query.value(0).toString();
        album = query.value(1).toString();
        title = query.value(2).toString();
        genre = query.value(3).toString();
        year = query.value(4).toInt();
        tracknum = query.value(5).toInt();
        length = query.value(6).toInt();
        filename = query.value(7).toString();
        id = query.value(8).toUInt();
    }
}

void Metadata::fillDataFromID(QSqlDatabase *db)
{       
    if (id == 0)
        return; 
        
    QString thequery;
    thequery = QString("SELECT title,artist,album,title,genre,year,tracknum,"
                       "length,filename FROM musicmetadata WHERE intid=%1;")
                      .arg(id);
        
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        title = query.value(0).toString();
        artist = query.value(1).toString();
        album = query.value(2).toString();
        title = query.value(3).toString();
        genre = query.value(4).toString();
        year = query.value(5).toInt();
        tracknum = query.value(6).toInt();
        length = query.value(7).toInt();
        filename = query.value(8).toString();
    }
}

