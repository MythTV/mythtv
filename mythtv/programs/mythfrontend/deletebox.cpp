#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qprogressbar.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "deletebox.h"
#include "infostructs.h"
#include "tv.h"
#include "dialogbox.h"

class ProgramListItem : public QListViewItem
{
  public:
    ProgramListItem(QListView *parent, ProgramInfo *lpginfo, 
                    RecordingInfo *lrecinfo, QString prefix);
   ~ProgramListItem() { delete pginfo; delete recinfo; }
  
    ProgramInfo *getProgramInfo() { return pginfo; }
    RecordingInfo *getRecordingInfo() { return recinfo; }
 
  protected:
    ProgramInfo *pginfo;
    RecordingInfo *recinfo;
};

ProgramListItem::ProgramListItem(QListView *parent, ProgramInfo *lpginfo,
                                 RecordingInfo *lrecinfo, QString prefix)
               : QListViewItem(parent)
{
    pginfo = lpginfo;
    recinfo = lrecinfo;
    setText(0, pginfo->channum);
    setText(1, pginfo->startts.toString("MMMM d h:mm AP"));
    setText(2, pginfo->title);

    string filename;

    recinfo->GetRecordFilename(prefix.ascii(), filename);

    struct stat64 st;

    long long size = 0;
    if (stat64(filename.c_str(), &st) == 0)
        size = st.st_size;
    long int mbytes = size / 1024 / 1024;
    QString filesize = QString("%1 MB").arg(mbytes);

    setText(3, filesize);
}

DeleteBox::DeleteBox(QString prefix, TV *ltv, QSqlDatabase *ldb, 
                     QWidget *parent, const char *name)
         : QDialog(parent, name)
{
    fileprefix = prefix;
    tv = ltv;
    db = ldb;

    title = NULL;

    setGeometry(0, 0, 800, 600);
    setFixedWidth(800);
    setFixedHeight(600);

    setFont(QFont("Arial", 16, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, 15);

    QLabel *label = new QLabel("Select a recording to permanantly delete:", this);
    vbox->addWidget(label);

    listview = new QListView(this);
    listview->addColumn("#");
    listview->addColumn("Date");
    listview->addColumn("Title");
    listview->addColumn("Size");
 
    listview->setColumnWidth(0, 40);
    listview->setColumnWidth(1, 190); 
    listview->setColumnWidth(2, 435);
    listview->setColumnWidth(3, 90);

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

            item = new ProgramListItem(listview, proginfo, tvrec, fileprefix); 
        }
    }
    else
    {
        // TODO: no recordings
    }
   
    listview->setFixedHeight(225);

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

    freespace = new QLabel(" ", this);
    vbox->addWidget(freespace);
    
    progressbar = new QProgressBar(this);
    UpdateProgressBar();
    vbox->addWidget(progressbar);

    listview->setCurrentItem(listview->firstChild());
}

void DeleteBox::Show()
{
    showFullScreen();
}

void DeleteBox::changed(QListViewItem *lvitem)
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

void DeleteBox::selected(QListViewItem *lvitem)
{
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();
    RecordingInfo *recinfo = pgitem->getRecordingInfo();

    QString message = "Are you sure you want to delete:<br><br>";
    message += rec->title;
    message += "<br>";
    
    QDateTime startts = rec->startts;
    QDateTime endts = rec->endts;

    QString timedate = endts.date().toString("ddd MMMM d") + QString(", ") +
                       startts.time().toString("h:mm AP") + QString(" - ") +
                       endts.time().toString("h:mm AP");

    message += timedate;
    message += "<br>";
    if (rec->subtitle != "(null)")
        message += rec->subtitle;
    message += "<br>";
    if (rec->description != "(null)")
        message += rec->description;

    message += "<br><br>It will be gone forever.";

    DialogBox *diag = new DialogBox(message);

    diag->AddButton("Yes, get rid of it");
    diag->AddButton("No, keep it, I changed my mind");

    diag->Show();

    int result = diag->exec();

    if (result == 0)
    {
        string filename;
        recinfo->GetRecordFilename(fileprefix.ascii(), filename);

        QSqlQuery query;
        char thequery[512];

        QString startts = rec->startts.toString("yyyyMMddhhmm");
        startts += "00";
        QString endts = rec->endts.toString("yyyyMMddhhmm");
        endts += "00";

        sprintf(thequery, "DELETE FROM recorded WHERE channum = %s AND "
                          "title = \"%s\" AND starttime = %s AND endtime = %s;",
                          rec->channum.ascii(), rec->title.ascii(), 
                          startts.ascii(), endts.ascii());

        query = db->exec(thequery);

        unlink(filename.c_str());

        if (lvitem->itemAbove())
            listview->setCurrentItem(lvitem->itemAbove());
        else 
            listview->setCurrentItem(listview->firstChild());
        delete lvitem;
        UpdateProgressBar();
    }    
}

void DeleteBox::GetFreeSpaceStats(int &totalspace, int &usedspace)
{
    QString command;
    command.sprintf("df -k -P %s", fileprefix.ascii());

    FILE *file = popen(command.ascii(), "r");

    if (!file)
    {
        totalspace = -1;
        usedspace = -1;
    }
    else
    {
        char buffer[1024];
        fgets(buffer, 1024, file);
        fgets(buffer, 1024, file);

        char dummy[1024];
        int dummyi;
        sscanf(buffer, "%s %d %d %d %s %s\n", dummy, &totalspace, &usedspace,
               &dummyi, dummy, dummy);

        totalspace /= 1000;
        usedspace /= 1000; 
        pclose(file);
    }
}
 
void DeleteBox::UpdateProgressBar(void)
{
    int total, used;
    GetFreeSpaceStats(total, used);

    QString usestr;
    char text[128];
    sprintf(text, "Storage: %d,%03d MB used out of %d,%03d MB total", 
            used / 1000, used % 1000, 
            total / 1000, total % 1000);

    usestr = text;

    freespace->setText(usestr);
    progressbar->setTotalSteps(total);
    progressbar->setProgress(used);
}
