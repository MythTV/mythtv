#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qregexp.h>

#include "viewscheduled.h"
#include "infostructs.h"
#include "tv.h"
#include "programlistitem.h"
#include "scheduler.h"
#include "dialogbox.h"

ViewScheduled::ViewScheduled(QString prefix, TV *ltv, QSqlDatabase *ldb, 
                             QWidget *parent, const char *name)
             : QDialog(parent, name)
{
    tv = ltv;
    db = ldb;
    fileprefix = prefix;

    title = NULL;

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    screenwidth = 800; screenheight = 600;

    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", 16 * hmult, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, 10 * wmult);

    desclabel = new QLabel("Select a recording to view:", this);
    vbox->addWidget(desclabel);

    listview = new MyListView(this);
    listview->addColumn("#");
    listview->addColumn("Date");
    listview->addColumn("Title");
 
    listview->setColumnWidth(0, 40 * wmult);
    listview->setColumnWidth(1, 210 * wmult); 
    listview->setColumnWidth(2, 500 * wmult);
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

    listview->setFixedHeight(250 * hmult);

    QLabel *key = new QLabel("Conflicting recordings are highlighted in <font color=\"red\">red</font>.<br>Deactivated recordings are highlighted in <font color=\"gray\">gray</font>.", this);
    key->setFont(QFont("Arial", 12 * hmult, QFont::Bold));
    vbox->addWidget(key);

    QFrame *f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth(4 * hmult);
    vbox->addWidget(f);     

    QGridLayout *grid = new QGridLayout(vbox, 4, 2, 1);
    
    title = new QLabel(" ", this);
    title->setFont(QFont("Arial", 20 * hmult, QFont::Bold));

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

    bool conflicts = sched->FillRecordLists(false);

    list<ProgramInfo *> *recordinglist = sched->getAllPending();
    list<ProgramInfo *>::reverse_iterator pgiter = recordinglist->rbegin();

    for (; pgiter != recordinglist->rend(); pgiter++)
    {
        ProgramInfo *originfo = (*pgiter);
        ProgramInfo *proginfo = new ProgramInfo(*originfo);

        item = new ProgramListItem(listview, proginfo, 3, tv, fileprefix);
    }

    if (conflicts)
        desclabel->setText("You have time conflicts.");
    else
        desclabel->setText("You have no recording conflicts.");

    listview->setCurrentItem(listview->firstChild());
}

void ViewScheduled::Show()
{
    showFullScreen();
    setActiveWindow();
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

    if (!rec->recording)
    {
        handleNotRecording(rec);
    } 
    else if (rec->conflicting)
    {
        handleConflicting(rec);
    }
}

void ViewScheduled::handleNotRecording(ProgramInfo *rec)
{
    QString message = "Recording this program has been deactivated becuase it conflicts with another scheduled recording.  Do you want to re-enable this recording?";

    DialogBox diag(message);

    QString button = "Yes, I want to record it.";
    diag.AddButton(button);

    button = "No, leave it disabled.";
    diag.AddButton(button);

    diag.Show();

    int ret = diag.exec();

    if (ret != 1)
        return;

    QString pstart = rec->startts.toString("yyyyMMddhhmm");
    pstart += "00";
    QString pend = rec->endts.toString("yyyyMMddhhmm");
    pend += "00";

    char thequery[512];

    sprintf(thequery, "INSERT INTO conflictresolutionoverride (channum, "
                      "starttime, endtime) VALUES (%d, %s, %s);",
                      rec->channum.toInt(), pstart.ascii(), pend.ascii());

    db->exec(thequery);

    list<ProgramInfo *> *conflictlist = sched->getConflicting(rec, false);

    QString dstart, dend;
    list<ProgramInfo *>::iterator i;
    for (i = conflictlist->begin(); i != conflictlist->end(); i++)
    {
        dstart = (*i)->startts.toString("yyyyMMddhhmm");
        dstart += "00";
        dend = (*i)->endts.toString("yyyyMMddhhmm");
        dend += "00";

        sprintf(thequery, "DELETE FROM conflictresolutionoverride WHERE "
                          "channum = %d AND starttime = %s AND "
                          "endtime = %s;",
                          (*i)->channum.toInt(), dstart.ascii(), dend.ascii());

        db->exec(thequery);
    }

    delete conflictlist;

    sprintf(thequery, "UPDATE settings SET data = \"yes\" WHERE "
                      "value = \"RecordChanged\";");

    db->exec(thequery);

    FillList();
}

