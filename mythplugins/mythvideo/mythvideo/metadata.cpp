#include <qsqldatabase.h>
#include <qfile.h>
#include <cmath>
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
    {
        userrating = data.toFloat();
        if (isnan(userrating)) 
            userrating = 0.0;
        if (userrating < -10.0 || userrating >= 10.0)
            userrating = 0.0;
    }
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

bool Metadata::Remove(QSqlDatabase *db)
{
    QFile videofile;
    videofile.setName(filename);
    bool isremoved = videofile.remove();
    if (isremoved)
    {
        QString thequery;
        thequery.sprintf("DELETE FROM videometadatagenre "
                        " WHERE idvideo = %d",id);
        MSqlQuery query(thequery,db);
        if (!query.isActive()){
            MythContext::DBError("delete from videometadatagenre", query);
        }
        thequery.sprintf("DELETE FROM videometadatacountry "
                        " WHERE idvideo = %d",id);
        query.exec(thequery);
        if (!query.isActive()){
            MythContext::DBError("delete from videometadatacountry", query);
        }
        thequery.sprintf("DELETE FROM videometadata "
                        " WHERE intid = %d",id);
        query.exec(thequery);
        if (!query.isActive()){
            MythContext::DBError("delete from videometadata", query);
        }
        thequery = QString("DELETE FROM filemarkup WHERE filename = '%1'")
                           .arg(filename);
        query.exec(thequery);
        if (!query.isActive()){
            MythContext::DBError("delete from filemarkup", query);
        }

    }
    else
    {
            cerr << "impossible de supprimmer le fichier" << endl;
    }
    return isremoved;
}

void Metadata::fillCategory(QSqlDatabase *db)
{
    MSqlQuery query(QString::null, db);
    query.prepare("SELECT videocategory.category"
                  " FROM videometadata INNER JOIN videocategory"
                  " ON videometadata.category = videocategory.intid"
                  " WHERE videometadata.intid = :ID ;");
    query.bindValue(":ID", id);

    if (query.exec() && query.isActive() && query.size()>0)
    {
        query.next();
        category = QString::fromUtf8(query.value(0).toString());
    }
}

void Metadata::fillGenres(QSqlDatabase *db)
{
    MSqlQuery query(QString::null, db);
    query.prepare("SELECT genre"
                  " FROM videometadatagenre INNER JOIN videogenre"
                  " ON videometadatagenre.idgenre = videogenre.intid"
                  " WHERE idvideo= :ID ;");
    query.bindValue(":ID", id);
    genres.clear();
    if (query.exec() && query.isActive() && query.size()>0)
    {
        while(query.next())
        {
            genres.append(QString::fromUtf8(query.value(0).toString()));
        }
    }
}

void Metadata::fillCountries(QSqlDatabase *db)
{
    MSqlQuery query(QString::null, db);
    query.prepare("SELECT country" 
                  " FROM videometadatacountry INNER JOIN videocountry"
                  " ON videometadatacountry.idcountry = videocountry.intid"
                  " WHERE idvideo= :ID ;");
    query.bindValue(":ID", id);
    countries.clear();
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while(query.next())
        {
            countries.append(QString::fromUtf8(query.value(0).toString()));
        }
    }

}

bool Metadata::fillDataFromFilename(QSqlDatabase *db)
{
    if (filename == "")
        return false;

    MSqlQuery query(QString::null, db);
    query.prepare("SELECT intid FROM videometadata WHERE filename = :FILE ;");
    query.bindValue(":FILE", filename.utf8());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        id = query.value(0).toInt();

        return fillDataFromID(db);
    }

    MythContext::DBError("fillfromfilename", query);

    return false;
}

