// ANSI C includes
#include <cstdlib>

// C++ includes
#include <iostream>
using namespace std;

// Qt includes
#include <qapplication.h>
#include <qregexp.h>

// MythTV plugin includes
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/lcddevice.h>
#include <mythtv/mythmedia.h>
#include <mythtv/audiooutput.h>

// MythMusic includes
#include "metadata.h"
#include "constants.h"
#include "streaminput.h"
#include "decoder.h"
#include "playbackbox.h"
#include "databasebox.h"
#include "mainvisual.h"
#include "smartplaylist.h"
#include "search.h"

PlaybackBoxMusic::PlaybackBoxMusic(MythMainWindow *parent, QString window_name,
                                   QString theme_filename, 
                                   PlaylistsContainer *the_playlists,
                                   AllMusic *the_music, const char *name)

                : MythThemedDialog(parent, window_name, theme_filename, name)
{
    //  A few internal variable defaults
 
    input = NULL;
    output = NULL;
    decoder = NULL;
    mainvisual = NULL;
    visual_mode_timer = NULL;
    lcd_update_timer = NULL;
    waiting_for_playlists_timer = NULL;
    playlist_tree = NULL;
    playlist_popup = NULL;    
    
    lcd_volume_visible = false; 
    isplaying = false;
    tree_is_done = false;
    first_playlist_check = true;
    outputBufferSize = 256;
    currentTime = 0;
    maxTime = 0;
    setContext(0);
    visual_mode_timer = new QTimer(this);
    visualizer_status = 0;
    curMeta = NULL;
    curSmartPlaylistCategory = "";
    curSmartPlaylistName = "";
    
    menufilters = gContext->GetNumSetting("MusicMenuFilters", 0);

    cd_reader_thread = NULL;
    cd_watcher = NULL;
    scan_for_cd = gContext->GetNumSetting("AutoPlayCD", 0);
 
    // Set our pointers the playlists and the metadata containers

    all_playlists = the_playlists;
    all_music = the_music;

    // Get some user set options

    show_whole_tree = gContext->GetNumSetting("ShowWholeTree", 1);
    keyboard_accelerators = gContext->GetNumSetting("KeyboardAccelerators", 1);
    if (!keyboard_accelerators)
        show_whole_tree = false;

    showrating = gContext->GetNumSetting("MusicShowRatings", 0);
    listAsShuffled = gContext->GetNumSetting("ListAsShuffled", 0);
    cycle_visualizer = gContext->GetNumSetting("VisualCycleOnSongChange", 0);

    // Through the magic of themes, our "GUI" already exists we just need to 
    // wire up it

    wireUpTheme();

    // Possibly (user-defined) control the volume
    
    volume_control = false;
    volume_display_timer = new QTimer(this);
    if (gContext->GetNumSetting("MythControlsVolume", 0))
    {
        volume_control = true;
        volume_display_timer->start(2000);
        connect(volume_display_timer, SIGNAL(timeout()),
                this, SLOT(hideVolume()));
    }
    
    // Figure out the shuffle mode

    QString playmode = gContext->GetSetting("PlayMode", "none");
    if (playmode.lower() == "random")
        setShuffleMode(SHUFFLE_RANDOM);
    else if (playmode.lower() == "intelligent")
        setShuffleMode(SHUFFLE_INTELLIGENT);
    else
        setShuffleMode(SHUFFLE_OFF);

    QString repeatmode = gContext->GetSetting("RepeatMode", "all");
    if (repeatmode.lower() == "track")
        setRepeatMode(REPEAT_TRACK);
    else if (repeatmode.lower() == "all")
        setRepeatMode(REPEAT_ALL);
    else
        setRepeatMode(REPEAT_OFF);

    // Set some button values
    
    if (!keyboard_accelerators) 
    {
        if (pledit_button)
            pledit_button->setText(tr("Edit Playlist"));
        if (vis_button)
            vis_button->setText(tr("Visualize"));
        if (!assignFirstFocus())
        {
            cerr << "playbackbox.o: Could not find a button to assign focus "
                    "to. What's in your theme?" << endl;
            exit(0);
        }
    } 
    else 
    {
        if (pledit_button)
            pledit_button->setText(tr("3 Edit Playlist"));
        if (vis_button)
            vis_button->setText(tr("4 Visualize"));
    }

    if (class LCD *lcd = LCD::Get())
    {
        // Set please wait on the LCD
        QPtrList<LCDTextItem> textItems;
        textItems.setAutoDelete(true);
        
        textItems.append(new LCDTextItem(1, ALIGN_CENTERED, "Please Wait", 
                         "Generic"));
        lcd->switchToGeneric(&textItems);
    }

    // We set a timer to load the playlists. We do this for two reasons: 
    // (1) the playlists may not be fully loaded, and (2) even if they are 
    // fully loaded, they do take a while to write out a GenericTree for 
    // navigation use, and that slows down the appearance of this dialog
    // if we were to do it right here.

    waiting_for_playlists_timer = new QTimer(this);
    connect(waiting_for_playlists_timer, SIGNAL(timeout()), this, 
            SLOT(checkForPlaylists()));
    waiting_for_playlists_timer->start(100);

    // Warm up the visualizer
    
    mainvisual = new MainVisual(this);
    if (visual_blackhole)
        mainvisual->setGeometry(visual_blackhole->getScreenArea());
    else
        mainvisual->setGeometry(screenwidth + 10, screenheight + 10, 160, 160);
    mainvisual->show();   
 
    visual_mode = gContext->GetSetting("VisualMode");
    visual_mode.simplifyWhiteSpace();
    visual_mode.replace(QRegExp("\\s"), ",");

    QString visual_delay = gContext->GetSetting("VisualModeDelay");
    bool delayOK;
    visual_mode_delay = visual_delay.toInt(&delayOK);
    if (!delayOK)
        visual_mode_delay = 0;
    if (visual_mode_delay > 0)
    {
        visual_mode_timer->start(visual_mode_delay * 1000);
        connect(visual_mode_timer, SIGNAL(timeout()), this, SLOT(visEnable()));
    }
    visualizer_status = 1;

    // Temporary workaround for visualizer Bad X Requests
    //
    // start on Blank, and then set the "real" mode after
    // the playlist timer fires. Seems to work.
    //
    // Suspicion: in most modes, SDL is not happy if the
    // window doesn't fully exist yet  (????)
    
    mainvisual->setVisual("Blank");

    // Ready to go. Let's update the foreground just to be safe.

    updateForeground();

    if (class LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
    }
}

