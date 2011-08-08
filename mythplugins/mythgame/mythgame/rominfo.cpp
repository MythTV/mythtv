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

void RomInfo::SaveToDatabase()
{
    MSqlQuery query(MSqlQuery::InitCon());

    bool inserting = false;

    if (id == 0)
        inserting = true;

    if (inserting)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Adding %1 - %2").arg(Rompath())
            .arg(Romname()));

        query.prepare("INSERT INTO gamemetadata "
                      "(system, romname, gamename, genre, year, gametype, "
                      "rompath, country, crc_value, diskcount, display, plot, "
                      "publisher, version, fanart, boxart, screenshot) "
                      "VALUES (:SYSTEM, :ROMNAME, :GAMENAME, :GENRE, :YEAR, "
                      ":GAMETYPE, :ROMPATH, :COUNTRY, :CRC32, '1', '1', :PLOT, "
                      ":PUBLISHER, :VERSION, :FANART, :BOXART, :SCREENSHOT)");

        query.bindValue(":SYSTEM",System());
        query.bindValue(":ROMNAME",Romname());
        query.bindValue(":GAMENAME",Gamename());
        query.bindValue(":GENRE",Genre());
        query.bindValue(":YEAR",Year());
        query.bindValue(":GAMETYPE",GameType());
        query.bindValue(":ROMPATH",Rompath());
        query.bindValue(":COUNTRY",Country());
        query.bindValue(":CRC32", QString());
        query.bindValue(":PLOT", Plot());
        query.bindValue(":PUBLISHER", Publisher());
        query.bindValue(":VERSION", Version());
        query.bindValue(":FANART", Fanart());
        query.bindValue(":BOXART", Boxart());
        query.bindValue(":SCREENSHOT", Screenshot());
    }
    else
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
    }

    if (!query.exec())
    {
        MythDB::DBError("RomInfo::SaveToDatabase", query);
        return;
    }
}

void RomInfo::DeleteFromDatabase()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Removing %1 - %2").arg(Rompath())
            .arg(Romname()));

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM gamemetadata WHERE "
                  "romname = :ROMNAME AND "
                  "rompath = :ROMPATH ");

    query.bindValue(":ROMNAME",Romname());
    query.bindValue(":ROMPATH",Rompath());

    if (!query.exec())
        MythDB::DBError("purgeGameDB", query);

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
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid field %1").arg(field));

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
                       "version,screenshot,fanart,boxart,inetref,intid FROM gamemetadata "
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
        setId(query.value(18).toInt());
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

QList<RomInfo*> RomInfo::GetAllRomInfo()
{
    QList<RomInfo*> ret;

    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = "SELECT intid,system,romname,gamename,genre,year,publisher,"
                       "favorite,rompath,screenshot,fanart,plot,boxart,"
                       "gametype,diskcount,country,crc_value,inetref,display,"
                       "version FROM gamemetadata ORDER BY diskcount DESC";

    query.prepare(querystr);

    if (!query.exec())
    {
        MythDB::DBError("GetAllRomInfo", query);
        return ret;
    }

    while (query.next())
    {
        RomInfo *add = new RomInfo(
                           query.value(0).toInt(),
                           query.value(2).toString(),
                           query.value(1).toString(),
                           query.value(3).toString(),
                           query.value(4).toString(),
                           query.value(5).toString(),
                           query.value(7).toBool(),
                           query.value(8).toString(),
                           query.value(15).toString(),
                           query.value(16).toString(),
                           query.value(14).toInt(),
                           query.value(13).toString(),
                           0, QString(),
                           query.value(11).toString(),
                           query.value(6).toString(),
                           query.value(19).toString(),
                           query.value(9).toString(),
                           query.value(10).toString(),
                           query.value(12).toString(),
                           query.value(17).toString());
        ret.append(add);
    }

    return ret;
}

RomInfo *RomInfo::GetRomInfoById(int id)
{
    RomInfo *ret = new RomInfo();

    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = "SELECT intid,system,romname,gamename,genre,year,publisher,"
                       "favorite,rompath,screenshot,fanart,plot,boxart,"
                       "gametype,diskcount,country,crc_value,inetref,display,"
                       "version FROM gamemetadata WHERE intid = :INTID";

    query.prepare(querystr);
    query.bindValue(":INTID", id);

    if (!query.exec())
    {
        MythDB::DBError("GetRomInfoById", query);
        return ret;
    }

    if (query.next())
    {
        ret = new RomInfo(
                           query.value(0).toInt(),
                           query.value(2).toString(),
                           query.value(1).toString(),
                           query.value(3).toString(),
                           query.value(4).toString(),
                           query.value(5).toString(),
                           query.value(7).toBool(),
                           query.value(8).toString(),
                           query.value(15).toString(),
                           query.value(16).toString(),
                           query.value(14).toInt(),
                           query.value(13).toString(),
                           0, QString(),
                           query.value(11).toString(),
                           query.value(6).toString(),
                           query.value(19).toString(),
                           query.value(9).toString(),
                           query.value(10).toString(),
                           query.value(12).toString(),
                           query.value(17).toString());
    }

    return ret;
}

QString RomInfo::toString()
{
    return QString ("Rom Info:\n"
               "ID: %1\n"
               "Game Name: %2\n"
               "Rom Name: %3\n"
               "Rom Path: %4")
               .arg(Id()).arg(Gamename())
               .arg(Romname()).arg(Rompath());
}
