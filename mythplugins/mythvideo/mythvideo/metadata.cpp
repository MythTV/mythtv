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
    else if (field == "childid")
        childID = data.toUInt();
}

void Metadata::fillData(QSqlDatabase *db)
{
    if (title == "")
        return;

    QString thequery = "SELECT title,director,plot,rating,year,userrating,"
                       "length,filename,showlevel,intid,coverfile,inetref,"
                       "childid FROM videometadata WHERE title=\"" + 
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
        childID = query.value(12).toUInt();
    }
}

void Metadata::fillDataFromID(QSqlDatabase *db)
{       
    if (id == 0)
        return; 
        
    QString thequery;
    thequery = QString("SELECT title,director,plot,rating,year,userrating,"
                       "length,filename,showlevel,coverfile,inetref,childid "
                       " FROM videometadata WHERE intid=%1;").arg(id);
        
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
        childID = query.value(11).toUInt();
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

    QString thequery;
    thequery.sprintf("INSERT INTO videometadata (title,director,plot,"
                              "rating,year,userrating,length,filename,showlevel,coverfile,inetref) VALUES "
                              "(\"%s\",\"%s\",\"%s\",\"%s\",%d,%f,%d,\"%s\",%d,\"%s\",\"%s\");",
                              title.latin1(), director.latin1(),
                              plot.latin1(), rating.latin1(), year,
                              userrating, length, sqlfilename.ascii(), showlevel,
                              sqlcoverfile.ascii(), inetref.ascii());

    db->exec(thequery);

    // easiest way to ensure we've got 'id' filled.
    fillData(db);
}

void Metadata::guessTitle()
{
    title = filename.right(filename.length() - filename.findRev("/") - 1);
    title.replace(QRegExp("_"), " ");
    title.replace(QRegExp("%20"), " ");
    title = title.left(title.findRev("."));
    title.replace(QRegExp("\\."), " ");
    title = title.left(title.find("["));
    title = title.left(title.find("("));
}

void Metadata::updateDatabase(QSqlDatabase *db)
{
    if (title == "")
        guessTitle();
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

    QString thequery;
    thequery.sprintf("UPDATE videometadata SET title=\"%s\",director=\"%s\",plot=\"%s\","
                              "rating=\"%s\",year=%d,userrating=%f,length=%d,filename=\"%s\","
                              "showlevel=%d,coverfile=\"%s\",inetref=\"%s\" WHERE intid=%d",
                              title.latin1(), director.latin1(),
                              plot.latin1(), rating.latin1(), year,
                              userrating, length, sqlfilename.ascii(), showlevel,
                              sqlcoverfile.ascii(), inetref.ascii(), id);

    db->exec(thequery);
}

