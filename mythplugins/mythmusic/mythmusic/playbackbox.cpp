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
#include <qheader.h>

#include <iostream>
using namespace std;

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

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>

#include "res/nextfile.xpm"
#include "res/next.xpm"
#include "res/pause.xpm"
#include "res/play.xpm"
#include "res/prevfile.xpm"
#include "res/prev.xpm"
#include "res/stop.xpm"

PlaybackBox::PlaybackBox(QSqlDatabase *ldb, QValueList<Metadata> *playlist,
                         QWidget *parent, const char *name)
           : MythDialog(parent, name)
{
    db = ldb;

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    mainvisual = new MainVisual();
    
    QVBoxLayout *vbox2 = new QVBoxLayout(vbox, (int)(2 * wmult));

    QGroupBox *topdisplay = new QGroupBox(this);
    vbox2->addWidget(topdisplay);

    topdisplay->setLineWidth(4);
    topdisplay->setMidLineWidth(3);
    topdisplay->setFrameShape(QFrame::Panel);
    topdisplay->setFrameShadow(QFrame::Sunken);
    topdisplay->setPaletteBackgroundColor(palette().color(QPalette::Active,
                                                          QColorGroup::Mid));

    QVBoxLayout *framebox = new QVBoxLayout(topdisplay, (int)(10 * wmult));

    titlelabel = new ScrollLabel(topdisplay);
    titlelabel->setFont(font());
    titlelabel->setText("  ");
    titlelabel->setPaletteBackgroundColor(palette().color(QPalette::Active,
                                                          QColorGroup::Mid));
    framebox->addWidget(titlelabel);   

    QHBoxLayout *framehbox = new QHBoxLayout(framebox);

    timelabel = new QLabel(topdisplay);
    timelabel->setFont(font());
    timelabel->setText("  ");
    timelabel->setPaletteBackgroundColor(palette().color(QPalette::Active,
                                                         QColorGroup::Mid));
    timelabel->setAlignment(AlignRight);
    framehbox->addWidget(timelabel);

    seekbar = new QSlider(Qt::Horizontal, this);
    seekbar->setFocusPolicy(NoFocus);
    seekbar->setTracking(false);   
    seekbar->blockSignals(true); 

    vbox2->addWidget(seekbar);

    QHBoxLayout *controlbox = new QHBoxLayout(vbox2, (int)(2 * wmult));

    MythToolButton *prevfileb = new MythToolButton(this);
    prevfileb->setAutoRaise(true);
    prevfileb->setIconSet(scalePixmap((const char **)prevfile_pix));  
    connect(prevfileb, SIGNAL(clicked()), this, SLOT(previous()));
 
    MythToolButton *prevb = new MythToolButton(this);
    prevb->setAutoRaise(true);
    prevb->setIconSet(scalePixmap((const char **)prev_pix));
    connect(prevb, SIGNAL(clicked()), this, SLOT(seekback()));

    MythToolButton *pauseb = new MythToolButton(this);
    pauseb->setAutoRaise(true);
    pauseb->setIconSet(scalePixmap((const char **)pause_pix));
    connect(pauseb, SIGNAL(clicked()), this, SLOT(pause()));

    MythToolButton *playb = new MythToolButton(this);
    playb->setAutoRaise(true);
    playb->setIconSet(scalePixmap((const char **)play_pix));
    connect(playb, SIGNAL(clicked()), this, SLOT(play()));

    MythToolButton *stopb = new MythToolButton(this);
    stopb->setAutoRaise(true);
    stopb->setIconSet(scalePixmap((const char **)stop_pix));
    connect(stopb, SIGNAL(clicked()), this, SLOT(stop()));
    
    MythToolButton *nextb = new MythToolButton(this);
    nextb->setAutoRaise(true);
    nextb->setIconSet(scalePixmap((const char **)next_pix));
    connect(nextb, SIGNAL(clicked()), this, SLOT(seekforward()));

    MythToolButton *nextfileb = new MythToolButton(this);
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

    QFont buttonfont("Arial", (int)((gContext->GetSmallFontSize() + 2) * hmult),
                     QFont::Bold);

    randomize = new MythToolButton(this);
    randomize->setAutoRaise(true);
    randomize->setText("Shuffle: Normal");
    randomize->setFont(buttonfont); 
    secondcontrol->addWidget(randomize);
    connect(randomize, SIGNAL(clicked()), this, SLOT(toggleShuffle()));

    repeat = new MythToolButton(this);
    repeat->setAutoRaise(true);
    repeat->setText("Repeat: Playlist");
    repeat->setFont(buttonfont);
    secondcontrol->addWidget(repeat);
    connect(repeat, SIGNAL(clicked()), this, SLOT(toggleRepeat()));

    MythToolButton *pledit = new MythToolButton(this);
    pledit->setAutoRaise(true);
    pledit->setText("Edit Playlist");
    pledit->setFont(buttonfont);
    secondcontrol->addWidget(pledit);
    connect(pledit, SIGNAL(clicked()), this, SLOT(editPlaylist()));

    MythToolButton *vis = new MythToolButton(this);
    vis->setAutoRaise(true);
    vis->setText("Visualize");
    vis->setFont(buttonfont);
    secondcontrol->addWidget(vis);
    connect(vis, SIGNAL(clicked()), this, SLOT(visEnable()));

    QString keyboard_accelerator_flag = gContext->GetSetting("KeyboardAccelerators");
    if(keyboard_accelerator_flag.lower() == "true")
    {
        // There may be a better key press
        // mapping, but I have a pretty
        // serious flu at the moment and 
        // I can't think of one

        prevfileb->setAccel(Key_Up);
        prevfileb->setFocusPolicy( QWidget::NoFocus);
        prevb->setAccel(Key_Left);
        prevb->setFocusPolicy( QWidget::NoFocus);
        pauseb->setAccel(Key_P);
        pauseb->setFocusPolicy( QWidget::NoFocus);
        playb->setAccel(Key_Space);
        playb->setFocusPolicy( QWidget::NoFocus);
        stopb->setAccel(Key_S);
        stopb->setFocusPolicy( QWidget::NoFocus);
        nextb->setAccel(Key_Right);
        nextb->setFocusPolicy( QWidget::NoFocus);
        nextfileb->setAccel(Key_Down);
        nextfileb->setFocusPolicy( QWidget::NoFocus);
		
        randomize->setAccel(Key_1);
        randomize->setFocusPolicy( QWidget::NoFocus);
        repeat->setAccel(Key_2);
        repeat->setFocusPolicy( QWidget::NoFocus);
        pledit->setAccel(Key_3);
        pledit->setFocusPolicy( QWidget::NoFocus);
        vis->setAccel(Key_4);
        vis->setFocusPolicy( QWidget::NoFocus);
    }

    playview = new MythListView(this);
    playview->addColumn("#");
    playview->addColumn("Artist");  
    playview->addColumn("Title");
    playview->addColumn("Length");
    playview->setFont(buttonfont);

    playview->setFocusPolicy(NoFocus);

    playview->setColumnWidth(0, (int)(50 * wmult));
    playview->setColumnWidth(1, (int)(210 * wmult));
    playview->setColumnWidth(2, (int)(385 * wmult));
    playview->setColumnWidth(3, (int)(90 * wmult));
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

    if(keyboard_accelerator_flag.lower() != "true")
    {
        playb->setFocus();
    }

    input = 0; decoder = 0; seeking = false; remainingTime = false;
    output = 0; outputBufferSize = 256;

    shufflemode = false;
    repeatmode = false;  

    curMeta = ((*plist)[playlistindex]);

    QString playmode = gContext->GetSetting("PlayMode");
    if (playmode == "random")
    {
        toggleShuffle();
        curMeta = ((*plist)[shuffleindex]);
    }

    setupPlaylist();

    isplaying = false;
 
    if (plist->size() > 0)
    { 
        playfile = curMeta.Filename();
        emit play();
    }
    
    //
    //	Load Visualization settings and set up timer
    //
	
    visual_mode = gContext->GetSetting("VisualMode");
    QString visual_delay = gContext->GetSetting("VisualModeDelay");
    
    bool delayOK;
    visual_mode_delay = visual_delay.toInt(&delayOK);
    if(!delayOK)
    {
    	visual_mode_delay = 0;
    }
    
    visual_mode_timer = new QTimer();
    if(visual_mode_delay > 0)
    {
    	visual_mode_timer->start(visual_mode_delay * 1000);
        connect(prevfileb, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(pauseb, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(playb, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(stopb, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(nextb, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(nextfileb, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(randomize, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(repeat, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(pledit, SIGNAL(clicked()), this, SLOT(resetTimer()));
        connect(vis, SIGNAL(clicked()), this, SLOT(resetTimer()));
    }

    visualizer_is_active = false;
    connect(visual_mode_timer, SIGNAL(timeout()), this, SLOT(visEnable()));
    connect(mainvisual, SIGNAL(hidingVisualization()), this, SLOT(restartTimer()));
}

PlaybackBox::~PlaybackBox(void)
{
    stopAll();
}

void PlaybackBox::resetTimer()
{
    if (visual_mode_delay > 0)
    {
        visual_mode_timer->changeInterval(visual_mode_delay * 1000);
    }
}

void PlaybackBox::restartTimer()
{
    if (visual_mode_delay > 0)
    {
        visual_mode_timer->start(visual_mode_delay * 1000);
    }
    visualizer_is_active = false;
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
        
        QString timestr;
        timestr.sprintf("%2d:%02d", min, secs);
        
        litem = new QListViewItem(playview, position, (*it).Artist(),
                                  (*it).Title(), timestr);
        listlist.prepend(litem);
        it--; count--;
    }

    QListViewItem *curItem = listlist.at(playlistindex);

    if (curItem)
    {
        playview->setCurrentItem(curItem);
        playview->setSelected(curItem, true);

        if (curItem->itemBelow())
            playview->ensureItemVisible(curItem->itemBelow());
        if (curItem->itemAbove())
            playview->ensureItemVisible(curItem->itemAbove());
        playview->ensureItemVisible(curItem);
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
        QString adevice = gContext->GetSetting("AudioDevice");

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
    playview->setSelected(curItem, true);

    if (curItem->itemBelow())
        playview->ensureItemVisible(curItem->itemBelow());
    if (curItem->itemAbove())
        playview->ensureItemVisible(curItem->itemAbove());
    playview->ensureItemVisible(curItem);

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
        
        gContext->LCDswitchToChannel(curMeta.Artist(), curMeta.Title(), "");
    }
}

void PlaybackBox::visEnable()
{
    if (!visualizer_is_active)
    {
        visual_mode_timer->stop();
        mainvisual->setVisual("Blank");
        mainvisual->showFullScreen();
        mainvisual->setVisual(visual_mode);
        visualizer_is_active = true;
    }
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
    gContext->LCDswitchToTime();
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
    QString paths = gContext->GetSetting("TreeLevels");
    DatabaseBox dbbox(db, paths, &dblist);

    visual_mode_timer->stop();
    dbbox.Show();
    dbbox.exec();
    if (visual_mode_delay > 0)
    {
        visual_mode_timer->start(visual_mode_delay * 1000);
    }

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
            float percent_heard;

            currentTime = rs = oe->elapsedSeconds();

            em = rs / 60;
            es = rs % 60;

            timeString.sprintf("%02d:%02d", em, es);
			
            percent_heard = ((float)rs / (float)curMeta.Length() ) * 1000.0;

            gContext->LCDsetChannelProgress(percent_heard);
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

