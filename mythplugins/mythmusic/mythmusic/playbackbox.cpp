#include <qapplication.h>
#include <qregexp.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

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

PlaybackBox::PlaybackBox(MythMainWindow *parent, QString window_name,
                         QString theme_filename, 
                         PlaylistsContainer *the_playlists,
                         AllMusic *the_music, const char *name)

           : MythThemedDialog(parent, window_name, theme_filename, name)
{
    //
    //  A few internal variable defaults
    //
 
    input = NULL;
    output = NULL;
    decoder = NULL;
    mainvisual = NULL;
    visual_mode_timer = NULL;
    lcd_update_timer = NULL;
    waiting_for_playlists_timer = NULL;
    playlist_tree = NULL;
 
    isplaying = false;
    outputBufferSize = 256;
    currentTime = 0;
    maxTime = 0;
    setContext(0);
    visual_mode_timer = new QTimer(this);
    visualizer_status = 0;
    curMeta = NULL;

    //
    //  Set our pointers the playlists and the
    //  metadata containers
    //

    all_playlists = the_playlists;
    all_music = the_music;

    //
    //  Get some user set options
    //

    show_whole_tree = gContext->GetNumSetting("ShowWholeTree", 1);
    keyboard_accelerators = gContext->GetNumSetting("KeyboardAccelerators", 1);
    if(!keyboard_accelerators)
    {
        show_whole_tree = false;
    }
    showrating = gContext->GetNumSetting("MusicShowRatings", 0);
    listAsShuffled = gContext->GetNumSetting("ListAsShuffled", 0);
    cycle_visualizer = gContext->GetNumSetting("VisualCycleOnSongChange", 0);

    //
    //  Through the magic of themes, our "GUI" already exists
    //  we just need to wire up it
    //

    wireUpTheme();

    //
    //  Possibly (user-defined) control the volume
    //
    
    volume_control = NULL;
    volume_display_timer = new QTimer(this);
    if(gContext->GetNumSetting("MythControlsVolume", 0))
    {
        volume_control = new VolumeControl(true);
        volume_display_timer->start(2000);
        connect(volume_display_timer, SIGNAL(timeout()), this, SLOT(hideVolume()));
    }
    
    //
    //  Figure out the shuffle mode
    //

    QString playmode = gContext->GetSetting("PlayMode");
    if (playmode.lower() == "random")
    {
        setShuffleMode(SHUFFLE_RANDOM);
    }
    else if (playmode.lower() == "intelligent")
    {
        setShuffleMode(SHUFFLE_INTELLIGENT);
    }
    else
    {
        setShuffleMode(SHUFFLE_OFF);
    }

    
    //
    //  Set some button values
    //
    
    if (!keyboard_accelerators) 
    {
        pledit_button->setText(tr("Edit Playlist"));
        vis_button->setText(tr("Visualize"));
        if(!assignFirstFocus())
        {
            cerr << "playbackbox.o: Could not find a button to assign focus to. What's in your theme?" << endl;
            exit(0);
        }
    } 
    else 
    {
        pledit_button->setText(tr("3 Edit Playlist"));
        vis_button->setText(tr("4 Visualize"));
    }

    //  
    //  We set a timer to load the playlists. We do this
    //  for two reasons: (1) the playlists may not be fully
    //  loaded, and (2) even if they are fully loaded, they
    //  do take a while to write out a GenericTree for navigation
    //  use, and that slows down the appearance of this dialog
    //  if we were to do it right here.
    //


    waiting_for_playlists_timer = new QTimer(this);
    connect(waiting_for_playlists_timer, SIGNAL(timeout()), this, SLOT(checkForPlaylists()));
    waiting_for_playlists_timer->start(100);

    setRepeatMode(REPEAT_ALL);
    
    //
    //  Warm up the visualizer
    //
    
    mainvisual = new MainVisual(this);
    mainvisual->setGeometry(visual_blackhole->getScreenArea());
    mainvisual->show();   
 
    visual_mode = gContext->GetSetting("VisualMode");
    visual_mode.simplifyWhiteSpace();
    visual_mode.replace(QRegExp("\\s"), ",");

    QString visual_delay = gContext->GetSetting("VisualModeDelay");
    bool delayOK;
    visual_mode_delay = visual_delay.toInt(&delayOK);
    if(!delayOK)
    {
    	visual_mode_delay = 0;
    }
    if(visual_mode_delay > 0)
    {
        visual_mode_timer->start(visual_mode_delay * 1000);
        connect(visual_mode_timer, SIGNAL(timeout()), this, SLOT(visEnable()));
    }
    visualizer_status = 1;

    //
    //  Temporary workaround for visualizer Bad X Requests
    //
    //  start on Blank, and then set the "real" mode after
    //  the playlist timer fires. Seems to work.
    //
    //  Suspicion: in most modes, SDL is not happy if the
    //  window doesn't fully exist yet  (????)
    //
    
    mainvisual->setVisual("Blank");
//    mainvisual->setVisual(visual_mode);

    //
    //  Ready to go. Let's update the foreground just to
    //  be safe.
    //

    updateForeground();
}

