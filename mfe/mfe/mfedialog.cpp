/*
	mfedialog.cpp

	Copyright (c) 2004-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <stdlib.h>
#include <sys/time.h>

#include <mythtv/mythcontext.h>
#include <mfdclient/playlist.h>
#include <mfdclient/speakertracker.h>

#include "mfedialog.h"
#include "visualize/visualwrapper.h"

MfeDialog::MfeDialog(
                        MythMainWindow *parent, 
                        QString window_name,
                        QString theme_filename,
                        MfdInterface*an_mfd_interface
                    )
          :MythThemedDialog(parent, window_name, theme_filename)
{
    
    //
    //  In the beginning, we don't have one of these
    //
    
    net_flasher = NULL;
    playlist_popup = NULL;
    playlist_name_edit = NULL;

    //
    //  Or one of these
    //
    
    playlist_dialog = NULL;
    mfd_id_for_playlist_dialog = -1;

    //
    //  Pointer to the mfd client library
    //
        
    mfd_interface = an_mfd_interface;


    //
    //  Creat the visualization, and register it through the mfd_interface
    //  (so it will automatically get fed PCM data in another thread
    //  whenever audio is playing)
    //

    visual_wrapper = new VisualWrapper(this);
    mfd_interface->registerVisualization(visual_wrapper);

    //
    //  Wire up the theme
    //

    wireUpTheme();

    //
    //  Connect up signals from the mfd client library
    //
    
    current_mfd = NULL;
    available_mfds.setAutoDelete(true);
    connectUpMfd();

    //
    //  Fire up the visualizer and set geometry to small size.
    //
    
    visual_wrapper->storeSmallGeometry(visual_blackhole->getScreenArea());
    visual_wrapper->storeLargeGeometry(QRect(0, 0, this->width(), this->height()));
    visual_wrapper->setGeometry(visual_blackhole->getScreenArea());
    visual_wrapper->show();

    

    //
    //  Redraw ourselves
    //

    updateForeground();

    //
    //  Do silly things
    //
    
    doSillyThings();

    //
    //  Start the menu last use timer
    //
    
    menu_lastuse_time.start();

} 

void MfeDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    switch(e->key())
    {
        case Key_Up:
            
            menu->MoveUp();            
            handled = true;
            break;
            
        case Key_Down:
        
            menu->MoveDown();
            handled = true;
            break;
            
        case Key_Left:
        
            if(menu->isShown())
            {
                if(menu->getDepth() > 0)
                {
                    menu->MoveLeft();
                    handled = true;
                }
                else
                {
                    menu->toggleShow();
                    handled = true;
                    visual_wrapper->show();
                    menu_lastuse_time.restart();
                }
            }
            break;
            
        case Key_Right:
        
            if(menu->isShown())
            {
                if(!menu->MoveRight())
                {
                    menu->select();
                }
            }
            else
            {
                visual_wrapper->hide();
                handled = true;
                menu->toggleShow();
                if(menu_lastuse_time.elapsed() > 10 * 1000) //  10 seconds
                {
                    menu->GoHome();
                }
            }
            
            handled = true;
            break;

        case Key_M:

            if(menu->toggleShow())
            {
                //
                //  true means it will now be drawn ... we should (?) move
                //  to top menu node (maybe turn this off in some kind of
                //  "expert" mode ?)
                //
                
                if(menu_lastuse_time.elapsed() > 10 * 1000) //  10 seconds
                {
                    menu->GoHome();
                }
                visual_wrapper->hide();
            }
            else
            {
                menu_lastuse_time.restart();
                visual_wrapper->show();
            }
            handled = true;
            break;
            
        case Key_1:
        case Key_C:

            cycleMfd();
            handled = true;
            break;

        case Key_2:
        case Key_S:

            stopAudio();
            handled = true;
            break;

        case Key_4:
            toggleVizFullscreen();
            handled = true;
            break;

        case Key_6:
        
            cycleVisualizer();
            handled = true;
            break;

        case Key_P:
        
            togglePause();
            handled = true;
            break;
            
        case Key_PageDown:

            seekAudio(true);
            handled = true;
            break;
            
        case Key_PageUp:
        
            seekAudio(false);
            handled = true;            
            break;
            
        case Key_Greater:
        case Key_Period:
        case Key_Z:
        case Key_End:
        
            nextPrevAudio(true);
            handled = true;
            break;

        case Key_Less:
        case Key_Comma:
        case Key_Q:
        case Key_Home:
        
            nextPrevAudio(false);
            handled = true;
            break;

        case Key_Enter:
        case Key_Space:
        case Key_Return:
        
            menu->select();
            handled = true;
            break;
            
        //
        //  If the menu's on, just escape out of that, otherwise let the
        //  escape go up to the parent (escape dialog)
        //
        
        case Key_Escape:
        
            if(visual_wrapper->isFullScreen())
            {
                visual_wrapper->toggleSize();
                handled = true;
            }
            else if(menu->isShown())
            {
                menu->toggleShow();
                handled = true;
                menu_lastuse_time.restart();
                visual_wrapper->show();
            }
            break;
            
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void MfeDialog::handleTreeSignals(UIListGenericTree *node)
{
    
    //
    //  The user hit Enter/Select/OK somewhere in the menu tree. Find out
    //  what they select, and then take action accordingly
    //
    
    if( node->getAttribute(1) == 1)
    {
        if(node->getAttribute(0) > 0 && node->getInt() > 0)
        {

            //
            //  It's a pure metadata item
            //

            mfd_interface->playAudio(
                                        current_mfd->getId(),
                                        node->getAttribute(0),
                                        node->getAttribute(1),
                                        node->getInt()
                                    );
        }
        else
        {
        
            //
            //  It's in the item tree, but above a selectable item
            //
        
            GenericTree *first_leaf = node->findLeaf();
            if(first_leaf->getAttribute(0) > 0 && first_leaf->getInt() > 0)
            {

                mfd_interface->playAudio(
                                            current_mfd->getId(),
                                            first_leaf->getAttribute(0),
                                            first_leaf->getAttribute(1),
                                            first_leaf->getInt()
                                        );
            }
        }
        
    }
    else if(node->getAttribute(1) == 2)
    {
    
        if(node->getAttribute(0) > 0 && node->getInt() > 0)
        {

            //
            //  It's an entry in a playlist
            //

            mfd_interface->playAudio(
                                        current_mfd->getId(),
                                        node->getAttribute(0),
                                        node->getAttribute(1),
                                        node->getInt(),
                                        node->getAttribute(2)
                                    );
        }
        else
        {
            //
            //  It's a playlist thing, but above a leaf item
            //
            
            GenericTree *first_leaf = node->findLeaf();
            if(first_leaf->getAttribute(0) > 0 && first_leaf->getInt() > 0)
            {

                //
                //  It's an entry in a playlist
                //

                mfd_interface->playAudio(
                                            current_mfd->getId(),
                                            first_leaf->getAttribute(0),
                                            first_leaf->getAttribute(1),
                                            first_leaf->getInt(),
                                            first_leaf->getAttribute(2)
                                        );
            }
        }
    }
    else if(node->getAttribute(1) == 3)
    {
        //
        //  It's an mfd to choose from
        //
        
        if(current_mfd)
        {
            if(current_mfd->getId() != node->getInt())
            {
                QStringList route_to_current = menu->getRouteToCurrent();
                current_mfd->setPreviousTreePosition(route_to_current);
                current_mfd->setShowingMenu(menu->isShown());
                switchToMfd(node->getInt());
            }
        }
    }
    else if(node->getAttribute(1) == 4 && node->getInt() > 0)
    {
        if(current_mfd)
        {
            //
            //  Pop up a dialogue to get a new playlist name
            //
        
            content_collection_for_new_playlist = node->getInt();
            showNewPlaylistPopup();
        }
    }
    else if(node->getAttribute(1) == 5)
    {
        //
        //  Use wants to edit a playlist, see if they selected an actualy playlist
        //
        
        if(node->getInt() > 0)
        {
            if(current_mfd)
            {
                doPlaylistDialog(node->getAttribute(0), node->getInt(), node->getString());
            }
            else
            {
                cerr << "mfedialog.o: no current_mfd while trying to switch "
                     << "to playlist editing"
                     <<endl;
            }
        }
    }
    else if(node->getAttribute(1) == 6)
    {
        //
        //  Speaker of some sort; toggle them on and off
        //
        
        if(node->getCheck() == 0)
        {
            node->setCheck(2);
            if(current_mfd)
            {
                mfd_interface->toggleSpeakers(current_mfd->getId(), node->getAttribute(0), true);
            }
        }
        else if(node->getCheck() == 2)
        {
            node->setCheck(0);
            if(current_mfd)
            {
                mfd_interface->toggleSpeakers(current_mfd->getId(), node->getAttribute(0), false);
            }
        }
        else
        {
            cerr << "how exactly is a speaker neither on nor off?"
                 << endl;
        }

        menu->refresh();
    }
    else if(node->getAttribute(1) == 8)
    {
        //
        //  User wants to delete a playlist
        //
        
        if(current_mfd && node->getInt() > 0)
        {
            //
            //  Pop up a dialogue to get a new playlist name
            //

            node->setActive(false);
            menu->refresh();
        
            mfd_interface->deletePlaylist(
                                            current_mfd->getId(), 
                                            node->getAttribute(0),
                                            node->getInt()
                                         );
        }
    }
}

void MfeDialog::doPlaylistDialog(int collection_id, int playlist_id, const QString playlist_name)
{
    if(!current_mfd)
    {
        cerr << "something called MfeDialog::doPlaylistDialog() while current_mfd == NULL" << endl;
        return;
    }

    UIListGenericTree *pristine_playlist_tree = current_mfd->getPlaylistTree(collection_id, playlist_id, true);
    UIListGenericTree *working_playlist_tree  = current_mfd->getPlaylistTree(collection_id, playlist_id, false);
    UIListGenericTree *pristine_content_tree  = current_mfd->getContentTree(collection_id, true);
    UIListGenericTree *working_content_tree   = current_mfd->getContentTree(collection_id, false);
                
    if ( pristine_content_tree && pristine_playlist_tree && working_content_tree && working_playlist_tree)
    {
        mfd_id_for_playlist_dialog = current_mfd->getId();
        playlist_dialog = new PlaylistDialog(
                                                gContext->GetMainWindow(),
                                                "playlist_dialog",
                                                "mfe-",
                                                mfd_interface,
                                                current_mfd,
                                                pristine_playlist_tree,
                                                pristine_content_tree,
                                                working_playlist_tree,
                                                working_content_tree,
                                                playlist_name
                                            );
        playlist_dialog->exec();
        if (playlist_dialog->commitEdits())
        {
            bool is_new = false;
            if(playlist_id == -1)
            {
                is_new = true;
            }
            mfd_interface->commitListEdits(
                                            mfd_id_for_playlist_dialog,
                                            playlist_dialog->getWorkingPlaylist(),
                                            is_new,
                                            playlist_name
                                          );
        }
        else if (playlist_dialog->playlistDeletedElsewhere() )
        {
            MythPopupBox::showOkPopup(
                                        gContext->GetMainWindow(), 
                                        "", 
                                        QString("The playlist you were editing was deleted out from under you")
                                     );
        }
        delete playlist_dialog;
        playlist_dialog = NULL;
        mfd_id_for_playlist_dialog = -1;
    }
    else
    {
        cerr << "mfedialog.o: current mfd could not give us a "
             << "playlist tree for playlist id of "
             << playlist_id
             << " in collection with id of "
             << collection_id
             << endl;
    }
}


void MfeDialog::wireUpTheme()
{


    menu = getUIListTreeType("musictree");
    if (!menu)
    {
        cerr << "no musictree in your theme doofus" << endl;
    }
    menu->hide();
    
    connect(menu, SIGNAL(selected(UIListGenericTree*)),
            this, SLOT(handleTreeSignals(UIListGenericTree*)));
 
    current_mfd_text = getUITextType("current_mfd_text");
    current_mfd_text->SetText("No mfd!!!");
    
    now_playing_texts = getUIMultiTextType("now_playing_text");
    
    time_progress = getUIStatusBarType("time_progress");
    if(!time_progress)
    {
        cerr << "you have no time_progress status bar in your theme" << endl;
    }
    time_progress->SetTotal(1000);
    time_progress->SetUsed(0);
 
    stop_button = getUIImageType("stop_button");
    stop_button->show();
 
    play_button = getUIImageType("play_button");
    play_button->hide();

    pause_button = getUIImageType("pause_button");
    pause_button->hide();
 
    ff_button = getUIPushButtonType("ff_button");
    rew_button = getUIPushButtonType("rew_button");
 
    next_button = getUIPushButtonType("next_button");
    prev_button = getUIPushButtonType("prev_button");
 
    network_icon = getUIImageType("net_flasher");
    if(network_icon)
    {
        network_icon->hide();
        net_flasher = new NetFlasher(network_icon);
    }
 
    background_image = getUIImageType("mfe_background");
    if(!background_image)
    {
        cerr << "You have no mfe_background image declared" << endl;
    }
 
    visual_blackhole = getUIBlackHoleType("visual_blackhole");

 
    //
    // Make the first few nodes in the tree that everything else hangs off
    // as children
    //

    menu_root_node = new UIListGenericTree(NULL, "Menu Root Node");

    browse_node  = new UIListGenericTree(menu_root_node, " Browse Music");
    manage_node  = new UIListGenericTree(menu_root_node, " Manage Content");
    connect_node = new UIListGenericTree(menu_root_node, " Control other Myth Boxes");
    setup_node   = new UIListGenericTree(menu_root_node, " Settings");
    speaker_node = new UIListGenericTree(setup_node, " Speakers");
    
    menu->SetTree(menu_root_node);

}


void MfeDialog::connectUpMfd()
{

    //
    //  NB: this has nothing to do with socket "connections", just set up
    //  how we handle signals the mfd client library fires off (in this main
    //  gui execution thread) when something happens.
    //


    connect(mfd_interface, SIGNAL(mfdDiscovery(int, QString, QString, bool)),
            this, SLOT(mfdDiscovered(int, QString, QString, bool)));

    connect(mfd_interface, SIGNAL(audioPluginDiscovery(int)),
            this, SLOT(audioPluginDiscovered(int)));

    connect(mfd_interface, SIGNAL(audioPaused(int, bool)),
            this, SLOT(paused(int, bool)));

    connect(mfd_interface, SIGNAL(audioStopped(int)),
            this, SLOT(stopped(int)));
    
    connect(mfd_interface, SIGNAL(audioPlaying(int, int, int, int, int, int, int, int, int)),
            this, SLOT(playing(int, int, int, int, int, int, int, int, int)));

    connect(mfd_interface, SIGNAL(metadataChanged(int, MfdContentCollection *)),
            this, SLOT(changeMetadata(int, MfdContentCollection *)));

    connect(mfd_interface, SIGNAL(playlistCheckDone()),
            this, SLOT(playlistCheckDone()));
            
    connect(mfd_interface, SIGNAL(speakerList(int, QPtrList<SpeakerTracker>*)),
            this, SLOT(speakerList(int, QPtrList<SpeakerTracker>*)));
            
}

void MfeDialog::mfdDiscovered(int which_mfd, QString name, QString host, bool found)
{
    if(found)
    {
        if(available_mfds.find(which_mfd))
        {
            cerr << "mfedialog.o: mfd client library is claiming a new mfd, but giving "
                 << "me an old id"
                 << endl;
        }
        else
        {

            MfdInfo *new_mfd = new MfdInfo(which_mfd, name, host);
            available_mfds.insert(which_mfd, new_mfd);
        
            if(current_mfd == NULL)
            {
                current_mfd = new_mfd;
                syncToCurrentMfd();
            }
        }
    }
    else
    {
        bool redo_current = false;
        if(current_mfd->getId() == which_mfd)
        {
            redo_current = true;
        }
        if(!available_mfds.remove(which_mfd))
        {
            cerr << "mfedialog.o: can't remove mfd that does not exist" << endl;
        }
        
        if(redo_current)
        {
            if(available_mfds.count() > 0)
            {
                current_mfd = available_mfds[0];
            }
            else
            {
                current_mfd = NULL;
            }
            syncToCurrentMfd();
        }
        if(playlist_dialog && mfd_id_for_playlist_dialog == which_mfd)
        {
            //
            //  Oh crap. User in the middle of editing a playlist, and the
            //  mfd went away. Not much we can do here, as even if we tried
            //  to grab the current edits the user has made, we literaly
            //  have nowhere to send them.
            //

            playlist_dialog->close();

        }
        
    }
    updateConnectionList();
}

void MfeDialog::audioPluginDiscovered(int which_mfd)
{
    mfd_interface->askForStatus(which_mfd);
}

void MfeDialog::changeMetadata(int which_mfd, MfdContentCollection *new_collection)
{
    if(net_flasher)
    {
        net_flasher->flash();
    }

    if(available_mfds.find(which_mfd))
    {
        available_mfds[which_mfd]->setMfdContentCollection(new_collection);

        if(current_mfd->getId() == which_mfd)
        {

            //
            //  Get route to where the user currently is
            //
            
            QStringList route_to_current = menu->getRouteToCurrent();

            //
            //  If there's anything there already, delete it
            //
    
            browse_node->pruneAllChildren();
    
            //
            //  add all the branches to content
            //
            
            browse_node->addNode(new_collection->getAudioArtistTree());
            browse_node->addNode(new_collection->getAudioGenreTree());
            browse_node->addNode(new_collection->getAudioPlaylistTree());
            browse_node->addNode(new_collection->getAudioCollectionTree());
            browse_node->setDrawArrow(true);

            //
            //  Add nodes that let us manage content
            //
            
            manage_node->setDrawArrow(false);
            manage_node->pruneAllChildren();

            UIListGenericTree *delete_nodes = new_collection->getDeletablePlaylistTree();
            if(delete_nodes)
            {
                manage_node->addNode(delete_nodes);
                manage_node->setDrawArrow(true);
            }

            UIListGenericTree *new_nodes = new_collection->getNewPlaylistTree();
            if(new_nodes)
            {
                manage_node->addNode(new_nodes);
                manage_node->setDrawArrow(true);
            }

            UIListGenericTree *edit_nodes = new_collection->getEditablePlaylistTree();
            if(edit_nodes)
            {
                manage_node->addNode(edit_nodes);
                manage_node->setDrawArrow(true);
            }

            //
            //  Set the current position back to where the user was
            //

            menu->tryToSetCurrent(route_to_current);
            menu->refresh();
    
            //
            //  If the mfd_info object doesn't know what's currently
            //  playing, see if it can now find out given that new metadata
            //  has just arrived
            //
            
            if(!current_mfd->knowsWhatsPlaying())
            {
                current_mfd->setCurrentPlayingData();
                now_playing_texts->setTexts(current_mfd->getPlayingStrings());
                visual_wrapper->assignOverlayText(current_mfd->getPlayingStrings());
                time_progress->SetUsed((int)(current_mfd->getPercentPlayed() * 1000));
            }    
        }
        
        //
        //  If we have playlist dialog open that is using data from the mfd
        //  that just updated, we need to tell it to swap to the pristine
        //  playlist and content tree
        //
        
        if(playlist_dialog && mfd_id_for_playlist_dialog == which_mfd)
        {
            playlist_dialog->swapToNewPristine();
        }
    }
    else
    {
        cerr << "mfedialog.o: seem to be getting metadata for an mfd that "
             << "does not exist"
             << endl;
    }
}

void MfeDialog::playlistCheckDone()
{
    if(playlist_dialog)
    {
        playlist_dialog->playlistCheckDone();
    }
    else
    {
        cerr << "mfedialog.o: I got a sorted playlist back, but have no "
             << "playlist dialog (?)"
             << endl;
    }
}

void MfeDialog::cycleMfd()
{
    if(current_mfd && available_mfds.count() > 1)
    {
        QStringList route_to_current = menu->getRouteToCurrent();
        current_mfd->setPreviousTreePosition(route_to_current);
        current_mfd->setShowingMenu(menu->isShown());

        QIntDictIterator<MfdInfo> it( available_mfds ); 
        for ( ; it.current(); ++it )
        {
            if(it.current()->getId() == current_mfd->getId())
            {
                ++it;
                if(!it.current())
                {
                    it.toFirst();
                }
                switchToMfd(it.current()->getId());
                break;
            }
        }
        
    }
}

void MfeDialog::stopAudio()
{
    if(current_mfd)
    {
        mfd_interface->stopAudio(current_mfd->getId());
    }
}

void MfeDialog::togglePause()
{
    if(current_mfd)
    {
        mfd_interface->pauseAudio(current_mfd->getId(), !current_mfd->getPauseState());
    }
}

void MfeDialog::seekAudio(bool forward_or_back)
{
    if(current_mfd)
    {
        if(forward_or_back)
        {
            ff_button->push();
            mfd_interface->seekAudio(current_mfd->getId(), 5);
        }
        else
        {
            rew_button->push();
            mfd_interface->seekAudio(current_mfd->getId(), -5);
        }
    }
}

void MfeDialog::nextPrevAudio(bool next_or_prev)
{
    if(current_mfd)
    {
        if(next_or_prev)
        {
            next_button->push();
            mfd_interface->nextAudio(current_mfd->getId());
        }
        else
        {
            prev_button->push();
            mfd_interface->prevAudio(current_mfd->getId());
        }
    }
}

void MfeDialog::paused(int which_mfd, bool paused)
{

    MfdInfo *relevant_mfd = available_mfds.find(which_mfd);
    
    if(relevant_mfd)
    {
        relevant_mfd->setPauseState(paused);
        if(paused)
        {
            relevant_mfd->isStopped(false);
        }
        if(relevant_mfd == current_mfd)
        {
            if(paused)
            {
                pause_button->show();
            }
            else
            {
                pause_button->hide();
            }
        }
    }
    else
    {
        cerr << "getting pause information for non-existant mfd" << endl;
    }    
}

void MfeDialog::stopped(int which_mfd)
{

    MfdInfo *relevant_mfd = available_mfds.find(which_mfd);
    
    if(relevant_mfd)
    {
        relevant_mfd->clearCurrentPlayingData();
        relevant_mfd->setPauseState(false);
        relevant_mfd->isStopped(true);
        if(relevant_mfd == current_mfd)
        {
            now_playing_texts->clearTexts();
            visual_wrapper->assignOverlayText(QStringList());
            time_progress->SetUsed(0);

            //
            //  Make buttons reflect the fact that the audio plugin in the mfd is
            //  stopped
            //
    
            play_button->hide();
            pause_button->hide();
            stop_button->show();
        }
    }
    else
    {
        cerr << "getting stopped() data for a non-existent mfd" << endl;
    }
    
}

void MfeDialog::playing(
                            int which_mfd, 
                            int /* playlist */,
                            int /* playlist_index */,
                            int container_id,
                            int metadata_id,
                            int seconds,
                            int /* channels */,
                            int /* bitrate */,
                            int /* frequency */
                         )
{

    //
    //  Update the playing status of whichever mfd this is for
    //
    
    
    MfdInfo *relevant_mfd = available_mfds.find(which_mfd);
    
    if(relevant_mfd)
    {
        bool update_texts = relevant_mfd->setCurrentPlayingData(container_id, metadata_id, seconds);
        relevant_mfd->isStopped(false);
        if(relevant_mfd == current_mfd)
        {
            if(update_texts)
            {
                now_playing_texts->setTexts(current_mfd->getPlayingStrings());
                visual_wrapper->assignOverlayText(current_mfd->getPlayingStrings());
            }
            time_progress->SetUsed((int)(current_mfd->getPercentPlayed() * 1000));
            
            stop_button->hide();
            play_button->show();
        }
    }
    else
    {
        cerr << "getting playing() data for a non-existent mfd" << endl;
    }
    
}

