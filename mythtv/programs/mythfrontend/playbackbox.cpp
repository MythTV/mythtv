#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <unistd.h>

#include "playbackbox.h"
#include "tv.h"
#include "programlistitem.h"
#include "NuppelVideoPlayer.h"
#include "yuv2rgb.h"

#include "libmyth/settings.h"
#include "libmyth/programinfo.h"

extern Settings *globalsettings;
extern char installprefix[];

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

    wmult = screenwidth / 800.0;
    hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(16 * hmult), QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    QLabel *label = new QLabel("Select a recording to view:", this);
    vbox->addWidget(label);

    QListView *listview = new QListView(this);

    listview->addColumn("Date");
    listview->addColumn("Title");
 
    listview->setColumnWidth(0, (int)(220 * wmult)); 
    listview->setColumnWidth(1, (int)(520 * wmult));
    listview->setColumnWidthMode(0, QListView::Manual);
    listview->setColumnWidthMode(1, QListView::Manual);

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
   
    thequery = "SELECT chanid,starttime,endtime,title,subtitle,description "
               "FROM recorded ORDER BY starttime;";
 
    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;
 
            proginfo->chanid = query.value(0).toString();
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

            QSqlQuery subquery;
            QString subquerystr;

            subquerystr = QString("SELECT channum FROM channel WHERE "
                                  "chanid = %1").arg(proginfo->chanid);
            subquery = db->exec(subquerystr);

            if (subquery.isActive() && subquery.numRowsAffected() > 0)
            {
                subquery.next();
 
                proginfo->chanstr = subquery.value(0).toString();
            }
            else
                proginfo->chanstr = proginfo->chanid;

            item = new ProgramListItem(listview, proginfo, 0, tv, fileprefix);
        }
    }
    else
    {
        // TODO: no recordings
    }
   
    listview->setFixedHeight((int)(300 * hmult));

    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    QGridLayout *grid = new QGridLayout(hbox, 5, 2, 1);
 
    title = new QLabel(" ", this);
    title->setFont(QFont("Arial", (int)(25 * hmult), QFont::Bold));

    QLabel *datelabel = new QLabel("Airdate: ", this);
    date = new QLabel(" ", this);

    QLabel *chanlabel = new QLabel("Channel: ", this);
    chan = new QLabel(" ", this);

    QLabel *sublabel = new QLabel("Episode: ", this);
    subtitle = new QLabel(" ", this);
    subtitle->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);
    QLabel *desclabel = new QLabel("Description: ", this);
    description = new QLabel(" ", this);
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

    if (globalsettings->GetNumSetting("GeneratePreviewPixmap") == 1 ||
        globalsettings->GetNumSetting("PlaybackPreview") == 1)
    {
        QPixmap temp((int)(160 * wmult), (int)(120 * hmult));

        pixlabel = new QLabel(this);
        pixlabel->setPixmap(temp);

        hbox->addWidget(pixlabel);
    }
    else
        pixlabel = NULL;

    nvp = NULL;
    timer = new QTimer(this);

    listview->setCurrentItem(listview->firstChild());

    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start(1000 / 30);
}

PlaybackBox::~PlaybackBox(void)
{
    killPlayer();
    delete timer;
}

void PlaybackBox::Show()
{
    showFullScreen();
    raise();
    setActiveWindow();
}

static void *SpawnDecoder(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    nvp->StartPlaying();
    return NULL;
}

void PlaybackBox::killPlayer(void)
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

void PlaybackBox::startPlayer(ProgramInfo *rec)
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
             break;
         usleep(50);
    }
}

void PlaybackBox::changed(QListViewItem *lvitem)
{
    killPlayer();

    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    if (!pgitem)
        return;
   
    if (!title)
        return;

    ProgramInfo *rec = pgitem->getProgramInfo();

    if (globalsettings->GetNumSetting("PlaybackPreview") == 1)
        startPlayer(rec);

    QDateTime startts = rec->startts;
    QDateTime endts = rec->endts;
       
    QString dateformat = globalsettings->GetSetting("DateFormat");
    if (dateformat == "")
        dateformat = "ddd MMMM d";
    QString timeformat = globalsettings->GetSetting("TimeFormat");
    if (timeformat == "")
        timeformat = "h:mm AP";

    QString timedate = endts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);
        
    date->setText(timedate);

    QString chantext = rec->chanstr;
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

    int width = date->width();
    if (width < (int)(350 * wmult))
        width = (int)(350 * wmult);

    description->setMinimumWidth(width);
    description->setMaximumHeight((int)(80 * hmult));

    QPixmap *pix = pgitem->getPixmap();

    if (pixlabel && pix)
        pixlabel->setPixmap(*pix);

    timer->start(1000 / 30);
}

void PlaybackBox::selected(QListViewItem *lvitem)
{
    killPlayer();

    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();

    ProgramInfo *tvrec = new ProgramInfo(*rec);
    tv->Playback(tvrec);

    sleep(1);

    TVState curstate = tv->GetState();
    while (tv->GetState() == curstate)
        usleep(50);

    startPlayer(rec);
    timer->start(1000 / 30);
}

void PlaybackBox::timeout(void)
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