PlaybackBox::~PlaybackBox(void)
{
    stopAll();
    if (volume_control)
        delete volume_control;
    if (playlist_tree)
        delete playlist_tree;
}

void PlaybackBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    resetTimer();

    switch (e->key())
    {
        case Key_PageDown:
            next_button->push();
            handled = true;
            break;
        case Key_PageUp:
            prev_button->push();
            handled = true;
            break;
        case Key_F:
            ff_button->push();
            handled = true;
            break;
        case Key_R:
            rew_button->push();
            handled = true;
            break;
        case Key_P:
            if (isplaying)
                pause_button->push();
            else
                play_button->push();
            handled = true;
            break;
        case Key_S:
            stop_button->push();
            handled = true;
            break;
        case Key_U:
            increaseRating();
            handled = true;
            break;
        case Key_D:
            decreaseRating();
            handled = true;
            break;
        case Key_1:
            shuffle_button->push();
            handled = true;
            break;
        case Key_2:
            repeat_button->push();
            handled = true;
            break;
        case Key_3:
            pledit_button->push();
            handled = true;
            break;
        case Key_6:
            CycleVisualizer();
            handled = true;
            break;
        case '[':
            changeVolume(false);
            handled = true;
            break;
        case ']':
            changeVolume(true);
            handled = true;
            break;
        case '|':
            toggleMute();
            handled = true;
            break;
    }

    if (visualizer_status == 2)
    {
        if (e->key() == Key_Escape || e->key() == Key_4)
        {
            visualizer_status = 1;
            mainvisual->setGeometry(visual_blackhole->getScreenArea());
            setUpdatesEnabled(true);
            handled = true;
        }
    }
    else
    {
        if (keyboard_accelerators)
        {
            switch (e->key())
            {
                case Key_Up:
                    music_tree_list->moveUp();
                    handled = true;
                    break;
                case Key_Down:
                    music_tree_list->moveDown();
                    handled = true;
                    break;
                case Key_Left:
                    music_tree_list->popUp();
                    handled = true;
                    break;
                case Key_Right:
                    music_tree_list->pushDown();
                    handled = true;
                    break;
                case Key_0:
                case Key_Home:
                    music_tree_list->syncCurrentWithActive();
                    music_tree_list->forceLastBin();
                    music_tree_list->refresh();
                    handled = true;
                    break;
                case Key_4:
                    vis_button->push();
                    handled = true;
                    break;
                case Key_Space:
                case Key_Return:
                case Key_Enter:
                    music_tree_list->select();
                    handled = true;
                    break;
            }
        }
        else
        {
            switch (e->key())
            {
                case Key_Up:
                case Key_Left:
                    nextPrevWidgetFocus(false);
                    handled = true;
                    break;
                case Key_Down:
                case Key_Right:
                    nextPrevWidgetFocus(true);
                    handled = true;
                    break;
                case Key_Space:
                case Key_Return:
                case Key_Enter:
                    activateCurrent();
                    music_tree_list->syncCurrentWithActive();
                    handled = true;
                    break;
            }
        }
    }

    if(!handled)
    {
        MythThemedDialog::keyPressEvent(e);
    }
}

