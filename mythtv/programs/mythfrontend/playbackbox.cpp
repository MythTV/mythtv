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
#include <qfile.h>


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
#include "libmythtv/programinfo.h"
#include "libmythtv/remoteutil.h"

PlaybackBox::PlaybackBox(MythContext *context, BoxType ltype, QWidget *parent, 
                         const char *name)
           : MythDialog(context, parent, name)
{
    type = ltype;

    title = NULL;
    rbuffer = NULL;
    nvp = NULL;

    ignoreevents = false;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(15 * wmult));

    QString message = "Select a recording to ";
    if (type == Delete)
        message += "permanantly delete:";
    else
        message += "view:";
    QLabel *label = new QLabel(message, this);
    label->setBackgroundOrigin(WindowOrigin);
    vbox->addWidget(label);

    listview = new MythListView(this);

    listview->addColumn("Date");
    listview->addColumn("Title");
    if (type == Delete)
        listview->addColumn("Size");

    if (type == Delete)
    {
        listview->setColumnWidth(0, (int)(200 * wmult)); 
        listview->setColumnWidth(1, (int)(450 * wmult));
        listview->setColumnWidth(2, (int)(90 * wmult));
    }
    else
    {
        listview->setColumnWidth(0, (int)(220 * wmult)); 
        listview->setColumnWidth(1, (int)(520 * wmult));
    }

    listview->setSorting(-1, false);

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
 
    item = (ProgramListItem *)FillList(false); 

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

    playbackPreview = (context->GetNumSetting("PlaybackPreview") != 0);
    generatePreviewPixmap = (context->GetNumSetting("GeneratePreviewPixmaps") != 0);
    displayChanNum = (context->GetNumSetting("DisplayChanNum") != 0);
    dateformat = context->GetSetting("DateFormat", "ddd MMMM d");
    timeformat = context->GetSetting("TimeFormat");

    if (playbackPreview || generatePreviewPixmap)
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
        progressbar->setBackgroundOrigin(WindowOrigin);
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
    descwidth = screenwidth - desclabel->width() - 4 * (int)(10 * wmult);
    if (pixlabel)
        descwidth -= pixlabel->width();
    titlewidth = descwidth + desclabel->width();

    if (item)
    {
        listview->setCurrentItem(item);
        listview->setSelected(item, true);
    }

    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start(1000 / 30);

    m_context->addListener(this);
}

PlaybackBox::~PlaybackBox(void)
{
    m_context->removeListener(this);
    killPlayer();
    delete timer;
}

QListViewItem *PlaybackBox::FillList(bool selectsomething)
{
    QString chanid = "";
    QDateTime startts;

    QListViewItem *curitem = listview->selectedItem();

    if (curitem)
    {
        ProgramListItem *pgitem = (ProgramListItem *)curitem;
        ProgramInfo *rec = pgitem->getProgramInfo();
        startts = rec->startts;
        chanid = rec->chanid;
    }

    listview->clear();

    ProgramListItem *selected = NULL;
    ProgramListItem *item = NULL;

    vector<ProgramInfo *> *infoList;
    infoList = RemoteGetRecordedList(m_context, type == Delete);
    if (infoList)
    {
        vector<ProgramInfo *>::iterator i = infoList->begin();
        for (; i != infoList->end(); i++)
        {
            item = new ProgramListItem(m_context, listview, (*i), 
                                       type == Delete);
            if (startts == (*i)->startts && chanid == (*i)->chanid)
            {
                selected = item;
            }
        }
        delete infoList;
    }

    if (selected)
    {
        listview->setCurrentItem(selected);
        listview->setSelected(selected, true);
    }
    else if (selectsomething && item)
    {
        listview->setCurrentItem(item);
        listview->setSelected(item, true);
    }

    lastUpdateTime = QDateTime::currentDateTime();

    return item;
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
        QTime curtime = QTime::currentTime();
        curtime = curtime.addSecs(2);

        ignoreevents = true;
        listview->SetAllowKeypress(false);
        while (!nvp->IsPlaying())
        {
            if (QTime::currentTime() > curtime)
                break;
            usleep(50);
            qApp->unlock();
            qApp->processEvents();
            qApp->lock();
        }
        listview->SetAllowKeypress(true);
        ignoreevents = false;

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
    if (rbuffer || nvp)
    {
        cout << "ERROR: preview window didn't clean up\n";
        return;
    }

    rbuffer = new RingBuffer(m_context, rec->pathname, false, true);

    nvp = new NuppelVideoPlayer(m_context);
    nvp->SetRingBuffer(rbuffer);
    nvp->SetAsPIP();
    nvp->SetOSDFontName(m_context->GetSetting("OSDFont"),
                        m_context->GetSetting("OSDCCFont"),
                        m_context->GetInstallPrefix());
    QString filters = "";
    nvp->SetVideoFilters(filters);
 
    pthread_create(&decoder, NULL, SpawnDecoder, nvp);

    QTime curtime = QTime::currentTime();
    curtime = curtime.addSecs(2);
 
    ignoreevents = true;
    listview->SetAllowKeypress(false);

    while (nvp && !nvp->IsPlaying())
    {
         if (QTime::currentTime() > curtime)
             break;
         usleep(50);
         qApp->unlock();
         qApp->processEvents();
         qApp->lock();
    }

    listview->SetAllowKeypress(true);
    ignoreevents = false;
}

