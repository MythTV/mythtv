#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qlistview.h>
#include <qdatetime.h>

#include "playbackbox.h"
#include "infostructs.h"
#include "tv.h"

class ProgramListItem : public QListViewItem
{
  public:
    ProgramListItem(QListView *parent, ProgramInfo *lpginfo);
   ~ProgramListItem() { delete pginfo; }
  
    ProgramInfo *getProgramInfo() { return pginfo; }
 
  protected:
    ProgramInfo *pginfo;
};

ProgramListItem::ProgramListItem(QListView *parent, ProgramInfo *lpginfo)
               : QListViewItem(parent)
{
    pginfo = lpginfo;
    setText(0, pginfo->channum);
    setText(1, pginfo->startts.toString("MMMM d h:mm AP"));
    setText(2, pginfo->title);
}

PlaybackBox::PlaybackBox(TV *ltv, QSqlDatabase *ldb, QWidget *parent, 
                         const char *name)
           : QDialog(parent, name)
{
    tv = ltv;
    db = ldb;

    setGeometry(0, 0, 800, 600);
    setFixedWidth(800);
    setFixedHeight(600);

    setFont(QFont("Arial", 16, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, 20);

    QLabel *label = new QLabel("Select a recording to view:", this);
    vbox->addWidget(label);

    QListView *listview = new QListView(this);
    listview->addColumn("#");
    listview->addColumn("Date");
    listview->addColumn("Title");
 
    listview->setColumnWidth(0, 40);
    listview->setColumnWidth(1, 210); 
    listview->setColumnWidth(2, 500);
    listview->setColumnWidthMode(0, QListView::Manual);
    listview->setColumnWidthMode(1, QListView::Manual);

    listview->setSorting(-1);
    listview->setAllColumnsShowFocus(TRUE);

    connect(listview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *))); 

    vbox->addWidget(listview, 1);

    QSqlQuery query;
    char thequery[512];
    ProgramListItem *item;
    
    sprintf(thequery, "SELECT * FROM recorded;");

    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;
 
            proginfo->channum = query.value(0).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->title = query.value(3).toString();
            proginfo->subtitle = query.value(4).toString();
            proginfo->description = query.value(5).toString();

            item = new ProgramListItem(listview, proginfo); 
        }
    }
    else
    {
        // TODO: no recordings
    }

    listview->setCurrentItem(listview->firstChild());
}

void PlaybackBox::Show()
{
    showFullScreen();
}

void PlaybackBox::selected(QListViewItem *lvitem)
{
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();

    char startt[128];
    char endt[128];
    
    QString starts = rec->startts.toString("yyyyMMddhhmm");
    QString endts = rec->endts.toString("yyyyMMddhhmm");

    sprintf(startt, "%s00", starts.ascii());
    sprintf(endt, "%s00", endts.ascii());

    RecordingInfo *tvrec = new RecordingInfo(rec->channum.ascii(),
                                             startt, endt, rec->title.ascii(),
                                             rec->subtitle.ascii(),
                                             rec->description.ascii());
    
    tv->Playback(tvrec);
}
