#include <qlayout.h>
#include <qapplication.h>
#include <qcursor.h>
#include <qpixmap.h>
#include <qstringlist.h>
#include <qfile.h>
#include <qsettings.h>
#include <qlabel.h>
#include <qgroupbox.h>
#include <qlistview.h>
#include <qtoolbutton.h>
#include <qslider.h>
#include <qstyle.h>
#include <qimage.h>

#include "scrolllabel.h"
#include "metadata.h"
#include "audiooutput.h"
#include "constants.h"
#include "streaminput.h"
#include "output.h"
#include "decoder.h"
#include "playbackbox.h"
#include "databasebox.h"
#include "mainvisual.h"

#include <mythtv/settings.h>

#include "res/nextfile.xpm"
#include "res/next.xpm"
#include "res/pause.xpm"
#include "res/play.xpm"
#include "res/prevfile.xpm"
#include "res/prev.xpm"
#include "res/stop.xpm"

extern Settings *globalsettings;

class MyButton : public QToolButton
{   
  public:
    MyButton(QWidget *parent) : QToolButton(parent) 
                { setFocusPolicy(StrongFocus); } 
    
    void drawButton(QPainter *p);
};  


void MyButton::drawButton( QPainter * p )
{
    QStyle::SCFlags controls = QStyle::SC_ToolButton;
    QStyle::SCFlags active = QStyle::SC_None;

    if (isDown())
        active |= QStyle::SC_ToolButton;

    QStyle::SFlags flags = QStyle::Style_Default;
    if (isEnabled())
        flags |= QStyle::Style_Enabled;
    if (hasFocus())
        flags |= QStyle::Style_MouseOver;
    if (isDown())
        flags |= QStyle::Style_Down;
    if (isOn())
        flags |= QStyle::Style_On;
    if (!autoRaise()) {
        flags |= QStyle::Style_AutoRaise;
        if (uses3D()) {
            flags |= QStyle::Style_MouseOver;
            if (! isOn() && ! isDown())
                flags |= QStyle::Style_Raised;
        }
    } else if (! isOn() && ! isDown())
        flags |= QStyle::Style_Raised;

    style().drawComplexControl(QStyle::CC_ToolButton, p, this, rect(), 
                               colorGroup(), flags, controls, active,
                               QStyleOption());
    drawButtonLabel(p);
}

