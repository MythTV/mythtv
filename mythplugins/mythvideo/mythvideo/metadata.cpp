#include <qsqldatabase.h>
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

    QString thequery = "SELECT artist,album,title,genre,year,tracknum,length,"
                       "filename,intid FROM musicmetadata WHERE title=\"" + 
                       title + "\"";

    if (album != "")
        thequery += " AND album=\"" + album + "\"";
    if (artist != "")
        thequery += " AND artist=\"" + artist + "\"";

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

