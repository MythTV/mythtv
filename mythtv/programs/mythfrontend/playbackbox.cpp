#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qprogressbar.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <qheader.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "playbackbox.h"
#include "tv.h"
#include "programlistitem.h"
#include "NuppelVideoPlayer.h"
#include "yuv2rgb.h"

#include "libmyth/mythcontext.h"
#include "libmyth/dialogbox.h"
#include "libmyth/programinfo.h"

PlaybackBox::PlaybackBox(MythContext *context, BoxType ltype, QWidget *parent, 
                         const char *name)
           : QDialog(parent, name)
{
    type = ltype;
    m_context = context;

    title = NULL;

    int screenheight = 0, screenwidth = 0;
    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(context->GetMediumFontSize() * hmult), 
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    context->ThemeWidget(this);
    
    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(15 * wmult));

    QString message = "Select a recording to ";
    if (type == Delete)
        message += "permanantly delete:";
    else
        message += "view:";
    QLabel *label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(label);

    listview = new MyListView(this);

    listview->addColumn("Date");
    listview->addColumn("Title");
    if (type == Delete)
        listview->addColumn("Size");

    if (type == Delete)
    {
        listview->setColumnWidth(0, (int)(200 * wmult)); 
        listview->setColumnWidth(1, (int)(455 * wmult));
        listview->setColumnWidth(2, (int)(90 * wmult));
    }
    else
    {
        listview->setColumnWidth(0, (int)(220 * wmult)); 
        listview->setColumnWidth(1, (int)(520 * wmult));
    }

    listview->setSorting(-1, false);
    listview->setAllColumnsShowFocus(TRUE);

    listview->viewport()->setPalette(palette());
    listview->horizontalScrollBar()->setPalette(palette());
    listview->verticalScrollBar()->setPalette(palette());
    listview->header()->setPalette(palette());
    listview->header()->setFont(font());

    connect(listview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *))); 
    connect(listview, SIGNAL(selectionChanged(QListViewItem *)), this,
            SLOT(changed(QListViewItem *)));
    connect(listview, SIGNAL(deletePressed(QListViewItem *)), this,
            SLOT(remove(QListViewItem *)));
    connect(listview, SIGNAL(playPressed(QListViewItem *)), this,
            SLOT(play(QListViewItem *))); 

    vbox->addWidget(listview, 1);

    ProgramListItem *item = NULL;
  
    vector<ProgramInfo *> *infoList = context->GetRecordedList(type == Delete);
    if (infoList)
    {
        vector<ProgramInfo *>::iterator i = infoList->begin();
        for (; i != infoList->end(); i++)
        {
            item = new ProgramListItem(context, listview, (*i), type == Delete);
        }
        delete infoList;
    }

    if (type == Delete)
        listview->setFixedHeight((int)(225 * hmult));
    else 
        listview->setFixedHeight((int)(300 * hmult));

    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * wmult));

    QGridLayout *grid = new QGridLayout(hbox, 5, 2, 1);
 
    title = new QLabel(" ", this);
    title->setBackgroundOrigin(WindowOrigin);
    title->setFont(QFont("Arial", (int)(context->GetBigFontSize() * hmult), 
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

    if (context->GetNumSetting("GeneratePreviewPixmap") == 1 ||
        context->GetNumSetting("PlaybackPreview") == 1)
    {
        QPixmap temp((int)(160 * wmult), (int)(120 * hmult));
        temp.fill(black);

        pixlabel = new QLabel(this);
        pixlabel->setBackgroundOrigin(WindowOrigin);
        pixlabel->setPixmap(temp);

        hbox->addWidget(pixlabel);
    }
    else
        pixlabel = NULL;

    if (type == Delete) 
    {
        freespace = new QLabel(" ", this);
        freespace->setBackgroundOrigin(WindowOrigin);
        vbox->addWidget(freespace);

        progressbar = new QProgressBar(this);
        progressbar->setBackgroundMode(PaletteBase);
        UpdateProgressBar();
        vbox->addWidget(progressbar);
    }
    else
    {
        freespace = NULL;
        progressbar = NULL;
    }

    nvp = NULL;
    timer = new QTimer(this);

    qApp->processEvents();
    descwidth = screenwidth - desclabel->width() - pixlabel->width() -
                4 * (int)(10 * wmult);

    if (item)
    {
        listview->setCurrentItem(item);
        listview->setSelected(item, true);
    }

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
    rbuffer = new RingBuffer(m_context, rec->pathname, false);

    nvp = new NuppelVideoPlayer();
    nvp->SetRingBuffer(rbuffer);
    nvp->SetAsPIP();
    nvp->SetOSDFontName(m_context->GetSetting("OSDFont"), 
                        m_context->GetInstallPrefix());
    QString filters = "";
    nvp->SetVideoFilters(filters);
 
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

    if (!title)
        return;

    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    if (!pgitem)
    {
        title->setText("");
        date->setText("");
        chan->setText("");
        subtitle->setText("");
        description->setText("");
        if (pixlabel)
            pixlabel->setPixmap(QPixmap(0, 0));
        return;
    }
   
    ProgramInfo *rec = pgitem->getProgramInfo();

    if (m_context->GetNumSetting("PlaybackPreview") == 1)
        startPlayer(rec);

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
    if (m_context->GetNumSetting("DisplayChanNum") == 0)
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

    subtitle->setMinimumWidth(descwidth);
    description->setMinimumWidth(descwidth);
    description->setMaximumHeight((int)(80 * hmult));

    QPixmap *pix = pgitem->getPixmap();

    if (pixlabel && pix)
        pixlabel->setPixmap(*pix);

    timer->start(1000 / 30);
}

void PlaybackBox::selected(QListViewItem *lvitem)
{
    if (!lvitem)
        return;

    switch (type) 
    {
        case Play: play(lvitem); break;
        case Delete: remove(lvitem); break;
    }
}

void PlaybackBox::play(QListViewItem *lvitem)
{
    killPlayer();

    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();

    ProgramInfo *tvrec = new ProgramInfo(*rec);

    TV *tv = new TV(m_context);
    tv->Init();
    tv->Playback(tvrec);

    while (tv->IsPlaying() || tv->ChangingState())
        usleep(50);

    delete tv;

    startPlayer(rec);
    timer->start(1000 / 30);
}

void PlaybackBox::remove(QListViewItem *lvitem)
{
    killPlayer();
       
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();

    QString message = "Are you sure you want to delete:<br><br>";
    message += rec->title;
    message += "<br>";
    
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

    message += timedate;
    message += "<br>";
    if (rec->subtitle != "(null)")
        message += rec->subtitle;
    message += "<br>";
    if (rec->description != "(null)")
        message += rec->description;

    message += "<br><br>It will be gone forever.";

    DialogBox diag(m_context, message);

    diag.AddButton("Yes, get rid of it");
    diag.AddButton("No, keep it, I changed my mind");

    diag.Show();

    int result = diag.exec();

    if (result == 1)
    {
        m_context->DeleteRecording(rec);

        if (lvitem->itemBelow())
        {
            listview->setCurrentItem(lvitem->itemBelow());
            listview->setSelected(lvitem->itemBelow(), true);
        }
        else if (lvitem->itemAbove())
        {
            listview->setCurrentItem(lvitem->itemAbove());
            listview->setSelected(lvitem->itemAbove(), true);
        }
        else
            changed(NULL);

        delete lvitem;
        if (type == Delete)
            UpdateProgressBar();
    }    
    else if (m_context->GetNumSetting("PlaybackPreview") == 1)
        startPlayer(rec);

    setActiveWindow();
    raise();

    timer->start(1000 / 30);
}

void PlaybackBox::GetFreeSpaceStats(int &totalspace, int &usedspace)
{
    m_context->GetFreeSpace(totalspace, usedspace);
}
 
void PlaybackBox::UpdateProgressBar(void)
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

void PlaybackBox::timeout(void)
{
    if (!nvp || !pixlabel)
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