PlaybackBoxMusic::~PlaybackBoxMusic(void)
{
    stopAll();

    if (cd_reader_thread)
    {
        cd_watcher->stop();
        cd_reader_thread->wait();
        delete cd_reader_thread;
    }

    if (playlist_tree)
        delete playlist_tree;

    if (shufflemode == SHUFFLE_INTELLIGENT)
        gContext->SaveSetting("PlayMode", "intelligent");
    else if (shufflemode == SHUFFLE_RANDOM)
        gContext->SaveSetting("PlayMode", "random");
    else
        gContext->SaveSetting("PlayMode", "none");

    if (repeatmode == REPEAT_TRACK)
        gContext->SaveSetting("RepeatMode", "track");
    else if (repeatmode == REPEAT_ALL)
        gContext->SaveSetting("RepeatMode", "all");
    else
        gContext->SaveSetting("RepeatMode", "none");
    if (class LCD *lcd = LCD::Get())
        lcd->switchToTime();
}

bool PlaybackBoxMusic::onMediaEvent(MythMediaDevice*)
{
    return scan_for_cd;
}

void PlaybackBoxMusic::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    resetTimer();

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Music", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "NEXTTRACK")
        {
            if (next_button)
                next_button->push();
            else
                next();
        }
        else if (action == "PREVTRACK")
        {
            if (prev_button)
                prev_button->push();
            else
                previous();
        }
        else if (action == "FFWD")
        { 
            if (ff_button)
                ff_button->push();
            else
                seekforward();
        }
        else if (action == "RWND")
        { 
            if (rew_button)
                rew_button->push();
            else
                seekback();
        }
        else if (action == "PAUSE")
        { 
            if (isplaying)
            {
                if (pause_button)
                    pause_button->push();
                else
                    pause();
            }
            else
            {
                if (play_button)
                    play_button->push();
                else
                    play();
            }
        }
        else if (action == "STOP")
        { 
            if (stop_button)
                stop_button->push();
            else
                stop();
        }
        else if (action == "THMBUP")
            increaseRating();
        else if (action == "THMBDOWN")
            decreaseRating();
        else if (action == "1")
        {
            if (shuffle_button)
                shuffle_button->push();
            else
                toggleShuffle();
        }
        else if (action == "2")
        {
            if (repeat_button)
                repeat_button->push();
            else
                toggleRepeat();
        }
        else if (action == "3")
        {
            if (pledit_button)
                pledit_button->push();
            else
                editPlaylist();
        }
        else if (action == "CYCLEVIS")
            CycleVisualizer();
        else if (action == "BLANKSCR")
            toggleFullBlankVisualizer();
        else if (action == "VOLUMEDOWN") 
            changeVolume(false);
        else if (action == "VOLUMEUP")
            changeVolume(true);
        else if (action == "MUTE")
            toggleMute();
        else if (action == "MENU")
        {
            menufilters = false;
            showMenu();
        }
        else if (action == "FILTER")
        {
            menufilters = true;
            showMenu();
        }
        else if (action == "INFO")
            showEditMetadataDialog();
        else
            handled = false;
    }

    if (handled)
        return;

    if (visualizer_status == 2)
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE" || action == "4")
            {
                visualizer_status = 1;
                QString visual_workaround = mainvisual->getCurrentVisual();
 
                // We may have gotten to full screen by pushing 7
                // (full screen blank). Or it may be blank because
                // the user likes "Blank". Figure out what to do ...
            
                if (visual_workaround == "Blank" && visual_mode != "Blank")
                    visual_workaround = visual_mode;

                mainvisual->setVisual("Blank");
                if (visual_blackhole)
                    mainvisual->setGeometry(visual_blackhole->getScreenArea());
                else
                    mainvisual->setGeometry(screenwidth + 10, 
                                            screenheight + 10, 
                                            160, 160);
                setUpdatesEnabled(true);
                mainvisual->setVisual(visual_workaround);
                handled = true;
            }
        }
    }
    else if (keyboard_accelerators)
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                music_tree_list->moveUp();
            else if (action == "DOWN")
                music_tree_list->moveDown();
            else if (action == "LEFT")
                music_tree_list->popUp();
            else if (action == "RIGHT")
                    music_tree_list->pushDown();
            else if (action == "4")
            {
                if (vis_button)
                    vis_button->push();
                else
                    visEnable();
            }
            else if (action == "SELECT")
            {
                music_tree_list->select();
                if (visualizer_status > 0 && cycle_visualizer)
                    CycleVisualizer();
            }
            else if (action == "REFRESH") 
            {
                music_tree_list->syncCurrentWithActive();
                music_tree_list->forceLastBin();
                music_tree_list->refresh();
            }
            else if (action == "PAGEUP")
                music_tree_list->pageUp();
            else if (action == "PAGEDOWN")
                music_tree_list->pageDown();
            else if (action.left(4) == "JUMP")
                music_tree_list->jumpToLetter(action.mid(4));
            else
                handled = false;
        }
    }
    else
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP" || action == "LEFT")
                nextPrevWidgetFocus(false);
            else if (action == "DOWN" || action == "RIGHT")
                nextPrevWidgetFocus(true);
            else if (action == "SELECT")
            {
                activateCurrent();
                music_tree_list->syncCurrentWithActive();
            }
            else
                handled = false;
        }
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void PlaybackBoxMusic::showMenu()
{
    if (playlist_popup)
        return;

    playlist_popup = new MythPopupBox(gContext->GetMainWindow(),
                                      "playlist_popup");

    if (menufilters)
    {
        QLabel *caption = playlist_popup->addLabel(tr("Change Filter"), MythPopupBox::Large);
        caption->setAlignment(Qt::AlignCenter);
    }

    QButton *button = playlist_popup->addButton(tr("Smart playlists"), this,
                              SLOT(showSmartPlaylistDialog()));

    QLabel *splitter = playlist_popup->addLabel(" ", MythPopupBox::Small);
    splitter->setLineWidth(2);
    splitter->setFrameShape(QFrame::HLine);
    splitter->setFrameShadow(QFrame::Sunken);
    splitter->setMaximumHeight((int) (5 * hmult));
    splitter->setMaximumHeight((int) (5 * hmult));

    playlist_popup->addButton(tr("Search"), this,
                              SLOT(showSearchDialog()));
    playlist_popup->addButton(tr("From CD"), this,
                              SLOT(fromCD()));
    playlist_popup->addButton(tr("All Tracks"), this,
                              SLOT(allTracks()));
    if (curMeta)
    {
        playlist_popup->addButton(tr("Tracks by current Artist"), this,
                                  SLOT(byArtist()));
        playlist_popup->addButton(tr("Tracks from current Album"), this,
                                  SLOT(byAlbum()));
        playlist_popup->addButton(tr("Tracks from current Genre"), this,
                                  SLOT(byGenre()));
        playlist_popup->addButton(tr("Tracks from current Year"), this,
                                  SLOT(byYear()));
    }
    
    playlist_popup->ShowPopup(this, SLOT(closePlaylistPopup()));

    button->setFocus();
}

