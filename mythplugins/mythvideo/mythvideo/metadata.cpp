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
    if (field == "title")
        title = data;
    else if (field == "director")
        director = data;
    else if (field == "plot")
        plot = data;
    else if (field == "rating")
        rating = data;
    else if (field == "year")
        year = data.toInt();
    else if (field == "userrating")
        userrating = data.toFloat();
    else if (field == "length")
        length = data.toInt();
    else if (field == "showlevel")
        showlevel = data.toInt();
    else if (field == "coverfile")
        coverfile = data;
    else if (field == "inetref")
        inetref = data;
}

void Metadata::fillData(QSqlDatabase *db)
{
    if (title == "")
        return;

    QString thequery = "SELECT title,director,plot,rating,year,userrating,length,"
                       "filename,showlevel,intid,coverfile,inetref FROM videometadata WHERE title=\"" + 
                       title + "\"";

    if (director != "")
        thequery += " AND director=\"" + director + "\"";
    if (plot != "")
        thequery += " AND plot=\"" + plot + "\"";

    thequery += ";";

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        title = query.value(0).toString();
        director = query.value(1).toString();
        plot = query.value(2).toString();
        rating = query.value(3).toString();
        year = query.value(4).toInt();
        userrating = (float)query.value(5).toDouble();
        length = query.value(6).toInt();
        filename = query.value(7).toString();
        showlevel = query.value(8).toInt();
        id = query.value(9).toUInt();
        coverfile = query.value(10).toString();
        inetref = query.value(11).toString();
    }
}

void Metadata::fillDataFromID(QSqlDatabase *db)
{       
    if (id == 0)
        return; 
        
    QString thequery;
    thequery = QString("SELECT title,director,plot,rating,year,userrating,length,"
                       "filename,showlevel,coverfile,inetref FROM videometadata WHERE intid=%1;")
                      .arg(id);
        
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        title = query.value(0).toString();
        director = query.value(1).toString();
        plot = query.value(2).toString();
        rating = query.value(3).toString();
        year = query.value(4).toInt();
        userrating = (float)query.value(5).toDouble();
        length = query.value(6).toInt();
        filename = query.value(7).toString();
        showlevel = query.value(8).toInt();
        coverfile = query.value(9).toString();
        inetref = query.value(10).toString();
    }
}

void Metadata::dumpToDatabase(QSqlDatabase *db)
{
    if (title == "")
        title = filename;
    if (director == "")
        director = "Unknown";
    if (plot == "")
        plot = "None";
    if (rating == "")
        rating = "Unknown Rating";
    if (coverfile == "")
        coverfile = "None";
    if (inetref == "")
        inetref = "00000000";

    title.replace(QRegExp("\""), QString("\\\""));
    director.replace(QRegExp("\""), QString("\\\""));
    plot.replace(QRegExp("\""), QString("\\\""));
    rating.replace(QRegExp("\""), QString("\\\""));

    QString sqlfilename = filename;
    sqlfilename.replace(QRegExp("\""), QString("\\\""));

    QString sqlcoverfile = coverfile;
    sqlcoverfile.replace(QRegExp("\""), QString("\\\""));

    QString thequery = QString("INSERT INTO videometadata (title,director,plot,"
                               "rating,year,userrating,length,filename,showlevel,coverfile,inetref) VALUES "
                               "(\"%1\",\"%2\",\"%3\",\"%4\",%5,%6,%7,\"%8\",%9,")
                              .arg(title.latin1()).arg(director.latin1())
                              .arg(plot.latin1()).arg(rating.latin1()).arg(year)
                              .arg(userrating).arg(length).arg(sqlfilename).arg(showlevel);
    thequery = thequery + QString("\"%1\",\"%2\");").arg(sqlcoverfile).arg(inetref);

    db->exec(thequery);

    // easiest way to ensure we've got 'id' filled.
    fillData(db);
}

void Metadata::updateDatabase(QSqlDatabase *db)
{
    if (title == "")
        title = filename;
    if (director == "")
        director = "Unknown";
    if (plot == "")
        plot = "None";
    if (rating == "")
        rating = "Unknown Rating";
    if (coverfile == "")
        coverfile = "None";
    if (inetref == "")
        inetref = "00000000";

    title.replace(QRegExp("\""), QString("\\\""));
    director.replace(QRegExp("\""), QString("\\\""));
    plot.replace(QRegExp("\""), QString("\\\""));
    rating.replace(QRegExp("\""), QString("\\\""));

    QString sqlfilename = filename;
    sqlfilename.replace(QRegExp("\""), QString("\\\""));

    QString sqlcoverfile = coverfile;
    sqlcoverfile.replace(QRegExp("\""), QString("\\\""));

    QString thequery = QString("UPDATE videometadata SET title=\"%1\",director=\"%2\",plot=\"%3\","
                               "rating=\"%4\",year=%5,userrating=%6,length=%7,filename=\"%8\","
                               "showlevel=%9,")
                              .arg(title.latin1()).arg(director.latin1())
                              .arg(plot.latin1()).arg(rating.latin1()).arg(year)
                              .arg(userrating).arg(length).arg(sqlfilename).arg(showlevel);
    thequery = thequery + QString("coverfile=\"%1\",inetref=\"%2\" WHERE intid=%3").arg(sqlcoverfile).arg(inetref)
                              .arg(id);

    db->exec(thequery);
}

