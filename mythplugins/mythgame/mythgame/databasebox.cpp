#include <qlayout.h>
#include "extendedlistview.h"
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <iostream.h>

#include "rominfo.h"
#include "databasebox.h"
#include "treeitem.h"
#include "settings.h"
#include "gamehandler.h"

extern Settings *globalsettings;

DatabaseBox::DatabaseBox(QSqlDatabase *ldb, QString &paths, 
                         QValueList<RomInfo> *romlist,
                         QWidget *parent, const char *name)
           : QDialog(parent, name)
{
    db = ldb;
    rlist = romlist;

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", 16 * hmult, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, 20 * wmult);

    ExtendedListView *listview = new ExtendedListView(this);
    listview->addColumn("Select game to play");

    listview->setSorting(-1);
    listview->setRootIsDecorated(false);
    listview->setAllColumnsShowFocus(true);
    listview->setColumnWidth(0, 730 * wmult);
    listview->setColumnWidthMode(0, QListView::Manual);

    connect(listview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(RightKeyPressed(QListViewItem *)), this,
            SLOT(editSettings(QListViewItem *)));

    fillList(listview, paths);

    vbox->addWidget(listview, 1);

    listview->setCurrentItem(listview->firstChild());
}

void DatabaseBox::Show()
{
    showFullScreen();
    setActiveWindow();
}

void DatabaseBox::fillList(QListView *listview, QString &paths)
{
    QString templevel = "system";
    QString temptitle = "All Games";
    TreeItem *allgames = new TreeItem(listview, temptitle,
                                                templevel, NULL);
    
    QStringList lines = QStringList::split(" ", paths);

    QString first = lines.front();

    char thequery[1024];
    sprintf(thequery, "SELECT DISTINCT %s FROM gamemetadata ORDER BY %s DESC;",
                      first.ascii(), first.ascii());

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString current = query.value(0).toString();

            QString querystr = first;
            QString matchstr = first + " = \"" + current + "\"";           
 
            QStringList::Iterator line = lines.begin();
            ++line;
            int num = 1;

            QString level = *line;

            RomInfo *rinfo = new RomInfo();
            rinfo->setField(first, current);

            TreeItem *item = new TreeItem(allgames, current,
                                                    first, rinfo);
            fillNextLevel(level, num, querystr, matchstr, line, lines,
                          item);
        }
    }

    listview->setOpen(allgames, true);
}

void DatabaseBox::fillNextLevel(QString level, int num, QString querystr, 
                                QString matchstr, QStringList::Iterator line,
                                QStringList lines, TreeItem *parent)
{
    if (level == "")
        return;

    QString orderstr = querystr;
 
    bool isleaf = false; 
    if (level == "gamename")
    {
        isleaf = true;
    }
    querystr += "," + level;
    orderstr += "," + level;

    char thequery[1024];
      sprintf(thequery, "SELECT DISTINCT %s FROM gamemetadata WHERE %s "
                        "ORDER BY %s DESC;",
                        querystr.ascii(), matchstr.ascii(), orderstr.ascii());
                      
    QSqlQuery query = db->exec(thequery);
  
    ++line; 
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString current = query.value(num).toString();
            QString matchstr2 = matchstr + " AND " + level + " = \"" + 
                                current + "\"";

            RomInfo *parentinfo = parent->getRomInfo();
            RomInfo *rinfo;
            if(isleaf)
            {
                rinfo = GameHandler::CreateRomInfo(parentinfo);
                rinfo->setField(level, current);
                rinfo->fillData(db);
            }
            else
            {
                rinfo = new RomInfo(*parentinfo);
                rinfo->setField(level, current);
            }

            TreeItem *item = new TreeItem(parent, current, level,
                                                    rinfo);

            if (line != lines.end())
                fillNextLevel(*line, num + 1, querystr, matchstr2, line, lines,
                              item);

            if (!isleaf)
                checkParent(item);
        }
    }
}

void DatabaseBox::selected(QListViewItem *item)
{
    doSelected(item);

    if (item->parent())
    {
        checkParent(item->parent());
    }
}

void DatabaseBox::editSettings(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    if (tcitem->childCount() <= 0)
    {
        GameHandler::EditSettings(this,tcitem->getRomInfo());
    }
}

void DatabaseBox::doSelected(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    if (tcitem->childCount() <= 0)
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

    bool same = true;
    while (child)
    {
        child = (TreeItem *)child->nextSibling();
    }

    if (tcitem->parent())
    {
        checkParent(tcitem->parent());
    }
}    