void PlaybackBoxMusic::closePlaylistPopup()
{
    if (!playlist_popup)
        return;

    playlist_popup->hide();
    delete playlist_popup;
    playlist_popup = NULL;
}

void PlaybackBoxMusic::allTracks()
{
   if (!playlist_popup)
        return;

   closePlaylistPopup();
   updatePlaylistFromQuickPlaylist("ORDER BY artist, album, tracknum");
}

void PlaybackBoxMusic::fromCD()
{
    if (!playlist_popup)
        return;

    updatePlaylistFromCD();
    closePlaylistPopup();
}

void PlaybackBoxMusic::showSmartPlaylistDialog()
{
   if (!playlist_popup)
        return;

    closePlaylistPopup();
    
    SmartPlaylistDialog dialog(gContext->GetMainWindow(), "smartplaylistdialog");
    dialog.setSmartPlaylist(curSmartPlaylistCategory, curSmartPlaylistName);

    int res = dialog.ExecPopup();
    
    if (res > 0)
    {
        dialog.getSmartPlaylist(curSmartPlaylistCategory, curSmartPlaylistName);
        updatePlaylistFromSmartPlaylist();
    }
}

void PlaybackBoxMusic::showSearchDialog()
{
   if (!playlist_popup)
        return;

    closePlaylistPopup();

    SearchDialog dialog(gContext->GetMainWindow(), "searchdialog");

    int res = dialog.ExecPopupAtXY(-1, 20);

    if (res != -1)
    {
          QString whereClause;
          dialog.getWhereClause(whereClause);
          updatePlaylistFromQuickPlaylist(whereClause);
    }
}

void PlaybackBoxMusic::byArtist()
{
    if (!playlist_popup || !curMeta)
        return;

    QString value = formattedFieldValue(curMeta->Artist().utf8());
    QString whereClause = "WHERE artist = " + value +
                          " ORDER BY album, tracknum"; 

    closePlaylistPopup();
    updatePlaylistFromQuickPlaylist(whereClause);
}

void PlaybackBoxMusic::byAlbum()
{
    if (!playlist_popup || !curMeta)
        return;

    QString value = formattedFieldValue(curMeta->Album().utf8());
    QString whereClause = "WHERE album = " + value + 
                          " ORDER BY tracknum";
    closePlaylistPopup();
    updatePlaylistFromQuickPlaylist(whereClause);
}

void PlaybackBoxMusic::byGenre()
{
   if (!playlist_popup || !curMeta)
        return;

    QString value = formattedFieldValue(curMeta->Genre().utf8()); 
    QString whereClause = "WHERE genre = " + value +
                          " ORDER BY artist, album, tracknum";   
    closePlaylistPopup();
    updatePlaylistFromQuickPlaylist(whereClause);
}

void PlaybackBoxMusic::byYear()
{
   if (!playlist_popup || !curMeta)
        return;

    QString value = formattedFieldValue(curMeta->Year()); 
    QString whereClause = "WHERE year = " + value + 
                          " ORDER BY artist, album, tracknum";
    closePlaylistPopup();
    updatePlaylistFromQuickPlaylist(whereClause);
}

void PlaybackBoxMusic::updatePlaylistFromQuickPlaylist(QString whereClause)
{
    doUpdatePlaylist(whereClause);
}