void PlaybackBox::checkForPlaylists()
{
    //
    //  This is only done off a timer on startup
    //

    if(all_playlists->doneLoading() &&
       all_music->doneLoading())
    {
        music_tree_list->showWholeTree(show_whole_tree);
        waiting_for_playlists_timer->stop();
        constructPlaylistTree();
        QValueList <int> branches_to_current_node;
        branches_to_current_node.append(0);                  //  Root node
        branches_to_current_node.append(1);                  //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0);                  //  Active play Queue
        music_tree_list->moveToNodesFirstChild(branches_to_current_node);
        music_tree_list->refresh();
        if(show_whole_tree)
        {
            setContext(1);
        }
        else
        {
            setContext(2);
        }
        updateForeground();
        mainvisual->setVisual(visual_mode);
    }
    else
    {
        //
        //  Visual Feedback ...
        //
    }


}

void PlaybackBox::changeVolume(bool up_or_down)
{
    if(volume_control)
    {
        if(up_or_down)
        {
            volume_control->AdjustCurrentVolume(2);
        }
        else
        {
            volume_control->AdjustCurrentVolume(-2);
        }
        showVolume(true);
    }
}

void PlaybackBox::toggleMute()
{
    if(volume_control)
    {
        volume_control->ToggleMute();
        showVolume(true);
    }
}

void PlaybackBox::showVolume(bool on_or_off)
{
    if(volume_control)
    {
        if(volume_status)
        {
            if(on_or_off)
            {
                volume_status->SetUsed(volume_control->GetCurrentVolume());
                volume_status->SetOrder(0);
                volume_status->refresh();
                volume_display_timer->changeInterval(2000);
            }
            else
            {
                volume_status->SetOrder(-1);
                volume_status->refresh();
            }
        }
    }
}

void PlaybackBox::resetTimer()
{
    if (visual_mode_delay > 0)
        visual_mode_timer->changeInterval(visual_mode_delay * 1000);
}

void PlaybackBox::play()
{
    if (isplaying)
    {
        stop();
    }

    if(curMeta)
    {
        playfile = curMeta->Filename();
    }
    else
    {
        //
        //  Perhaps we can descend to something playable?
        //
        wipeTrackInfo();
        return;
    }

    QUrl sourceurl(playfile);
    QString sourcename(playfile);

    bool startoutput = false;

    if (!output)
    {
        QString adevice = gContext->GetSetting("AudioDevice");

        output = new MMAudioOutput(outputBufferSize * 1024, adevice);
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
    } 
    else
    {
        input = new QFile(sourceurl.toString(FALSE, FALSE));
    }

    if (decoder && !decoder->factory()->supports(sourcename))
    {
        decoder = 0;
    }

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

    currentTime = 0;

    mainvisual->setDecoder(decoder);
    mainvisual->setOutput(output);
    
    if (decoder->initialize()) 
    {
        if (output)
        {
            if (startoutput)
            {
                output->start();
            }
            else
            {
                output->resetTime();
            }
        }

        decoder->start();

        isplaying = true;
        curMeta->setLastPlay();
        curMeta->incPlayCount();    
    }
}

void PlaybackBox::visEnable()
{
    if (!visualizer_status != 2 && isplaying)
    {
        setUpdatesEnabled(false);
        //visual_mode_timer->stop();
        mainvisual->setGeometry(0, 0, screenwidth, screenheight);
        visualizer_status = 2;
    }
}

