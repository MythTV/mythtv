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
#include <qheader.h>

#include <iostream>
using namespace std;

#include "viewscheduled.h"
#include "tv.h"
#include "programlistitem.h"
#include "setrectime.h"

#include "libmyth/dialogbox.h"
#include "libmyth/mythcontext.h"
#include "libmythtv/remoteutil.h"
    
ViewScheduled::ViewScheduled(MythContext *context, QSqlDatabase *ldb,
                             QWidget *parent, const char *name)
             : MythDialog(context, parent, name)
{
    db = ldb;

    title = NULL;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(10 * wmult));

    desclabel = new QLabel("Select a recording to view:", this);
    desclabel->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(desclabel);

    listview = new MythListView(this);
    listview->addColumn("Chan");
    listview->addColumn("Date");
    listview->addColumn("Title");
    //listview->addColumn("Quality");
 
//     listview->setColumnWidth(0, (int)(100 * wmult));
//     listview->setColumnWidth(1, (int)(210 * wmult)); 
//     listview->setColumnWidth(2, (int)(350 * wmult));
//     listview->setColumnWidth(3, (int)(90 * wmult));
    listview->setColumnWidth(0, (int)(80 * wmult));
    listview->setColumnWidth(1, (int)(210 * wmult)); 
    listview->setColumnWidth(2, (int)(460 * wmult));

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

    listview->setFixedHeight((int)(250 * hmult));

    QLabel *key = new QLabel("Conflicting recordings are highlighted in "
                             "<font color=\"red\">red</font>.<br>Deactivated "
                             "recordings are highlighted in <font "
                             "color=\"gray\">gray</font>.", this);
    key->setFont(QFont("Arial", (int)(m_context->GetSmallFontSize() * hmult), 
                 QFont::Bold));
    key->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(key);

    QFrame *f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);     

    QGridLayout *grid = new QGridLayout(vbox, 5, 2, 1);
    
    title = new QLabel(" ", this);
    title->setBackgroundOrigin(WindowOrigin);
    title->setFont(QFont("Arial", (int)(m_context->GetBigFontSize() * hmult), 
                   QFont::Bold));

    QLabel *datelabel = new QLabel("Airdate: ", this);
    datelabel->setBackgroundOrigin(WindowOrigin);
    date = new QLabel(" ", this);
    date->setBackgroundOrigin(WindowOrigin);

    QLabel *chanlabel = new QLabel("Channel: ", this);
    chanlabel->setBackgroundOrigin(WindowOrigin);
    chan = new QLabel(" ", this);
    chan->setBackgroundOrigin(WindowOrigin);

    QLabel *sublabel = new QLabel("Episode: ", this);
    sublabel->setBackgroundOrigin(WindowOrigin);
    subtitle = new QLabel(" ", this);
    subtitle->setBackgroundOrigin(WindowOrigin);
    subtitle->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    QLabel *desclabel = new QLabel("Description: ", this);
    desclabel->setBackgroundOrigin(WindowOrigin);
    description = new QLabel(" ", this);
    description->setBackgroundOrigin(WindowOrigin);
    description->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);
 
    grid->addMultiCellWidget(title, 0, 0, 0, 1, Qt::AlignLeft);
    grid->addWidget(datelabel, 1, 0, Qt::AlignLeft);
    grid->addWidget(date, 1, 1, Qt::AlignLeft);
    grid->addWidget(chanlabel, 2, 0, Qt::AlignLeft);
    grid->addWidget(chan, 2, 1, Qt::AlignLeft);
    grid->addWidget(sublabel, 3, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(subtitle, 3, 1, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(desclabel, 4, 0, Qt::AlignLeft | Qt::AlignTop);   
    grid->addWidget(description, 4, 1, Qt::AlignLeft | Qt::AlignTop);
 
    grid->setColStretch(1, 1);
    grid->setRowStretch(4, 1);

    FillList();
}

void ViewScheduled::FillList(void)
{
    listview->clear();

    bool conflicts = false;
    vector<ProgramInfo *> recordinglist;

    conflicts = RemoteGetAllPendingRecordings(m_context, recordinglist);

    vector<ProgramInfo *>::reverse_iterator pgiter = recordinglist.rbegin();

    for (; pgiter != recordinglist.rend(); pgiter++)
        new ProgramListItem(m_context, listview, (*pgiter), 2);

    if (conflicts)
        desclabel->setText("You have time conflicts.");
    else
        desclabel->setText("You have no recording conflicts.");

    listview->setCurrentItem(listview->firstChild());
    listview->setSelected(listview->firstChild(), true);
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

    QString dateformat = m_context->GetSetting("DateFormat");
    if (dateformat == "")
        dateformat = "ddd MMMM d";
    QString timeformat = m_context->GetSetting("TimeFormat");
    if (timeformat == "")
        timeformat = "h:mm AP";
        
    QString timedate = endts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);
        
    date->setText(timedate);

    QString chantext;
    if (m_context->GetNumSetting("DisplayChanNum") != 0)
        chantext = rec->channame + " [" + rec->chansign + "]";
    else
        chantext = rec->chanstr;
    chan->setText(chantext);

    title->setText(rec->title);
    if (rec->subtitle != "(null)")
        subtitle->setText(rec->subtitle);
    else
        subtitle->setText("");
    if (rec->description != "(null)")
        description->setText(rec->description);
    else
        description->setText("");

    int minwidth = (int)(500 * wmult);

    title->setMinimumWidth(minwidth);
    title->setMaximumWidth(minwidth);

    subtitle->setMinimumWidth(minwidth);
    subtitle->setMaximumWidth(minwidth);

    description->setMinimumWidth(minwidth);
    description->setMaximumWidth(minwidth);
}