void MfeDialog::syncToCurrentMfd()
{
    if(current_mfd)
    {
        current_mfd_text->SetText(current_mfd->getHost());
        
        //
        //  Swap out browse nodes for the ones from this mfd
        //

        browse_node->pruneAllChildren();
        browse_node->setDrawArrow(false);
        manage_node->pruneAllChildren();
        manage_node->setDrawArrow(false);

        MfdContentCollection *current_collection = current_mfd->getMfdContentCollection();
        
        if(current_collection)
        {
            browse_node->setDrawArrow(true);
            browse_node->addNode(current_collection->getAudioArtistTree());
            browse_node->addNode(current_collection->getAudioGenreTree());
            browse_node->addNode(current_collection->getAudioPlaylistTree());
            browse_node->addNode(current_collection->getAudioCollectionTree());

            //
            //  Add nodes that let us manage content
            //
            
            UIListGenericTree *delete_nodes = current_collection->getDeletablePlaylistTree();
            if(delete_nodes)
            {
                manage_node->addNode(delete_nodes);
                manage_node->setDrawArrow(true);
            }

            UIListGenericTree *new_nodes = current_collection->getNewPlaylistTree();
            if(new_nodes)
            {
                manage_node->addNode(new_nodes);
                manage_node->setDrawArrow(true);
            }

            UIListGenericTree *edit_nodes = current_collection->getEditablePlaylistTree();
            if(edit_nodes)
            {
                manage_node->addNode(edit_nodes);
                manage_node->setDrawArrow(true);
            }

        }

        //
        //  Set the current position back to where the user was
        //

        if(current_mfd->getShowingMenu())
        {
            menu->show();
        }
        else
        {
            menu->hide();
        }

        menu->tryToSetCurrent(current_mfd->getPreviousTreePosition());
        menu->refresh();
        
        //
        //  Update the now playing text
        //
        
        if(current_mfd->isStopped())
        {
            play_button->hide();
            pause_button->hide();
            stop_button->show();
            now_playing_texts->clearTexts();
            visual_wrapper->assignOverlayText(QStringList());
            time_progress->SetUsed(0);
        }
        else
        {
            stop_button->hide();
            play_button->show();
            if(!current_mfd->knowsWhatsPlaying())
            {
                current_mfd->setCurrentPlayingData();
            }
            now_playing_texts->setTexts(current_mfd->getPlayingStrings());
            visual_wrapper->assignOverlayText(current_mfd->getPlayingStrings());
            time_progress->SetUsed((int)(current_mfd->getPercentPlayed() * 1000));
            if(current_mfd->getPauseState())
            {
                pause_button->show();
            }
            else
            {
                pause_button->hide();
            }
        }
        updateConnectionList();
        updateSpeakerDisplay();
    }
    else
    {
        current_mfd_text->SetText("No mfd!!!");
        browse_node->pruneAllChildren();
        browse_node->setDrawArrow(false);
        if(menu->isShown())
        {
            menu->toggleShow();
            menu_lastuse_time.restart();
        }
        //menu->GoHome();
        now_playing_texts->clearTexts();
        visual_wrapper->assignOverlayText(QStringList());
        time_progress->SetUsed(0);
        speaker_node->deleteAllChildren();
    }
}

