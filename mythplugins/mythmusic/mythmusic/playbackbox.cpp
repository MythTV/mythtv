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
#include <qaction.h>
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

class PlaybackListViewItem: public QListViewItem {
public:
    PlaybackListViewItem(QListView * parent,
                         QString label1, QString label2 = QString::null,
                         QString label3 = QString::null, QString label4 = QString::null,
                         QString label5 = QString::null, QString label6 = QString::null,
                         QString label7 = QString::null, QString label8 = QString::null):
        QListViewItem(parent, label1, label2, label3, label4, label5, label6, label7, label8) {};

    int compare(QListViewItem* i, int col, bool ascending) const 
    {
        if (col == 0) {
            (void)ascending;
            int a = text(col).toInt();
            int b = i->text(col).toInt();
            if (a < b)
               return -1;
            if (a > b)
                return 1;
            return 0;
        } else 
            return QListViewItem::compare(i,col,ascending);
    };
};

PlaybackBox::PlaybackBox(PlaylistsContainer *the_playlists,
                         AllMusic *the_music,
                         QWidget *parent, const char *name)
           : MythDialog(parent, name),
             input(NULL), output(NULL), decoder(NULL), plist(NULL),
             shufflelabel(NULL), repeatlabel(NULL), timelabel(NULL),
             titlelabel(NULL), playview(NULL), seekbar(NULL), randomize(NULL),
             repeat(NULL), pledit(NULL), vis(NULL), pauseb(NULL), prevb(NULL),
             prevfileb(NULL), stopb(NULL), nextb(NULL), nextfileb(NULL), 
             rateup(NULL), ratedn(NULL), playb(NULL), prevfileAction(NULL), 
             prevAction(NULL), pauseAction(NULL), playAction(NULL), 
             stopAction(NULL), nextAction(NULL), nextfileAction(NULL), 
             rateupAction(NULL), ratednAction(NULL), shuffleAction(NULL), 
             repeatAction(NULL), pleditAction(NULL), visAction(NULL), 
             timeDisplaySelect(NULL), playlistViewAction(NULL),
             visual_mode_timer(NULL), lcd_update_timer(NULL),
             playlist_timer(NULL)
{
    wait_counter = 0;
    keyboard_accelerator_flag = gContext->GetNumSetting("KeyboardAccelerators");
    showrating = gContext->GetNumSetting("MusicShowRatings", 0);
    listAsShuffled = gContext->GetNumSetting("ListAsShuffled", 0);
    cycle_visualizer = gContext->GetNumSetting("VisualCycleOnSongChange", 0);

    QFont buttonfont("Arial", (int)((gContext->GetSmallFontSize() + 2) * hmult),
                     QFont::Bold);

    all_playlists = the_playlists;
    all_music = the_music;

    dummy_data = Metadata("dummy_data");
   
    QGridLayout *vbox = new QGridLayout(this, 2, 1, (int)(20 * wmult), 
                                        (int)(10 * wmult));

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

    QFont statusFont = font();
    statusFont.setPointSize(statusFont.pointSize() - 2);

    framehbox->addWidget(new QLabel(topdisplay));

    infolabel = new QLabel(topdisplay);
    infolabel->setFont(statusFont);
    infolabel->setText("");
    infolabel->setPaletteBackgroundColor(palette().color(QPalette::Active,
                                                           QColorGroup::Mid));
    infolabel->setAlignment(Qt::AlignCenter);
    framehbox->addWidget(infolabel);

    ratinglabel = new QLabel(topdisplay);
    ratinglabel->setFont(statusFont);
    ratinglabel->setText("");
    ratinglabel->setPaletteBackgroundColor(palette().color(QPalette::Active,
                                                           QColorGroup::Mid));
    ratinglabel->setAlignment(Qt::AlignRight);

    framehbox->addWidget(ratinglabel);

    framehbox = new QHBoxLayout(framebox);

    shufflelabel = new QLabel(topdisplay);
    shufflelabel->setFont(statusFont);
    shufflelabel->setText("");
    shufflelabel->setPaletteBackgroundColor(palette().color(QPalette::Active,
                                                            QColorGroup::
                                                            Mid));
    shufflelabel->setAlignment(AlignLeft);
    framehbox->addWidget(shufflelabel);

    repeatlabel = new QLabel(topdisplay);
    repeatlabel->setFont(statusFont);
    repeatlabel->setText("");
    repeatlabel->setPaletteBackgroundColor(palette().color(QPalette::Active,
                                                           QColorGroup::Mid));
    framehbox->addWidget(repeatlabel);

    timelabel = new QLabel(topdisplay);
    timelabel->setFont(statusFont);
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

    prevfileAction = new QAction(this, "prevfileAction");
    connect(prevfileAction, SIGNAL(activated()), this, SLOT(previous()));

    prevAction = new QAction(this, "prevAction");
    connect(prevAction, SIGNAL(activated()), this, SLOT(seekback()));

    pauseAction = new QAction(this, "pauseAction");
    connect(pauseAction, SIGNAL(activated()), this, SLOT(pause()));

    playAction = new QAction(this, "playAction");
    connect(playAction, SIGNAL(activated()), this, SLOT(play()));

    stopAction = new QAction(this, "stopAction");
    connect(stopAction, SIGNAL(activated()), this, SLOT(stop()));

    nextAction = new QAction(this, "nextAction");
    connect(nextAction, SIGNAL(activated()), this, SLOT(seekforward()));

    nextfileAction = new QAction(this, "nextfileAction");
    connect(nextfileAction, SIGNAL(activated()), this, SLOT(next()));

    rateupAction = new QAction(this, "rateupAction");
    connect(rateupAction, SIGNAL(activated()), this, SLOT(increaseRating()));

    ratednAction = new QAction(this, "ratednAction");
    connect(ratednAction, SIGNAL(activated()), this, SLOT(decreaseRating()));

    shuffleAction = new QAction(this, "shuffleAction");
    connect(shuffleAction, SIGNAL(activated()), this, SLOT(toggleShuffle()));

    repeatAction = new QAction(this, "repeatAction");
    connect(repeatAction, SIGNAL(activated()), this, SLOT(toggleRepeat()));

    pleditAction = new QAction(this, "pleditAction");
    connect(pleditAction, SIGNAL(activated()), this, SLOT(editPlaylist()));

    visAction = new QAction(this, "visAction");
    connect(visAction, SIGNAL(activated()), this, SLOT(visEnable()));

    timeDisplaySelect = new QAction(this, "timeDisplaySelect");
    connect(timeDisplaySelect, SIGNAL(activated()), this, SLOT(toggleTime()));

    playlistViewAction = new QAction(this, "playlistViewAction");
    connect(playlistViewAction, SIGNAL(activated()), this,
            SLOT(togglePlaylistView()));

    if (!keyboard_accelerator_flag) 
    {
        QHBoxLayout *controlbox = new QHBoxLayout(vbox2, (int)(2 * wmult));

        prevfileb = new MythToolButton(this);
        prevfileb->setAutoRaise(true);
        prevfileb->setIconSet(scalePixmap((const char **) prevfile_pix));
        connect(prevfileb, SIGNAL(clicked()), prevfileAction,
                SIGNAL(activated()));

        prevb = new MythToolButton(this);
        prevb->setAutoRaise(true);
        prevb->setIconSet(scalePixmap((const char **) prev_pix));
        connect(prevb, SIGNAL(clicked()), prevAction, SIGNAL(activated()));

        pauseb = new MythToolButton(this);
        pauseb->setAutoRaise(true);
        pauseb->setToggleButton(true);
        pauseb->setIconSet(scalePixmap((const char **) pause_pix));
        connect(pauseb, SIGNAL(clicked()), pauseAction, SIGNAL(activated()));

        playb = new MythToolButton(this);
        playb->setAutoRaise(true);
        playb->setIconSet(scalePixmap((const char **) play_pix));
        connect(playb, SIGNAL(clicked()), playAction, SIGNAL(activated()));

        stopb = new MythToolButton(this);
        stopb->setAutoRaise(true);
        stopb->setIconSet(scalePixmap((const char **) stop_pix));
        connect(stopb, SIGNAL(clicked()), stopAction, SIGNAL(activated()));

        nextb = new MythToolButton(this);
        nextb->setAutoRaise(true);
        nextb->setIconSet(scalePixmap((const char **) next_pix));
        connect(nextb, SIGNAL(clicked()), nextAction, SIGNAL(activated()));

        nextfileb = new MythToolButton(this);
        nextfileb->setAutoRaise(true);
        nextfileb->setIconSet(scalePixmap((const char **) nextfile_pix));
        connect(nextfileb, SIGNAL(clicked()), nextfileAction,
                SIGNAL(activated()));

        rateup = NULL;
        ratedn = NULL;

        if (showrating) 
        {
            rateup = new MythToolButton(this);
            rateup->setAutoRaise(true);
            rateup->setIconSet(scalePixmap((const char **) rateup_pix));
            connect(rateup, SIGNAL(clicked()), rateupAction,
                    SIGNAL(activated()));

            ratedn = new MythToolButton(this);
            ratedn->setAutoRaise(true);
            ratedn->setIconSet(scalePixmap((const char **) ratedn_pix));
            connect(ratedn, SIGNAL(clicked()), ratednAction,
                    SIGNAL(activated()));
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

        randomize = new MythToolButton(this);
        randomize->setAutoRaise(true);
        randomize->setText("Shuffle Mode");
        randomize->setFont(buttonfont);
        secondcontrol->addWidget(randomize);
        connect(randomize, SIGNAL(clicked()), shuffleAction, 
                SIGNAL(activated()));

        repeat = new MythToolButton(this);
        repeat->setAutoRaise(true);
        repeat->setText("Repeat Mode");
        repeat->setFont(buttonfont);
        secondcontrol->addWidget(repeat);
        connect(repeat, SIGNAL(clicked()), repeatAction, SIGNAL(activated()));

        pledit = new MythToolButton(this);
        pledit->setAutoRaise(true);
        pledit->setText("Edit Playlist");
        pledit->setFont(buttonfont);
        secondcontrol->addWidget(pledit);
        connect(pledit, SIGNAL(clicked()), pleditAction, SIGNAL(activated()));

        vis = new MythToolButton(this);
        vis->setAutoRaise(true);
        vis->setText("Visualize");
        vis->setFont(buttonfont);
        secondcontrol->addWidget(vis);
        connect(vis, SIGNAL(clicked()), visAction, SIGNAL(activated()));
    } 
    else 
    {
        // There may be a better key press
        // mapping, but I have a pretty
        // serious flu at the moment and 
        // I can't think of one
        prevfileAction->setAccel(Key_Up);
        prevAction->setAccel(Key_Left);
        pauseAction->setAccel(Key_P);
        playAction->setAccel(Key_Space);
        stopAction->setAccel(Key_S);
        nextAction->setAccel(Key_Right);
        nextfileAction->setAccel(Key_Down);

        if (showrating) 
        {
            rateupAction->setAccel(Key_U);
            ratednAction->setAccel(Key_D);
        }

        shuffleAction->setAccel(Key_1);
        repeatAction->setAccel(Key_2);
        pleditAction->setAccel(Key_3);
        visAction->setAccel(Key_4);
    }

    timeDisplaySelect->setAccel(Key_T);
    playlistViewAction->setAccel(Key_L);

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
        playview->setColumnWidth(1, (int)(75 * wmult));
        playview->setColumnWidth(2, (int)(195 * wmult));
        playview->setColumnWidth(3, (int)(345 * wmult));
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

    //plist = playlist;
    plist = new QPtrList <Metadata>;

    //  
    //  Write the playlist if the data's there
    //
    if(all_playlists->doneLoading())
    {
        all_playlists->writeActive(plist);
    }
    else
    {
        //  I dunno, maybe a timer to keep checking
        
        waiting_for_playlists_timer = new QTimer();
        connect(waiting_for_playlists_timer, SIGNAL(timeout()), this, SLOT(checkForPlaylists()));
        waiting_for_playlists_timer->start(300);
    }


    playlistindex = 0;
    shuffleindex = 0;
    setupListView();

    vbox->addWidget(playview, 1, 0);
    vbox->setRowStretch(0, 0);
    vbox->setRowStretch(1, 10);

    if (!keyboard_accelerator_flag)
    {
        playb->setFocus();
    }

    input = 0; 
    decoder = 0; 
    seeking = false; 
    output = 0; 
    outputBufferSize = 256;
    plTime = 0;
    plElapsed = 0;
    timeMode = TRACK_ELAPSED;

    setRepeatMode(REPEAT_ALL);

    curMeta = &dummy_data;

    QString playmode = gContext->GetSetting("PlayMode");
    if (playmode.lower() == "random")
        setShuffleMode(SHUFFLE_RANDOM);
    else if (playmode.lower() == "intelligent")
        setShuffleMode(SHUFFLE_INTELLIGENT);
    else
        setShuffleMode(SHUFFLE_OFF);

    if(plist->count() > 0)
        playlistindex = playlistorder[shuffleindex];

    // this is a hack to fix the playlist's refusal to update w/o a SIG
    playlist_timer = new QTimer();
    connect(playlist_timer, SIGNAL(timeout()), this, SLOT(jumpToItem()));

    isplaying = false;
 
    if (plist->count() > 0)
    { 
        playfile = curMeta->Filename();
        emit play();
    }
    
    //
    // Load Visualization settings and set up timer
    //
	
    visual_mode = gContext->GetSetting("VisualMode");
    QString visual_delay = gContext->GetSetting("VisualModeDelay");
    
    bool delayOK;
    visual_mode_delay = visual_delay.toInt(&delayOK);
    if(!delayOK)
    	visual_mode_delay = 0;
    
    visual_mode_timer = new QTimer();
    if(visual_mode_delay > 0)
    {
        visual_mode_timer->start(visual_mode_delay * 1000);
        connect(prevfileAction, SIGNAL(activated()), this,
                SLOT(resetTimer()));
        connect(pauseAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(playAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(stopAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(nextAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(nextfileAction, SIGNAL(activated()), this,
                SLOT(resetTimer()));
        connect(shuffleAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(repeatAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(pleditAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(visAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(rateupAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(ratednAction, SIGNAL(activated()), this, SLOT(resetTimer()));
        connect(timeDisplaySelect, SIGNAL(activated()), this,
                SLOT(resetTimer()));
        connect(playlistViewAction, SIGNAL(activated()), this,
                SLOT(resetTimer()));
    }

    visualizer_is_active = false;
    connect(visual_mode_timer, SIGNAL(timeout()), this, SLOT(visEnable()));

    connect(mainvisual, SIGNAL(hidingVisualization()), 
            this, SLOT(restartTimer()));
    connect(mainvisual, SIGNAL(keyPress(QKeyEvent *)),
            this, SLOT(keyPressFromVisual(QKeyEvent *)));
}

void PlaybackBox::checkForPlaylists()
{
    if(all_playlists->doneLoading())
    {
        waiting_for_playlists_timer->stop();    
        listlock.lock();

        all_playlists->writeActive(plist);
        setupPlaylist();
    
        listlock.unlock();
        setupListView();
        if (listAsShuffled)
            sortListAsShuffled();

        emit play();
    }
    else
    {
        wait_counter++;
        QString a_string = "";
        for(int j = 0 ; j < wait_counter % 4; j++)
        {
            a_string += ".";
        }
        a_string += "Loading Playlist";
        for(int i = 0 ; i < wait_counter % 4; i++)
        {
            a_string += ".";
        }
        titlelabel->setText(a_string);
    }
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

            case Key_6:
                CycleVisualizer();
                break;
        }
    }
}

void PlaybackBox::resetTimer()
{
    if (visual_mode_delay > 0)
        visual_mode_timer->changeInterval(visual_mode_delay * 1000);
}

void PlaybackBox::restartTimer()
{
    if (visual_mode_delay > 0)
        visual_mode_timer->start(visual_mode_delay * 1000);
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

    int count = plist->count();
    if (count == 0)
        return;

    QListViewItem *litem;

    QPtrListIterator<Metadata> it(*plist);
    it.toLast();
    Metadata *searcher;
    while ((searcher = it.current()) != 0)   
    {
        --it;
        QString position = QString("%1").arg(count);
        int secs = searcher->Length() / 1000;
        int min = secs / 60;
        secs -= min * 60;
        
        QString timestr;
        timestr.sprintf("%2d:%02d", min, secs);
       
        QString rating;
        for (int i = 0; i < searcher->Rating(); i++)
            rating.append("|");

        if (showrating)
            litem = new PlaybackListViewItem(playview, position, rating, 
                                             searcher->Artist(), 
                                             searcher->Title(), timestr);
        else
            litem = new PlaybackListViewItem(playview, position, searcher->Artist(),
                                             searcher->Title(), timestr);
        listlist.prepend(litem);
        count--;
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

double PlaybackBox::computeIntelligentWeight(Metadata *mdata, 
                                             double currentDateTime)
{
    int rating;
    int playcount;
    double lastplay;
    double ratingValue = 0;
    double playcountValue = 0;
    double lastplayValue = 0;

    rating = mdata->Rating();
    playcount = mdata->PlayCount();
    lastplay = mdata->LastPlay();
    ratingValue = (double)rating / 10;
    playcountValue = (double)playcount / 50;
    lastplayValue = (currentDateTime - lastplay) / currentDateTime * 2000;
    return (35 * ratingValue - 25 * playcountValue + 25 * lastplayValue +
            15 * (double)rand() / (RAND_MAX + 1.0));
}

void PlaybackBox::sortListAsShuffled(void)
{
    int max = playlistorder.size();

    for(int x = 0; x < max; x++)
        listlist.at(playlistorder[x])->setText(0, QString::number(x));

    playview->setSorting(0);
    playview->sort();
    jumpToItem(listlist.at(playlistorder[shuffleindex]));
}

void PlaybackBox::setupPlaylist(void)
{
    if (playlistorder.size() > 0)
        playlistorder.clear();

    plTime = 0;
    plElapsed = 0;

    if (playlistindex >= (int)plist->count())
    {
        playlistindex = 0;
        shuffleindex = 0;
    }

    if (plist->count() == 0)
    {
        shuffleindex = 0;
        return;
    }

    // Reserve QValueVector memory up front
    // to decrease memory allocation overhead
    playlistorder.reserve(plist->count());

    bool doing_elapsed = true;

    if (curMeta == &dummy_data)
        doing_elapsed = false;

    if (shufflemode == 0)
    {
        for (int i = 0; i < (int)plist->count(); i++)
        {
            Metadata *meta = (plist->at(i));
            plTime += meta->Length() / 1000;
            playlistorder.push_back(i);
            if (*curMeta == (*meta))
            {
                shuffleindex = playlistindex = i;
                doing_elapsed = false;
            }
            else if (doing_elapsed)
                plElapsed += meta->Length() / 1000;
        } 
    }
    else if (shufflemode == 1)
    {
        int max = plist->count();
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
            int length = plist->at(index)->Length() / 1000;
            plTime += length;
            playlistorder.push_back(index);
            lastindex = index;

            if (*curMeta == (*plist->at(i)))
                playlistindex = i;
            if (*curMeta == (*plist->at(index)))
            {
                shuffleindex = i;
                doing_elapsed = false;
            }
            else if (doing_elapsed)
                plElapsed += length;
        }
    }
    else if (shufflemode == 2)
    {
        int max = plist->count();
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
            weight = computeIntelligentWeight(plist->at(i), currentDateTime); 
            playlistweight.push_back(weight);
            playlistorder.push_back(i);
            plTime += plist->at(i)->Length() / 1000;
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

        for (i = 0; i < max && doing_elapsed; i++)
        {
            if (*curMeta == *plist->at(i))
            {
                shuffleindex = i;
                doing_elapsed = false;
            } 
            else
                plElapsed += plist->at(i)->Length() / 1000;
        }
    }

    if (plist->count() > 0)
    {
        for (unsigned int i = 0, j = 0; i < plist->count(); i++)
        {
            j = playlistorder[i];
            if (curMeta == &(*plist->at(j)))
            {
                shuffleindex = i;
                break;
            }
        }

        playlistindex = playlistorder[shuffleindex];
        curMeta = &(*plist->at(playlistindex));
    }
    else
    {
        curMeta = &dummy_data;
    }
}

void PlaybackBox::play()
{
    if (isplaying)
        stop();

    if (plist->count() == 0)
        return;

    playfile = curMeta->Filename();

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
 
    if (!sourceurl.isLocalFile()) 
    {
        StreamInput streaminput(sourceurl);
        streaminput.setup();
        input = streaminput.socket();
    } else
        input = new QFile(sourceurl.toString(FALSE, FALSE));

    if (decoder && !decoder->factory()->supports(sourcename))
        decoder = 0;

    if (!decoder) 
    {
        decoder = Decoder::create(sourcename, input, output);

        if (!decoder) 
        {
            printf("mythmusic: unsupported fileformat\n");
            stopAll();
            return;
        }

        decoder->setBlockSize(globalBlockSize);
        decoder->addListener(this);
    } 
    else 
    {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    QString disp = curMeta->Artist() + "  ~  " + curMeta->Album() + 
                   "  ~   " + curMeta->Title();
    titlelabel->setText(disp);

    if (showrating) 
    {
        ratingString.sprintf("Rating: %2d / 10", curMeta->Rating());
        ratinglabel->setText(ratingString);
    }

    jumpToItem(listlist.at(playlistindex));

    currentTime = 0;
    maxTime = curMeta->Length() / 1000;

    mainvisual->setDecoder(decoder);
    mainvisual->setOutput(output);
    
    if (decoder->initialize()) 
    {
        seekbar->setMinValue(0);
        seekbar->setValue(0);
        seekbar->setMaxValue(maxTime);
        if (seekbar->maxValue() == 0)
            seekbar->setEnabled(false);
        else
            seekbar->setEnabled(true);
 
        if (output)
        {
            if (startoutput)
                output->start();
            else
                output->resetTime();
        }

        decoder->start();

        isplaying = true;
        curMeta->setLastPlay();
        curMeta->incPlayCount();    
   
        playlist_timer->start(1, true);
 
        gContext->LCDswitchToChannel(curMeta->Artist(), curMeta->Title(), "");
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

void PlaybackBox::CycleVisualizer()
{
    QString new_visualizer;

    //Only change the visualizer if there is more than 1 visualizer
    // and the user currently has a visualizer active
    if (mainvisual->numVisualizers() > 1 && visualizer_is_active)
    {
        if (visual_mode != "Random")
        {
            QStringList allowed_modes = QStringList::split(",", visual_mode);
            //Find a visual thats not like the previous visual
            do
            {
                new_visualizer =  allowed_modes[rand() % allowed_modes.size()];
            } while (new_visualizer == mainvisual->getCurrentVisual());
        }
        else
            new_visualizer = visual_mode;

        //Change to the new visualizer
        visual_mode_timer->stop();
        mainvisual->setVisual("Blank");
        mainvisual->showFullScreen();
        mainvisual->setVisual(new_visualizer);
        visualizer_is_active = true;
    }
}

void PlaybackBox::pause(void)
{
    if(plist->count() < 1)
    {
        if (pauseb)
            pauseb->setOn(false);
        return;
    }
    if (output) 
    {
        output->mutex()->lock();
        output->pause();
        isplaying = !isplaying;
        if (pauseb)
            pauseb->setOn(!isplaying);
        output->mutex()->unlock();
    }

    // wake up threads
    if (decoder) 
    {
        decoder->mutex()->lock();
        decoder->cond()->wakeAll();
        decoder->mutex()->unlock();
    }

    if (output) 
    {
        output->recycler()->mutex()->lock();
        output->recycler()->cond()->wakeAll();
        output->recycler()->mutex()->unlock();
    }

    if (isplaying)
        infolabel->setText("");
//        infolabel->setText(infoString);
    else if (infolabel->text().compare("Stopped") != 0)
            infolabel->setText("Paused");
}

void PlaybackBox::stopDecoder(void)
{
    if (decoder && decoder->running()) 
    {
        decoder->mutex()->lock();
        decoder->stop();
        decoder->mutex()->unlock();
    }

    if (decoder) 
    {
        decoder->mutex()->lock();
        decoder->cond()->wakeAll();
        decoder->mutex()->unlock();
    }

    if (decoder)
        decoder->wait();
}

void PlaybackBox::stop(void)
{
    if (decoder && decoder->running()) 
    {
        decoder->mutex()->lock();
        decoder->stop();
        decoder->mutex()->unlock();
    }

    if (output && output->running()) 
    {
        output->mutex()->lock();
        output->stop();
        output->mutex()->unlock();
    }

    // wake up threads
    if (decoder) 
    {
        decoder->mutex()->lock();
        decoder->cond()->wakeAll();
        decoder->mutex()->unlock();
    }

    if (output) 
    {
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
    ratinglabel->setText(" ");
    infolabel->setText("Stopped");

    isplaying = false;
}

void PlaybackBox::stopAll()
{
    gContext->LCDswitchToTime();
    stop();

    if (decoder) 
    {
        decoder->removeListener(this);
        decoder = 0;
    }
}

void PlaybackBox::previous()
{
    if(plist->count() < 1)
        return;
    listlock.lock();

    shuffleindex--;
    if (shuffleindex < 0)
    {
        shuffleindex = plist->count() - 1;
        plElapsed = plTime;
    }

    playlistindex = playlistorder[shuffleindex];

    curMeta = &(*plist->at(playlistindex));
    plElapsed -= curMeta->Length() / 1000;

    listlock.unlock();

    play();

    if (visualizer_is_active && cycle_visualizer)
        CycleVisualizer();
}

void PlaybackBox::next()
{
    if(plist->count() < 1)
        return;
        
    listlock.lock();

    plElapsed += curMeta->Length() / 1000;

    shuffleindex++;
    if (shuffleindex >= (int)plist->count())
    {
        shuffleindex = 0;
        plElapsed = 0;
    }

    playlistindex = playlistorder[shuffleindex];

    curMeta = &(*plist->at(playlistindex));

    listlock.unlock();

    if (shuffleindex == 0 && repeatmode == REPEAT_OFF)
        stop();
    else
    {
        play();

        if (visualizer_is_active && cycle_visualizer)
            CycleVisualizer();
    }
}

void PlaybackBox::nextAuto()
{
    stopDecoder();

    isplaying = false;

    if (repeatmode == REPEAT_TRACK)
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
    if (output && output->running()) 
    {
        output->mutex()->lock();
        output->seek(pos);

        if (decoder && decoder->running()) 
        {
            decoder->mutex()->lock();
            decoder->seek(pos);

            if (mainvisual) 
            {
                mainvisual->mutex()->lock();
                mainvisual->prepare();
                mainvisual->mutex()->unlock();
            }
            decoder->mutex()->unlock();
        }
        output->mutex()->unlock();
    }
}

void PlaybackBox::setShuffleMode(unsigned int mode)
{
    shufflemode = mode;

    setupPlaylist();
    if (listAsShuffled)
        sortListAsShuffled();

    switch (shufflemode) {
        case SHUFFLE_INTELLIGENT:
            shufflelabel->setText("Shuffle: Intelligent");
            break;
        case SHUFFLE_RANDOM:
            shufflelabel->setText("Shuffle: Random");
            break;
        default:
            shufflelabel->setText("");
            break;
    }
}

void PlaybackBox::toggleShuffle(void)
{
    setShuffleMode(++shufflemode % MAX_SHUFFLE_MODES);
}

void PlaybackBox::setTimeMode(unsigned int mode)
{
    timeMode = mode;
}

void PlaybackBox::toggleTime()
{
    setTimeMode(++timeMode % MAX_TIME_DISPLAY_MODES);
}

void PlaybackBox::increaseRating()
{
    if(plist->count() < 1)
        return;
    curMeta->incRating();

    QString rating;
    for (int i = 0; i < curMeta->Rating(); i++)
        rating.append("|");
    if (showrating)
    {
        QListViewItem *curItem = listlist.at(playlistindex);
        curItem->setText(1, rating);
        ratingString.sprintf("Rating: %2d / 10", curMeta->Rating());
        ratinglabel->setText(ratingString);
    }
}

void PlaybackBox::decreaseRating()
{
    if(plist->count() < 1)
        return;
    curMeta->decRating();

    QString rating;
    for (int i = 0; i < curMeta->Rating(); i++)
        rating.append("|");

    if (showrating)
    {
        QListViewItem *curItem = listlist.at(playlistindex);
        curItem->setText(1, rating);
        ratingString.sprintf("Rating: %2d / 10", curMeta->Rating());
        ratinglabel->setText(ratingString);
    }
}

void PlaybackBox::togglePlaylistView()
{
    if (playview->isVisible())
        playview->hide();
    else
        playview->show();
}

void PlaybackBox::setRepeatMode(unsigned int mode)
{
    repeatmode = mode;

    switch (repeatmode) {
        case REPEAT_TRACK:
            repeatlabel->setText("Repeat: Track");
            break;
        case REPEAT_ALL:
            repeatlabel->setText("Repeat: All");
            break;
        default:
            repeatlabel->setText("");
            break;
    }
}

void PlaybackBox::toggleRepeat()
{
    setRepeatMode(++repeatmode % MAX_REPEAT_MODES);
}

void PlaybackBox::editPlaylist()
{
    Metadata *firstdata;
    firstdata = curMeta;
    
    DatabaseBox dbbox(all_playlists, all_music);

    visual_mode_timer->stop();
    dbbox.Show();
    dbbox.exec();
    if (visual_mode_delay > 0)
        visual_mode_timer->start(visual_mode_delay * 1000);


    if(all_playlists->doneLoading())
    {
        listlock.lock();
        all_playlists->writeActive(plist);
        setupPlaylist();
    
        listlock.unlock();
        setupListView();
        if (listAsShuffled)
            sortListAsShuffled();

        if (isplaying && firstdata != curMeta)
        {
            stop();
            if (plist->count() > 0)
            {
                playlistindex = playlistorder[shuffleindex];
                curMeta = &(*plist->at(playlistindex));
                play();
            }
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
    switch ((int)event->type()) 
    {
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

            int eh, em, es, rs, ts;
            float percent_heard;
            char *timeModeString = NULL;

            currentTime = rs = ts = oe->elapsedSeconds();

            switch (timeMode)
            {
                case TRACK_REMAIN:
                    ts = (curMeta->Length() / 1000) - ts;
                    timeModeString = "                -";
                    break;
                case PLIST_ELAPSED:
                    ts = plElapsed + ts;
                    timeModeString = "Total Elapsed:   ";
                    break;
                case PLIST_REMAIN:
                    ts = plTime - plElapsed - ts;
                    timeModeString = "Total Remaining: ";
                    break;
                case TRACK_ELAPSED:
                default:
                    timeModeString = "                 ";
                    break;
            }

            eh = ts / 3600;
            em = (ts / 60) % 60;
            es = ts % 60;

            if (0 < eh)
                timeString.sprintf("%s%5d:%02d:%02d", timeModeString, eh, em,
                                   es);
            else
                timeString.sprintf("%s      %2d:%02d", timeModeString, em, es);


            percent_heard = ((float)rs / (float)curMeta->Length()) * 1000.0;

            gContext->LCDsetChannelProgress(percent_heard);
            if (!seeking)
                seekbar->setValue(oe->elapsedSeconds());

            infoString.sprintf("%d kbps, %.1f kHz %s.",
                               oe->bitrate(), float(oe->frequency()) / 1000.0,
                               oe->channels() > 1 ? "stereo" : "mono");

            timelabel->setText(timeString);

            if (isplaying)
                infolabel->setText("");
//                infolabel->setText(infoString);

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

