// Qt
#include <QFile>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdb.h>

// MythGame
#include "romedit.h"
#include "rominfo.h"

#define LOC_ERR QString("MythGame:ROMINFO Error: ")
#define LOC QString("MythGame:ROMINFO: ")

bool operator==(const RomInfo& a, const RomInfo& b)
{
    return a.Romname() == b.Romname();
}

void RomInfo::SaveToDatabase() const
{
    MSqlQuery query(MSqlQuery::InitCon());

    bool inserting = false;

    if (m_id == 0)
        inserting = true;

    if (inserting)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Adding %1 - %2")
            .arg(Rompath(), Romname()));

        query.prepare("INSERT INTO gamemetadata "
                      "(`system`, romname, gamename, genre, year, gametype, "
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

void RomInfo::DeleteFromDatabase() const
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Removing %1 - %2")
        .arg(Rompath(), Romname()));

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
int romInDB(const QString& rom, const QString& gametype)
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
    return std::any_of(graphic_formats.cbegin(), graphic_formats.cend(),
                       [BaseFileName,result](const auto & format)
                           { *result = BaseFileName + format;
                             return QFile::exists(*result); } );
}

void RomInfo::setField(const QString& field, const QString& data)
{
    if (field == "system")
        m_system = data;
    else if (field == "gamename")
        m_gamename = data;
    else if (field == "genre")
        m_genre = data;
    else if (field == "year")
        m_year = data;
    else if (field == "favorite")
        m_favorite = (data.toInt() != 0);
    else if (field == "rompath")
        m_rompath = data;
    else if (field == "screenshot")
        m_screenshot = data;
    else if (field == "fanart")
        m_fanart = data;
    else if (field == "boxart")
        m_boxart = data;
    else if (field == "country")
        m_country = data;
    else if (field == "plot")
        m_plot = data;
    else if (field == "publisher")
        m_publisher = data;
    else if (field == "crc_value")
        m_crcValue = data;
    else if (field == "inetref")
        m_inetref = data;
    else if (field == "diskcount")
        m_diskcount = data.toInt();
    else if (field == "gametype")
        m_gametype = data;
    else if (field == "romcount")
        m_romcount = data.toInt();
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid field %1").arg(field));

}

void RomInfo::setFavorite(bool updateDatabase)
{
    m_favorite = !m_favorite;

    if (updateDatabase)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("UPDATE gamemetadata SET favorite = :FAV "
                      "WHERE romname = :ROMNAME");

        query.bindValue(":FAV", m_favorite);
        query.bindValue(":ROMNAME",m_romname);

        if (!query.exec())
        {
            MythDB::DBError("RomInfo::setFavorite", query);
        }
    }
}

QString RomInfo::getExtension() const
{
    int pos = Romname().lastIndexOf(".");
    if (pos == -1)
        return nullptr;

    pos = Romname().length() - pos;
    pos--;

    QString ext = Romname().right(pos);
    return ext;
}

void RomInfo::fillData()
{
    if (m_gamename == "")
    {
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    QString systemtype;
    if (m_system != "") {
        systemtype  += " AND `system` = :SYSTEM ";
    }

    QString thequery = "SELECT `system`,gamename,genre,year,romname,favorite,"
                       "rompath,country,crc_value,diskcount,gametype,plot,publisher,"
                       "version,screenshot,fanart,boxart,inetref,intid FROM gamemetadata "
                       "WHERE gamename = :GAMENAME "
                       + systemtype + " ORDER BY diskcount DESC";

    query.prepare(thequery);
    query.bindValue(":SYSTEM", m_system);
    query.bindValue(":GAMENAME", m_gamename);

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

    setRomCount(romInDB(m_romname,m_gametype));

    // If we have more than one instance of this rom in the DB fill in all
    // systems available to play it.
    if (RomCount() > 1)
    {
        query.prepare("SELECT DISTINCT `system` FROM gamemetadata "
                      "WHERE romname = :ROMNAME");
        query.bindValue(":ROMNAME", Romname());
        if (!query.exec())
            MythDB::DBError("RomInfo::fillData - selecting systems", query);

        while (query.next())
        {
            if (m_allsystems.isEmpty())
                m_allsystems = query.value(0).toString();
            else
                m_allsystems += "," + query.value(0).toString();
        }
    }
    else
    {
        m_allsystems = m_system;
    }
}

QList<RomInfo*> RomInfo::GetAllRomInfo()
{
    QList<RomInfo*> ret;

    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = "SELECT intid,`system`,romname,gamename,genre,year,publisher,"
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
        auto *add = new RomInfo(
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
    RomInfo *ret = nullptr;

    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = "SELECT intid,`system`,romname,gamename,genre,year,publisher,"
                       "favorite,rompath,screenshot,fanart,plot,boxart,"
                       "gametype,diskcount,country,crc_value,inetref,display,"
                       "version FROM gamemetadata WHERE intid = :INTID";

    query.prepare(querystr);
    query.bindValue(":INTID", id);

    if (!query.exec())
        MythDB::DBError("GetRomInfoById", query);

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

    if (!ret)
        ret = new RomInfo();

    return ret;
}

QString RomInfo::toString() const
{
    return QString ("Rom Info:\n"
               "ID: %1\n"
               "Game Name: %2\n"
               "Rom Name: %3\n"
               "Rom Path: %4")
               .arg(QString::number(Id()), Gamename(), Romname(), Rompath());
}
