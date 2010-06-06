#include <QFile>

#include <mythdb.h>
#include <mythcontext.h>

#include "rominfo.h"
#include "romedit.h"

#define LOC_ERR QString("MythGame:ROMINFO Error: ")
#define LOC QString("MythGame:ROMINFO: ")

bool operator==(const RomInfo& a, const RomInfo& b)
{
    if (a.Romname() == b.Romname())
        return true;
    return false;
}

void RomInfo::UpdateDatabase()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT gamename,genre,year,country,plot,publisher, "
                  "favorite,screenshot,fanart,boxart,inetref "
                  "FROM gamemetadata "
                  "WHERE gametype = :GAMETYPE "
                  "AND romname = :ROMNAME");

    query.bindValue(":GAMETYPE", GameType());
    query.bindValue(":ROMNAME", Romname());

    if (!query.exec())
    {
        MythDB::DBError("RomInfo::UpdateDatabase", query);
        return;
    }

    if (!query.next())
        return;

    QString t_gamename  = query.value(0).toString();
    QString t_genre     = query.value(1).toString();
    QString t_year      = query.value(2).toString();
    QString t_country   = query.value(3).toString();
    QString t_plot      = query.value(4).toString();
    QString t_publisher = query.value(5).toString();
    bool    t_favourite = query.value(6).toBool();
    QString t_screenshot = query.value(7).toString();
    QString t_fanart = query.value(8).toString();
    QString t_boxart = query.value(9).toString();
    QString t_inetref = query.value(10).toString();

    if ((t_gamename  != Gamename())  || (t_genre     != Genre())   ||
        (t_year      != Year())      || (t_country   != Country()) ||
        (t_plot      != Plot())      || (t_publisher != Publisher()) ||
        (t_favourite != Favorite())  || (t_screenshot != Screenshot()) ||
        (t_fanart    != Fanart())    || (t_boxart    != Boxart()) ||
        (t_inetref   != Inetref()))
    {
        query.prepare("UPDATE gamemetadata "
                      "SET version = 'CUSTOM', "
                      "    gamename = :GAMENAME,"
                      "    genre = :GENRE,"
                      "    year = :YEAR,"
                      "    country = :COUNTRY,"
                      "    plot = :PLOT,"
                      "    publisher = :PUBLISHER,"
                      "    favorite = :FAVORITE,"
                      "    screenshot = :SCREENSHOT,"
                      "    fanart = :FANART,"
                      "    boxart = :BOXART, "
                      "    inetref = :INETREF "
                      "WHERE gametype = :GAMETYPE AND "
                      "      romname  = :ROMNAME");
        query.bindValue(":GAMENAME", Gamename());
        query.bindValue(":GENRE", Genre());
        query.bindValue(":YEAR", Year());
        query.bindValue(":COUNTRY", Country());
        query.bindValue(":PLOT", Plot());
        query.bindValue(":PUBLISHER", Publisher());
        query.bindValue(":FAVORITE", Favorite());
        query.bindValue(":SCREENSHOT", Screenshot());
        query.bindValue(":FANART", Fanart());
        query.bindValue(":BOXART", Boxart());
        query.bindValue(":INETREF", Inetref());
        query.bindValue(":GAMETYPE", GameType());
        query.bindValue(":ROMNAME", Romname());

        if (!query.exec())
        {
            MythDB::DBError("RomInfo::UpdateDatabase", query);
            return;
        }
    }
}

