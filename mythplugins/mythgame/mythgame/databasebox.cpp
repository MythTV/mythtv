#include <qlayout.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <iostream>

using namespace std;

#include "rominfo.h"
#include "databasebox.h"
#include "treeitem.h"
#include "gamehandler.h"
#include "extendedlistview.h"

DatabaseBox::DatabaseBox(QSqlDatabase *ldb, QString &paths, QWidget *parent, 
                         const char *name)
           : MythDialog(parent, name)
{
    db = ldb;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    ExtendedListView *listview = new ExtendedListView(this);
    listview->addColumn("Select game to play");

    listview->setSorting(-1);
    listview->setRootIsDecorated(false);
    listview->setAllColumnsShowFocus(true);
    listview->setColumnWidth(0, (int)(730 * wmult));
    listview->setColumnWidthMode(0, QListView::Manual);

    connect(listview, SIGNAL(KeyPressed(QListViewItem *, int)), this,
            SLOT(handleKey(QListViewItem *, int)));
    connect(listview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));

    fillList(listview, paths);

    vbox->addWidget(listview, 1);

    listview->setCurrentItem(listview->firstChild());
    listview->setSelected(listview->firstChild(), true);
}

void DatabaseBox::fillList(QListView *listview, QString &paths)
{
    QString templevel = "system";
    QString temptitle = "All Games";
    TreeItem *allgames = new TreeItem(listview, temptitle, templevel, NULL);
    
    QStringList lines = QStringList::split(" ", paths);

    QString first = lines.front();

    QStringList regSystems;
    if (first == "system")
    {
        for (uint i = 0; i < GameHandler::count(); ++i)
            regSystems.append(GameHandler::getHandler(i)->Systemname());
    }

    QString thequery = QString("SELECT DISTINCT %1 FROM gamemetadata "
                               "ORDER BY %2 DESC;").arg(first).arg(first);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString current = query.value(0).toString();

            // Don't display non-registered systems, even if they are in the
            // database.
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

void DatabaseBox::selected(QListViewItem *item)
{
    doSelected(item);

    if (item->parent())
    {
        checkParent(item->parent());
    }
}

void DatabaseBox::handleKey(QListViewItem *item, int key)
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

void DatabaseBox::editSettings(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    if("system" == tcitem->getLevel())
    {
        GameHandler::EditSystemSettings(this, tcitem->getRomInfo());
    }
    else if (tcitem->childCount() <= 0)
    {
        GameHandler::EditSettings(this, tcitem->getRomInfo());
    }
}

void DatabaseBox::doSelected(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    if (tcitem->childCount() <= 0 && tcitem->parent())
    {
        GameHandler::Launchgame(tcitem->getRomInfo());
    }
}

void DatabaseBox::checkParent(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    TreeItem *child = (TreeItem *)tcitem->firstChild();
    if (!child)
        return;

    while (child)
    {
        child = (TreeItem *)child->nextSibling();
    }

    if (tcitem->parent())
    {
        checkParent(tcitem->parent());
    }
}    