void PlaybackBox::changed(QListViewItem *lvitem)
{
    //if (ignoreevents)
    //    return;

    ignoreevents = true;

    killPlayer();

    if (!title)
    {
        ignoreevents = false;
        return;
    }

    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    if (!pgitem)
    {
        title->setText("");
        date->setText("");
        chan->setText("");
        subtitle->setText("");
        description->setText("");
        if (pixlabel)
        {
            QPixmap temp((int)(160 * wmult), (int)(120 * hmult));
            temp.fill(black);

            pixlabel->setPixmap(temp);
        }
        ignoreevents = false;
        return;
    }
   
    ProgramInfo *rec = pgitem->getProgramInfo();

    QDateTime startts = rec->startts;
    QDateTime endts = rec->endts;
       
    QString timedate = endts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);
        
    date->setText(timedate);

    QString chantext;
    if (displayChanNum)
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

    title->setMinimumWidth(titlewidth);
    title->setMaximumWidth(titlewidth);

    subtitle->setMinimumWidth(descwidth);
    subtitle->setMaximumWidth(descwidth);
    description->setMinimumWidth(descwidth);
    description->setMaximumWidth(descwidth);
    description->setMaximumHeight((int)(80 * hmult));

    if (pixlabel)
    {
        QPixmap *pix = pgitem->getPixmap();
        if (pix)
            pixlabel->setPixmap(*pix);
    }

    ignoreevents = false;

    timer->start(1000 / 30);
}

void PlaybackBox::selected(QListViewItem *lvitem)
{
    if (!lvitem || ignoreevents)
        return;

    switch (type) 
    {
        case Play: play(lvitem); break;
        case Delete: remove(lvitem); break;
    }
}

