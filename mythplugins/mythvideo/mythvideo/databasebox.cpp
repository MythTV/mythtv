#include <qlayout.h>
#include <qlistview.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <qheader.h>
#include <iostream>
#include <unistd.h>
#include <qregexp.h>
using namespace std;

#include "metadata.h"
#include "databasebox.h"
#include "treecheckitem.h"

#include <mythtv/mythcontext.h>

DatabaseBox::DatabaseBox(MythContext *context, QSqlDatabase *ldb, 
                         QString &paths, QValueList<Metadata> *playlist, 
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

    QListView *listview = new QListView(this);
    listview->addColumn("Select file to be played:");
    
    listview->setSorting(-1);
    //listview->setRootIsDecorated(true);
    listview->setAllColumnsShowFocus(true);
    listview->setColumnWidth(0, (int)(730 * wmult));
    listview->setColumnWidthMode(0, QListView::Manual);

    listview->viewport()->setPalette(palette());
    listview->horizontalScrollBar()->setPalette(palette());
    listview->verticalScrollBar()->setPalette(palette());
    listview->header()->setPalette(palette());
    listview->header()->setFont(font());

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


void DatabaseBox::fillList(QListView *listview, QValueList<Metadata> *playlist)
{
      QString title;
   QValueList<Metadata>::iterator it;
          for ( it = playlist->begin(); it != playlist->end(); ++it )
	    {
            Metadata *mdata = new Metadata();
	    QString filename =(*it).Filename();
            mdata->setFilename(filename);
            mdata->setField("title",(*it).Filename());
	    title=(*it).Filename().section('/',-1);
            TreeCheckItem *item = new TreeCheckItem(m_context, listview, 
                                                    title, NULL, mdata);
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

        if (mdata != NULL)
        {

	  QString filename = mdata->Filename();
	  
	 QString title=mdata->Filename().section('/',-1);
	 QString ext=mdata->Filename().section('.',-1);
	 
	 QString handler = m_context->GetSetting(QString("%1_helper") . arg(ext));
	 //	 cout << "handler for" << ext.ascii() << ":" << handler.ascii() << endl;
	 QString command = handler.replace( QRegExp("%s"), QString("'%1'").arg(filename)); // string == "ba"
	 //	 cout << "command:" << command << endl;

	 system((QString("%1 &") .arg(command)).ascii() );
        }

}