void PlaybackBoxMusic::doUpdatePlaylist(QString whereClause)
{
    bool bRemoveDups;
    InsertPLOption insertOption;
    PlayPLOption playOption;
    int curTrackID, trackCount;

    if (!menufilters && !getInsertPLOptions(insertOption, playOption, bRemoveDups))
        return;

    QValueList <int> branches_to_current_node;
    trackCount = music_tree_list->getCurrentNode()->siblingCount();

    // store path to current track
    if (curMeta)
    {
        QValueList <int> *a_route;
        a_route = music_tree_list->getRouteToActive();
        branches_to_current_node = *a_route;
        curTrackID = curMeta->ID();
    }
    else
    {
        // No current metadata, so when we come back we'll try and play the 
        // first thing on the active queue
        branches_to_current_node.clear();
        branches_to_current_node.append(0); //  Root node
        branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0); //  Active play Queue
        curTrackID = 0;
    }

    visual_mode_timer->stop();

    if (whereClause != "")
    {
        // update playlist from quick playlist
        if (menufilters)
            all_playlists->getActive()->fillSonglistFromQuery(
                        whereClause, false, PL_FILTERONLY, curTrackID);
        else
            all_playlists->getActive()->fillSonglistFromQuery(
                        whereClause, bRemoveDups, insertOption, curTrackID);
    }
    else
    {
        // update playlist from smart playlist
        if (menufilters)
            all_playlists->getActive()->fillSonglistFromSmartPlaylist(
                    curSmartPlaylistCategory, curSmartPlaylistName,
                    false, PL_FILTERONLY, curTrackID);
        else
        {
            all_playlists->getActive()->fillSonglistFromSmartPlaylist(
                    curSmartPlaylistCategory, curSmartPlaylistName,
                    bRemoveDups, insertOption, curTrackID);
        }
    }

    if (visual_mode_delay > 0)
        visual_mode_timer->start(visual_mode_delay * 1000);

    constructPlaylistTree();

    if (!menufilters)
    {
        switch (playOption)
        {
            case PL_CURRENT:
                // try to move to the current track
                if (!music_tree_list->tryToSetActive(branches_to_current_node))
                    playFirstTrack();
                break;

            case PL_FIRST:
                playFirstTrack();
                break;

            case PL_FIRSTNEW:
            {
                switch (insertOption)
                {
                    case PL_REPLACE:
                        playFirstTrack();
                        break;

                    case PL_INSERTATEND:
                    {
                        GenericTree *node = NULL;
                        pause();
                        if (music_tree_list->tryToSetActive(branches_to_current_node))
                        {
                            node = music_tree_list->getCurrentNode()->getParent();
                            if (node)
                            {
                                node = node->getChildAt((uint) trackCount);
                            }
                        }

                        if (node)
                        {
                            music_tree_list->setCurrentNode(node);
                            music_tree_list->select();
                        }
                        else
                            playFirstTrack();

                        break;
                    }

                    case PL_INSERTAFTERCURRENT:
                        pause();
                        if (music_tree_list->tryToSetActive(branches_to_current_node))
                            next();
                        else
                            playFirstTrack();
                        break;

                    default:
                        playFirstTrack();
                }

                break;
            }
        }
    }

    music_tree_list->refresh();
}

void PlaybackBoxMusic::playFirstTrack()
{
    QValueList <int> branches_to_current_node;

    stop();
    wipeTrackInfo();
    branches_to_current_node.clear();
    branches_to_current_node.append(0); //  Root node
    branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
    branches_to_current_node.append(0); //  Active play Queue
    music_tree_list->moveToNodesFirstChild(branches_to_current_node);
}

void PlaybackBoxMusic::updatePlaylistFromCD()
{
    if (!cd_reader_thread)
    {
        cd_reader_thread = new ReadCDThread(all_playlists, all_music);
        cd_reader_thread->start();
    }

    if (!cd_watcher)
    {
        cd_watcher = new QTimer(this);
        connect(cd_watcher, SIGNAL(timeout()), this, SLOT(occasionallyCheckCD()));
        cd_watcher->start(1000);
    }

}

void PlaybackBoxMusic::postUpdate()
{
   QValueList <int> branches_to_current_node;

   if (visual_mode_delay > 0)
       visual_mode_timer->start(visual_mode_delay * 1000);

   constructPlaylistTree();

   stop();
   wipeTrackInfo();

   // move to first track in list
   branches_to_current_node.clear();
   branches_to_current_node.append(0); //  Root node
   branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
   branches_to_current_node.append(0); //  Active play Queue
   music_tree_list->moveToNodesFirstChild(branches_to_current_node);
   music_tree_list->refresh();
}

void PlaybackBoxMusic::occasionallyCheckCD()
{
    if (cd_reader_thread->getLock()->locked())
        return;

    if (!scan_for_cd) {
        cd_watcher->stop();
        delete cd_watcher;
        cd_watcher = NULL;

        cd_reader_thread->wait();
        delete cd_reader_thread;
        cd_reader_thread = NULL;
    }

    if (!scan_for_cd || cd_reader_thread->statusChanged())
    {
        all_playlists->clearCDList();
        all_playlists->getActive()->ripOutAllCDTracksNow();

        if(all_music->getCDTrackCount()) {
            visual_mode_timer->stop();

            all_playlists->getActive()->removeAllTracks();
            all_playlists->getActive()->fillSongsFromCD();

            postUpdate();
        }
    }

    if (scan_for_cd && !cd_reader_thread->running())
        cd_reader_thread->start();
}

void PlaybackBoxMusic::updatePlaylistFromSmartPlaylist()
{
    doUpdatePlaylist("");
}

void PlaybackBoxMusic::showEditMetadataDialog()
{
    if(!curMeta)
    {
        return;
    }

    // store the current track metadata in case the track changes
    // while we show the edit dialog
    Metadata *editMeta = curMeta;
    GenericTree *node = music_tree_list->getCurrentNode();

    EditMetadataDialog editDialog(editMeta, gContext->GetMainWindow(),
                      "edit_metadata", "music-", "edit metadata");
    if (editDialog.exec())
    {
        // update the metadata copy stored in all_music
        if (all_music->updateMetadata(editMeta->ID(), editMeta))
        {
           // update the displayed track info
           if (node)
           {
               bool errorFlag;
               node->setString(all_music->getLabel(editMeta->ID(), &errorFlag));
               music_tree_list->refresh();
           }
        }
    }
}

void PlaybackBoxMusic::checkForPlaylists()
{
    if (first_playlist_check)
    {
        first_playlist_check = false;
        repaint();
        return;
    }

    // This is only done off a timer on startup

    if (all_playlists->doneLoading() &&
        all_music->doneLoading())
    {
        if (tree_is_done)
        {
            if (scan_for_cd)
                updatePlaylistFromCD();

            music_tree_list->showWholeTree(show_whole_tree);
            waiting_for_playlists_timer->stop();
            QValueList <int> branches_to_current_node;
            branches_to_current_node.append(0); //  Root node
            branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
            branches_to_current_node.append(0); //  Active play Queue
            music_tree_list->moveToNodesFirstChild(branches_to_current_node);
            music_tree_list->refresh();
            if (show_whole_tree)
                setContext(1);
            else
                setContext(2);
            updateForeground();
            mainvisual->setVisual(visual_mode);
            
            if (curMeta) 
                setTrackOnLCD(curMeta);
        }    
        else
            constructPlaylistTree();
    }
    else
    {
        // Visual Feedback ...
    }
}

void PlaybackBoxMusic::changeVolume(bool up_or_down)
{
    if (volume_control && output)
    {
        if (up_or_down)
            output->AdjustCurrentVolume(2);
        else
            output->AdjustCurrentVolume(-2);
        showVolume(true);
    }
}

