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

    title = NULL;

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
    connect(listview, SIGNAL(selectionChanged(QListViewItem *)), this,
            SLOT(changed(QListViewItem *)));

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
   
    listview->setFixedHeight(300);

    QGridLayout *grid = new QGridLayout(vbox, 4, 2, 1);
    
    title = new QLabel(" ", this);
    title->setFont(QFont("Arial", 25, QFont::Bold));

    QLabel *datelabel = new QLabel("Airdate: ", this);
    date = new QLabel(" ", this);

    QLabel *sublabel = new QLabel("Episode: ", this);
    subtitle = new QLabel(" ", this);
    QLabel *desclabel = new QLabel("Description: ", this);
    description = new QLabel(" ", this);
    description->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);
 
    grid->addMultiCellWidget(title, 0, 0, 0, 1, Qt::AlignLeft);
    grid->addWidget(datelabel, 1, 0, Qt::AlignLeft);
    grid->addWidget(date, 1, 1, Qt::AlignLeft);
    grid->addWidget(sublabel, 2, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(subtitle, 2, 1, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(desclabel, 3, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(description, 3, 1, Qt::AlignLeft | Qt::AlignTop);
    
    grid->setColStretch(1, 1);
    grid->setRowStretch(3, 1);

    listview->setCurrentItem(listview->firstChild());
}

void PlaybackBox::Show()
{
    showFullScreen();
}

void PlaybackBox::changed(QListViewItem *lvitem)
{
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    if (!pgitem)
        return;
   
    if (!title)
        return;

    ProgramInfo *rec = pgitem->getProgramInfo();

    QDateTime startts = rec->startts;
    QDateTime endts = rec->endts;
        
    QString timedate = endts.date().toString("ddd MMMM d") + QString(", ") +
                       startts.time().toString("h:mm AP") + QString(" - ") +
                       endts.time().toString("h:mm AP");
        
    date->setText(timedate);

    title->setText(rec->title);
    if (rec->subtitle != "(null)")
        subtitle->setText(rec->subtitle);
    else
        subtitle->setText("");
    if (rec->description != "(null)")
        description->setText(rec->description);
    else
        description->setText("");
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