void PlaybackBox::play(QListViewItem *lvitem)
{
    if (!lvitem || ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    ProgramInfo *rec = pgitem->getProgramInfo();

    if (fileExists(rec->pathname) == false)
    {
        cerr << "Error: " << rec->pathname << " file not found\n";
        killPlayer();
        return;
    }

    ProgramInfo *tvrec = new ProgramInfo(*rec);

    TV *tv = new TV(m_context);
    tv->Init();
    tv->Playback(tvrec);

    ignoreevents = true;
    listview->SetAllowKeypress(false);
    while (tv->IsPlaying() || tv->ChangingState())
    {
        qApp->unlock();
        qApp->processEvents();
        usleep(50);
        qApp->lock();
    }
    listview->SetAllowKeypress(true);
    ignoreevents = false;

    if (tv->getRequestDelete())
    {
        remove(lvitem);
    }
    else if (tv->getEndOfRecording() && 
             m_context->GetNumSetting("EndOfRecordingExitPrompt"))
    {
        promptEndOfRecording(lvitem);
    }

    delete tv;

    FillList(true);

    ignoreevents = false;

    timer->start(1000 / 30);
}

void PlaybackBox::doRemove(QListViewItem *lvitem)
{
    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    if (!pgitem)
    {
        ignoreevents = false;
        return;
    }

    ProgramInfo *rec = pgitem->getProgramInfo();

    RemoteDeleteRecording(m_context, rec);

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

}

void PlaybackBox::remove(QListViewItem *lvitem)
{
    if (!lvitem || ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    if (!pgitem)
    {
        ignoreevents = false;
        return;
    }

    ProgramInfo *rec = pgitem->getProgramInfo();

    QString message = "Are you sure you want to delete:<br><br>";
    message += rec->title;
    message += "<br>";
    
    QDateTime startts = rec->startts;
    QDateTime endts = rec->endts;

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
        doRemove(lvitem);
    }

    ignoreevents = false;

    timer->start(1000 / 30);    
}

void PlaybackBox::promptEndOfRecording(QListViewItem *lvitem)
{
    if (!lvitem || ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    ProgramListItem *pgitem = (ProgramListItem *)lvitem;
    if (!pgitem)
    {
        ignoreevents = false;
        return;
    }

    ProgramInfo *rec = pgitem->getProgramInfo();

    QString message = "You have finished watching:<br><br>";
    message += rec->title;
    message += "<br>";
    
    QDateTime startts = rec->startts;
    QDateTime endts = rec->endts;

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

    message += "<br><br>Would you like to delete this recording?  It will "
               "be gone forever.";

    DialogBox diag(m_context, message);

    diag.AddButton("Yes, get rid of it");
    diag.AddButton("No, I might want to watch it again.");

    diag.Show();

    int result = diag.exec();

    if (result == 1)
    {
	doRemove(lvitem);
    }

    ignoreevents = false;

    timer->start(1000 / 30);    
}

void PlaybackBox::UpdateProgressBar(void)
{
    int total, used;
    RemoteGetFreeSpace(m_context, total, used);

    QString usestr;
    usestr.sprintf("Storage: %d,%03d MB used out of %d,%03d MB total", 
                   used / 1000, used % 1000, 
                   total / 1000, total % 1000);

    freespace->setText(usestr);
    progressbar->setTotalSteps(total);
    progressbar->setProgress(used);
}

void PlaybackBox::timeout(void)
{
    if (ignoreevents)
        return;

    if (!nvp && pixlabel)
    {
        if (playbackPreview)
        {
            QListViewItem *curitem = listview->selectedItem();

            if (curitem)
            {
                ProgramListItem *pgitem = (ProgramListItem *)curitem;
                ProgramInfo *rec = pgitem->getProgramInfo();

                if (fileExists(rec->pathname) == false)
                {
                    title->setText(title->text() + "     Error: File Missing!");
                    QPixmap temp((int)(160 * wmult), (int)(120 * hmult));
                    temp.fill(black);
                    pixlabel->setPixmap(temp);

                    killPlayer();
                    return;
                }

                startPlayer(rec);
            }
        }
    }

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
    if (!pmap || pmap->isNull())
        return;

    QPainter p(pmap);

    p.drawImage(0, 0, img);
    p.end();

    bitBlt(pixlabel, 0, 
           (int)((pixlabel->contentsRect().height() - 120 * hmult) / 2), 
           pmap);
}

void PlaybackBox::customEvent(QCustomEvent *e)
{
    if (ignoreevents)
        return;

    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();
 
        if (message == "RECORDING_LIST_CHANGE")
        {
            if (QDateTime::currentDateTime() > lastUpdateTime.addSecs(1))
                FillList(true);      
            if (type == Delete)
                UpdateProgressBar();
        }
    }
}

bool PlaybackBox::fileExists(const QString &pathname)
{
    if (pathname.left(7) == "myth://")
        return RemoteGetCheckFile(m_context, pathname);

    QFile checkFile(pathname);

    return checkFile.exists();
}
