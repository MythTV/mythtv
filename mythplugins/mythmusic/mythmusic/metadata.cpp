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

bool Metadata::isInDatabase(QSqlDatabase *db)
{
    bool retval = false;

    char thequery[512];
    sprintf(thequery, "SELECT artist,album,title,genre,year,tracknum,length "
                      "FROM musicmetadata WHERE filename = \"%s\";",
                      filename.ascii());

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

    char thequery[4096];
    sprintf(thequery, "INSERT INTO musicmetadata (artist,album,title,genre,"
                      "year,tracknum,length,filename) VALUES (\"%s\",\"%s\","
                      "\"%s\",\"%s\",%d,%d,%d,\"%s\");", artist.latin1(),
                      album.latin1(), title.latin1(), genre.ascii(),
                      year, tracknum, length, filename.ascii());

    db->exec(thequery);
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
                       "filename FROM musicmetadata WHERE title=\"" + title +
                       "\"";

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
    }
}
