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
#include <stdlib.h>

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
#include "res/rateup.xpm"
#include "res/ratedn.xpm"

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

    pauseb = new MythToolButton(this);
    pauseb->setAutoRaise(true);
    pauseb->setToggleButton(true);
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

    showrating = gContext->GetNumSetting("MusicShowRatings", 0);

    MythToolButton *rateup = NULL, *ratedn = NULL;

    if (showrating)
    {    
        rateup = new MythToolButton(this);
        rateup->setAutoRaise(true);
        rateup->setIconSet(scalePixmap((const char **)rateup_pix));
        connect(rateup, SIGNAL(clicked()), this, SLOT(increaseRating()));

        ratedn = new MythToolButton(this);
        ratedn->setAutoRaise(true);
        ratedn->setIconSet(scalePixmap((const char **)ratedn_pix));
        connect(ratedn, SIGNAL(clicked()), this, SLOT(decreaseRating()));
    }

    controlbox->addWidget(prevfileb);
    controlbox->addWidget(prevb);
    controlbox->addWidget(pauseb);
    controlbox->addWidget(playb);
    controlbox->addWidget(stopb);
    controlbox->addWidget(nextb);
    controlbox->addWidget(nextfileb);

    if (showrating)
    {
        controlbox->addWidget(rateup);
        controlbox->addWidget(ratedn);
    }

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

    pledit = new MythToolButton(this);
    pledit->setAutoRaise(true);
    pledit->setText("Edit Playlist");
    pledit->setFont(buttonfont);
    secondcontrol->addWidget(pledit);
    connect(pledit, SIGNAL(clicked()), this, SLOT(editPlaylist()));

    vis = new MythToolButton(this);
    vis->setAutoRaise(true);
    vis->setText("Visualize");
    vis->setFont(buttonfont);
    secondcontrol->addWidget(vis);
    connect(vis, SIGNAL(clicked()), this, SLOT(visEnable()));

    keyboard_accelerator_flag = gContext->GetNumSetting("KeyboardAccelerators");
    if(keyboard_accelerator_flag)
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

        if (showrating)
        {
            rateup->setAccel(Key_U);
            rateup->setFocusPolicy( QWidget::NoFocus);
            ratedn->setAccel(Key_D);
            ratedn->setFocusPolicy( QWidget::NoFocus);	
        }
	
        randomize->setAccel(Key_1);
        randomize->setFocusPolicy( QWidget::NoFocus);
        repeat->setAccel(Key_2);
        repeat->setFocusPolicy( QWidget::NoFocus);
        pledit->setAccel(Key_3);
        pledit->setFocusPolicy( QWidget::NoFocus);
        vis->setAccel(Key_4);
        vis->setFocusPolicy( QWidget::NoFocus);
    }

    showrating = gContext->GetNumSetting("MusicShowRatings", 0);

    playview = new MythListView(this);
    playview->addColumn("#");
    if (showrating)
        playview->addColumn("Rating");
    playview->addColumn("Artist");  
    playview->addColumn("Title");
    playview->addColumn("Length");
    playview->setFont(buttonfont);

    playview->setFocusPolicy(NoFocus);

    if (showrating)
    {
        playview->setColumnWidth(0, (int)(50 * wmult));
        playview->setColumnWidth(1, (int)(60 * wmult));
        playview->setColumnWidth(2, (int)(195 * wmult));
        playview->setColumnWidth(3, (int)(355 * wmult));
        playview->setColumnWidth(4, (int)(80 * wmult));
        playview->setColumnWidthMode(0, QListView::Manual);
        playview->setColumnWidthMode(1, QListView::Manual);
        playview->setColumnWidthMode(2, QListView::Manual);
        playview->setColumnWidthMode(3, QListView::Manual);
        playview->setColumnWidthMode(4, QListView::Manual);

        playview->setColumnAlignment(4, Qt::AlignRight);
    }
    else
    {
        playview->setColumnWidth(0, (int)(50 * wmult));
        playview->setColumnWidth(1, (int)(210 * wmult));
        playview->setColumnWidth(2, (int)(385 * wmult));
        playview->setColumnWidth(3, (int)(90 * wmult));
        playview->setColumnWidthMode(0, QListView::Manual);
        playview->setColumnWidthMode(1, QListView::Manual);
        playview->setColumnWidthMode(2, QListView::Manual);
        playview->setColumnWidthMode(3, QListView::Manual);

        playview->setColumnAlignment(3, Qt::AlignRight);
    }

    playview->setSorting(-1);
    playview->setAllColumnsShowFocus(true);

    plist = playlist;
    playlistindex = 0;
    shuffleindex = 0;
    setupListView();

    vbox->addWidget(playview, 1);

    if (!keyboard_accelerator_flag)
    {
        playb->setFocus();
    }

    input = 0; decoder = 0; seeking = false; remainingTime = false;
    output = 0; outputBufferSize = 256;

    shufflemode = 0;
    repeatmode = false;  

    curMeta = Metadata("dummy.music");

    QString playmode = gContext->GetSetting("PlayMode");
    if (playmode.lower() == "random")
        toggleShuffle();
    else if (playmode.lower() == "intelligent")
    {
        shufflemode++;
        toggleShuffle();
    }
    else
        setupPlaylist();

    playlistindex = playlistorder[shuffleindex];
    curMeta = (*plist)[playlistindex];

    // this is a hack to fix the playlist's refusal to update w/o a SIG
    playlist_timer = new QTimer();
    connect(playlist_timer, SIGNAL(timeout()), this, SLOT(jumpToItem()));

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
        if (showrating)
        {
            connect(rateup, SIGNAL(clicked()), this, SLOT(resetTimer()));
            connect(ratedn, SIGNAL(clicked()), this, SLOT(resetTimer()));
        }
    }

    visualizer_is_active = false;
    connect(visual_mode_timer, SIGNAL(timeout()), this, SLOT(visEnable()));

    connect(mainvisual, SIGNAL(hidingVisualization()), 
            this, SLOT(restartTimer()));
    connect(mainvisual, SIGNAL(keyPress(QKeyEvent *)),
            this, SLOT(keyPressFromVisual(QKeyEvent *)));
}

