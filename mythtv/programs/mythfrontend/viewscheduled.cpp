#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qlistview.h>
#include <qdatetime.h>

#include "viewscheduled.h"
#include "infostructs.h"
#include "tv.h"
#include "programlistitem.h"
#include "scheduler.h"

ViewScheduled::ViewScheduled(QString prefix, TV *ltv, QSqlDatabase *ldb, 
                             QWidget *parent, const char *name)
             : QDialog(parent, name)
{
    tv = ltv;
    db = ldb;
    fileprefix = prefix;

    title = NULL;

    setGeometry(0, 0, 800, 600);
    setFixedWidth(800);
    setFixedHeight(600);

    setFont(QFont("Arial", 16, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, 20);

    desclabel = new QLabel("Select a recording to view:", this);
    vbox->addWidget(desclabel);

    listview = new QListView(this);
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

    sched = new Scheduler(db);

    listview->setFixedHeight(250);

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

    FillList();
}

void ViewScheduled::FillList(void)
{
    listview->clear();
    ProgramListItem *item;

    bool conflicts = sched->FillRecordLists();

    list<ProgramInfo *> *recordinglist = sched->getAllPending();
    list<ProgramInfo *>::reverse_iterator pgiter = recordinglist->rbegin();

    for (; pgiter != recordinglist->rend(); pgiter++)
    {
        ProgramInfo *originfo = (*pgiter);
        ProgramInfo *proginfo = new ProgramInfo;

        proginfo->channum = originfo->channum;
        proginfo->startts = originfo->startts;
        proginfo->endts = originfo->endts;
        proginfo->title = originfo->title;
        proginfo->subtitle = originfo->subtitle;
        proginfo->description = originfo->description;
        proginfo->conflicting = originfo->conflicting;

        char startt[128];
        char endt[128];

        QString starts = proginfo->startts.toString("yyyyMMddhhmm");
        QString endts = proginfo->endts.toString("yyyyMMddhhmm");

        sprintf(startt, "%s00", starts.ascii());
        sprintf(endt, "%s00", endts.ascii());

        RecordingInfo *tvrec = new RecordingInfo(proginfo->channum.ascii(),
                                                 startt, endt,
                                                 proginfo->title.ascii(),
                                                 proginfo->subtitle.ascii(),
                                                 proginfo->description.ascii());

        item = new ProgramListItem(listview, proginfo, tvrec, 3, tv,
                                   fileprefix);
    }

    if (conflicts)
        desclabel->setText("You've got time conflicts.  All conflicting programs are highlighted in <font color=\"red\">red</font>.");
    else
        desclabel->setText("You have no recording conflicts.");

    listview->setCurrentItem(listview->firstChild());
}

void ViewScheduled::Show()
{
    showFullScreen();
}

void ViewScheduled::changed(QListViewItem *lvitem)
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

void ViewScheduled::selected(QListViewItem *lvitem)
{
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();
}