void PlaybackBoxMusic::toggleMute()
{
    if (volume_control && output)
    {
        output->ToggleMute();
        showVolume(true);
    }
}

void PlaybackBoxMusic::showVolume(bool on_or_off)
{
    float volume_level;
    if (volume_control && output)
    {
        if (volume_status)
        {
            if(on_or_off)
            {
                volume_status->SetUsed(output->GetCurrentVolume());
                volume_status->SetOrder(0);
                volume_status->refresh();
                volume_display_timer->changeInterval(2000);
                if (!lcd_volume_visible)
                {
                    lcd_volume_visible = true;
                    if (class LCD *lcd = LCD::Get())
                        lcd->switchToVolume("Music");
                }
                if (output->GetMute())
                    volume_level = 0.0;
                else
                    volume_level = (float)output->GetCurrentVolume() / 
                                   (float)100;

                if (class LCD *lcd = LCD::Get())
                    lcd->setVolumeLevel(volume_level);
            }
            else
            {
                if (volume_status->getOrder() != -1)
                {
                    volume_status->SetOrder(-1);
                    volume_status->refresh();

                    if (curMeta)
                        setTrackOnLCD (curMeta);
                }
            }
        }
    }
}

void PlaybackBoxMusic::resetTimer()
{
    if (visual_mode_delay > 0)
        visual_mode_timer->changeInterval(visual_mode_delay * 1000);
}

void PlaybackBoxMusic::play()
{
    if (isplaying)
        stop();

    if (curMeta)
        playfile = curMeta->Filename();
    else
    {
        // Perhaps we can descend to something playable?
        wipeTrackInfo();
        return;
    }

    QUrl sourceurl(playfile);
    QString sourcename(playfile);

    bool startoutput = false;

    if (!output)
    {
        QString adevice = gContext->GetSetting("AudioDevice");

        // TODO: Error checking that device is opened correctly!
        output = AudioOutput::OpenAudio(adevice, 16, 2, 44100, 
                                     AUDIOOUTPUT_MUSIC, true ); 
        output->setBufferSize(outputBufferSize * 1024);
        output->SetBlocking(false);
        output->addListener(this);
        output->addListener(mainvisual);
        output->addVisual(mainvisual);
    
        startoutput = true;

    }
   
    if (output->GetPause())
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
        input = new QFile(playfile);
    
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
        decoder->setFilename(sourcename);
        decoder->setOutput(output);
    }

    currentTime = 0;

    mainvisual->setDecoder(decoder);
    mainvisual->setOutput(output);
    
    if (decoder->initialize()) 
    {
        if (output)
        {
            output->Reset();
        }

        decoder->start();

        isplaying = true;
        curMeta->setLastPlay();
        curMeta->incPlayCount();    
    }
}

void PlaybackBoxMusic::visEnable()
{
    if (!visualizer_status != 2 && isplaying)
    {
        setUpdatesEnabled(false);
        mainvisual->setGeometry(0, 0, screenwidth, screenheight);
        visualizer_status = 2;
    }
}