bool Metadata::fillDataFromID(QSqlDatabase *db)
{       
    if (id == 0)
        return false; 
        
    MSqlQuery query(QString::null, db); 
    query.prepare("SELECT title,director,plot,rating,year,userrating,"
                  "length,filename,showlevel,coverfile,inetref,childid,"
                  "browse,playcommand, videocategory.category "
                  " FROM videometadata LEFT JOIN videocategory"
                  " ON videometadata.category = videocategory.intid"
                  "  WHERE videometadata.intid= :ID ;");
    query.bindValue(":ID", id);
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        title = QString::fromUtf8(query.value(0).toString());
        director = QString::fromUtf8(query.value(1).toString());
        plot = QString::fromUtf8(query.value(2).toString());
        rating = query.value(3).toString();
        year = query.value(4).toInt();
        userrating = (float)query.value(5).toDouble();
        if (isnan(userrating)) 
            userrating = 0.0;
        if (userrating < -10.0 || userrating >= 10.0)
            userrating = 0.0;
        length = query.value(6).toInt();
        filename = QString::fromUtf8(query.value(7).toString());
        showlevel = query.value(8).toInt();
        coverfile = QString::fromUtf8(query.value(9).toString());
        inetref = QString::fromUtf8(query.value(10).toString());
        childID = query.value(11).toUInt();
        browse = query.value(12).toBool();
        playcommand = query.value(13).toString();
        category = query.value(14).toString();

    // Genres
        fillGenres(db);
    
        //Countries
        fillCountries(db);

        return true;
    }

    MythContext::DBError("fillfromid", query);
    return false;
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
        coverfile = QObject::tr("No Cover");
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

    if (isnan(userrating))
        userrating = 0.0;
    if (userrating < -10.0 || userrating >= 10.0)
        userrating = 0.0;

    MSqlQuery a_query(QString::null, db);
    a_query.prepare("INSERT INTO videometadata (title,director,plot,"
                    "rating,year,userrating,length,filename,showlevel,"
                    "coverfile,inetref,browse) VALUES (:TITLE, :DIRECTOR, "
                    ":PLOT, :RATING, :YEAR, :USERRATING, :LENGTH, "
                    ":FILENAME, :SHOWLEVEL, :COVERFILE, :INETREF, "
                    ":BROWSE );");
    a_query.bindValue(":TITLE", title.utf8());
    a_query.bindValue(":DIRECTOR", director.utf8());
    a_query.bindValue(":PLOT", plot.utf8());
    a_query.bindValue(":RATING", rating.utf8());
    a_query.bindValue(":YEAR", year);
    a_query.bindValue(":USERRATING", userrating);
    a_query.bindValue(":LENGTH", length);
    a_query.bindValue(":FILENAME", filename.utf8());
    a_query.bindValue(":SHOWLEVEL", showlevel);
    a_query.bindValue(":COVERFILE", coverfile.utf8()); 
    a_query.bindValue(":INETREF", inetref.utf8());
    a_query.bindValue(":BROWSE", browse);
    
    
    if (!a_query.exec() || !a_query.isActive())
    {
        MythContext::DBError("Write video metadata", a_query);
        return;
    }

    // Must make sure we have 'id' filled before we call updateGenres or updateCountries
    a_query.exec("SELECT LAST_INSERT_ID();");

    if (!a_query.isActive() || a_query.size() < 1)
    {
        MythContext::DBError("metadata id get", a_query);
        return;      
    }      

    a_query.next();
    id = a_query.value(0).toUInt();

    if (0 == id)
    {
        cerr << "metadata.o: The id of the last inserted row to videometadata seems to be 0. This is odd." << endl;
        return;
    }

    updateGenres(db);
    updateCountries(db);
}

void Metadata::guessTitle()
{
    title = filename.right(filename.length() - filename.findRev("/") - 1);
    title.replace(QRegExp("_"), " ");
    title.replace(QRegExp("%20"), " ");
    title = title.left(title.findRev("."));
    title.replace(QRegExp("\\."), " ");
    
    eatBraces("[", "]");
    eatBraces("(", ")");
    eatBraces("{", "}");
    
    title = title.stripWhiteSpace();
}