PlaybackBox::PlaybackBox(QSqlDatabase *ldb, QValueList<Metadata> *playlist,
                         QWidget *parent, const char *name)
           : QDialog(parent, name)
{
    db = ldb;

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

    setFont(QFont("Arial", (int)(18 * hmult), QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    mainvisual = new MainVisual();
    
    QVBoxLayout *vbox2 = new QVBoxLayout(vbox, (int)(2 * wmult));

    QGroupBox *topdisplay = new QGroupBox(this);
    vbox2->addWidget(topdisplay);

    topdisplay->setLineWidth(4);
    topdisplay->setMidLineWidth(3);
    topdisplay->setFrameShape(QFrame::Panel);
    topdisplay->setFrameShadow(QFrame::Sunken);
    topdisplay->setPaletteBackgroundColor(QColor("grey"));

    QVBoxLayout *framebox = new QVBoxLayout(topdisplay, (int)(10 * wmult));

    titlelabel = new ScrollLabel(topdisplay);
    titlelabel->setText("  ");
    titlelabel->setPaletteBackgroundColor(QColor("grey"));
    framebox->addWidget(titlelabel);   

    QHBoxLayout *framehbox = new QHBoxLayout(framebox);

    timelabel = new QLabel(topdisplay);
    timelabel->setText("  ");
    timelabel->setPaletteBackgroundColor(QColor("grey"));
    timelabel->setAlignment(AlignRight);
    framehbox->addWidget(timelabel);

    seekbar = new QSlider(Qt::Horizontal, this);
    seekbar->setFocusPolicy(NoFocus);
    seekbar->setTracking(false);   
    seekbar->blockSignals(true); 

    vbox2->addWidget(seekbar);

    QHBoxLayout *controlbox = new QHBoxLayout(vbox2, (int)(2 * wmult));

    MyButton *prevfileb = new MyButton(this);
    prevfileb->setAutoRaise(true);
    prevfileb->setIconSet(scalePixmap((const char **)prevfile_pix));  
    connect(prevfileb, SIGNAL(clicked()), this, SLOT(previous()));
 
    MyButton *prevb = new MyButton(this);
    prevb->setAutoRaise(true);
    prevb->setIconSet(scalePixmap((const char **)prev_pix));
    connect(prevb, SIGNAL(clicked()), this, SLOT(seekback()));

    MyButton *pauseb = new MyButton(this);
    pauseb->setAutoRaise(true);
    pauseb->setIconSet(scalePixmap((const char **)pause_pix));
    connect(pauseb, SIGNAL(clicked()), this, SLOT(pause()));

    MyButton *playb = new MyButton(this);
    playb->setAutoRaise(true);
    playb->setIconSet(scalePixmap((const char **)play_pix));
    connect(playb, SIGNAL(clicked()), this, SLOT(play()));

    MyButton *stopb = new MyButton(this);
    stopb->setAutoRaise(true);
    stopb->setIconSet(scalePixmap((const char **)stop_pix));
    connect(stopb, SIGNAL(clicked()), this, SLOT(stop()));
    
    MyButton *nextb = new MyButton(this);
    nextb->setAutoRaise(true);
    nextb->setIconSet(scalePixmap((const char **)next_pix));
    connect(nextb, SIGNAL(clicked()), this, SLOT(seekforward()));

    MyButton *nextfileb = new MyButton(this);
    nextfileb->setAutoRaise(true);
    nextfileb->setIconSet(scalePixmap((const char **)nextfile_pix));
    connect(nextfileb, SIGNAL(clicked()), this, SLOT(next()));    

    controlbox->addWidget(prevfileb);
    controlbox->addWidget(prevb);
    controlbox->addWidget(pauseb);
    controlbox->addWidget(playb);
    controlbox->addWidget(stopb);
    controlbox->addWidget(nextb);
    controlbox->addWidget(nextfileb);

    QHBoxLayout *secondcontrol = new QHBoxLayout(vbox2, (int)(2 * wmult));

    randomize = new MyButton(this);
    randomize->setAutoRaise(true);
    randomize->setText("Shuffle: Normal");
    randomize->setFont(QFont("Arial", (int)(14 * hmult), QFont::Bold));
    secondcontrol->addWidget(randomize);
    connect(randomize, SIGNAL(clicked()), this, SLOT(toggleShuffle()));

    repeat = new MyButton(this);
    repeat->setAutoRaise(true);
    repeat->setText("Repeat: Playlist");
    repeat->setFont(QFont("Arial", (int)(14 * hmult), QFont::Bold));
    secondcontrol->addWidget(repeat);
    connect(repeat, SIGNAL(clicked()), this, SLOT(toggleRepeat()));

    MyButton *pledit = new MyButton(this);
    pledit->setAutoRaise(true);
    pledit->setText("Edit Playlist");
    pledit->setFont(QFont("Arial", (int)(14 * hmult), QFont::Bold));
    secondcontrol->addWidget(pledit);
    connect(pledit, SIGNAL(clicked()), this, SLOT(editPlaylist()));

    MyButton *vis = new MyButton(this);
    vis->setAutoRaise(true);
    vis->setText("Visualize");
    vis->setFont(QFont("Arial", (int)(14 * hmult), QFont::Bold));
    secondcontrol->addWidget(vis);
    connect(vis, SIGNAL(clicked()), this, SLOT(visEnable()));

    playview = new QListView(this);
    playview->addColumn("#");
    playview->addColumn("Artist");  
    playview->addColumn("Title");
    playview->addColumn("Length");
    playview->setFont(QFont("Arial", (int)(14 * hmult), QFont::Bold));

    playview->setFocusPolicy(NoFocus);

    playview->setColumnWidth(0, (int)(50 * wmult));
    playview->setColumnWidth(1, (int)(210 * wmult));
    playview->setColumnWidth(2, (int)(400 * wmult));
    playview->setColumnWidth(3, (int)(80 * wmult));
    playview->setColumnWidthMode(0, QListView::Manual);
    playview->setColumnWidthMode(1, QListView::Manual);
    playview->setColumnWidthMode(2, QListView::Manual);
    playview->setColumnWidthMode(3, QListView::Manual);

    playview->setColumnAlignment(3, Qt::AlignRight);

    playview->setSorting(-1);
    playview->setAllColumnsShowFocus(true);

    plist = playlist;
    playlistindex = 0;
    setupListView();

    vbox->addWidget(playview, 1);

    playb->setFocus();

    input = 0; decoder = 0; seeking = false; remainingTime = false;
    output = 0; outputBufferSize = 256;

    shufflemode = false;
    repeatmode = false;  

    curMeta = ((*plist)[playlistindex]);

    setupPlaylist();

    isplaying = false;
 
    if (plist->size() > 0)
    { 
        playfile = curMeta.Filename();
        emit play();
    }
}

PlaybackBox::~PlaybackBox(void)
{
    stopAll();
}

QPixmap PlaybackBox::scalePixmap(const char **xpmdata)
{
    QImage tmpimage(xpmdata);
    QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                       (int)(tmpimage.height() * hmult));
    QPixmap ret;
    ret.convertFromImage(tmp2);

    return ret;
}

void PlaybackBox::Show(void)
{
    showFullScreen();
}

void PlaybackBox::setupListView(void)
{
    playview->clear();

    QListViewItem *litem;
   
    QValueList<Metadata>::iterator it = plist->end();
    int count = plist->size();
    if (count == 0)
        return;

    it--;
    while (it != plist->end())
    {
        QString position = QString("%1").arg(count);
        int secs = (*it).Length() / 1000;
        int min = secs / 60;
        secs -= min * 60;
        
        char timestr[64];
        sprintf(timestr, "%2d:%02d", min, secs);
        
        litem = new QListViewItem(playview, position, (*it).Artist(),
                                  (*it).Title(), timestr);
        listlist.prepend(litem);
        it--; count--;
    }

    QListViewItem *curItem = listlist.at(playlistindex);

    if (curItem)
    {
        playview->setCurrentItem(curItem);
        playview->ensureItemVisible(curItem);
        playview->setSelected(curItem, true);
    }
}

void PlaybackBox::setupPlaylist(bool toggle)
{
    if (toggle)
        shufflemode = !shufflemode;

    if (playlistorder.size() > 0)
        playlistorder.clear();

    playlistindex = 0;
    shuffleindex = 0;

    if (plist->size() == 0)
    {
        curMeta = Metadata("dummy.music");
        return;
    }

    if (!shufflemode)
    {
        for (int i = 0; i < (int)plist->size(); i++)
        {
            playlistorder.push_back(i);
            if (curMeta == (*plist)[i])
                playlistindex = i;
        } 
    }
    else
    {
        int max = plist->size();
        srand((unsigned int)time(NULL));

        int i;
        bool usedList[max];
        for (i = 0; i < max; i++)
            usedList[i] = false;

        bool used = true;
        int index = 0; 
        int lastindex = 0;

        for (i = 0; i < max; i++)
        {
            while (used)
            {
                index = (int)((double)rand() / (RAND_MAX + 1.0) * max);
                if (usedList[index] == false)
                    used = false;
                if (max - i > 50 && abs(index - lastindex) < 10)
                    used = true;
            }
            usedList[index] = true;
            playlistorder.push_back(index);
            used = true;
            lastindex = index;

            if (curMeta == (*plist)[i])
                playlistindex = i;
        }
    }

    curMeta = (*plist)[playlistindex];
    shuffleindex = playlistorder.findIndex(playlistindex);
}

void PlaybackBox::play()
{
    if (isplaying)
        stop();

    if (plist->size() == 0)
        return;

    playfile = curMeta.Filename();

    QUrl sourceurl(playfile);
    QString sourcename(playfile);

    bool startoutput = false;

    if (!output)
    {
        QString adevice = globalsettings->GetSetting("AudioDevice");

        output = new AudioOutput(outputBufferSize * 1024, adevice);
        output->setBufferSize(outputBufferSize * 1024);
        output->addListener(this);
        output->addListener(mainvisual);
        output->addVisual(mainvisual);
	
        startoutput = true;

        if (!output->initialize())
            return;
    }

    if (!sourceurl.isLocalFile()) {
        StreamInput streaminput(sourceurl);
        streaminput.setup();
        input = streaminput.socket();
    } else
        input = new QFile(sourceurl.toString(FALSE, FALSE));

    if (decoder && !decoder->factory()->supports(sourcename))
        decoder = 0;

    if (!decoder) {
        decoder = Decoder::create(sourcename, input, output);

        if (! decoder) {
            printf("mythmusic: unsupported fileformat\n");
            stopAll();
            return;
        }

        decoder->setBlockSize(globalBlockSize);
        decoder->addListener(this);
    } else {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    QString disp = curMeta.Artist() + "  ~  " + curMeta.Album() + 
                   "  ~   " + curMeta.Title();
    titlelabel->setText(disp);

    QListViewItem *curItem = listlist.at(playlistindex);
    playview->setCurrentItem(curItem);
    playview->ensureItemVisible(curItem);
    playview->setSelected(curItem, true);

    currentTime = 0;
    maxTime = curMeta.Length() / 1000;

    mainvisual->setDecoder(decoder);
    mainvisual->setOutput(output);
    
    if (decoder->initialize()) {
        seekbar->setMinValue(0);
        seekbar->setValue(0);
        seekbar->setMaxValue(maxTime);
        if (seekbar->maxValue() == 0) {
            seekbar->setEnabled(false);
        } else {
            seekbar->setEnabled(true);
        }
 
        if (output)
        {
            if (startoutput)
                output->start();
            else
                output->resetTime();
        }

        decoder->start();

        isplaying = true;
    }
}

void PlaybackBox::visEnable(void)
{
    mainvisual->setVisual("Synaesthesia");
    mainvisual->showFullScreen();
}

void PlaybackBox::pause(void)
{
    if (output) {
        output->mutex()->lock();
        output->pause();
        output->mutex()->unlock();
    }

    // wake up threads
    if (decoder) {
        decoder->mutex()->lock();
        decoder->cond()->wakeAll();
        decoder->mutex()->unlock();
    }

    if (output) {
        output->recycler()->mutex()->lock();
        output->recycler()->cond()->wakeAll();
        output->recycler()->mutex()->unlock();
    }
}

void PlaybackBox::stopDecoder(void)
{
    if (decoder && decoder->running()) {
        decoder->mutex()->lock();
        decoder->stop();
        decoder->mutex()->unlock();
    }

    if (decoder) {
        decoder->mutex()->lock();
        decoder->cond()->wakeAll();
        decoder->mutex()->unlock();
    }

    if (decoder)
        decoder->wait();
}

void PlaybackBox::stop(void)
{
    if (decoder && decoder->running()) {
        decoder->mutex()->lock();
        decoder->stop();
        decoder->mutex()->unlock();
    }

    if (output && output->running()) {
        output->mutex()->lock();
        output->stop();
        output->mutex()->unlock();
    }

    // wake up threads
    if (decoder) {
        decoder->mutex()->lock();
        decoder->cond()->wakeAll();
        decoder->mutex()->unlock();
    }

    if (output) {
        output->recycler()->mutex()->lock();
        output->recycler()->cond()->wakeAll();
        output->recycler()->mutex()->unlock();
    }

    if (decoder)
        decoder->wait();

    if (output)
        output->wait();

    if (output)
    {
        delete output;
        output = 0;
    }

    mainvisual->setDecoder(0);
    mainvisual->setOutput(0);

    delete input;
    input = 0;

    seekbar->setValue(0);
    titlelabel->setText(" ");
    timelabel->setText(" ");

    isplaying = false;
}

void PlaybackBox::stopAll()
{
    stop();

    if (decoder) {
        decoder->removeListener(this);
        decoder = 0;
    }
}

void PlaybackBox::previous()
{
    stop();

    listlock.lock();

    shuffleindex--;
    if (shuffleindex < 0)
        shuffleindex = plist->size() - 1;

    playlistindex = playlistorder[shuffleindex];

    curMeta = ((*plist)[playlistindex]);

    listlock.unlock();

    play();
}

void PlaybackBox::next()
{
//    stop();

    listlock.lock();

    shuffleindex++;
    if (shuffleindex >= (int)plist->size())
        shuffleindex = 0;

    playlistindex = playlistorder[shuffleindex];

    curMeta = ((*plist)[playlistindex]);

    listlock.unlock();

    play();
}

void PlaybackBox::nextAuto()
{
    stopDecoder();

    isplaying = false;

    if (repeatmode)
        play();
    else 
        next();
}

void PlaybackBox::seekforward()
{
    int nextTime = currentTime + 5;
    if (nextTime > maxTime)
        nextTime = maxTime;
    seek(nextTime);
}

void PlaybackBox::seekback()
{
    int nextTime = currentTime - 5;
    if (nextTime < 0)
        nextTime = 0;
    seek(nextTime);
}

void PlaybackBox::startseek()
{
    seeking = true;
}

void PlaybackBox::doneseek()
{
    seeking = false;
}

void PlaybackBox::seek(int pos)
{
    if (output && output->running()) {
        output->mutex()->lock();
        output->seek(pos);

        if (decoder && decoder->running()) {
            decoder->mutex()->lock();
            decoder->seek(pos);

            if (mainvisual) {
                mainvisual->mutex()->lock();
                mainvisual->prepare();
                mainvisual->mutex()->unlock();
            }

            decoder->mutex()->unlock();
        }

        output->mutex()->unlock();
    }
}

void PlaybackBox::changeSong()
{
    stop();
    play();
}

void PlaybackBox::toggleShuffle()
{
    setupPlaylist(true);
    if (shufflemode)
        randomize->setText("Shuffle: Random");
    else
        randomize->setText("Shuffle: Normal"); 
}

void PlaybackBox::toggleRepeat()
{
    repeatmode = !repeatmode;

    if (repeatmode)
        repeat->setText("Repeat: Track");
    else
        repeat->setText("Repeat: Playlist");
}

void PlaybackBox::editPlaylist()
{
    Metadata firstdata = curMeta;
    
    QValueList<Metadata> dblist = *plist; 
    QString paths = globalsettings->GetSetting("TreeLevels");
    DatabaseBox dbbox(db, paths, &dblist);

    dbbox.Show();
    dbbox.exec();

    listlock.lock();

    *plist = dblist;
    setupPlaylist();

    listlock.unlock();

    setupListView();

    if (isplaying && firstdata != curMeta)
    {
        stop();
        if (plist->size() > 0)
        {
            playlistindex = playlistorder[shuffleindex];
            curMeta = ((*plist)[playlistindex]);

            play();
        }
    }    
}

void PlaybackBox::closeEvent(QCloseEvent *event)
{
    stopAll();

    hide();

    event->accept();
}

void PlaybackBox::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

void PlaybackBox::customEvent(QCustomEvent *event)
{
    switch ((int) event->type()) {
    case OutputEvent::Playing:
        {
            statusString = tr("Playing stream.");

            break;
        }

    case OutputEvent::Buffering:
        {
            statusString = tr("Buffering stream.");

            break;
        }

    case OutputEvent::Paused:
        {
            statusString = tr("Stream paused.");

            break;
        }

    case OutputEvent::Info:
        {
            OutputEvent *oe = (OutputEvent *) event;

            int em, es, rs;

            currentTime = rs = oe->elapsedSeconds();

            em = rs / 60;
            es = rs % 60;

            timeString.sprintf("%02d:%02d", em, es);

            if (! seeking) 
                seekbar->setValue(oe->elapsedSeconds());

            infoString.sprintf("%d kbps, %.1f kHz %s.",
                               oe->bitrate(), float(oe->frequency()) / 1000.0,
                               oe->channels() > 1 ? "stereo" : "mono");

            timelabel->setText(timeString);

            break;
        }
    case OutputEvent::Error:
        {
            statusString = tr("Output error.");
            timeString = "--m --s";

            //OutputEvent *aoe = (OutputEvent *) event;
            //QMessageBox::critical(qApp->activeWindow(),
            //                      statusString,
            //                      *aoe->errorMessage());

            stopAll();

            break;
        }
    case DecoderEvent::Stopped:
        {
            statusString = tr("Stream stopped.");
            timeString = "--m --s";

            break;
        }
    case DecoderEvent::Finished:
        {
            statusString = tr("Finished playing stream.");
            nextAuto();
            break;
        }

    case DecoderEvent::Error:
        {
            stopAll();
            QApplication::sendPostedEvents();

            statusString = tr("Decoder error.");

            //DecoderEvent *dxe = (DecoderEvent *) event;
            //QMessageBox::critical(qApp->activeWindow(),
            //                      statusString,
            //                      *dxe->errorMessage());
            break;
        }
    }

    QWidget::customEvent(event);
}

