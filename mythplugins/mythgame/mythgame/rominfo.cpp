#include <qfile.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "rominfo.h"
#include "romedit.h"

bool operator==(const RomInfo& a, const RomInfo& b)
{
    if (a.Romname() == b.Romname())
        return true;
    return false;
}


void RomInfo::edit_rominfo()
{
    RomEditDLG romeditdlg(Romname().latin1());
    romeditdlg.exec();
}

// Return the count of how many times this appears in the db already
int romInDB(QString rom, QString gametype)
{
    QString thequery;
    MSqlQuery query(MSqlQuery::InitCon());

    int count = 0;

    thequery = QString("SELECT count(*) FROM gamemetadata WHERE gametype = \"%1\" AND romname = \"%2\";")
                        .arg(gametype)
                        .arg(rom);
    query.exec(thequery);

    if (query.isActive() && query.size() > 0);
    {
        query.next();
        count = query.value(0).toInt();
    };

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
    else
        cout << "Error: Invalid field " << field << endl;

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

        setSystem(query.value(0).toString());
        setGamename(query.value(1).toString());
        setGenre(query.value(2).toString());
        setYear(query.value(3).toInt());
        setRomname(query.value(4).toString());
        setField("favorite",query.value(5).toString());
        setRompath(query.value(6).toString());
        setCountry(query.value(7).toString());
        setCRC_VALUE(query.value(8).toString());
        setDiskCount(query.value(9).toInt());
        setGameType(query.value(10).toString());
    }

    thequery = "SELECT screenshots FROM gameplayers WHERE playername = \"" + system + "\";";

    query.exec(thequery);

    if (query.isActive() && query.size() > 0);
    {
        query.next();
        if (query.value(0).toString()) 
        {
            QString Image = query.value(0).toString() + "/" + romname;
            if (FindImage(query.value(0).toString() + "/" + romname, &Image))
                setImagePath(Image);
            else
                setImagePath("");
        }
    }

    setRomCount(romInDB(romname,gametype));

    // If we have more than one instance of this rom in the DB fill in all
    // systems available to play it.
    if (RomCount() > 1) 
    {
        thequery = "SELECT DISTINCT system FROM gamemetadata WHERE romname = \"" + Romname() + "\";";

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

