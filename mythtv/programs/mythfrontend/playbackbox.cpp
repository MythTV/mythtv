#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qapplication.h>

#include "playbackbox.h"
#include "infostructs.h"
#include "programinfo.h"
#include "tv.h"
#include "programlistitem.h"
#include "settings.h"

extern Settings *globalsettings;


PlaybackBox::PlaybackBox(QString prefix, TV *ltv, QSqlDatabase *ldb, 
                         QWidget *parent, const char *name)
           : QDialog(parent, name)
{
    tv = ltv;
    db = ldb;
    fileprefix = prefix;

    title = NULL;

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", 16 * hmult, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, 20 * wmult);

    QLabel *label = new QLabel("Select a recording to view:", this);
    vbox->addWidget(label);

    QListView *listview = new QListView(this);
    listview->addColumn("#");
    listview->addColumn("Date");
    listview->addColumn("Title");
 
    listview->setColumnWidth(0, 40 * wmult);
    listview->setColumnWidth(1, 210 * wmult); 
    listview->setColumnWidth(2, 500 * wmult);
    listview->setColumnWidthMode(0, QListView::Manual);
    listview->setColumnWidthMode(1, QListView::Manual);

    listview->setSorting(1, false);
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
    
    sprintf(thequery, "SELECT channum,starttime,endtime,title,subtitle,"
                      "description FROM recorded;");

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
            proginfo->conflicting = false;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";

            item = new ProgramListItem(listview, proginfo, 3, tv, fileprefix); 
        }
    }
    else
    {
        // TODO: no recordings
    }
   
    listview->setFixedHeight(300 * hmult);

    QHBoxLayout *hbox = new QHBoxLayout(vbox, 10 * wmult);

    QGridLayout *grid = new QGridLayout(hbox, 4, 2, 1);
 
    title = new QLabel(" ", this);
    title->setFont(QFont("Arial", 25, QFont::Bold));

    QLabel *datelabel = new QLabel("Airdate: ", this);
    date = new QLabel(" ", this);

    QLabel *sublabel = new QLabel("Episode: ", this);
    subtitle = new QLabel(" ", this);
    subtitle->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);
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

    QPixmap temp(160 * wmult, 120 * hmult);

    pixlabel = new QLabel(this);
    pixlabel->setPixmap(temp);

    hbox->addWidget(pixlabel);

    listview->setCurrentItem(listview->firstChild());
}

void PlaybackBox::Show()
{
    showFullScreen();
    raise();
    setActiveWindow();
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

    QPixmap *pix = pgitem->getPixmap();

    if (pix)
        pixlabel->setPixmap(*pix);
}

void PlaybackBox::selected(QListViewItem *lvitem)
{
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();

    ProgramInfo *tvrec = new ProgramInfo(*rec);
    tv->Playback(tvrec);
}
