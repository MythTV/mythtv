#include <qlayout.h>
#include <qlistview.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qheader.h>
#include <qpixmap.h>
#include <qregexp.h>

#include "metadata.h"
#include "databasebox.h"
#include "treecheckitem.h"
#include "cddecoder.h"

#include <mythtv/mythcontext.h>

DatabaseBox::DatabaseBox(QSqlDatabase *ldb, QString &paths, 
                         QValueList<Metadata> *playlist, 
                         QWidget *parent, const char *name)
           : MythDialog(parent, name)
{
    db = ldb;
    plist = playlist;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    MythListView *listview = new MythListView(this);
    listview->DontFixSpaceBar();
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

    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
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

        TreeCheckItem *item = new TreeCheckItem(cditem, title, level, track);

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
    cditem = new TreeCheckItem(listview, title, level, NULL);

    QString templevel = "genre";
    QString temptitle = "All My Music";
    TreeCheckItem *allmusic = new TreeCheckItem(listview, temptitle, templevel,
                                                NULL);
   
    if ((paths == "directory") || (paths == "subdirectory"))
    {
        // tree levels match directory tree

        QString base_dir = gContext->GetSetting("MusicLocation");

        QString levels[10];
        int current_level = 1;
        int first = 1;
        TreeCheckItem *node[10];

        node[0] = allmusic;

        QString thequery = "SELECT filename, title, artist, album, tracknum "
                           "FROM musicmetadata ORDER BY filename DESC;"; 

        QSqlQuery query = db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                int sub_level = 1;

                QString filename = query.value(0).toString();
                QString current = query.value(0).toString();
                QString title = query.value(1).toString();
                QString artist = query.value(2).toString();
                QString album = query.value(3).toString();
                QString disp_title = query.value(4).toString();
                QString current_dir;

                current.replace(QRegExp(base_dir), QString(""));
                current.replace(QRegExp("/[^/]*$"), QString(""));

                QStringList subdirs = QStringList::split("/", current);
                QStringList::iterator subdir_it = subdirs.begin();

                for(; subdir_it != subdirs.end(); subdir_it++, sub_level++ )
                {
                    QString subdir = *subdir_it;
                    QString subdir_txt = subdir;

                    current_dir = current_dir + subdir + "/";

                    subdir_txt.replace(QRegExp("_"), QString(" "));

                    if ((first) || (levels[sub_level] != subdir))
                    {
                        Metadata *mdata = new Metadata();
                        mdata->setField("filename", current_dir);
                        levels[sub_level] = subdir;
                        node[sub_level] = new TreeCheckItem(node[sub_level-1],
                                                            subdir_txt, subdir,
                                                            mdata);
                        if (plist->find(*mdata) != plist->end())
                             node[sub_level]->setOn(true);

                        checkParent(node[sub_level]);
                        first = 0;
                    }
                    current_level = sub_level;
                }

                // now fill in the file entry and it's metadata
                Metadata *mdata = new Metadata();
                mdata->setField("title", title);
                mdata->setField("artist", artist);
                mdata->setField("album", album);
                mdata->setField("filename", filename);
                mdata->fillData(db);

                disp_title += " - " + title;

                TreeCheckItem *item = new TreeCheckItem(node[current_level],
                                                        disp_title, disp_title,
                                                        mdata);

                if (plist->find(*mdata) != plist->end())
                    item->setOn(true);
            }
        }
    } 
    else 
    {
        QStringList lines = QStringList::split(" ", paths);

        QString first = lines.front();

        QString thequery = QString("SELECT DISTINCT %1 FROM musicmetadata "
                                   "ORDER BY %2 DESC;").arg(first).arg(first);

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

                TreeCheckItem *item = new TreeCheckItem(allmusic, current, 
                                                        first, mdata);

                fillNextLevel(level, num, querystr, matchstr, line, lines,
                              item);

                if (plist->find(*mdata) != plist->end())
                    item->setOn(true);
            }
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

    QString thequery = QString("SELECT DISTINCT %1 FROM musicmetadata WHERE %2 "
                               "ORDER BY %3 DESC;").arg(querystr).arg(matchstr)
                                                   .arg(orderstr);
                      
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


            TreeCheckItem *item = new TreeCheckItem(parent, current, level, 
                                                    mdata);

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
