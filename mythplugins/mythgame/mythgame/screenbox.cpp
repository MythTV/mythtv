#include <qlayout.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <iostream>
#include <stdlib.h>

using namespace std;

#include "rominfo.h"
#include "screenbox.h"
#include "treeitem.h"
#include "gamehandler.h"
#include "extendedlistview.h"

#include <mythtv/mythcontext.h>

ScreenBox::ScreenBox(QSqlDatabase *ldb, QString &paths, MythMainWindow *parent, 
                     const char *name)
         : MythDialog(parent, name)
{
    db = ldb;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(5 * wmult), 
                                        (int)(5 * wmult));

    mGameLabel = new QLabel(this);
    mGameLabel->setBackgroundOrigin(WindowOrigin);
    mGameLabel->setFixedHeight((int)(23 * hmult));
    mGameLabel->setAlignment(Qt::AlignHCenter);
    mGameLabel->setText("");

    ExtendedListView *listview = new ExtendedListView(this);
    listview->setFixedHeight((int)(170 * hmult));
    listview->addColumn("Select genre", -1);

    listview->setSorting(-1);
    listview->setRootIsDecorated(false);
    listview->setAllColumnsShowFocus(true);
    listview->setColumnWidthMode(0, QListView::Maximum);
    listview->setResizeMode(QListView::LastColumn);

    PicFrame = new SelectFrame(this);
    PicFrame->setFixedWidth((int)(790 * wmult));
    PicFrame->setFixedHeight((int)(350 * hmult));
    PicFrame->setPaletteBackgroundColor(palette().color(QPalette::Active,
                                                        QColorGroup::Base));
    PicFrame->setFocusPolicy(QWidget::NoFocus);

    connect(listview, SIGNAL(currentChanged(QListViewItem *)), this,
            SLOT(setImages(QListViewItem*)));
    connect(PicFrame, SIGNAL(gameChanged(const QString&)), mGameLabel,
            SLOT(setText(const QString&)));
    connect(listview, SIGNAL(KeyPressed(QListViewItem *, int)), this,
            SLOT(handleKey(QListViewItem *, int)));

    fillList(listview, paths);

    vbox->addWidget(listview, 0);
    vbox->addWidget(mGameLabel, 1);
    vbox->addWidget(PicFrame, 0);

    PicFrame->setDimensions();
    PicFrame->setButtons(NULL);
    PicFrame->setUpdatesEnabled(true);

    listview->setCurrentItem(listview->firstChild());
    listview->setSelected(listview->firstChild(), true);
}

void ScreenBox::fillList(QListView *listview, QString &paths)
{
    QString templevel = "system";
    QString temptitle = "All Games";
    TreeItem *allgames = new TreeItem(listview, temptitle, templevel, NULL);
    
    QStringList lines = QStringList::split(" ", paths);
    int Levels = lines.count();

    if(lines[Levels - 1] == "gamename" && Levels > 1)
        leafLevel = lines[Levels - 2];
    else
        leafLevel = lines[Levels - 1];

    QString first = lines.front();

    QStringList regSystems;
    if (first == "system")
    {
        for (uint i = 0; i < GameHandler::count(); ++i)
            regSystems.append(GameHandler::getHandler(i)->Systemname());
    }

    QString thequery = QString("SELECT DISTINCT %1 FROM gamemetadata ORDER "
                               "BY %2 DESC;").arg(first).arg(first);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString current = query.value(0).toString();

            // Don't display non-registered systems, even if they are
            // in the database.
            if (first == "system" && 
                (regSystems.find(current) == regSystems.end()))
                continue;

            RomInfo *rinfo = new RomInfo();
            rinfo->setField(first, current);

            TreeItem *item = new TreeItem(allgames, current, first, rinfo);
            item->setExpandable(true);
        }
    }

    listview->setOpen(allgames, true);
}

void ScreenBox::checkParent(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    TreeItem *child = (TreeItem *)tcitem->firstChild();
    if (!child)
        return;

    //bool same = true;
    while (child)
    {
        child = (TreeItem *)child->nextSibling();
    }

    if (tcitem->parent())
    {
        checkParent(tcitem->parent());
    }
}

void ScreenBox::setImages(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;
    if(tcitem->getLevel() == leafLevel)
    {
        PicFrame->setFocusPolicy(QWidget::TabFocus);
        RomInfo *romdata = tcitem->getRomInfo();
        QString thequery;
        QString matchstr = "system = \"" + romdata->System() + "\"";
        if(romdata->Genre() != "")
            matchstr+= " AND genre = \"" + romdata->Genre() + "\"";
        if(romdata->Year() != 0)
        {
            QString year;
            year.sprintf("%d",romdata->Year());
            matchstr+= " AND year = \"" + year + "\"";
        }
        thequery = QString("SELECT DISTINCT gamename FROM gamemetadata "
                           "WHERE %1 ORDER BY gamename ASC;")
                           .arg(matchstr);
        QSqlQuery query = db->exec(thequery);
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            RomInfo *rinfo;
            QPtrList<RomInfo> *romlist = new QPtrList<RomInfo>;
            while (query.next())
            {
                rinfo = GameHandler::CreateRomInfo(romdata);
                rinfo->setField("gamename",query.value(0).toString());
                rinfo->fillData(db);
                romlist->append(rinfo);
            }
            PicFrame->setRomlist(romlist);
        }
    }
    else
    {
        PicFrame->clearButtons();
        PicFrame->setFocusPolicy(QWidget::NoFocus);
    }
}

void ScreenBox::editSettings(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    if("system" == tcitem->getLevel())
    {
        GameHandler::EditSystemSettings(gContext->GetMainWindow(), 
                                        tcitem->getRomInfo());
    }
}

void ScreenBox::handleKey(QListViewItem *item, int key)
{
  TreeItem *tcitem = (TreeItem *)item;
  switch(key)
  {
  case Key_E:
    editSettings(tcitem);
    break;
  default:
    break;
  }
}                
