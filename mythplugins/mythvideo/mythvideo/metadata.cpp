#include <qsqldatabase.h>
#include <iostream>

using namespace std;

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
    else if (field == "browse")
    {
        bool browse_setting = false;
        bool ok;
        int browse_int_setting = data.toUInt(&ok);
        if(!ok)
        {
            cerr << "metadata.o: Problems setting the browse flag from this data: " << data << endl;
        }
        else
        {
            if(browse_int_setting)
            {
                browse_setting = true;
            }
        }
        browse = browse_setting;
    }
    else if (field == "playcommand")
    {
        playcommand = data;
    }
}

void Metadata::fillData(QSqlDatabase *db)
{
    if (title == "")
        return;

    QString thequery = "SELECT title,director,plot,rating,year,userrating,"
                       "length,filename,showlevel,intid,coverfile,inetref,"
                       "childid, browse, playcommand FROM videometadata WHERE title=\"" + 
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
        browse = query.value(13).toBool();
        playcommand = query.value(14).toString();
    }
}

void Metadata::fillDataFromID(QSqlDatabase *db)
{       
    if (id == 0)
        return; 
        
    QString thequery;
    thequery = QString("SELECT title,director,plot,rating,year,userrating,"
                       "length,filename,showlevel,coverfile,inetref,childid,"
                       "browse,playcommand FROM videometadata WHERE intid=%1;")
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
        childID = query.value(11).toUInt();
        browse = query.value(12).toBool();
        playcommand = query.value(13).toString();
    }
}

void Metadata::dumpToDatabase(QSqlDatabase *db)
{
    if (title == "")
        title = filename;
    if (director == "")
        director = QObject::tr("Unknown");
    if (plot == "")
        plot = QObject::tr("None");
    if (rating == "")
        rating = QObject::tr("Unknown Rating");
    if (coverfile == "")
        coverfile = QObject::tr("None");
    if (inetref == "")
        inetref = "00000000";
    
    if(gContext->GetNumSetting("VideoNewBrowsable", 1))
    {
        browse = true;
    }
    else
    {
        browse = false;
    }

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
                     "rating,year,userrating,length,filename,showlevel,"
                     "coverfile,inetref,browse) VALUES "
                     "(\"%s\",\"%s\",\"%s\",\"%s\",%d,%f,%d,\"%s\",%d,\"%s\","
                     "\"%s\", %d);",
                     title.utf8().data(), director.utf8().data(),
                     plot.utf8().data(), rating.utf8().data(), year,
                     userrating, length, sqlfilename.utf8().data(), showlevel,
                     sqlcoverfile.utf8().data(), inetref.utf8().data(),browse);

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
        director = QObject::tr("Unknown");
    if (plot == "")
        plot = QObject::tr("None");
    if (rating == "")
        rating = QObject::tr("Unknown Rating");
    if (coverfile == "")
        coverfile = QObject::tr("None");
    if (inetref == "")
        inetref = "00000000";

    title.replace(QRegExp("\""), QString("\\\""));
    director.replace(QRegExp("\""), QString("\\\""));
    plot.replace(QRegExp("\""), QString("\\\""));
    rating.replace(QRegExp("\""), QString("\\\""));
    playcommand.replace(QRegExp("\""), QString("\\\""));
    QString sqlfilename = filename;
    sqlfilename.replace(QRegExp("\""), QString("\\\""));

    QString sqlcoverfile = coverfile;
    sqlcoverfile.replace(QRegExp("\""), QString("\\\""));

    QString thequery;
    thequery.sprintf("UPDATE videometadata SET title=\"%s\",director=\"%s\","
                     "plot=\"%s\",rating=\"%s\",year=%d,userrating=%f,"
                     "length=%d,filename=\"%s\",showlevel=%d,coverfile=\"%s\","
                     "inetref=\"%s\",browse=%d,playcommand=\"%s\",childid=%d"
                     " WHERE intid=%d",
                     title.utf8().data(), director.utf8().data(),
                     plot.utf8().data(), rating.utf8().data(), year,
                     userrating, length, sqlfilename.utf8().data(), showlevel,
                     sqlcoverfile.utf8().data(), inetref.utf8().data(), browse,
                     playcommand.utf8().data(), childID, id);

    QSqlQuery a_query(thequery, db);
    if(!a_query.isActive())
    {
        
        cerr << "metadata.o: The following metadata update failed: " << thequery << endl;
    }
}

