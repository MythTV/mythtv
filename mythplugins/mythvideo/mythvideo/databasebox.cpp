#include <qlayout.h>
#include <qlistview.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <iostream>
#include <unistd.h>
#include <qregexp.h>
#include <qscrollview.h> 

using namespace std;

#include "metadata.h"
#include "databasebox.h"
#include "treecheckitem.h"
#include "dirlist.h"
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>

#ifdef ENABLE_LIRC
#include "lirc_client.h"
extern struct lirc_config *config;
#endif

DatabaseBox::DatabaseBox(MythContext *context, QSqlDatabase *ldb, 
                         QValueList<Metadata> *playlist, 
                         QWidget *parent, const char *name)
           : QDialog(parent, name)
{
    db = ldb;
    plist = playlist;
    m_context = context;

    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    context->ThemeWidget(this);

    setFont(QFont("Arial", (int)(m_context->GetMediumFontSize() * hmult), 
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    listview = new MythListView(this);
    listview->addColumn("Select file to be played:");
    
    listview->setSorting(-1);
    listview->setAllColumnsShowFocus(true);
    listview->setColumnWidth(0, (int)(730 * wmult));
    listview->setColumnWidthMode(0, QListView::Manual);
    connect(listview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    cditem = NULL;

    fillList(listview, playlist);

    vbox->addWidget(listview, 1);

    listview->setCurrentItem(listview->firstChild());
}

void DatabaseBox::Show()
{
    showFullScreen();
}

#ifdef ENABLE_LIRC

void DatabaseBox::dataReceived()
{
  //  printf("get lirc data\n");
    char *code;
    char *c;
    int ret;
    char buffer[200];
    size_t l;

    while (ret=lirc_nextcode(&code)==0 && code !=NULL)
    {
        while ((ret=lirc_code2char(config,code,&c))==0 && c!=NULL)
        {
            if (code==NULL) 
                continue;
            QString str = QString(c);
            if (str == "Up")
	    { 
                listview->setCurrentItem(listview->currentItem()->itemAbove());
                listview->setSelected(listview->currentItem(),TRUE);
   	        listview->ensureItemVisible(listview->currentItem());
	    }
            else if (str == "Down")
   	    {
                listview->setCurrentItem(listview->currentItem()->itemBelow());
                listview->setSelected(listview->currentItem(),TRUE);
	        listview->ensureItemVisible(listview->currentItem());
	    }
            else if(str == "Enter")
	    {
	        doSelected(listview->currentItem());
	    }
        }
   
        free(code);
        if(ret==-1) break;
    }
}
#endif

void DatabaseBox::fillList(MythListView *listview, 
                           QValueList<Metadata> *playlist)
{
    QString title;
    QValueList<Metadata>::iterator it;
    for ( it = playlist->begin(); it != playlist->end(); ++it )
    {
        Metadata *mdata = new Metadata();
        QString filename =(*it).Filename();
        mdata->setFilename(filename);
        mdata->setGenre((*it).Genre());
        mdata->setField("title",(*it).Filename());
        title=(*it).Filename().section('/',-1);
        new TreeCheckItem(m_context, listview, title, NULL, mdata);
    }
}

void DatabaseBox::selected(QListViewItem *item)
{
    doSelected(item);
}

void DatabaseBox::doSelected(QListViewItem *item)
{
    TreeCheckItem *tcitem = (TreeCheckItem *)item;

    Metadata *mdata = tcitem->getMetadata();

    if (mdata != NULL && mdata->Genre() != "dir")
    {
        QString filename = mdata->Filename();	  
        QString title = mdata->Filename().section('/',-1);
        QString ext = mdata->Filename().section('.',-1);
	 
        QString handler = m_context->GetSetting(QString("%1_helper").arg(ext));
 //	 cout << "handler for" << ext.ascii() << ":" << handler.ascii() << endl;
        QString command = handler.replace(QRegExp("%s"), 
                                          QString("'%1'").arg(filename));
 //	 cout << "command:" << command << endl;

        system((QString("%1 ") .arg(command)).ascii() );
    }
    else if (mdata != NULL && mdata->Genre() == "dir")
    {
 //	 printf("got directory %s\n",mdata->Filename().ascii());
        QString dirname = mdata->Filename();
        Dirlist md(m_context, dirname);

        QValueList<Metadata> playlist = md.GetPlaylist();
        listview->clear();
        fillList(listview, &playlist);
    }
}

