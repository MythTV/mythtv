#include <qfile.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "rominfo.h"

bool operator==(const RomInfo& a, const RomInfo& b)
{
    if (a.Romname() == b.Romname())
        return true;
    return false;
}

// Return the count of how many times this apperas in the db already
int romInDB(QString rom, QString gametype)
{
    QString thequery;
    MSqlQuery query(MSqlQuery::InitCon());

    int romcount = 0;

    thequery = QString("SELECT count(*) FROM gamemetadata WHERE gametype = \"%1\" AND romname = \"%2\";")
                        .arg(gametype)
                        .arg(rom);
    query.exec(thequery);

    if (query.isActive() && query.size() > 0);
    {
        query.next();
        romcount = query.value(0).toInt();
    };

    return romcount;
}


bool RomInfo::FindImage(QString BaseFileName, QString *result)
{
    QStringList graphic_formats;
    graphic_formats.append("png");
    graphic_formats.append("gif");
    graphic_formats.append("jpg");
    graphic_formats.append("jpeg");
    graphic_formats.append("xpm");
    graphic_formats.append("bmp");
    graphic_formats.append("pnm");
    graphic_formats.append("tif");
    graphic_formats.append("tiff");

    int dotLocation = BaseFileName.findRev('.');
    if(dotLocation == -1)
    {
        BaseFileName.append('.');
        dotLocation = BaseFileName.length();
    }


    BaseFileName.truncate(dotLocation + 1);
    for (QStringList::Iterator i = graphic_formats.begin(); i != graphic_formats.end(); i++)
    {
        *result = BaseFileName + *i;
        cout << "looking for " << *result << endl;
        if(QFile::exists(*result))
            return true;
    }
    return false;
}

void RomInfo::setField(QString field, QString data)
{
    if (field == "system")
        system = data;
    else if (field == "gamename")
        gamename = data;
    else if (field == "genre")
        genre = data;
    else if (field == "year")
        year = data.toInt();
    else if (field == "favorite")
        favorite = data.toInt();
    else if (field == "rompath")
        rompath = data;
    else if (field == "country")
        country = data;
    else if (field == "crc_value")
        crc_value = data;
    else if (field == "diskcount")
        diskcount = data.toInt();
    else if (field == "gametype")
        gametype = data;
    else if (field == "romcount")
        romcount = data.toInt();

}

void RomInfo::setFavorite()
{
    favorite = 1 - favorite;

    QString thequery = QString("UPDATE gamemetadata SET favorite=\"%1\" WHERE "
                               "romname=\"%2\";").arg(favorite).arg(romname);
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);
}

QString RomInfo::getExtension()
{
    int pos = Romname().findRev(".");
    if (pos == -1)
        return NULL;

    pos = Romname().length() - pos;
    pos--;

    QString ext = Romname().right(pos);
    return ext;
}

void RomInfo::fillData()
{
    if (gamename == "")
    {
        return;
    }

    QString thequery = "SELECT system,gamename,genre,year,romname,favorite,"
                       "rompath,country,crc_value,diskcount,gametype FROM gamemetadata WHERE gamename=\"" 
                       + gamename + "\"";

    if (system != "")
        thequery += " AND system=\"" + system + "\"";

    thequery += ";";

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);

    if (query.isActive() && query.size() > 0);
    {
        query.next();

        system = query.value(0).toString();
        gamename = query.value(1).toString();
        genre = query.value(2).toString();
        year = query.value(3).toInt();
        romname = query.value(4).toString();
        favorite = query.value(5).toInt();
        rompath = query.value(6).toString();
        country = query.value(7).toString();
        crc_value = query.value(8).toString();
        diskcount = query.value(9).toInt();
        gametype = query.value(10).toString();
    }

    thequery = "SELECT screenshots FROM gameplayers WHERE playername = \"" + system + "\";";

    query.exec(thequery);

    if (query.isActive() && query.size() > 0);
    {
        query.next();
        QString Image = query.value(0).toString() + "/" + romname;
        if (FindImage(query.value(0).toString() + "/" + romname, &Image))
            setImagePath(Image);
        else
            setImagePath("");
    }

    romcount = romInDB(romname,gametype);

    // If we have more than one instance of this rom in the DB fill in all
    // systems available to play it.
    if (romcount > 1) 
    {
        thequery = "SELECT DISTINCT system FROM gamemetadata WHERE romname = \"" + romname + "\";";

        query.exec(thequery);

        if (query.isActive() && query.size() > 0);
        {
            while(query.next())
            {
                if (allsystems == "")
                    allsystems = query.value(0).toString();
                else
                    allsystems += "," + query.value(0).toString();
            }
        }
    } else 
        allsystems = system;

}