void Metadata::eatBraces(const QString &left_brace, const QString &right_brace)
{
    bool keep_checking = true;
    
    while(keep_checking)
    {
        int left_position = title.find(left_brace);
        int right_position = title.find(right_brace);
        if(left_position == -1 || right_position == -1)
        {
            //
            //  No matching sets of these braces left.
            //

            keep_checking = false;
        }
        else
        {
            if(left_position < right_position)
            {
                //
                //  We have a matching set like:  (  foo  )
                //

                title = title.left(left_position) + 
                        title.right(title.length() - right_position - 1);
            }
            else if (left_position > right_position)
            {
                //
                //  We have a matching set like:  )  foo  (
                //

                title = title.left(right_position) + 
                        title.right(title.length() - left_position - 1);
            }
        }
    }
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
        coverfile = QObject::tr("No Cover");
    if (inetref == "")
        inetref = "00000000";

    int idCategory = getIdCategory(db);

    MSqlQuery query(QString::null, db);
    query.prepare("UPDATE videometadata SET title = :TITLE, "
                  "director = :DIRECTOR, plot = :PLOT, rating= :RATING, "
                  "year = :YEAR, userrating = :USERRATING, length = :LENGTH, "
                  "filename = :FILENAME, showlevel = :SHOWLEVEL, "
                  "coverfile = :COVERFILE, inetref = :INETREF, "
                  "browse = :BROWSE, playcommand = :PLAYCOMMAND, "
                  "childid = :CHILDID, category = :CATEGORY "
                  "WHERE intid = :INTID");
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":DIRECTOR", director.utf8());
    query.bindValue(":PLOT", plot.utf8());
    query.bindValue(":RATING", rating.utf8());
    query.bindValue(":YEAR", year);
    query.bindValue(":USERRATING", userrating);
    query.bindValue(":LENGTH", length);
    query.bindValue(":FILENAME", filename.utf8());
    query.bindValue(":SHOWLEVEL", showlevel);
    query.bindValue(":COVERFILE", coverfile.utf8());
    query.bindValue(":INETREF", inetref.utf8());
    query.bindValue(":BROWSE", browse);
    query.bindValue(":PLAYCOMMAND", playcommand.utf8());
    query.bindValue(":CHILDID", childID);
    query.bindValue(":CATEGORY", idCategory);
    query.bindValue(":INTID", id);

    if(!query.exec() || !query.isActive())
    {
        MythContext::DBError("video metadata update", query);
    }
    updateGenres(db);
    updateCountries(db);
}

int Metadata::getIdCategory(QSqlDatabase *db)
{
    int idcategory = 0;
    if (category != "")
    {
        MSqlQuery query(QString::null, db);
        query.prepare("SELECT intid FROM videocategory"
                      " WHERE category like :CATEGORY ;");
        query.bindValue(":CATEGORY" ,category.utf8());

        if (query.exec() && query.isActive() && query.size()>0)
        {
            query.next();
            idcategory = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO videocategory (category) "
                          "VALUES (:CATEGORY );");
            query.bindValue(":CATEGORY", category.utf8());
            if (query.exec() && query.isActive())
            {
                query.prepare("SELECT intid FROM videocategory"
                              " WHERE category like :CATEGORY ;");
                query.bindValue(":CATEGORY" ,category.utf8());

                if (query.exec() && query.isActive() && query.size()>0)
                {
                    query.next();
                    idcategory = query.value(0).toInt();
                }
                else
                    MythContext::DBError("get category id", query);
            }
        }
    }
    return idcategory;
    
}

void Metadata::setIdCategory(QSqlDatabase *db, int id)
{
    if (id==0)
        category = "";
    else 
    {
        MSqlQuery query(QString::null, db);
        query.prepare("SELECT category FROM videocategory"
                      " WHERE intid = :ID;");
        query.bindValue(":ID", id);

        if (query.exec() && query.isActive() && query.size()>0)
        {
            query.next();
            category = QString::fromUtf8(query.value(0).toString());
        }
    }
}