void MfeDialog::updateConnectionList()
{
    //
    //  Make sure the list of mfd's this frontend is connected to is up to
    //  date
    //
    
    QStringList route_to_current = menu->getRouteToCurrent();

    connect_node->deleteAllChildren();
    connect_node->setDrawArrow(false);
    
    if(available_mfds.count() > 1)
    {
        connect_node->setDrawArrow(true);
    }
    
    if(current_mfd)
    {
        QIntDictIterator<MfdInfo> it(available_mfds); 
        for( ; it.current(); ++it)
        {
            if(it.current()->getId() != current_mfd->getId())
            {
                UIListGenericTree *an_mfd_node = new UIListGenericTree(connect_node, it.current()->getHost());
                an_mfd_node->setInt(it.current()->getId());
                an_mfd_node->setAttribute(1, 3);
            }
        }
    }
    menu->tryToSetCurrent(route_to_current);
    menu->refresh();

}

void MfeDialog::switchToMfd(int an_mfd_id)
{
    MfdInfo *target_mfd = available_mfds.find(an_mfd_id);
    if(!target_mfd)
    {
        cerr << "can't switch to mfd that doesn't exist" << endl;
        return;
    }
    
    current_mfd = target_mfd;
    syncToCurrentMfd();
}

void MfeDialog::speakerList(int which_mfd, QPtrList<SpeakerTracker>* speakers)
{

    MfdInfo *target_mfd = available_mfds.find(which_mfd);
    if(!target_mfd)
    {
        cerr << "can't store speaker information for an mfd that doesn't exist" << endl;
        return;
    }
    
    target_mfd->setSpeakerList(speakers);
    
    if(current_mfd == target_mfd)
    {
        updateSpeakerDisplay();
    }
}
    