void PlaybackBox::CycleVisualizer()
{
    QString new_visualizer;

    //Only change the visualizer if there is more than 1 visualizer
    // and the user currently has a visualizer active
    if (mainvisual->numVisualizers() > 1 && visualizer_status > 0)
    {
        if (visual_mode != "Random")
        {
            QStringList allowed_modes = QStringList::split(",", visual_mode);
            //Find a visual thats not like the previous visual
            do
            {
                new_visualizer =  allowed_modes[rand() % allowed_modes.size()];
            } 
            while (new_visualizer == mainvisual->getCurrentVisual() &&
                   allowed_modes.count() > 1);
        }
        else
        {
            new_visualizer = visual_mode;
        }

        //Change to the new visualizer
        visual_mode_timer->stop();
        mainvisual->setVisual("Blank");
        mainvisual->setVisual(new_visualizer);
    }
}


void PlaybackBox::pause(void)
{
    if (output) 
    {
        output->mutex()->lock();
        output->pause();
        isplaying = !isplaying;
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

    QString time_string;
    int maxh = maxTime / 3600;
    int maxm = (maxTime / 60) % 60;
    int maxs = maxm % 60;
    if(maxh > 0)
    {
        time_string.sprintf("%d:%02d:%02d", maxh, maxm, maxs);
    }
    else
    {
        time_string.sprintf("%02d:%02d", maxm, maxs);
    }
    time_text->SetText(time_string);
    info_text->SetText("");
    //ratinglabel->setText(" ");

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
    if(repeatmode == REPEAT_ALL)
    {
        if(music_tree_list->prevActive(true, show_whole_tree))
        {
            music_tree_list->activate();
        }
    }
    else
    {
        if(music_tree_list->prevActive(false, show_whole_tree))
        {
            music_tree_list->activate();
        }
    }
     
    if (visualizer_status > 0 && cycle_visualizer)
        CycleVisualizer();
}

void PlaybackBox::next()
{
    if(repeatmode == REPEAT_ALL)
    {
        //
        //  Grab the next track after this
        //  one. First flag is to wrap around
        //  to the beginning of the list. Second
        //  decides if we will traverse up and down
        //  tree.
        //
        if(music_tree_list->nextActive(true, show_whole_tree))
        {
            music_tree_list->activate();
        }
    }
    else
    {
        if(music_tree_list->nextActive(false, show_whole_tree))
        {
            music_tree_list->activate();
        }
    }
     
    if (visualizer_status > 0 && cycle_visualizer)
        CycleVisualizer();
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

    switch (shufflemode)
    {
        case SHUFFLE_INTELLIGENT:
            if (keyboard_accelerators)
                shuffle_button->setText(tr("1 Shuffle: Smart"));
            else
                shuffle_button->setText(tr("Shuffle: Smart"));
            music_tree_list->scrambleParents(true);
            break;
        case SHUFFLE_RANDOM:
            if (keyboard_accelerators)
                shuffle_button->setText(tr("1 Shuffle: Rand"));
            else
                shuffle_button->setText(tr("Shuffle: Rand"));
            music_tree_list->scrambleParents(true);
            break;
        default:
            if (keyboard_accelerators)
                shuffle_button->setText(tr("1 Shuffle: None"));
            else
                shuffle_button->setText(tr("Shuffle: None"));
            music_tree_list->scrambleParents(false);
            break;
    }
    music_tree_list->setTreeOrdering(shufflemode + 1);
    if(listAsShuffled)
    {
        music_tree_list->setVisualOrdering(shufflemode + 1);
    }
    else
    {
        music_tree_list->setVisualOrdering(1);
    }
    music_tree_list->refresh();
}

void PlaybackBox::toggleShuffle(void)
{
    setShuffleMode(++shufflemode % MAX_SHUFFLE_MODES);
}

void PlaybackBox::increaseRating()
{
    if(!curMeta)
    {
        return;
    }

    //
    //  Rationale here is that if you can't get 
    //  visual feedback on ratings adjustments,
    //  you probably should not be changing them
    //

    if(showrating)
    {
        curMeta->incRating();
        ratings_image->setRepeat(curMeta->Rating());
    }
}

void PlaybackBox::decreaseRating()
{
    if(!curMeta)
    {
        return;
    }
    if(showrating)
    {
        curMeta->decRating();
        ratings_image->setRepeat(curMeta->Rating());
    }
}

void PlaybackBox::setRepeatMode(unsigned int mode)
{
    repeatmode = mode;

    switch (repeatmode)
    {
        case REPEAT_ALL:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: All"));
            else
                repeat_button->setText(tr("Repeat: All"));
            break;
        case REPEAT_TRACK:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: Track"));
            else
                repeat_button->setText(tr("Repeat: Track"));
            break;
        default:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: None"));
            else
                repeat_button->setText(tr("Repeat: None"));
            break;
    }
}

void PlaybackBox::toggleRepeat()
{
    setRepeatMode(++repeatmode % MAX_REPEAT_MODES);
}

void PlaybackBox::constructPlaylistTree()
{
    if (playlist_tree)
        delete playlist_tree;

    playlist_tree = new GenericTree("playlist root", 0);
    playlist_tree->setAttribute(0, 0);
    playlist_tree->setAttribute(1, 0);
    playlist_tree->setAttribute(2, 0);
    playlist_tree->setAttribute(3, 0);

    //
    //  We ask the playlist object to write out the whole
    //  tree (all playlists and all music). It will set
    //  attributes for nodes in the tree, such as whether
    //  a node is selectable, how it can be ordered (normal,
    //  random, intelligent), etc. 
    //

    all_playlists->writeTree(playlist_tree);
    music_tree_list->assignTreeData(playlist_tree);
}

void PlaybackBox::editPlaylist()
{
    //
    //  Get a reference to the current track
    //  

    QValueList <int> branches_to_current_node;

    if(curMeta)
    {
        QValueList <int> *a_route;
        a_route = music_tree_list->getRouteToActive();
        branches_to_current_node = *a_route;
    }
    else
    {
        //
        //  No current metadata, so when we come back we'll
        //  try and play the first thing on the active queue
        //
        
        branches_to_current_node.clear();
        branches_to_current_node.append(0);                  //  Root node
        branches_to_current_node.append(1);                  //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0);                  //  Active play Queue
    }

    visual_mode_timer->stop();
    DatabaseBox dbbox(all_playlists, all_music, gContext->GetMainWindow(),
                      "database box");
    dbbox.exec();
    if (visual_mode_delay > 0)
        visual_mode_timer->start(visual_mode_delay * 1000);

    //
    //  OK, we're back ....
    //  now what do we do? see if we can find the same track 
    //  at the same level
    //

    constructPlaylistTree();
    if(music_tree_list->tryToSetActive(branches_to_current_node))
    {
        //  All is well
    }
    else
    {
        stop();
        wipeTrackInfo();
        branches_to_current_node.clear();
        branches_to_current_node.append(0);                  //  Root node
        branches_to_current_node.append(1);                  //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0);                  //  Active play Queue
        music_tree_list->moveToNodesFirstChild(branches_to_current_node);
    }
    music_tree_list->refresh();
    
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
            currentTime = rs = ts = oe->elapsedSeconds();

            eh = ts / 3600;
            em = (ts / 60) % 60;
            es = ts % 60;

            QString time_string;
            
            int maxh = maxTime / 3600;
            int maxm = (maxTime / 60) % 60;
            int maxs = maxTime % 60;
            
            if(maxh > 0)
            {
                time_string.sprintf("%d:%02d:%02d / %02d:%02d:%02d", eh, em, es, maxh, maxm, maxs);
            }
            else
            {
                time_string.sprintf("%02d:%02d / %02d:%02d", em, es, maxm, maxs);
            }
            
            //percent_heard = ((float)rs / (float)curMeta->Length()) * 1000.0;
            //gContext->LCDsetChannelProgress(percent_heard);

            QString info_string;

            //
            //  Hack around for cd bitrates
            //
            if(oe->bitrate() < 2000)
            {
                info_string.sprintf("%d kbps   %.1f kHz   %s ch",
                                   oe->bitrate(), float(oe->frequency()) / 1000.0,
                                   oe->channels() > 1 ? "2" : "1");
            }
            else
            {
                info_string.sprintf("%.1f kHz   %s ch",
                                   float(oe->frequency()) / 1000.0,
                                   oe->channels() > 1 ? "2" : "1");
            }
        
            if(curMeta)
            {
                time_text->SetText(time_string);
                info_text->SetText(info_string);
                current_visualization_text->SetText(mainvisual->getCurrentVisual());
            }

            break;
        }
    case OutputEvent::Error:
        {
            statusString = tr("Output error.");

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

void PlaybackBox::wipeTrackInfo()
{
        title_text->SetText("");
        artist_text->SetText("");
        album_text->SetText("");
        time_text->SetText("");
        info_text->SetText("");
        ratings_image->setRepeat(0);
        current_visualization_text->SetText("");
}

void PlaybackBox::handleTreeListSignals(int node_int, IntVector *attributes)
{
    //  Debugging
    //  cout << "User selected int=" << node_int << endl ;
    
    if(attributes->size() < 4)
    {
        cerr << "playbackbox.o: Worringly, a managed tree list is handing back item attributes of the wrong size" << endl;
        return;
    }
    if(attributes->at(0) == 1)
    {
        //
        //  It's a track
        //
        stop();

        curMeta = all_music->getMetadata(node_int);
        title_text->SetText(curMeta->Title());
        artist_text->SetText(curMeta->Artist());
        album_text->SetText(curMeta->Album());
        maxTime = curMeta->Length() / 1000;

        QString time_string;
        int maxh = maxTime / 3600;
        int maxm = (maxTime / 60) % 60;
        int maxs = maxm % 60;
        if(maxh > 0)
        {
            time_string.sprintf("%d:%02d:%02d", maxh, maxm, maxs);
        }
        else
        {
            time_string.sprintf("%02d:%02d", maxm, maxs);
        }
        time_text->SetText(time_string);
        if(showrating)
        {
            ratings_image->setRepeat(curMeta->Rating());
        }

        play();
        
    }
    else
    {
        curMeta = NULL;
        wipeTrackInfo();
    }
}


void PlaybackBox::wireUpTheme()
{
    //playlist_tree = new GenericTree("playlist root", "root", 0);
    //all_playlists->writeTree(playlist_tree);

    //
    //  The self managed music tree list
    //
    music_tree_list = getUIManagedTreeListType("musictreelist");
    if(!music_tree_list)
    {
        cerr << "playbackbox.o: Couldn't find a music tree list in your theme" << endl;
        exit(0);
    }
    connect(music_tree_list, SIGNAL(nodeSelected(int, IntVector*)), this, SLOT(handleTreeListSignals(int, IntVector*)));

    //
    //  Text area for title (of song)
    //
    title_text = getUITextType("title_text");
    if(!title_text)
    {
        cerr << "playbackbox.o: Couldn't find a text area called title_text in your theme" << endl;
        exit(0);
    }

    //
    //  Text area for artist
    //
    artist_text = getUITextType("artist_text");
    if(!artist_text)
    {
        cerr << "playbackbox.o: Couldn't find a text area called artist_text in your theme" << endl;
        exit(0);
    }

    //
    //  Text area for time
    //
    time_text = getUITextType("time_text");
    if(!time_text)
    {
        cerr << "playbackbox.o: Couldn't find a text area called time_text in your theme" << endl;
        exit(0);
    }

    //
    //  Text area for info (encoding type, bits, etc.)
    //
    info_text = getUITextType("info_text");
    if(!info_text)
    {
        cerr << "playbackbox.o: Couldn't find a text area called info_text in your theme" << endl;
        exit(0);
    }

    //
    //  Text area for album
    //
    album_text = getUITextType("album_text");
    if(!album_text)
    {
        cerr << "playbackbox.o: Couldn't find a text area called album_text in your theme" << endl;
        exit(0);
    }

    //
    //  Ratings image for track
    //
    ratings_image = getUIRepeatedImageType("ratings_image");
    if(!ratings_image)
    {
        cerr << "playbackbox.o: Couldn't find a repeated image called ratings_image in your theme" << endl;
        exit(0);
    }

    //
    //  Current visualization
    //
    current_visualization_text = getUITextType("current_visualization_text");
    if(!current_visualization_text)
    {
        cerr << "playbackbox.o: Couldn't find a text area called current_visualization in your theme" << endl;
        exit(0);
    }

    //
    //  Status bar for volume display
    //  (but don't complain if we can't find it)
    volume_status = getUIStatusBarType("volume_status");
    if(volume_status)
    {
        volume_status->SetTotal(100);
        volume_status->SetOrder(-1);
    }
        
    //
    //  Black Hole for visualizer
    //
    visual_blackhole = getUIBlackHoleType("visual_blackhole");
    if(!visual_blackhole)
    {
        cerr << "playbackbox.o: Couldn't find a black hole called visual_blackhole in your theme" << endl;
        exit(0);
    }

    //
    //  Buttons
    //
    prev_button = getUIPushButtonType("prev_button");
    if(!prev_button)
    {
        cerr << "playbackbox.o: Couldn't find a push button called prev_button in your theme" << endl;
        exit(0);
    }
    connect(prev_button, SIGNAL(pushed()), this, SLOT(previous()));

    rew_button = getUIPushButtonType("rew_button");
    if(!rew_button)
    {
        cerr << "playbackbox.o: Couldn't find a push button called rew_button in your theme" << endl;
        exit(0);
    }
    connect(rew_button, SIGNAL(pushed()), this, SLOT(seekback()));

    pause_button = getUIPushButtonType("pause_button");
    if(!pause_button)
    {
        cerr << "playbackbox.o: Couldn't find a push button called pause_button in your theme" << endl;
        exit(0);
    }
    connect(pause_button, SIGNAL(pushed()), this, SLOT(pause()));

    play_button = getUIPushButtonType("play_button");
    if(!play_button)
    {
        cerr << "playbackbox.o: Couldn't find a push button called play_button in your theme" << endl;
        exit(0);
    }
    connect(play_button, SIGNAL(pushed()), this, SLOT(play()));

    stop_button = getUIPushButtonType("stop_button");
    if(!stop_button)
    {
        cerr << "playbackbox.o: Couldn't find a push button called stop_button in your theme" << endl;
        exit(0);
    }
    connect(stop_button, SIGNAL(pushed()), this, SLOT(stop()));

    ff_button = getUIPushButtonType("ff_button");
    if(!ff_button)
    {
        cerr << "playbackbox.o: Couldn't find a push button called ff_button in your theme" << endl;
        exit(0);
    }
    connect(ff_button, SIGNAL(pushed()), this, SLOT(seekforward()));

    next_button = getUIPushButtonType("next_button");
    if(!next_button)
    {
        cerr << "playbackbox.o: Couldn't find a push button called next_button in your theme" << endl;
        exit(0);
    }
    connect(next_button, SIGNAL(pushed()), this, SLOT(next()));

    shuffle_button = getUITextButtonType("shuffle_button");
    if(!shuffle_button)
    {
        cerr << "playbackbox.o: Couldn't find a text button called shuffle_button in your theme" << endl;
        exit(0);
    }
    connect(shuffle_button, SIGNAL(pushed()), this, SLOT(toggleShuffle()));

    repeat_button = getUITextButtonType("repeat_button");
    if(!repeat_button)
    {
        cerr << "playbackbox.o: Couldn't find a text button called repeat_button in your theme" << endl;
        exit(0);
    }
    connect(repeat_button, SIGNAL(pushed()), this, SLOT(toggleRepeat()));

    pledit_button = getUITextButtonType("pledit_button");
    if(!pledit_button)
    {
        cerr << "playbackbox.o: Couldn't find a text button called pledit_button in your theme" << endl;
        exit(0);
    }
    connect(pledit_button, SIGNAL(pushed()), this, SLOT(editPlaylist()));

    vis_button = getUITextButtonType("vis_button");
    if(!vis_button)
    {
        cerr << "playbackbox.o: Couldn't find a text button called vis_button in your theme" << endl;
        exit(0);
    }
    connect(vis_button, SIGNAL(pushed()), this, SLOT(visEnable()));
}
