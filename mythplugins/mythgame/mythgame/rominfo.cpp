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
        return false;
    BaseFileName.truncate(dotLocation + 1);
    for (QStringList::Iterator i = graphic_formats.begin(); i != graphic_formats.end(); i++)
    {
        *result = BaseFileName + *i;
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
}

void RomInfo::setFavorite()
{
    favorite = 1 - favorite;

    QString thequery = QString("UPDATE gamemetadata SET favorite=\"%1\" WHERE "
                               "romname=\"%2\";").arg(favorite).arg(romname);
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);
}

void RomInfo::fillData()
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

}