void MfeDialog::updateSpeakerDisplay()
{
    if(current_mfd)
    {
        QStringList route_to_current = menu->getRouteToCurrent();

        speaker_node->deleteAllChildren();

        QPtrList<SpeakerTracker> *speakers = current_mfd->getSpeakerList();
        SpeakerTracker *a_speaker;
        QPtrListIterator<SpeakerTracker> iter( *speakers );

        while ( (a_speaker = iter.current()) != 0 )
        {
            UIListGenericTree *new_node = new UIListGenericTree(speaker_node, a_speaker->getName());
            new_node->setAttribute(1, 6);
            new_node->setAttribute(0, a_speaker->getId());
            if(a_speaker->getInUse() == "yes")
            {
                new_node->setCheck(2);
            }
            else
            {
                new_node->setCheck(0);
            }
            ++iter;
        }

        menu->tryToSetCurrent(route_to_current);
        menu->refresh();
    }
}

void MfeDialog::showNewPlaylistPopup()
{

    if (playlist_popup)
    {
        return;
    }

    playlist_popup = new MythPopupBox(gContext->GetMainWindow(), "playlist_popup");
    
    if(background_image)
    {
        playlist_popup->setErasePixmap(background_image->GetImage());
    }


    playlist_name_edit = new MythRemoteLineEdit(playlist_popup);
    playlist_name_edit->setText(constructPlaylistName());
    playlist_name_edit->setAlignment(Qt::AlignHCenter);
    playlist_popup->addWidget(playlist_name_edit);

    QButton *create_b = playlist_popup->addButton("Create This Playlist",
                                               this, SLOT(createNewPlaylist()));

    QButton *cancel_b = playlist_popup->addButton("Cancel",
                                               this, SLOT(hideNewPlaylistPopup()));

    playlist_popup->ShowPopup(this, SLOT(hideNewPlaylistPopup()));

    cancel_b->setFocus();   // avoiding -Wall
    create_b->setFocus();

}

