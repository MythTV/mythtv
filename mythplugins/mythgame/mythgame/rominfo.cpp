#include <qsqldatabase.h>
#include "rominfo.h"

bool operator==(const RomInfo& a, const RomInfo& b)
{
    if (a.Romname() == b.Romname())
        return true;
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
}

void RomInfo::setFavorite(QSqlDatabase *db)
{
    favorite = 1 - favorite;

    QString thequery = QString("UPDATE gamemetadata SET favorite=\"%1\" WHERE "
                               "romname=\"%2\";").arg(favorite).arg(romname);

    db->exec(thequery);
}

void RomInfo::fillData(QSqlDatabase *db)
{
    if (gamename == "")
    {
        return;
    }

    QString thequery = "SELECT system,gamename,genre,year,romname,favorite"
                       " FROM gamemetadata WHERE gamename=\"" + gamename + "\"";

    if (system != "")
        thequery += " AND system=\"" + system + "\"";

    thequery += ";";

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0);
    {
        query.next();

        system = query.value(0).toString();
        gamename = query.value(1).toString();
        genre = query.value(2).toString();
        year = query.value(3).toInt();
        romname = query.value(4).toString();
        favorite = query.value(5).toInt();
    }
}

