#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qprogressbar.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <unistd.h>

#include <unistd.h>
#include <stdio.h>

#include "deletebox.h"
#include "tv.h"
#include "programlistitem.h"
#include "NuppelVideoPlayer.h"
#include "yuv2rgb.h"

#include "libmyth/programinfo.h"
#include "libmyth/settings.h"
#include "libmyth/dialogbox.h"

extern Settings *globalsettings;
extern char installprefix[];

DeleteBox::DeleteBox(QString prefix, TV *ltv, QSqlDatabase *ldb, 
                     QWidget *parent, const char *name)
         : QDialog(parent, name)
{
    fileprefix = prefix;
    tv = ltv;
    db = ldb;

    title = NULL;

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    wmult = screenwidth / 800.0;
    hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedWidth(screenwidth);
    setFixedHeight(screenheight);

    setFont(QFont("Arial", (int)(16 * hmult), QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(15 * wmult));

    QLabel *label = new QLabel("Select a recording to permanantly delete:", 
                               this);
    vbox->addWidget(label);

    listview = new MyListView(this);
    listview->addColumn("#");
    listview->addColumn("Date");
    listview->addColumn("Title");
    listview->addColumn("Size");
 
    listview->setColumnWidth(0, (int)(40 * wmult));
    listview->setColumnWidth(1, (int)(185 * wmult)); 
    listview->setColumnWidth(2, (int)(435 * wmult));
    listview->setColumnWidth(3, (int)(90 * wmult));

    listview->setSorting(-1, false);
    listview->setAllColumnsShowFocus(TRUE);

    connect(listview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *))); 
    connect(listview, SIGNAL(selectionChanged(QListViewItem *)), this,
            SLOT(changed(QListViewItem *)));

    vbox->addWidget(listview, 1);

    QSqlQuery query;
    QString thequery;
    ProgramListItem *item;
    
    thequery = "SELECT channel.chanid,starttime,endtime,title,subtitle,"
               "description,channel.channum FROM recorded,channel "
               "WHERE recorded.chanid = channel.chanid "
               "ORDER BY starttime DESC;";

    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;
            QString chanstr;
 
            proginfo->chanid = query.value(0).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->title = query.value(3).toString();
            proginfo->subtitle = query.value(4).toString();
            proginfo->description = query.value(5).toString();
            proginfo->chanstr = query.value(6).toString();
            proginfo->conflicting = false;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";

            item = new ProgramListItem(listview, proginfo, 4, tv, fileprefix);
        }
    }
    else
    {
        // TODO: no recordings
    }
   
    listview->setFixedHeight((int)(225 * hmult));

    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    QGridLayout *grid = new QGridLayout(hbox, 4, 2, 1);
    
    title = new QLabel(" ", this);
    title->setFont(QFont("Arial", (int)(25 * hmult), QFont::Bold));

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

    QPixmap temp((int)(160 * wmult), (int)(120 * hmult));
    
    pixlabel = new QLabel(this);
    pixlabel->setPixmap(temp);
    
    hbox->addWidget(pixlabel); 

    freespace = new QLabel(" ", this);
    vbox->addWidget(freespace);
    
    progressbar = new QProgressBar(this);
    UpdateProgressBar();
    vbox->addWidget(progressbar);

    nvp = NULL;
    timer = new QTimer(this);
    
    listview->setCurrentItem(listview->firstChild());

    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start(1000 / 30);
}

DeleteBox::~DeleteBox(void)
{
    killPlayer();
    delete timer;
}

void DeleteBox::Show()
{
    showFullScreen();
    setActiveWindow();
}


static void *SpawnDecoder(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    nvp->StartPlaying();
    return NULL;
}

void DeleteBox::killPlayer(void)
{
    timer->stop();
    while (timer->isActive())
        usleep(50);

    if (nvp)
    {
        nvp->StopPlaying();
        pthread_join(decoder, NULL);
        delete nvp;
        delete rbuffer;

        nvp = NULL;
        rbuffer = NULL;
    }
}

void DeleteBox::startPlayer(ProgramInfo *rec)
{
    rbuffer = new RingBuffer(rec->GetRecordFilename(fileprefix), false);

    nvp = new NuppelVideoPlayer();
    nvp->SetRingBuffer(rbuffer);
    nvp->SetAsPIP();
    nvp->SetOSDFontName(globalsettings->GetSetting("OSDFont"),
                        installprefix);

    pthread_create(&decoder, NULL, SpawnDecoder, nvp);

    QTime curtime = QTime::currentTime();
    curtime.addSecs(1);
    while (!nvp->IsPlaying())
    {
         if (QTime::currentTime() > curtime)
         {
             break;
         }
         usleep(50);
    }
}

void DeleteBox::changed(QListViewItem *lvitem)
{
    killPlayer();
	
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    if (!pgitem)
        return;
   
    if (!title)
        return;

    ProgramInfo *rec = pgitem->getProgramInfo();
   
    startPlayer(rec);

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

    timer->start(1000 / 30);
}

void DeleteBox::selected(QListViewItem *lvitem)
{
    killPlayer();
	
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();

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

    DialogBox diag(message);

    diag.AddButton("Yes, get rid of it");
    diag.AddButton("No, keep it, I changed my mind");

    diag.Show();

    int result = diag.exec();

    if (result == 1)
    {
        QString filename = rec->GetRecordFilename(fileprefix);

        QSqlQuery query;
        QString thequery;

        QString startts = rec->startts.toString("yyyyMMddhhmm");
        startts += "00";
        QString endts = rec->endts.toString("yyyyMMddhhmm");
        endts += "00";

        thequery = QString("DELETE FROM recorded WHERE chanid = %1 AND title "
                           "= \"%2\" AND starttime = %3 AND endtime = %4;")
                           .arg(rec->chanid).arg(rec->title).arg(startts)
                           .arg(endts);

        query = db->exec(thequery);

        unlink(filename.ascii());

        filename += ".png";

        unlink(filename.ascii());

        if (lvitem->itemAbove())
            listview->setCurrentItem(lvitem->itemAbove());
        else 
            listview->setCurrentItem(listview->firstChild());
        delete lvitem;
        UpdateProgressBar();
    }    
    else
        startPlayer(rec);

    setActiveWindow();
    raise();

    timer->start(1000 / 30);
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

void DeleteBox::timeout(void)
{
    if (!nvp)
        return;

    int w = 0, h = 0;
    unsigned char *buf = nvp->GetCurrentFrame(w, h);

    if (w == 0 || h == 0 || !buf)
        return;

    unsigned char *outputbuf = new unsigned char[w * h * 4];
    yuv2rgb_fun convert = yuv2rgb_init_mmx(32, MODE_RGB);

    convert(outputbuf, buf, buf + (w * h), buf + (w * h * 5 / 4), w, h);

    QImage img(outputbuf, w, h, 32, NULL, 65536 * 65536, QImage::LittleEndian);
    img = img.scale((int)(160 * wmult), (int)(120 * hmult));

    delete [] outputbuf;

    QPixmap *pmap = pixlabel->pixmap();
    QPainter p(pmap);

    p.drawImage(0, 0, img);
    p.end();

    bitBlt(pixlabel, 0, 
           (int)((pixlabel->contentsRect().height() - 120 * hmult) / 2), 
           pmap);
}

