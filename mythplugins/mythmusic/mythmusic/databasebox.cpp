#include <qlayout.h>
#include <qlistview.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qheader.h>
#include <qpixmap.h>

#include "metadata.h"
#include "databasebox.h"
#include "treecheckitem.h"
#include "cddecoder.h"

#include <mythtv/mythcontext.h>

DatabaseBox::DatabaseBox(MythContext *context, QSqlDatabase *ldb, 
                         QString &paths, QValueList<Metadata> *playlist, 
                         QWidget *parent, const char *name)
           : MythDialog(context, parent, name)
{
    db = ldb;
    plist = playlist;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    MythListView *listview = new MythListView(this);
    listview->addColumn("Select music to be played:");

    listview->setSorting(-1);
    listview->setRootIsDecorated(true);
    listview->setAllColumnsShowFocus(true);
    listview->setColumnWidth(0, (int)(730 * wmult));
    listview->setColumnWidthMode(0, QListView::Manual);

    connect(listview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));

    cditem = NULL;

    fillList(listview, paths);

    vbox->addWidget(listview, 1);

    listview->setCurrentItem(listview->firstChild());
}

void DatabaseBox::fillCD(void)
{
    if (cditem)
    {
        while (cditem->firstChild())
        {
            delete cditem->firstChild();
        }
        cditem->setText(0, "CD -- none");
    }

    CdDecoder *decoder = new CdDecoder(m_context, "cda", NULL, NULL, NULL);
    int tracknum = decoder->getNumTracks();

    bool setTitle = false;

    while (tracknum > 0) 
    {
        Metadata *track = decoder->getMetadata(db, tracknum);

        if (!setTitle)
        {
            QString parenttitle = " " + track->Artist() + " ~ " + 
                                  track->Album();
            cditem->setText(0, parenttitle);
            setTitle = true;
        }

        QString title = QString(" %1").arg(tracknum);
        title += " - " + track->Title();

        QString level = "title";

        TreeCheckItem *item = new TreeCheckItem(m_context, cditem, title, 
                                                level, track);

        if (plist->find(*track) != plist->end())
            item->setOn(true);

        tracknum--;
    }

    checkParent(cditem);
}

void DatabaseBox::fillList(QListView *listview, QString &paths)
{
    QString title = "CD -- none";
    QString level = "cd";
    cditem = new TreeCheckItem(m_context, listview, title, level, NULL);

    QString templevel = "genre";
    QString temptitle = "All My Music";
    TreeCheckItem *allmusic = new TreeCheckItem(m_context, listview, temptitle,
                                                templevel, NULL);
    
    QStringList lines = QStringList::split(" ", paths);

    QString first = lines.front();

    char thequery[1024];
    sprintf(thequery, "SELECT DISTINCT %s FROM musicmetadata ORDER BY %s DESC;",
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

            Metadata *mdata = new Metadata();
            mdata->setField(first, current);

            TreeCheckItem *item = new TreeCheckItem(m_context, allmusic, 
                                                    current, first, mdata);

            fillNextLevel(level, num, querystr, matchstr, line, lines,
                          item);

            if (plist->find(*mdata) != plist->end())
                item->setOn(true);
        }
    }

    fillCD();

    listview->setOpen(allmusic, true);
}

void DatabaseBox::fillNextLevel(QString level, int num, QString querystr, 
                                QString matchstr, QStringList::Iterator line,
                                QStringList lines, TreeCheckItem *parent)
{
    if (level == "")
        return;

    QString orderstr = querystr;
 
    bool isleaf = false; 
    if (level == "title")
    {
        isleaf = true;
        querystr += "," + level + ",tracknum";
        orderstr += ",tracknum";
    }
    else
    {
        querystr += "," + level;
        orderstr += "," + level;
    }

    char thequery[1024];
    sprintf(thequery, "SELECT DISTINCT %s FROM musicmetadata WHERE %s "
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

            Metadata *parentdata = parent->getMetadata();
            Metadata *mdata = new Metadata(*parentdata);
            mdata->setField(level, current);

            if (isleaf)
            {
                mdata->fillData(db);

                QString temp = query.value(num+1).toString();
                temp += " - " + current;
                current = temp;
            }


            TreeCheckItem *item = new TreeCheckItem(m_context, parent, current,
                                                    level, mdata);

            if (isleaf)
            {
                if (plist->find(*mdata) != plist->end())
                    item->setOn(true);
            }

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

void DatabaseBox::doSelected(QListViewItem *item)
{
    TreeCheckItem *tcitem = (TreeCheckItem *)item;

    if (tcitem->childCount() > 0)
    {
        TreeCheckItem *child = (TreeCheckItem *)tcitem->firstChild();
        while (child) 
        {
            if (child->isOn() != tcitem->isOn())
            {
                child->setOn(tcitem->isOn());
                doSelected(child);
            }
            child = (TreeCheckItem *)child->nextSibling();
        }
    }
    else 
    {
        Metadata *mdata = tcitem->getMetadata();
        if (mdata != NULL)
        {
            if (tcitem->isOn())
                plist->push_back(*(tcitem->getMetadata()));
            else
                plist->remove(*(tcitem->getMetadata()));
        }
    }
}

void DatabaseBox::checkParent(QListViewItem *item)
{
    TreeCheckItem *tcitem = (TreeCheckItem *)item;

    TreeCheckItem *child = (TreeCheckItem *)tcitem->firstChild();
    if (!child)
        return;

    bool state = child->isOn();
    bool same = true;
    while (child)
    {
        if (child->isOn() != state)
            same = false;
        child = (TreeCheckItem *)child->nextSibling();
    }

    if (same)
        tcitem->setOn(state);

    if (!same)
        tcitem->setOn(false);

    if (tcitem->parent())
    {
        checkParent(tcitem->parent());
    }
}    