PlaybackBox::~PlaybackBox(void)
{
    stopAll();
}

void PlaybackBox::keyPressFromVisual(QKeyEvent *e)
{
    if (keyboard_accelerator_flag)
    {
        //  This is really stupid ... I cannot for the life of me
        //  force Qt to have this (object) accept the keypresses
        //  automatically to the buttons they were assigned to in
        //  the constructor ... so ... repeat the same logic here
        switch(e->key())
        {
            case Key_Up:
                previous();
                break;
                
            case Key_Left:
                seekback();
                break;
                
            case Key_P:
                pause();
                break;
                
            case Key_Space:
                play();
                break;
                
            case Key_S:
                stop();
                break;
                
            case Key_Right:
                seekforward();
                break;
               
            case Key_Down:
                next();
                break;
                
            case Key_1:
                toggleRepeat();
                break; 

            case Key_2:
                toggleShuffle();
                break; 

        }
    }
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
       
        QString rating;
        for (int i = 0; i < (*it).Rating(); i++)
            rating.append("|");

        if (showrating)
            litem = new QListViewItem(playview, position, rating, 
                                      (*it).Artist(), (*it).Title(), timestr);
        else
            litem = new QListViewItem(playview, position, (*it).Artist(),
                                      (*it).Title(), timestr);
        listlist.prepend(litem);
        it--; count--;
    }

    jumpToItem(listlist.at(playlistindex));
}

void PlaybackBox::jumpToItem(void)
{
    if (playlist_timer->isActive())
        playlist_timer->stop();
    jumpToItem(listlist.at(playlistindex));
}

void PlaybackBox::jumpToItem(QListViewItem *curItem)
{
    if (curItem)
    {
        if (curItem->itemBelow())
            playview->ensureItemVisible(curItem->itemBelow());
        if (curItem->itemAbove())
            playview->ensureItemVisible(curItem->itemAbove());
        playview->setCurrentItem(curItem);
        playview->setSelected(curItem, true);
        playview->ensureItemVisible(curItem);
    }
}

double PlaybackBox::computeIntelligentWeight(Metadata & mdata, 
                                             double currentDateTime)
{
    int rating;
    int playcount;
    double lastplay;
    double ratingValue = 0;
    double playcountValue = 0;
    double lastplayValue = 0;

    rating = mdata.Rating();
    playcount = mdata.PlayCount();
    lastplay = mdata.LastPlay();
    ratingValue = (double)rating / 10;
    playcountValue = (double)playcount / 50;
    lastplayValue = (currentDateTime - lastplay) / currentDateTime * 2000;
    return (35 * ratingValue - 25 * playcountValue + 25 * lastplayValue +
            15 * (double)rand() / (RAND_MAX + 1.0));
}