// Return the count of how many times this appears in the db already
int romInDB(QString rom, QString gametype)
{
    MSqlQuery query(MSqlQuery::InitCon());

    int count = 0;

    query.prepare("SELECT count(*) FROM gamemetadata "
                  "WHERE gametype = :GAMETYPE "
                  "AND romname = :ROMNAME");

    query.bindValue(":GAMETYPE", gametype);
    query.bindValue(":ROMNAME", rom);

    if (!query.exec())
    {
        MythDB::DBError("romInDB", query);
        return -1;
    }


    if (query.next())
        count = query.value(0).toInt();

    return count;
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

    int dotLocation = BaseFileName.lastIndexOf('.');
    if (dotLocation == -1)
    {
        BaseFileName.append('.');
        dotLocation = BaseFileName.length();
    }


    BaseFileName.truncate(dotLocation + 1);
    for (QStringList::Iterator i = graphic_formats.begin(); i != graphic_formats.end(); i++)
    {
        *result = BaseFileName + *i;
        if (QFile::exists(*result))
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
        year = data;
    else if (field == "favorite")
        favorite = data.toInt();
    else if (field == "rompath")
        rompath = data;
    else if (field == "screenshot")
        screenshot = data;
    else if (field == "fanart")
        fanart = data;
    else if (field == "boxart")
        boxart = data;
    else if (field == "country")
        country = data;
    else if (field == "plot")
        plot = data;
    else if (field == "publisher")
        publisher = data;
    else if (field == "crc_value")
        crc_value = data;
    else if (field == "inetref")
        inetref = data;
    else if (field == "diskcount")
        diskcount = data.toInt();
    else if (field == "gametype")
        gametype = data;
    else if (field == "romcount")
        romcount = data.toInt();
    else
        VERBOSE(VB_GENERAL, LOC_ERR + QString("Invalid field %1").arg(field));

}

void RomInfo::setFavorite(bool updateDatabase)
{
    favorite = 1 - favorite;

    if (updateDatabase)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("UPDATE gamemetadata SET favorite = :FAV "
                      "WHERE romname = :ROMNAME");

        query.bindValue(":FAV", favorite);
        query.bindValue(":ROMNAME",romname);

        if (!query.exec())
        {
            MythDB::DBError("RomInfo::setFavorite", query);
        }
    }
}

QString RomInfo::getExtension()
{
    int pos = Romname().lastIndexOf(".");
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

    MSqlQuery query(MSqlQuery::InitCon());

    QString systemtype;
    if (system != "") {
        systemtype  += " AND system = :SYSTEM ";
    }

    QString thequery = "SELECT system,gamename,genre,year,romname,favorite,"
                       "rompath,country,crc_value,diskcount,gametype,plot,publisher,"
                       "version,screenshot,fanart,boxart,inetref FROM gamemetadata "
                       "WHERE gamename = :GAMENAME "
                       + systemtype + " ORDER BY diskcount DESC";

    query.prepare(thequery);
    query.bindValue(":SYSTEM", system);
    query.bindValue(":GAMENAME", gamename);

    if (query.exec() && query.next())
    {
        setSystem(query.value(0).toString());
        setGamename(query.value(1).toString());
        setGenre(query.value(2).toString());
        setYear(query.value(3).toString());
        setRomname(query.value(4).toString());
        setField("favorite",query.value(5).toString());
        setRompath(query.value(6).toString());
        setCountry(query.value(7).toString());
        setCRC_VALUE(query.value(8).toString());
        setDiskCount(query.value(9).toInt());
        setGameType(query.value(10).toString());
        setPlot(query.value(11).toString());
        setPublisher(query.value(12).toString());
        setVersion(query.value(13).toString());
        setScreenshot(query.value(14).toString());
        setFanart(query.value(15).toString());
        setBoxart(query.value(16).toString());
        setInetref(query.value(17).toString());
    }

    setRomCount(romInDB(romname,gametype));

    // If we have more than one instance of this rom in the DB fill in all
    // systems available to play it.
    if (RomCount() > 1)
    {
        query.prepare("SELECT DISTINCT system FROM gamemetadata "
                      "WHERE romname = :ROMNAME");
        query.bindValue(":ROMNAME", Romname());
        if (!query.exec())
            MythDB::DBError("RomInfo::fillData - selecting systems", query);

        while (query.next())
        {
            if (allsystems.isEmpty())
                allsystems = query.value(0).toString();
            else
                allsystems += "," + query.value(0).toString();
        }
    }
    else
    {
        allsystems = system;
    }
}

