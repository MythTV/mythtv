#include <iostream>
#include <qsqldatabase.h>
#include <mythtv/mythcontext.h>
#include <sys/types.h>
#include <cstdlib>
#include <unistd.h>
#include "mamerominfo.h"
#include "mametypes.h"

extern struct Prefs general_prefs;

void MameRomInfo::fillData(QSqlDatabase *db)
{
    if (gamename == "")
    {
        return;
    }

    RomInfo::fillData(db);

    QString thequery = "SELECT manu,cloneof,romof,driver,"
                       "cpu1,cpu2,cpu3,cpu4,sound1,sound2,sound3,sound4,"
                       "players,buttons,image_searched FROM mamemetadata WHERE "
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
        image_searched = query.value(14).toInt();
    }
}

bool MameRomInfo::FindImage(QString type, QString *result)
{
    QString directories;
    if(type == "screenshot")
        directories = general_prefs.screenshot_dir;
    else if(type == "flyer")
        directories = general_prefs.flyer_dir;
    else if(type == "cabinet")
        directories = general_prefs.cabinet_dir;
    else
        return false;
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
    int i;
    QStringList dirs;
    QString imagename;
    QString games[3];
    FILE *imagefile;

    *result = "";
    games[0] = romname.latin1();
    if (cloneof == "-")
    {
        games[1] = "";
    }
    else
    {
        games[1] = cloneof.latin1();
        games[2] = "";
    }

    if (directories != "")
        dirs = QStringList::split(":",directories);
    if ((dirs.isEmpty()) && (directories != ""))
        dirs.append(directories);

    for (i = 0; (games[i] != ""); i++) {
        for (QStringList::Iterator j = graphic_formats.begin(); j != graphic_formats.end(); j++)
        {
            for (QStringList::Iterator k = dirs.begin(); k != dirs.end(); k++)
            {
                imagename = *k + "/" + games[i] + "." + *j;
                if ((imagefile = fopen(imagename, "r")))
                {
                    *result = imagename;
                    fclose(imagefile);
                }
                if (*result != "")
                    break;
            }
            /* NOTE: This requires that png is the first format in the list */
            if (*result != "")
                break;
        }
        if (*result != "")
            break;
    }

    return (*result != "");
}