void ViewScheduled::selected(QListViewItem *lvitem)
{
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();

    m_context->KickDatabase(db);

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
    QString message = "Recording this program has been deactivated becuase it "
                      "conflicts with another scheduled recording.  Do you "
                      "want to re-enable this recording?";

    DialogBox diag(m_context, message);

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

    QString thequery;
    thequery = QString("INSERT INTO conflictresolutionoverride (chanid,"
                       "starttime, endtime) VALUES (%1, %2, %3);")
                       .arg(rec->chanid).arg(pstart).arg(pend);

    QSqlQuery qquery = db->exec(thequery);
    if (!qquery.isActive())
    {
        cerr << "DB Error: conflict resolution insertion failed, SQL query "
             << "was:" << endl;
        cerr << thequery << endl;
    }

    vector<ProgramInfo *> *conflictlist = RemoteGetConflictList(m_context, rec, 
                                                                     false);

    QString dstart, dend;
    vector<ProgramInfo *>::iterator i;
    for (i = conflictlist->begin(); i != conflictlist->end(); i++)
    {
        dstart = (*i)->startts.toString("yyyyMMddhhmm");
        dstart += "00";
        dend = (*i)->endts.toString("yyyyMMddhhmm");
        dend += "00";

        thequery = QString("DELETE FROM conflictresolutionoverride WHERE "
                           "chanid = %1 AND starttime = %2 AND "
                           "endtime = %3;")
                           .arg((*i)->chanid).arg(dstart).arg(dend);

        qquery = db->exec(thequery);
        if (!qquery.isActive())
        {
            cerr << "DB Error: conflict resolution deletion failed, SQL query "
                 << "was:" << endl;
            cerr << thequery << endl;
        }
    }

    vector<ProgramInfo *>::iterator iter = conflictlist->begin();
    for (; iter != conflictlist->end(); iter++)
    {
        delete (*iter);
    }
    delete conflictlist;

    thequery = "UPDATE settings SET data = \"yes\" WHERE value = "
               "\"RecordChanged\";";
    db->exec(thequery);

    FillList();
}

void ViewScheduled::handleConflicting(ProgramInfo *rec)
{
    DialogBox diag(m_context, "How do you want to resolve this conflict?");

    diag.AddButton("Adjust this program's recording time");
    diag.AddButton("Record only one program in this timeslot");

    diag.Show();
    int ret = diag.exec();

    if (ret == 1)
    {
        SetRecTimeDialog srt(m_context,rec,db);
        srt.Show();
        srt.exec();
    }
    else
    {
        chooseConflictingProgram(rec);
    }
}