void PlaybackBox::setupPlaylist(void)
{
    if (playlistorder.size() > 0)
        playlistorder.clear();

    if (playlistindex >= (int)plist->size())
    {
        playlistindex = 0;
        shuffleindex = 0;
    }

    if (plist->size() == 0)
    {
        curMeta = Metadata("dummy.music");
        shuffleindex = 0;
        return;
    }

    // Reserve QValueVector memory up front
    // to decrease memory allocation overhead
    playlistorder.reserve(plist->size());

    if (shufflemode == 0)
    {
        for (int i = 0; i < (int)plist->size(); i++)
        {
            playlistorder.push_back(i);
            if (curMeta == (*plist)[i])
                shuffleindex = playlistindex = i;
        } 
    }
    else if (shufflemode == 1)
    {
        int max = plist->size();
        srand((unsigned int)time(NULL));

        int i;
        bool usedList[max];
        for (i = 0; i < max; i++)
            usedList[i] = false;

        int index = 0; 
        int lastindex = 0;

        for (i = 0; i < max; i++)
        {
            while (1)
            {
                index = (int)((double)rand() / (RAND_MAX + 1.0) * max);
                if (max - i > 50 && abs(index - lastindex) < 10)
                    continue;
                if (usedList[index] == false)
                    break;
            }
            usedList[index] = true;
            playlistorder.push_back(index);
            lastindex = index;

            if (curMeta == (*plist)[i])
                playlistindex = i;
            if (curMeta == (*plist)[index])
                shuffleindex = i;
        }
    }
    else if (shufflemode == 2)
    {
        int max = plist->size();
        srand((unsigned int)time(NULL));

        int i, j, temp;
        double tempd;
        QDateTime cTime = QDateTime::currentDateTime();
        double currentDateTime = cTime.toString("yyyyMMddhhmmss").toDouble();
        double weight;

        QValueVector<double> playlistweight;
        playlistweight.reserve(max);

        for (i = 0; i < max; i++)
        {
            weight = computeIntelligentWeight((*plist)[i], currentDateTime); 
            playlistweight.push_back(weight);
            playlistorder.push_back(i);
        }

        for (i = 0; i < (max - i); i++)
        {
            for (j = i + 1; j < max; j++)
            {
                if (playlistweight[j] > playlistweight[i])
                {
                    tempd = playlistweight[i];
                    playlistweight[i] = playlistweight[j];
                    playlistweight[j] = tempd;
 
                    temp = playlistorder[i];
                    playlistorder[i] = playlistorder[j];
                    playlistorder[j] = temp;
                }
            }
        }
    }

    playlistindex = playlistorder[shuffleindex];
    curMeta = (*plist)[playlistindex];
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
   
    if (output->isPaused())
    {
        pause();
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

    jumpToItem(listlist.at(playlistindex));

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
        curMeta.setLastPlay(db);
        curMeta.incPlayCount(db);    
   
        playlist_timer->start(1, true);
 
        gContext->LCDswitchToChannel(curMeta.Artist(), curMeta.Title(), "");
    }
}

void PlaybackBox::visEnable()
{
    if (!visualizer_is_active && isplaying)
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
        isplaying = !isplaying;
        pauseb->setOn(!isplaying);
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
    listlock.lock();

    if (isplaying == true)
        curMeta.decRating(db);

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

void PlaybackBox::toggleShuffle()
{
    shufflemode = (shufflemode + 1) % 3;

    setupPlaylist();

    if (shufflemode == 2)
        randomize->setText("Shuffle: Intelligent");
    else if (shufflemode == 1)
        randomize->setText("Shuffle: Random");
    else
        randomize->setText("Shuffle: Normal"); 

    if (keyboard_accelerator_flag)
        randomize->setAccel(Key_1);
}

void PlaybackBox::increaseRating()
{
    curMeta.incRating(db);

    QString rating;
    for (int i = 0; i < curMeta.Rating(); i++)
        rating.append("|");
    if (showrating)
    {
        QListViewItem *curItem = listlist.at(playlistindex);
        curItem->setText(1, rating);
    }
}

void PlaybackBox::decreaseRating()
{
    curMeta.decRating(db);

    QString rating;
    for (int i = 0; i < curMeta.Rating(); i++)
        rating.append("|");

    if (showrating)
    {
        QListViewItem *curItem = listlist.at(playlistindex);
        curItem->setText(1, rating);
    }
}

void PlaybackBox::toggleRepeat()
{
    repeatmode = !repeatmode;

    if (repeatmode)
        repeat->setText("Repeat: Track");
    else
        repeat->setText("Repeat: Playlist");

    if (keyboard_accelerator_flag)
        repeat->setAccel(Key_2);
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

