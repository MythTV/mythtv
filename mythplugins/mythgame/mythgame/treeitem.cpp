#include <qapplication.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qsqldatabase.h>

#include "treeitem.h"
#include "gamehandler.h"
#include <mythtv/mythcontext.h>

#include "res/system_pix.xpm"
#include "res/genre_pix.xpm"
#include "res/game_pix.xpm"

QPixmap *TreeItem::system = NULL;
QPixmap *TreeItem::game = NULL;
QPixmap *TreeItem::genre = NULL;
bool TreeItem::pixmapsSet = false;

TreeItem::TreeItem(QListView *parent, QString &ltext, const QString &llevel, 
                   RomInfo *rinfo)
        : QListViewItem(parent, ltext.prepend(" "))
{
    level = llevel;
    rominfo = rinfo;

    pickPixmap();
}

TreeItem::TreeItem(TreeItem *parent, QString &ltext, const QString &llevel, 
                   RomInfo *rinfo)
        : QListViewItem(parent, ltext.prepend(" "))
{
    level = llevel;
    rominfo = rinfo;

    pickPixmap();
}

void TreeItem::pickPixmap(void)
{
    if (!pixmapsSet)
        setupPixmaps();

    if (level == "system")
        setPixmap(0, *system);
    else if (level == "gamename")
    {
        //QPixmap test;
        //test.load("/screens/005.png");
        //setPixmap(0,test);
        setPixmap(0, *game);
        //Cabinet.load(picfile);
        //ScaleImageLabel(Cabinet,CabinetPic);
        //CabinetPic->setPixmap(Cabinet);
    }
    else if (level == "genre")
        setPixmap(0, *genre);
}

void TreeItem::setupPixmaps(void)
{
    int screenwidth = 0, screenheight = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    if (screenheight != 600 || screenwidth != 800) 
    {   
        system = scalePixmap((const char **)system_pix, wmult, hmult);
        game = scalePixmap((const char **)game_pix, wmult, hmult);
        genre = scalePixmap((const char **)genre_pix, wmult, hmult);
    }
    else
    {
        system = new QPixmap((const char **)system_pix);
        game = new QPixmap((const char **)game_pix);
        genre = new QPixmap((const char **)genre_pix);
    }

    pixmapsSet = true;
}

QPixmap *TreeItem::scalePixmap(const char **xpmdata, float wmult,
                                    float hmult)
{
    QImage tmpimage(xpmdata);
    QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult), 
                                       (int)(tmpimage.height() * hmult));
    QPixmap *ret = new QPixmap();
    ret->convertFromImage(tmp2);

    return ret;
}

void TreeItem::setOpen(bool o)
{
    // Add this item's children if this is the first time we are opening it.
    if (o && !childCount())
    {
        // Create a SQL query to get the children.
        QString whereClause;
        QString conjunction;
        QString column;

        QStringList fields = QStringList::split(" ", gContext->GetSetting("TreeLevels"));
        for (QStringList::Iterator field = fields.begin();
             field != fields.end();
             ++field)
        {
            whereClause += conjunction + getClause(*field);
            conjunction = " AND ";

            if (*field == level)
            {
                ++field;
                column = *field;
                break;
            }
        }

        QString thequery = QString("SELECT DISTINCT %1 FROM gamemetadata "
                                   "WHERE %2 ORDER BY %3 DESC;")
                                   .arg(column).arg(whereClause).arg(column);

        // Add the children.
        QString leafLevel;
        if (gContext->GetNumSetting("ShotCount"))
        {
            int nfields = fields.count();
            if(fields[nfields - 1] == "gamename" && nfields > 1)
                leafLevel = fields[nfields - 2];
            else
                leafLevel = fields[nfields - 1];
        }
        else
        {
            leafLevel = "gamename";
        }
        bool isleaf = (column == leafLevel);

        QStringList regSystems;
        if (column == "system")
        {
            for (uint i = 0; i < GameHandler::count(); ++i)
                regSystems.append(GameHandler::getHandler(i)->Systemname());
        }

        QSqlDatabase* db = QSqlDatabase::database();
        QSqlQuery query = db->exec(thequery);
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                QString current = query.value(0).toString();

                // Don't display non-registered systems, even if they are
                // in the database.
                if (column == "system" && (regSystems.find(current) == regSystems.end()))
                    continue;

                RomInfo* rinfo;
                if (isleaf)
                {
                    rinfo = GameHandler::CreateRomInfo(rominfo);
                    rinfo->setField(column, current);
                    rinfo->fillData(db);
                }
                else
                {
                    rinfo = new RomInfo(*rominfo);
                    rinfo->setField(column, current);
                }

                TreeItem* item = new TreeItem(this, current, column, rinfo);
                if (!isleaf)
                    item->setExpandable(TRUE);
            }
        }
    }
    QListViewItem::setOpen(o);
}

QString TreeItem::getClause(QString field)
{
    if (rominfo == 0)
        return 0;

    QString clause = field + " = \"";
    if (field == "system")
        clause += rominfo->System();
    else if (field == "year")
        clause += QString::number(rominfo->Year());
    else if (field == "genre")
        clause += rominfo->Genre();
    else if (field == "gamename")
        clause += rominfo->Gamename();
    clause += "\"";
    return clause;
}