void ViewScheduled::handleConflicting(ProgramInfo *rec)
{
    list<ProgramInfo *> *conflictlist = sched->getConflicting(rec, true);

    QString message = "The follow scheduled recordings conflict with each other.  Which would you like to record?";

    DialogBox diag(message, "Remember this choice and use it automatically in the future");
 
    QString button; 
    button = rec->title + QString("\n");
    button += rec->startts.toString("ddd MMMM d h:mm AP");
    button += QString(" on channel ") + rec->channum;

    diag.AddButton(button);

    list<ProgramInfo *>::iterator i = conflictlist->begin();
    for (; i != conflictlist->end(); i++)
    {
        ProgramInfo *info = (*i);

        button = info->title + QString("\n");
        button += info->startts.toString("ddd MMMM d h:mm AP");
        button += QString(" on channel ") + info->channum;

        diag.AddButton(button);
    }

    diag.Show();
    int ret = diag.exec();
    int boxstatus = diag.getCheckBoxState();

    if (ret == 0)
    {
        delete conflictlist;
        return;
    }

    ProgramInfo *prefer = NULL;
    list<ProgramInfo *> *dislike = new list<ProgramInfo *>;
    if (ret == 2)
    {
        prefer = rec;
        for (i = conflictlist->begin(); i != conflictlist->end(); i++)
            dislike->push_back(*i);
    }
    else
    {
        dislike->push_back(rec);
        int counter = 3;
        for (i = conflictlist->begin(); i != conflictlist->end(); i++) 
        {
            if (counter == ret)
                prefer = (*i);
            else
                dislike->push_back(*i);
            counter++;
        }
    }

    if (!prefer)
    {
        printf("Ack, no preferred recording\n");
        delete dislike;
        delete conflictlist;
        return;
    }

    char thequery[512];

    if (boxstatus == 1)
    {
        for (i = dislike->begin(); i != dislike->end(); i++)
        {
            sprintf(thequery, "INSERT INTO conflictresolutionany "
                              "(prefertitle, disliketitle) VALUES "
                              "(\"%s\", \"%s\");", prefer->title.ascii(),
                              (*i)->title.ascii());
            db->exec(thequery);
        }
    } 
    else
    {
        QString pstart = prefer->startts.toString("yyyyMMddhhmm");
        pstart += "00";
        QString pend = prefer->endts.toString("yyyyMMddhhmm");
        pend += "00";

        QString dstart, dend;

        for (i = dislike->begin(); i != dislike->end(); i++)
        {
            dstart = (*i)->startts.toString("yyyyMMddhhmm");
            dstart += "00";
            dend = (*i)->endts.toString("yyyyMMddhhmm");
            dend += "00";

            sprintf(thequery, "INSERT INTO conflictresolutionsingle "
                              "(preferchannum, preferstarttime, "
                              "preferendtime, dislikechannum, "
                              "dislikestarttime, dislikeendtime) VALUES "
                              "(%d, %s, %s, %d, %s, %s);", 
                              prefer->channum.toInt(), pstart.ascii(),
                              pend.ascii(), (*i)->channum.toInt(),
                              dstart.ascii(), dend.ascii());

            db->exec(thequery);
        }
    }  

    QString dstart, dend;
    for (i = dislike->begin(); i != dislike->end(); i++)
    {
        dstart = (*i)->startts.toString("yyyyMMddhhmm");
        dstart += "00";
        dend = (*i)->endts.toString("yyyyMMddhhmm");
        dend += "00";

        sprintf(thequery, "DELETE FROM conflictresolutionoverride WHERE "
                          "channum = %d AND starttime = %s AND "
                          "endtime = %s;",
                          (*i)->channum.toInt(), dstart.ascii(), dend.ascii());

        db->exec(thequery);
    }

    delete dislike;
    delete conflictlist;

    sprintf(thequery, "UPDATE settings SET data = \"yes\" WHERE "
                      "value = \"RecordChanged\";");

    db->exec(thequery);

    FillList();
}