void MfeDialog::hideNewPlaylistPopup()
{

    if (!playlist_popup)
    {
        return;
    }
    
    playlist_popup->hide();
    {
        delete playlist_popup;
        playlist_popup = NULL;
    }
}

void MfeDialog::createNewPlaylist()
{
    QString new_playlist_name;
    if(playlist_name_edit)
    {
        new_playlist_name = playlist_name_edit->text();
    }
    hideNewPlaylistPopup();
    
    if(new_playlist_name.length() > 0 && current_mfd)
    {
        doPlaylistDialog(content_collection_for_new_playlist, -1, new_playlist_name);
        content_collection_for_new_playlist = -1;
    }
}

QString MfeDialog::constructPlaylistName()
{
    QString response;

    if(rand() % 100 > 50)
    {
        response.append(possessors[rand() % possessors.count()]);
        response.append(" ");
    }
    else
    {
        response.append(modifiers[rand() % modifiers.count()]);
        response.append(" ");
    }

    response.append(modifiers[rand() % modifiers.count()]);
    response.append(" ");

    response.append(nouns[rand() % nouns.count()]);

    return response;
}

void MfeDialog::doSillyThings()
{

    //
    //  (Pseudo-)Randomly Seed the (Pseudo-)Random number generator
    //

    struct timeval tv;
    gettimeofday(&tv,0);
    unsigned int seed = tv.tv_sec + tv.tv_usec;
    srand(seed);

    //
    //  Fill in some data structures that let us create random playlist
    //  names
    //
    
    possessors.append("Big Daddy's");
    possessors.append("Certainly");
    possessors.append("Chutt's");
    possessors.append("Deviously");
    possessors.append("Everyone's");
    possessors.append("Existentially");
    possessors.append("Flatly");
    possessors.append("Foobar's");
    possessors.append("His Majesty's");
    possessors.append("Infinitely");
    possessors.append("Neatly");
    possessors.append("Nobody's");
    possessors.append("One's");
    possessors.append("The Other's");
    possessors.append("Our");
    possessors.append("Plainly");
    possessors.append("Squarely");
    possessors.append("Walter's");
    possessors.append("Yo Mama's");
    possessors.append("Zippy's");


    modifiers.append("Blue");
    modifiers.append("Bouncy");
    modifiers.append("Buzzed");
    modifiers.append("Cacaphonous");
    modifiers.append("Dark");
    modifiers.append("Diabolical");
    modifiers.append("Dotted");
    modifiers.append("Easy");
    modifiers.append("Efficient");
    modifiers.append("Happy");
    modifiers.append("Inverted");
    modifiers.append("Obtuse");
    modifiers.append("Odd");
    modifiers.append("Orthogonal");
    modifiers.append("Pedantic");
    modifiers.append("Refracted");
    modifiers.append("Spotted");
    modifiers.append("Sublime");
    modifiers.append("Tempted");
    modifiers.append("Weak");
    
    
    nouns.append("Accoutrement");
    nouns.append("Appliance");
    nouns.append("Big Bah");
    nouns.append("Bottom");
    nouns.append("Breakdown");
    nouns.append("Creep");
    nouns.append("Device");
    nouns.append("Endeavour");
    nouns.append("Expanse");
    nouns.append("Expression");
    nouns.append("Freedom");
    nouns.append("Gesture");
    nouns.append("Inversion");
    nouns.append("Mountains");
    nouns.append("Mix");
    nouns.append("Other");
    nouns.append("Staples");
    nouns.append("Stretch");
    nouns.append("Top");
    nouns.append("Wonder Bread");

}

void MfeDialog::toggleVizFullscreen()
{
    visual_wrapper->toggleSize();
}

void MfeDialog::cycleVisualizer()
{
    cout << "I should be cycling the visual_wrapper" << endl;
}

MfeDialog::~MfeDialog()
{

    mfd_interface->deregisterVisualization();
    available_mfds.clear();
    if(menu_root_node)
    {
        delete menu_root_node;
        menu_root_node = NULL;
    }
    
    if(net_flasher)
    {
        delete net_flasher;
        net_flasher = NULL;
    }
    
}