void PlaybackBoxMusic::CycleVisualizer()
{
    QString new_visualizer;

    // Only change the visualizer if there is more than 1 visualizer
    // and the user currently has a visualizer active
    if (mainvisual->numVisualizers() > 1 && visualizer_status > 0)
    {
        QStringList allowed_modes = QStringList::split(",", visual_mode);
        if (allowed_modes[0].stripWhiteSpace().endsWith("*"))
        {
            //User has a favorite, but requested the next in the list
            //Find the current position in the list
            QString current_visual = mainvisual->getCurrentVisual();
            unsigned int index = 0;
            while ((index < allowed_modes.size()) &&
                   (!allowed_modes[index++].stripWhiteSpace().startsWith(current_visual))) { }
            // index is 1 greater than the current visualizer's index
            if (index >= allowed_modes.size()) {
                index = 0;
            }
            new_visualizer = allowed_modes[index].stripWhiteSpace();
            // Make sure the asterisk isn't passed through
            if (new_visualizer.endsWith("*")) {
                new_visualizer.truncate(new_visualizer.length() - 1);
            }
        }
        else if (visual_mode != "Random")
        {
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
    else if (mainvisual->numVisualizers() == 1 && visual_mode == "AlbumArt" && 
             visualizer_status > 0) 
    {
        // If only the AlbumArt visualization is selected, then go ahead and
        // restart the visualization.  This will give AlbumArt the opportunity
        // to change images if there are multiple images available.
        new_visualizer = visual_mode;
        visual_mode_timer->stop();
        mainvisual->setVisual("Blank");
        mainvisual->setVisual(new_visualizer);
    }
}

void PlaybackBoxMusic::setTrackOnLCD(Metadata *mdata)
{
    LCD *lcd = LCD::Get();
    if (!lcd)
        return;

    // Set the Artist and Tract on the LCD
    lcd->switchToMusic(mdata->Artist(), 
                       mdata->Album(), 
                       mdata->Title());
}

void PlaybackBoxMusic::pause(void)
{
    if (output) 
    {
        isplaying = !isplaying;
        output->Pause(!isplaying); //Note pause doesn't take effet instantly
    }
    // wake up threads
    if (decoder) 
    {
        decoder->mutex()->lock();
        decoder->cond()->wakeAll();
        decoder->mutex()->unlock();
    }

}

void PlaybackBoxMusic::stopDecoder(void)
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

void PlaybackBoxMusic::stop(void)
{
    if (decoder && decoder->running()) 
    {
        decoder->mutex()->lock();
        decoder->stop();
        decoder->mutex()->unlock();
    }

    // wake up threads
    if (decoder) 
    {
        decoder->mutex()->lock();
        decoder->cond()->wakeAll();
        decoder->mutex()->unlock();
    }

    if (decoder)
        decoder->wait();

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
    if (maxh > 0)
        time_string.sprintf("%d:%02d:%02d", maxh, maxm, maxs);
    else
        time_string.sprintf("%02d:%02d", maxm, maxs);

    if (time_text)
        time_text->SetText(time_string);
    if (info_text)
        info_text->SetText("");

    isplaying = false;
}

void PlaybackBoxMusic::stopAll()
{
    if (class LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
    }

    stop();

    if (decoder) 
    {
        decoder->removeListener(this);
        decoder = 0;
    }
}

void PlaybackBoxMusic::previous()
{
    if (repeatmode == REPEAT_ALL)
    {
        if (music_tree_list->prevActive(true, show_whole_tree))
            music_tree_list->activate();
    }
    else
    {
        if (music_tree_list->prevActive(false, show_whole_tree))
            music_tree_list->activate();
    }
     
    if (visualizer_status > 0 && cycle_visualizer)
        CycleVisualizer();
}

void PlaybackBoxMusic::next()
{
    if (repeatmode == REPEAT_ALL)
    {
        // Grab the next track after this one. First flag is to wrap around
        // to the beginning of the list. Second decides if we will traverse up 
        // and down the tree.
        if (music_tree_list->nextActive(true, show_whole_tree))
            music_tree_list->activate();
        else 
            end();
    }
    else
    {
        if (music_tree_list->nextActive(false, show_whole_tree))
            music_tree_list->activate(); 
        else 
            end();
    }
     
    if (visualizer_status > 0 && cycle_visualizer)
        CycleVisualizer();
}

void PlaybackBoxMusic::nextAuto()
{
    stopDecoder();

    isplaying = false;

    if (repeatmode == REPEAT_TRACK)
        play();
    else 
        next();
}

void PlaybackBoxMusic::seekforward()
{
    int nextTime = currentTime + 5;
    if (nextTime > maxTime)
        nextTime = maxTime;
    seek(nextTime);
}

void PlaybackBoxMusic::seekback()
{
    int nextTime = currentTime - 5;
    if (nextTime < 0)
        nextTime = 0;
    seek(nextTime);
}

void PlaybackBoxMusic::seek(int pos)
{
    if (output)
    {
        output->Reset();
        output->SetTimecode(pos*1000);

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
    }
}

void PlaybackBoxMusic::setShuffleMode(unsigned int mode)
{
    shufflemode = mode;

    switch (shufflemode)
    {
        case SHUFFLE_INTELLIGENT:
            if(shuffle_button)
            {
                if (keyboard_accelerators)
                    shuffle_button->setText(tr("1 Shuffle: Smart"));
                else
                    shuffle_button->setText(tr("Shuffle: Smart"));
            }
            music_tree_list->scrambleParents(true);

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_SMART);
            break;
        case SHUFFLE_RANDOM:
            if(shuffle_button)
            {
                if (keyboard_accelerators)
                    shuffle_button->setText(tr("1 Shuffle: Rand"));
                else
                    shuffle_button->setText(tr("Shuffle: Rand"));
            }
            music_tree_list->scrambleParents(true);

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_RAND);
            break;
        default:
            if(shuffle_button)
            {
                if (keyboard_accelerators)
                    shuffle_button->setText(tr("1 Shuffle: None"));
                else
                    shuffle_button->setText(tr("Shuffle: None"));
            }
            music_tree_list->scrambleParents(false);

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_NONE);
            break;
    }
    music_tree_list->setTreeOrdering(shufflemode + 1);
    if (listAsShuffled)
        music_tree_list->setVisualOrdering(shufflemode + 1);
    else
        music_tree_list->setVisualOrdering(1);
    music_tree_list->refresh();
    
    if (isplaying)
        setTrackOnLCD(curMeta);
}

void PlaybackBoxMusic::toggleShuffle(void)
{
    setShuffleMode(++shufflemode % MAX_SHUFFLE_MODES);
}

void PlaybackBoxMusic::increaseRating()
{
    if(!curMeta)
        return;

    // Rationale here is that if you can't get visual feedback on ratings 
    // adjustments, you probably should not be changing them

    if (showrating)
    {
        curMeta->incRating();
        if (ratings_image)
            ratings_image->setRepeat(curMeta->Rating());
    }
}

void PlaybackBoxMusic::decreaseRating()
{
    if(!curMeta)
        return;

    if (showrating)
    {
        curMeta->decRating();
        if (ratings_image)
            ratings_image->setRepeat(curMeta->Rating());
    }
}

void PlaybackBoxMusic::setRepeatMode(unsigned int mode)
{
    repeatmode = mode;

    if (!repeat_button)
        return;

    switch (repeatmode)
    {
        case REPEAT_ALL:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: All"));
            else
                repeat_button->setText(tr("Repeat: All"));

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicRepeat (LCD::MUSIC_REPEAT_ALL);
            break;
        case REPEAT_TRACK:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: Track"));
            else
                repeat_button->setText(tr("Repeat: Track"));

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicRepeat (LCD::MUSIC_REPEAT_TRACK);
            break;
        default:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: None"));
            else
                repeat_button->setText(tr("Repeat: None"));

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicRepeat (LCD::MUSIC_REPEAT_NONE);
            break;
    }
}

void PlaybackBoxMusic::toggleRepeat()
{
    setRepeatMode(++repeatmode % MAX_REPEAT_MODES);
}

void PlaybackBoxMusic::constructPlaylistTree()
{
    if (playlist_tree)
        delete playlist_tree;

    playlist_tree = new GenericTree(tr("playlist root"), 0);
    playlist_tree->setAttribute(0, 0);
    playlist_tree->setAttribute(1, 0);
    playlist_tree->setAttribute(2, 0);
    playlist_tree->setAttribute(3, 0);

    // We ask the playlist object to write out the whole tree (all playlists 
    // and all music). It will set attributes for nodes in the tree, such as 
    // whether a node is selectable, how it can be ordered (normal, random, 
    // intelligent), etc. 

    all_playlists->writeTree(playlist_tree);
    music_tree_list->assignTreeData(playlist_tree);
    tree_is_done = true;
}