void ViewScheduled::chooseConflictingProgram(ProgramInfo *rec)
{
    vector<ProgramInfo *> *conflictlist = RemoteGetConflictList(m_context, rec,
                                                                true);

    QString dateformat = m_context->GetSetting("DateFormat");
    if (dateformat == "")
        dateformat = "ddd MMMM d";
    QString timeformat = m_context->GetSetting("TimeFormat");
    if (timeformat == "")
        timeformat = "h:mm AP";

    QString message = "The follow scheduled recordings conflict with each "
                      "other.  Which would you like to record?";

    DialogBox diag(m_context, message, "Remember this choice and use it "
                   "automatically in the future");
 
    QString button; 
    button = rec->title + QString("\n");
    button += rec->startts.toString(dateformat + " " + timeformat);
    if (m_context->GetNumSetting("DisplayChanNum") == 0)
        button += " on " + rec->channame + " [" + rec->chansign + "]";
    else
        button += QString(" on channel ") + rec->chanstr;

    diag.AddButton(button);

    vector<ProgramInfo *>::iterator i = conflictlist->begin();
    for (; i != conflictlist->end(); i++)
    {
        ProgramInfo *info = (*i);

        button = info->title + QString("\n");
        button += info->startts.toString(dateformat + " " + timeformat);
        if (m_context->GetNumSetting("DisplayChanNum") == 0)
            button += " on " + info->channame + " [" + info->chansign + "]";
        else
            button += QString(" on channel ") + info->chanstr;

        diag.AddButton(button);
    }

    diag.Show();
    int ret = diag.exec();
    int boxstatus = diag.getCheckBoxState();

    if (ret == 0)
    {
        vector<ProgramInfo *>::iterator iter = conflictlist->begin();
        for (; iter != conflictlist->end(); iter++)
        {
            delete (*iter);
        }

        delete conflictlist;
        return;
    }

    ProgramInfo *prefer = NULL;
    vector<ProgramInfo *> *dislike = new vector<ProgramInfo *>;
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
        vector<ProgramInfo *>::iterator iter = conflictlist->begin();
        for (; iter != conflictlist->end(); iter++)
        {
            delete (*iter);
        }

        delete conflictlist;
        return;
    }

    QString thequery;

    if (boxstatus == 1)
    {
        for (i = dislike->begin(); i != dislike->end(); i++)
        {
            QString sqltitle1 = prefer->title;
            QString sqltitle2 = (*i)->title;

            sqltitle1.replace(QRegExp("\""), QString("\\\""));
            sqltitle2.replace(QRegExp("\""), QString("\\\""));

            thequery = QString("INSERT INTO conflictresolutionany "
                               "(prefertitle, disliketitle) VALUES "
                               "(\"%1\", \"%2\");").arg(sqltitle1)
                               .arg(sqltitle2);
            QSqlQuery qquery = db->exec(thequery);
            if (!qquery.isActive())
            {
                cerr << "DB Error: conflict resolution insertion failed, SQL "
                     << "query was:" << endl;
                cerr << thequery << endl;
            }
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

            thequery = QString("INSERT INTO conflictresolutionsingle "
                               "(preferchanid, preferstarttime, "
                               "preferendtime, dislikechanid, "
                               "dislikestarttime, dislikeendtime) VALUES "
                               "(%1, %2, %3, %4, %5, %6);") 
                               .arg(prefer->chanid).arg(pstart).arg(pend)
                               .arg((*i)->chanid).arg(dstart).arg(dend);

            QSqlQuery qquery = db->exec(thequery);
            if (!qquery.isActive())
            {
                cerr << "DB Error: conflict resolution insertion failed, SQL "
                     << "query was:" << endl;
                cerr << thequery << endl;
            }
        }
    }  

    QString dstart, dend;
    for (i = dislike->begin(); i != dislike->end(); i++)
    {
        dstart = (*i)->startts.toString("yyyyMMddhhmm");
        dstart += "00";
        dend = (*i)->endts.toString("yyyyMMddhhmm");
        dend += "00";

        thequery = QString("DELETE FROM conflictresolutionoverride WHERE "
                           "chanid = %1 AND starttime = %2 AND "
                           "endtime = %3;").arg((*i)->chanid).arg(dstart)
                           .arg(dend);

        QSqlQuery qquery = db->exec(thequery);
        if (!qquery.isActive())
        {
            cerr << "DB Error: conflict resolution deletion failed, SQL query "
                 << "was:" << endl;
            cerr << thequery << endl;
        }
    }

    delete dislike;
    vector<ProgramInfo *>::iterator iter = conflictlist->begin();
    for (; iter != conflictlist->end(); iter++)
    {
        delete (*iter);
    }
    delete conflictlist;

    thequery = "UPDATE settings SET data = \"yes\" WHERE value = "
               "\"RecordChanged\";";

    db->exec(thequery);

    FillList();
}