void Metadata::updateGenres(QSqlDatabase *db)
{
    MSqlQuery query(QString::null, db);
    query.prepare("DELETE FROM videometadatagenre where idvideo = :ID;");
    query.bindValue(":ID", id);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("delete videometadatagenre", query);

    QStringList::Iterator genre;
    for (genre = genres.begin() ; genre != genres.end() ; ++genre)
    {
        // Search id of genre
        query.prepare("SELECT intid FROM videogenre where genre like :GENRE ;");
        query.bindValue(":GENRE", (*genre).utf8());

        int idgenre=0;

        if (query.exec() && query.isActive())
        {
            if (query.size()>0)
            {
                query.next();
                idgenre = query.value(0).toInt();
            }
            else
            {
                // We must add a new genre

                query.prepare("INSERT INTO videogenre (genre) VALUES (:GENRE);");
                query.bindValue(":GENRE", (*genre).utf8());
                if (query.exec() && query.isActive() && query.numRowsAffected() > 0)
                {
                    //search the new idgenre
                    query.prepare("SELECT LAST_INSERT_ID();");
                    if (query.exec() && query.isActive() && query.size() > 0)
                    {
                        query.next();
                        idgenre=query.value(0).toInt();
                    }
                    else
                        MythContext::DBError("insert genre", query);
                }
                else
                    MythContext::DBError("insert genre 0", query);
            }
        }
        else
            MythContext::DBError("search genre", query);
        
        if (idgenre>0)
        {
            // add current genre for this video
            query.prepare("INSERT INTO videometadatagenre (idvideo, idgenre) "
                          "VALUES (:ID,:GENREID);");
            query.bindValue(":ID", id);
            query.bindValue(":GENREID", idgenre);
            if (!query.exec() && !query.isActive())
                MythContext::DBError("metadatagenre insert", query);
        }
    }
}

void Metadata::updateCountries(QSqlDatabase *db)
{
    //remove countries for this video

    MSqlQuery query(QString::null, db);
    query.prepare("DELETE FROM videometadatacountry where idvideo = :ID;");
    query.bindValue(":ID", id);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("delete videometadatacountry", query);

    QStringList::Iterator country;
    for (country = countries.begin() ; country != countries.end() ; ++country)
    {
        // Search id of country
        query.prepare("SELECT intid FROM videocountry WHERE "
                      " country LIKE :COUNTRY ;");
        query.bindValue(":COUNTRY", (*country).utf8());

        int idcountry=0;
        if (query.exec() && query.isActive())
        {
            if (query.size()>0)
            {
                query.next();
                idcountry = query.value(0).toInt();
            }
            else
            {
                //We must add a new countrya

                query.prepare("INSERT INTO videocountry (country) "
                              "VALUES (:COUNTRY);");
                query.bindValue(":COUNTRY", (*country).utf8());
                if (query.exec() && query.isActive() && query.numRowsAffected()> 0)
                {
                    //search the new idgenre
                    query.prepare("SELECT LAST_INSERT_ID();");
                    if (query.exec() && query.isActive() && query.size() > 0)
                    {
                        query.next();
                        idcountry=query.value(0).toInt();
                    }
                    else
                        MythContext::DBError("insert country", query);
                }
                else
                    MythContext::DBError("insert country 0", query);
            }
        }
        else
            MythContext::DBError("search genre", query);

        if (idcountry>0)
        {
            // add current country for this video
            query.prepare("INSERT INTO videometadatacountry (idvideo, idcountry) "
                          "VALUES (:ID,:COUNTRYID);");
            query.bindValue(":ID", id);
            query.bindValue(":COUNTRYID", idcountry);
            if (!query.exec() && !query.isActive())
                MythContext::DBError("metadatacountry insert", query);
        }
    }
}


QImage* Metadata::getCoverImage()
{
    if (!coverImage && (CoverFile() != QObject::tr("No Cover")) && (CoverFile() != QObject::tr("None")))
    {
        coverImage = new QImage();
        if (!coverImage->load(coverfile))
        {
            delete coverImage;
            coverImage = NULL;
        }
    }
        
    return coverImage;
}


QPixmap* Metadata::getCoverPixmap()
{
    if (coverPixmap)
        return coverPixmap;
    
    if (coverfile)
    {
        coverPixmap = new QPixmap();
        coverPixmap->load(coverfile);
    }
    return coverPixmap;
}
