#include <qsqldatabase.h>
#include "mamerominfo.h"

void MameRomInfo::setField(QString field, QString data)
{
    RomInfo::setField(field, data);
}

void MameRomInfo::fillData(QSqlDatabase *db)
{
    if (gamename == "")
    {
        return;
    }

    RomInfo::fillData(db);

    QString thequery = "SELECT manu,cloneof,romof,driver,"
                       "cpu1,cpu2,cpu3,cpu4,sound1,sound2,sound3,sound4,"
                       "players,buttons FROM mamemetadata WHERE "
                       "romname=\"" + romname + "\";";

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0);
    {
        query.next();
        manu = query.value(0).toString();
        cloneof = query.value(1).toString();
        romof = query.value(2).toString();
        driver = query.value(3).toString();
        cpu1 = query.value(4).toString();
        cpu2 = query.value(5).toString();
        cpu3 = query.value(6).toString();
        cpu4 = query.value(7).toString();
        sound1 = query.value(8).toString();
        sound1 = query.value(9).toString();
        sound1 = query.value(10).toString();
        sound1 = query.value(11).toString();
        num_players = query.value(12).toInt();
        num_buttons = query.value(13).toInt();
    }
}