void PlaybackBoxMusic::editPlaylist()
{
    // Get a reference to the current track

    QValueList <int> branches_to_current_node;

    if (curMeta)
    {
        QValueList <int> *a_route;
        a_route = music_tree_list->getRouteToActive();
        branches_to_current_node = *a_route;
    }
    else
    {
        // No current metadata, so when we come back we'll try and play the 
        // first thing on the active queue
        
        branches_to_current_node.clear();
        branches_to_current_node.append(0); //  Root node
        branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0); //  Active play Queue
    }

    visual_mode_timer->stop();
    DatabaseBox dbbox(all_playlists, all_music, gContext->GetMainWindow(),
                      "music_select", "music-", "database box");
    dbbox.exec();
    if (visual_mode_delay > 0)
        visual_mode_timer->start(visual_mode_delay * 1000);

    // OK, we're back ....
    // now what do we do? see if we can find the same track at the same level

    constructPlaylistTree();
    if (music_tree_list->tryToSetActive(branches_to_current_node))
    {
        //  All is well
    }
    else
    {
        stop();
        wipeTrackInfo();
        branches_to_current_node.clear();
        branches_to_current_node.append(0); //  Root node
        branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0); //  Active play Queue
        music_tree_list->moveToNodesFirstChild(branches_to_current_node);
    }
    music_tree_list->refresh();
}

void PlaybackBoxMusic::closeEvent(QCloseEvent *event)
{
    stopAll();

    hide();
    event->accept();
}

void PlaybackBoxMusic::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

void PlaybackBoxMusic::customEvent(QCustomEvent *event)
{
    switch ((int)event->type()) 
    {
        case OutputEvent::Playing:
        {
            setTrackOnLCD(curMeta);
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
            
            if (maxh > 0)
                time_string.sprintf("%d:%02d:%02d / %02d:%02d:%02d", eh, em, 
                                    es, maxh, maxm, maxs);
            else
                time_string.sprintf("%02d:%02d / %02d:%02d", em, es, maxm, 
                                    maxs);
            
            if (curMeta)
            {
                if (class LCD *lcd = LCD::Get())
                {
                    float percent_heard = ((float)rs / 
                                           (float)curMeta->Length()) * 1000.0;
                    lcd->setMusicProgress(time_string, percent_heard);
                }
            }

            QString info_string;

            //  Hack around for cd bitrates
            if (oe->bitrate() < 2000)
            {
                info_string.sprintf("%d "+tr("kbps")+ "   %.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
                                   oe->bitrate(), float(oe->frequency()) / 1000.0,
                                   oe->channels() > 1 ? "2" : "1");
            }
            else
            {
                info_string.sprintf("%.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
                                   float(oe->frequency()) / 1000.0,
                                   oe->channels() > 1 ? "2" : "1");
            }
        
            if (curMeta)
            {
                if (time_text)
                    time_text->SetText(time_string);
                if (info_text)
                    info_text->SetText(info_string);
                if (current_visualization_text)
                {
                    current_visualization_text->SetText(mainvisual->getCurrentVisualDesc());
                    current_visualization_text->refresh();
                }
            }

            break;
        }
        case OutputEvent::Error:
        {
            statusString = tr("Output error.");

            OutputEvent *aoe = (OutputEvent *) event;

            cerr << statusString << " " << *aoe->errorMessage() << endl;
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                      statusString,
                                      QString("MythMusic has encountered the following error:\n%1")
                                      .arg(*aoe->errorMessage()));
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

            DecoderEvent *dxe = (DecoderEvent *) event;
 
            cerr << statusString << " " << *dxe->errorMessage() << endl;
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                      statusString,
                                      QString("MythMusic has encountered the following error:\n%1")
                                      .arg(*dxe->errorMessage()));
            break;
        }
    }

    QWidget::customEvent(event);
}

void PlaybackBoxMusic::wipeTrackInfo()
{
        if (title_text)
            title_text->SetText("");
        if (artist_text)
            artist_text->SetText("");
        if (album_text)
            album_text->SetText("");
        if (time_text)
            time_text->SetText("");
        if (info_text)
            info_text->SetText("");
        if (ratings_image)
            ratings_image->setRepeat(0);
        if (current_visualization_text)
            current_visualization_text->SetText("");
}

void PlaybackBoxMusic::handleTreeListSignals(int node_int, IntVector *attributes)
{
    if (attributes->size() < 4)
    {
        cerr << "playbackbox.o: Worringly, a managed tree list is handing "
                "back item attributes of the wrong size" << endl;
        return;
    }

    if (attributes->at(0) == 1)
    {
        //  It's a track

        curMeta = all_music->getMetadata(node_int);
        if (title_text)
            title_text->SetText(curMeta->FormatTitle());
        if (artist_text)
            artist_text->SetText(curMeta->FormatArtist());
        if (album_text)
            album_text->SetText(curMeta->Album());

        setTrackOnLCD(curMeta);

        maxTime = curMeta->Length() / 1000;

        QString time_string;
        int maxh = maxTime / 3600;
        int maxm = (maxTime / 60) % 60;
        int maxs = maxm % 60;
        if (maxh > 0)
            time_string.sprintf("%d:%02d:%02d", maxh, maxm, maxs);
        else
            time_string.sprintf("%02d:%02d", maxm, maxs);
        if (time_text)
            time_text->SetText(time_string);
        if (showrating)
        {
            if(ratings_image)
                ratings_image->setRepeat(curMeta->Rating());
        }

        if (output && output->GetPause())
        {
            stop();
            if(play_button)
            {
                play_button->push();
            }
            else
            {
                play();
            }
        }
        else
            play();
    }
    else
    {
        curMeta = NULL;
        wipeTrackInfo();
    }
}


void PlaybackBoxMusic::toggleFullBlankVisualizer()
{
    if( mainvisual->getCurrentVisual() == "Blank" &&
        visualizer_status == 2)
    {
        //
        //  If we are already full screen and 
        //  blank, go back to regular dialog
        //

        if(visual_blackhole)
            mainvisual->setGeometry(visual_blackhole->getScreenArea());
        else
            mainvisual->setGeometry(screenwidth + 10, screenheight + 10, 
                                    160, 160);
        mainvisual->setVisual(visual_mode);
        visualizer_status = 1;
        if(visual_mode_delay > 0)
        {
            visual_mode_timer->start(visual_mode_delay * 1000);
        }
        if (current_visualization_text)
        {
            current_visualization_text->SetText(mainvisual->getCurrentVisualDesc());
            current_visualization_text->refresh();
        }
        setUpdatesEnabled(true);
    }
    else
    {
        //
        //  Otherwise, go full screen blank
        //

        mainvisual->setVisual("Blank");
        mainvisual->setGeometry(0, 0, screenwidth, screenheight);
        visualizer_status = 2;
        visual_mode_timer->stop();
        setUpdatesEnabled(false);
    }
}

void PlaybackBoxMusic::end()
{
    if (class LCD *lcd = LCD::Get()) 
        lcd->switchToTime ();
}

void PlaybackBoxMusic::wireUpTheme()
{
    // The self managed music tree list
    //
    // Complain if we can't find this
    music_tree_list = getUIManagedTreeListType("musictreelist");
    if (!music_tree_list)
    {
        cerr << "playbackbox.o: Couldn't find a music tree list in your theme" 
             << endl;
        exit(0);
    }
    connect(music_tree_list, SIGNAL(nodeSelected(int, IntVector*)), 
            this, SLOT(handleTreeListSignals(int, IntVector*)));

    // All the other GUI elements are **optional**
    title_text = getUITextType("title_text");
    artist_text = getUITextType("artist_text");
    time_text = getUITextType("time_text");
    info_text = getUITextType("info_text");
    album_text = getUITextType("album_text");
    ratings_image = getUIRepeatedImageType("ratings_image");
    current_visualization_text = getUITextType("current_visualization_text");
    volume_status = getUIStatusBarType("volume_status");
    if (volume_status)
    {
        volume_status->SetTotal(100);
        volume_status->SetOrder(-1);
    }
    visual_blackhole = getUIBlackHoleType("visual_blackhole");

    //  Buttons
    prev_button = getUIPushButtonType("prev_button");
    if (prev_button)
        connect(prev_button, SIGNAL(pushed()), this, SLOT(previous()));

    rew_button = getUIPushButtonType("rew_button");
    if (rew_button)
        connect(rew_button, SIGNAL(pushed()), this, SLOT(seekback()));

    pause_button = getUIPushButtonType("pause_button");
    if (pause_button)
        connect(pause_button, SIGNAL(pushed()), this, SLOT(pause()));

    play_button = getUIPushButtonType("play_button");
    if (play_button)
        connect(play_button, SIGNAL(pushed()), this, SLOT(play()));

    stop_button = getUIPushButtonType("stop_button");
    if (stop_button)
        connect(stop_button, SIGNAL(pushed()), this, SLOT(stop()));

    ff_button = getUIPushButtonType("ff_button");
    if (ff_button)
        connect(ff_button, SIGNAL(pushed()), this, SLOT(seekforward()));

    next_button = getUIPushButtonType("next_button");
    if (next_button)
        connect(next_button, SIGNAL(pushed()), this, SLOT(next()));

    shuffle_button = getUITextButtonType("shuffle_button");
    if (shuffle_button)
        connect(shuffle_button, SIGNAL(pushed()), this, SLOT(toggleShuffle()));

    repeat_button = getUITextButtonType("repeat_button");
    if (repeat_button)
        connect(repeat_button, SIGNAL(pushed()), this, SLOT(toggleRepeat()));

    pledit_button = getUITextButtonType("pledit_button");
    if (pledit_button)
        connect(pledit_button, SIGNAL(pushed()), this, SLOT(editPlaylist()));

    vis_button = getUITextButtonType("vis_button");
    if (vis_button)
        connect(vis_button, SIGNAL(pushed()), this, SLOT(visEnable()));
}

bool PlaybackBoxMusic::getInsertPLOptions(InsertPLOption &insertOption,
                                          PlayPLOption &playOption, bool &bRemoveDups)
{
    MythPopupBox *popup = new MythPopupBox(gContext->GetMainWindow(),
                                      "playlist_popup");

    QLabel *caption = popup->addLabel(tr("Update Playlist Options"), MythPopupBox::Medium);
    caption->setAlignment(Qt::AlignCenter);

    QButton *button = popup->addButton(tr("Replace"));
    popup->addButton(tr("Insert after current track"));
    popup->addButton(tr("Append to end"));
    button->setFocus();

    QLabel *splitter = popup->addLabel(" ", MythPopupBox::Small);
    splitter->setLineWidth(2);
    splitter->setFrameShape(QFrame::HLine);
    splitter->setFrameShadow(QFrame::Sunken);
    splitter->setMinimumHeight((int) (25 * hmult));
    splitter->setMaximumHeight((int) (25 * hmult));

    // only give the user a choice of the track to play if shuffle mode is off
    MythComboBox *playCombo = NULL;
    if (shufflemode == SHUFFLE_OFF)
    {
        playCombo = new MythComboBox(false, popup, "play_combo" );
        playCombo->insertItem(tr("Continue playing current track"));
        playCombo->insertItem(tr("Play first track"));
        playCombo->insertItem(tr("Play first new track"));
        playCombo->setBackgroundOrigin(ParentOrigin);
        popup->addWidget(playCombo);
    }
    
    MythCheckBox *dupsCheck = new MythCheckBox(popup);
    dupsCheck->setText(tr("Remove Duplicates"));
    dupsCheck->setChecked(false);
    dupsCheck->setBackgroundOrigin(ParentOrigin);
    popup->addWidget(dupsCheck);

    int res = popup->ExecPopup();
    switch (res)
    {
        case 0:
            insertOption = PL_REPLACE;
            break;
        case 1:
            insertOption = PL_INSERTAFTERCURRENT;
            break;
        case 2:
            insertOption = PL_INSERTATEND;
            break;
    }

    bRemoveDups = dupsCheck->isChecked();


    // if shuffle mode != SHUFFLE_OFF we always try to continue playing
    // the current track
    if (shufflemode == SHUFFLE_OFF)
    {
        switch (playCombo->currentItem())
        {
            case 0:
                playOption = PL_CURRENT;
                break;
            case 1:
                playOption = PL_FIRST;
                break;
            case 2:
                playOption = PL_FIRSTNEW;
                break;
            default:
                playOption = PL_CURRENT;
        }
    }
    else
        playOption = PL_CURRENT;

    delete popup;

    return (res >= 0);
}


