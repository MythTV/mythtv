/*
	playlistdialog.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <iostream>
using namespace std;

#include <mfdclient/playlist.h>

#include "playlistdialog.h"

PlaylistDialog::PlaylistDialog(
                                MythMainWindow *parent, 
                                QString window_name,
                                QString theme_filename,
                                MfdInterface *an_mfd_interface,
                                MfdInfo *an_mfd,
                                UIListGenericTree *a_pristine_playlist_tree,
                                UIListGenericTree *a_pristine_content_tree,
                                UIListGenericTree *a_working_playlist_tree,
                                UIListGenericTree *a_working_content_tree,
                                const QString& playlist_name
                              )
          :MythThemedDialog(parent, window_name, theme_filename)
{
    playlist_deleted_elsewhere = false;
    //
    //  Pointer to the mfd client library
    //
    
    mfd_interface = an_mfd_interface;
    
    //
    //  Pointer to object that holds information about content in "current" mfd
    //
        
    current_mfd = an_mfd;

    //
    //  What the hell is all this? Well ... we get two copies of the
    //  playlist (thing on the left) and two copies of the selectable data
    //  (thing on the right). We only show the first copy (pristine), but
    //  don't let the user edit it. This is just a trick to show that the
    //  system is being responsive while another thread is busy ticking off
    //  the content tree to reflect the current state of the playlist. Once
    //  that's ready (if you have a fast computer and/or small amounts of
    //  tracks you probably won't even notice), the editable data gets
    //  swapped in. We can't show the editable data at startup because the
    //  other thread (PlaylistChecker object in mfd client library) is busy
    //  writing to it.
    //


    pristine_playlist_tree = a_pristine_playlist_tree;
    pristine_content_tree = a_pristine_content_tree;

    working_playlist_tree = a_working_playlist_tree;
    working_content_tree = a_working_content_tree;

    current_playlist_tree = pristine_playlist_tree;
    current_content_tree = pristine_content_tree;

    //
    //  We do not start out holding a track
    //      
    
    holding_track = false;
    held_track = NULL;

    //
    //  Wire up the theme
    //

    wireUpTheme();

    //
    //  Set the edit title
    //

    edit_title->SetText(QString("Edit Playlist: %1").arg(playlist_name));

    //
    //  On startup, we do not allow editing
    //

    allowEditing(false);

    //
    //  Almost tautologically, at startup, nothing has been moved around
    //
    
    something_changed_position = false;

    //
    //  Now that the user is effectively locked out of touching anything,
    //  ask the mfd client library to find, sort, and pre-check the data we
    //  need
    //

    mfd_interface->startPlaylistCheck(
                                        current_mfd->getMfdContentCollection(),
                                        working_playlist_tree,
                                        working_content_tree,
                                        &playlist_additions,    // empty
                                        &playlist_deletions
                                     );
                                     
    //
    //  Calculate and show the number of tracks on the playlist in question
    //
 
    countAndDisplayPlaylistTracks();

    //
    //  Resize the two QIntDict's that keep track of local additions and
    //  deletions to nice big prime numbers
    //
    
    playlist_additions.resize(9973);
    playlist_deletions.resize(9973);
} 

void PlaylistDialog::allowEditing(bool yes_or_no)
{
    editing_allowed = yes_or_no;
    
    if (editing_allowed)
    {

        if (net_flasher)
        {
            net_flasher->stop();
        }

        playlist_tree_menu->RedrawCurrent();
        content_tree_menu->RedrawCurrent();
        
    }
    else
    {
        if (net_flasher)
        {
            net_flasher->flash();
        }
    }
}

void PlaylistDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    
    if (holding_track)
    {
        //
        //  User is moving a track around in the playlist, so only a limited
        //  set of keys should work
        //
        
        switch(e->key())
        {
            case Key_Up:
            
                moveHeldUpDown(true);
                break;
            
            case Key_Down:
            
                moveHeldUpDown(false);
                break;
            
            case Key_Escape:
            case Key_Enter:
            case Key_Space:
            case Key_Return:
        
                stopHoldingTrack();
                break;
        }

        return;
    }

    switch(e->key())
    {

        case Key_Up:
            
            current_menu->MoveUp();            
            handled = true;
            break;
            
        case Key_Down:
        
            current_menu->MoveDown();
            handled = true;
            break;
            
        case Key_Left:
        
            if (current_menu == content_tree_menu)
            {
                if (current_menu->getDepth() > 0)
                {
                    current_menu->MoveLeft();
                }
                else
                {
                    current_menu = playlist_tree_menu;
                    playlist_tree_menu->setActive(true);
                    content_tree_menu->setActive(false);
                    current_menu->enter();
                    countAndDisplayPlaylistTracks();
                }
            }
            else
            {
                this->close();
            }
            handled = true;
            break;
            
        case Key_Right:
        
            if (current_menu == playlist_tree_menu)
            {
                current_menu = content_tree_menu;
                playlist_tree_menu->setActive(false);
                content_tree_menu->setActive(true);
                current_menu->enter();
                countAndDisplayPlaylistTracks();
            }
            else
            {
                current_menu->MoveRight();
            }
            handled = true;
            break;

        case Key_Enter:
        case Key_Space:
        case Key_Return:
        
            current_menu->select();
            handled = true;
            break;
            
    }

    //
    //  Whatever happened above, draw the left/right arrows to jump between
    //  menus correctly
    //
    
    if (current_menu == playlist_tree_menu)
    {
        leftarrow_on->hide();
        leftarrow_off->show();
        rightarrow_on->show();
        rightarrow_off->hide();
    }
    else
    {
        rightarrow_on->hide();
        rightarrow_off->show();
        if (current_menu->getDepth() < 1)
        {
            leftarrow_on->show();
            leftarrow_off->hide();
        }
        else
        {
            leftarrow_off->show();
            leftarrow_on->hide();
        }
    }
    
    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void PlaylistDialog::playlistCheckDone()
{
    //
    //  We get handed this announcement via an event that bubbles up from
    //  the mfd client library. Until something else tells us otherwise, it
    //  is now fine to let the user edit things.
    //
    
    QStringList route_to_playlist = playlist_tree_menu->getRouteToCurrent();
    QStringList route_to_content = content_tree_menu->getRouteToCurrent();
    
    
    current_playlist_tree = working_playlist_tree;
    current_content_tree = working_content_tree;
    
    playlist_tree_menu->SetTree(working_playlist_tree);
    content_tree_menu->SetTree(working_content_tree);

    content_tree_menu->tryToSetCurrent(route_to_content);
    if(!playlist_tree_menu->tryToSetCurrent(persistent_route_to_playlist))
    {
        playlist_tree_menu->tryToSetCurrent(route_to_playlist);
    }

    playlist_tree_menu->RefreshCurrentLevel();
    content_tree_menu->RefreshCurrentLevel();


    allowEditing(true);

    current_menu->enter();
    
    countAndDisplayPlaylistTracks();
}

void PlaylistDialog::swapToNewPristine()
{
    //
    //  DAMN. Mfd sent us new data, and we were in the middle of something.
    //  Oh well.
    //
    
    if (holding_track)
    {
        stopHoldingTrack();
    }

    allowEditing(false);


    persistent_route_to_playlist = playlist_tree_menu->getRouteToCurrent();
    QStringList route_to_content = content_tree_menu->getRouteToCurrent();

    int which_collection = pristine_playlist_tree->getAttribute(0);
    int which_playlist = pristine_playlist_tree->getInt();

    pristine_playlist_tree = current_mfd->getPlaylistTree(
                                                            which_collection,
                                                            which_playlist,
                                                            true
                                                         );
    if (!pristine_playlist_tree)
    {
        //  
        //  Uh, oh. Playlist we were in the middle of editing got deleted
        //  somewhere else
        //
        
        playlist_deleted_elsewhere = true;
        close();
        return;
    }                                           
    working_playlist_tree = current_mfd->getPlaylistTree(
                                                            which_collection,
                                                            which_playlist,
                                                            false
                                                         );
    if (!working_playlist_tree)
    {
        playlist_deleted_elsewhere = true;
        close();
        return;
    }
                                                         
    pristine_content_tree = current_mfd->getContentTree(
                                                        which_collection,
                                                        true
                                                       );
    

    working_content_tree = current_mfd->getContentTree(
                                                        which_collection,
                                                        false
                                                       );
    current_playlist_tree = pristine_playlist_tree;
    current_content_tree = pristine_content_tree;

    playlist_tree_menu->SetTree(current_playlist_tree);
    content_tree_menu->SetTree(current_content_tree);

    content_tree_menu->tryToSetCurrent(route_to_content);
    playlist_tree_menu->tryToSetCurrent(persistent_route_to_playlist);
    
    playlist_tree_menu->RefreshCurrentLevel();
    content_tree_menu->RefreshCurrentLevel();

    current_menu->enter();

    countAndDisplayPlaylistTracks();

    //
    //  Set the tree checking thread working again
    //

    mfd_interface->startPlaylistCheck(
                                        current_mfd->getMfdContentCollection(),
                                        working_playlist_tree,
                                        working_content_tree,
                                        &playlist_additions,
                                        &playlist_deletions
                                     );

}

bool PlaylistDialog::commitEdits()
{
    if(playlist_deleted_elsewhere)
    {
        return false;
    }
    
    //
    //  If someone is asking us this question, it means that we are about to
    //  disappear. As long as we're not running pristine and at least one
    //  change occured, say YUP.
    //
    
    if (current_content_tree == working_content_tree)
    {
        if (
              something_changed_position   ||
            ! playlist_additions.isEmpty() ||
            ! playlist_deletions.isEmpty()
           )
        {
            return true;
        }
    }
    else
    {
        //
        //  The Playlist Checking thread must still be running then ... shut
        //  that puppy down
        //
        
        mfd_interface->stopPlaylistCheck();
        cerr << "Playlist editor exited before working copy was finished. "
             << "Edits will be lost. Remind someone to fix this."
             << endl;

    }
    return false;
}

void PlaylistDialog::handlePlaylistEntered(UIListTreeType*, UIListGenericTree *node)
{

    if (current_menu != playlist_tree_menu)
    {
        return;
    }

    if (node)
    {
        if (node->getAttribute(3) < 0)
        {
            ClientPlaylist *playlist = current_mfd->getAudioPlaylist(
                                                                        node->getAttribute(0),
                                                                        node->getInt()
                                                                     );
            if (playlist)
            {                                                                    
                setDisplayInfo(
                                true, 
                                playlist->getName(),
                                current_mfd->getName(),
                                "Regular", //   Someday, this could be Smart as well
                                QString("%1").arg(playlist->getActualTrackCount())
                              );
            }
            else
            {
                cerr << "playlistdialog.o: Got non-existent playlist back from a playlist" << endl;
                clearDisplayInfo(false);
            }
           
        }
        else if (node->getAttribute(3) > 0)
        {
            AudioMetadata *audio_item = current_mfd->getAudioMetadata(
                                                                        node->getAttribute(0),
                                                                        node->getInt()
                                                                     );
            if (audio_item)
            {                                                                    
                setDisplayInfo(
                                false, 
                                audio_item->getGenre(),
                                audio_item->getArtist(),
                                audio_item->getAlbum(),
                                audio_item->getTitle()
                              );
            }
            else
            {
                cerr << "playlistdialog.o: Got non-existent item back from a playlist" << endl;
                clearDisplayInfo(false);
            }
           
        }  
        else
        {
            cerr << "playlistdialog.o: handlePlaylistEntered got a node with a 0 attribute" << endl;
        }
        
    } 
    else
    {
        cerr << "playlistdialog.o: handlePlaylistEntered got passed a NULL node" << endl;
    }
}

void PlaylistDialog::handlePlaylistSelected(UIListTreeType*, UIListGenericTree *node)
{

    //
    //  If editing is not currently allowed (our data is out of sync with
    //  reality), then user should not be able to  select anything
    //

    if (!editing_allowed)
    {
        return;
    }

    //
    //  Deal with moving tracks up and down within the playlist column
    //
    
    if (holding_track)
    {
        cerr << "playlistdialog.o: should not be possible to select a node " 
             << "while holding track is true "
             << endl;
    }
    else
    {
        startHoldingTrack(node);
    }
}

void PlaylistDialog::startHoldingTrack(UIListGenericTree *node)
{
    if (holding_track)
    {
        cerr << "playlistdialog.o: startHoldingTrack() called, but "
             << "holding_track is already true "
             << endl;
    }

    current_mfd->markNodeAsHeld(node, true);
    holding_track = true;
    held_track = node;
    grabKeyboard();
    playlist_tree_menu->RedrawCurrent();
}

void PlaylistDialog::stopHoldingTrack()
{
    if (!holding_track)
    {
        cerr << "playlistdialog.o: stopHoldingTrack() called, but "
             << "holding_track is already false "
             << endl;
    }

    current_mfd->markNodeAsHeld(held_track, false);
    holding_track = false;
    held_track = NULL;
    releaseKeyboard();
    playlist_tree_menu->RedrawCurrent();
}

void PlaylistDialog::moveHeldUpDown(bool up_or_down)
{
    something_changed_position = true;
    if (held_track)
    {
        held_track->movePositionUpDown(up_or_down);
        playlist_tree_menu->RedrawCurrent();
    }
    else
    {
        cerr << "playlistdialog.o: moveHeldUpDown() called, "
             << "but *held_track is NULL ?!?"
             << endl;
    }
}

void PlaylistDialog::handleContentEntered(UIListTreeType*, UIListGenericTree *node)
{
    if (node)
    {
    
        /*
        
            Debugging
            
        */

        /*    
            cout << "Hit node:" << endl;
            cout << "\t   int is " << node->getInt() << endl;
            cout << "\tstring is \"" << node->getString() << "\"" << endl;
            cout << "\t  att0 is " << node->getAttribute(0) << endl;
            cout << "\t  att1 is " << node->getAttribute(1) << endl;
            cout << "\t  att2 is " << node->getAttribute(2) << endl;
            cout << "\t  att3 is " << node->getAttribute(3) << endl;
        */
    
        if (node->getInt() < 0)
        {
            ClientPlaylist *playlist = current_mfd->getAudioPlaylist(
                                                                        node->getAttribute(0),
                                                                        node->getInt() * -1
                                                                     );
            if (playlist)
            {                                                                    
                setDisplayInfo(
                                true, 
                                playlist->getName(),
                                current_mfd->getName(),
                                "Regular", //   Someday, this could be "Smart" as well
                                QString("%1").arg(playlist->getActualTrackCount())
                              );
            }
            else
            {
                cerr << "playlistdialog.o: Got non-existent playlist back from a content tree" << endl;
                clearDisplayInfo(false);
            }
           
        }
        else if (node->getInt() > 0)
        {
            AudioMetadata *audio_item = current_mfd->getAudioMetadata(
                                                                        node->getAttribute(0),
                                                                        node->getInt()
                                                                     );
            if (audio_item)
            {                                                                    
                setDisplayInfo(
                                false, 
                                audio_item->getGenre(),
                                audio_item->getArtist(),
                                audio_item->getAlbum(),
                                audio_item->getTitle()
                              );
            }
            else
            {
                cerr << "playlistdialog.o: Got non-existent item back from a content list" << endl;
                clearDisplayInfo(false);
            }
           
        }  
        else
        {
            QString genre_string = "";
            QString artist_string = "";
            QString album_string = "";

            fillWhatWeKnow(node, genre_string, artist_string, album_string);

            if (
                genre_string.length() < 1 &&
                artist_string.length() < 1 &&
                album_string.length() < 1)
            {
                clearDisplayInfo(false);
            }
            else
            {
                setDisplayInfo(
                                false, 
                                genre_string,
                                artist_string,
                                album_string,
                                ""
                              );
            }
        }
        
    } 
    else
    {
        cerr << "playlistdialog.o: handleContentEntered got passed a NULL node" << endl;
        clearDisplayInfo(false);
    }
}

void PlaylistDialog::handleContentSelected(UIListTreeType* , UIListGenericTree *node)
{

    //
    //  If editing is not currently allowed (our data is out of sync with
    //  reality), then user should not be able to check anything
    //

    if (!editing_allowed)
    {
        return;
    }

    if (!node)
    {
        cerr << "PlaylistDialog::handleContentSelected() called with a NULL node!" << endl;
    }

    if (!node->getActive())
    {
        return;
    }

    //
    //  User selected this node. First step is to figure out what kind of
    //  node it is.
    //
    
    if (node->getInt() != 0)
    {
    
        //
        //  Are we turning on or off?
        //
        
        int current_state = node->getCheck();
        
        if (current_state == 0)
        {
            toggleItem(node, true);
            updatePlaylistDeltas(true, node->getInt());
        }
        else if (current_state == 2)
        {
            toggleItem(node, false);
            updatePlaylistDeltas(false, node->getInt());
        }
        else if (current_state == 1)
        {
            cerr << "leaf content nodes should not be partially on!" << endl;
        }
        else
        {
            cerr << "content node with a bizarre getCheck() of " << current_state << endl;
        }
        
        content_tree_menu->refresh();
        

        //
        //  If we added something, make sure the playlist display is all the way
        //  at the bottom (so the user can see the new thing go on)
        //
    
        if (current_state == 0)
        {
            playlist_tree_menu->SetTree(current_playlist_tree);
            playlist_tree_menu->MoveDown(UIListTreeType::MoveMax);
        }
        else
        {
            //
            //  We have just deleted (possibly a large number of) item(s).
            //  If there are now less items than there are slots for them on
            //  the display, we need to jigger things around so they're all
            //  visible.
            //

            if (current_playlist_tree->childCount() <= playlist_tree_menu->getNumbItemsVisible())
            {
                playlist_tree_menu->MoveUp(UIListTreeType::MoveMax);
            }
            else
            {
                playlist_tree_menu->RefreshCurrentLevel();
                playlist_tree_menu->Redraw();
            }
        }
    }
    else
    {
        //
        //  It's a higher level thing. We need to turn on (or off)
        //  everything below this node
        //
        
        int current_state = node->getCheck();
        
        if (current_state == 0 || current_state == 1)
        {
            toggleTree(node, true);
        }
        else if (current_state == 2)
        {
            toggleTree(node, false);
        }
        else
        {
            cerr << "intermediate content node with a bizarre getCheck() of " << current_state << endl;
        }

        content_tree_menu->refresh();

        if (current_state == 0 || current_state == 1)
        {
            playlist_tree_menu->SetTree(current_playlist_tree);
            playlist_tree_menu->MoveDown(UIListTreeType::MoveMax);
        }
        else
        {
            //
            //  We have just deleted (possibly a large number of) item(s).
            //  If there are now less items than there are slots for them on
            //  the display, we need to jigger things around so they're all
            //  visible.
            //

            if (current_playlist_tree->childCount() <= playlist_tree_menu->getNumbItemsVisible())
            {
                playlist_tree_menu->MoveUp(UIListTreeType::MoveMax);
            }
            else
            {
                playlist_tree_menu->RefreshCurrentLevel();
                playlist_tree_menu->Redraw();
            }
        }
    }
    
    countAndDisplayPlaylistTracks();
    
}    


void PlaylistDialog::countAndDisplayPlaylistTracks()
{

    //
    //  Ask the client library helper to count the tracks on the playlist we
    //  are editing.
    //
    
    int numb_tracks = current_mfd->countTracks(current_playlist_tree);
    
    if (numb_tracks < 1)
    {
        playlist_subtitle->SetText(QString("Playlist: No tracks"));
        
        //
        //  No tracks, so we need to show the correct empty container
        //
        
        if (current_menu == playlist_tree_menu)
        {
            empty_grey->hide();
            empty_grey_high->show();
        }
        else
        {
            empty_grey->show();
            empty_grey_high->hide();
        }

    }
    else if (numb_tracks == 1)
    {
            empty_grey->hide();
            empty_grey_high->hide();
        playlist_subtitle->SetText(QString("Playlist: 1 track"));
    }
    else
    {
            empty_grey->hide();
            empty_grey_high->hide();
        playlist_subtitle->SetText(QString("Playlist: %1 tracks").arg(numb_tracks));
    }
    
}

void PlaylistDialog::setDisplayInfo(
                                bool is_a_playlist,
                                const QString &string1, 
                                const QString &string2,
                                const QString &string3,
                                const QString &string4
                            )
{
    if (is_a_playlist)
    {
        genre_label->hide();
        artist_label->hide();
        album_label->hide();
        title_label->hide();
        
        mfe_genre_icon->hide();
        mfe_artist_icon->hide();
        mfe_album_icon->hide();
        mfe_title_icon->hide();
                
        playlist_name_label->show();
        playlist_source_label->show();
        playlist_type_label->show();
        playlist_numtracks_label->show();
    }
    else
    {
        genre_label->show();
        artist_label->show();
        album_label->show();
        title_label->show();
        
        mfe_genre_icon->show();
        mfe_artist_icon->show();
        mfe_album_icon->show();
        mfe_title_icon->show();
                
        playlist_name_label->hide();
        playlist_source_label->hide();
        playlist_type_label->hide();
        playlist_numtracks_label->hide();
    }

    genre_text->SetText(string1);
    artist_text->SetText(string2);
    album_text->SetText(string3);
    title_text->SetText(string4);
}                            
                                
void PlaylistDialog::clearDisplayInfo(bool clear_only_right)
{
    genre_text->SetText("");
    artist_text->SetText("");
    album_text->SetText("");
    title_text->SetText("");

    if (clear_only_right)
    {
        mfe_genre_icon->show();
        mfe_artist_icon->show();
        mfe_album_icon->show();
        mfe_title_icon->show();

        genre_label->show();
        artist_label->show();
        album_label->show();
        title_label->show();
                
        playlist_name_label->hide();
        playlist_source_label->hide();
        playlist_type_label->hide();
        playlist_numtracks_label->hide();
    }
    else
    {
        mfe_genre_icon->hide();
        mfe_artist_icon->hide();
        mfe_album_icon->hide();
        mfe_title_icon->hide();

        genre_label->hide();
        artist_label->hide();
        album_label->hide();
        title_label->hide();
                
        playlist_name_label->hide();
        playlist_source_label->hide();
        playlist_type_label->hide();
        playlist_numtracks_label->hide();
    }
}                            
                                

void PlaylistDialog::fillWhatWeKnow(
                                    UIListGenericTree *node, 
                                    QString &genre_string, 
                                    QString &artist_string, 
                                    QString &album_string
                                   )
{
    if (node)
    {
        if (node->getAttribute(3) == 1)
        {
            genre_string = node->getString();
        }
        else if (node->getAttribute(3) == 2)
        {
            artist_string = node->getString();
        }
        else if (node->getAttribute(3) == 3)
        {
            album_string = node->getString();
        }
        GenericTree *parent = node->getParent();
        if (parent)
        {
            if ( UIListGenericTree *parent_node = dynamic_cast<UIListGenericTree*>(parent))
            {
                fillWhatWeKnow(parent_node, genre_string, artist_string, album_string);
            }
        }
    }
}


void PlaylistDialog::toggleItem(UIListGenericTree *node, bool turn_on)
{

    //
    //  Ask the mfd client library to turn this on/off (because it knows about
    //  other nodes in the tree that are actually the same item, and can
    //  turn those on/off as well)
    //

    current_mfd->toggleItem(node, turn_on);

    //
    //  Now ask the mfd client library to add or remove this from the
    //  playlist list
    //

    current_mfd->alterPlaylist(playlist_tree_menu, current_playlist_tree, node, turn_on);
    
}                                   

void PlaylistDialog::toggleTree(UIListGenericTree *node, bool turn_on)
{

    //
    //  Ask the mfd client library to turn this on/off (because it knows about
    //  other nodes in the tree that are actually the same item, and can
    //  turn those on/off as well). It will also update the playlist with added
    //  or deleted entries
    //

    current_mfd->toggleTree(
                            playlist_tree_menu, 
                            current_playlist_tree, 
                            node, 
                            turn_on, 
                            &playlist_additions,
                            &playlist_deletions
                           );
    
}                                   

void PlaylistDialog::updatePlaylistDeltas(bool addition, int item_id)
{
    //
    //  We own the addition and deletions lists, but we ask the mfd client
    //  lib to do the calculating for us
    //

    current_mfd->updatePlaylistDeltas(
                                        &playlist_additions,
                                        &playlist_deletions,
                                        addition,
                                        item_id
                                     );
}


void PlaylistDialog::wireUpTheme()
{


    playlist_tree_menu = getUIListTreeType("playlist_tree");
    if (!playlist_tree_menu)
    {
        cerr << "no playlist_tree in your theme doofus" << endl;
    }
    
    content_tree_menu = getUIListTreeType("content_tree");
    if (!content_tree_menu)
    {
        cerr << "no content_tree in your theme doofus" << endl;
    }
    

    playlist_subtitle = getUITextType("playlist_subtitle");
    playlist_subtitle->SetText("Ha Ha Ha");
    
    empty_grey = getUIImageType("playlist_greyout");
    empty_grey->hide();
    
    empty_grey_high = getUIImageType("playlist_greyout_high");
    empty_grey_high->hide();
    
    genre_text = getUITextType("genre_text");
    artist_text = getUITextType("artist_text");
    album_text = getUITextType("album_text");
    title_text = getUITextType("title_text");
    
    genre_label = getUITextType("genre_label");
    artist_label = getUITextType("artist_label");
    album_label = getUITextType("album_label");
    title_label = getUITextType("title_label");
    edit_title = getUITextType("edit_title");

    mfe_genre_icon = getUIImageType("mfe_genre_icon");
    mfe_artist_icon = getUIImageType("mfe_artist_icon");
    mfe_album_icon = getUIImageType("mfe_album_icon");
    mfe_title_icon = getUIImageType("mfe_title_icon");
    
    playlist_name_label = getUITextType("playlist_name_label");
    playlist_source_label = getUITextType("playlist_source_label");
    playlist_type_label = getUITextType("playlist_type_label");
    playlist_numtracks_label = getUITextType("playlist_numtracks_label");

    leftarrow_off = getUIImageType("mfe_leftarrow_off");
    leftarrow_on = getUIImageType("mfe_leftarrow_on");
    rightarrow_off = getUIImageType("mfe_rightarrow_off");
    rightarrow_on = getUIImageType("mfe_rightarrow_on");


    
    playlist_tree_menu->SetTree(current_playlist_tree);
    playlist_tree_menu->setActive(true);
    current_menu = playlist_tree_menu;
    leftarrow_on->hide();
    leftarrow_off->show();
    rightarrow_on->show();
    rightarrow_off->hide();
    

    network_icon = getUIImageType("net_flasher");
    if (network_icon)
    {
        network_icon->hide();
        net_flasher = new NetFlasher(network_icon);
    }
 
    connect(
            playlist_tree_menu, 
            SIGNAL(itemEntered(UIListTreeType*, UIListGenericTree*)),
            this,
            SLOT(handlePlaylistEntered(UIListTreeType*, UIListGenericTree*))
           );

    connect(
            playlist_tree_menu, 
            SIGNAL(itemSelected(UIListTreeType*, UIListGenericTree*)),
            this,
            SLOT(handlePlaylistSelected(UIListTreeType*, UIListGenericTree*))
           );
    
    content_tree_menu->SetTree(current_content_tree);
    content_tree_menu->setActive(false);

    connect(
            content_tree_menu, 
            SIGNAL(itemEntered(UIListTreeType*, UIListGenericTree*)),
            this,
            SLOT(handleContentEntered(UIListTreeType*, UIListGenericTree*))
           );

    connect(
            content_tree_menu, 
            SIGNAL(itemSelected(UIListTreeType*, UIListGenericTree*)),
            this,
            SLOT(handleContentSelected(UIListTreeType*, UIListGenericTree*))
           );
    
    clearDisplayInfo();
    
    playlist_tree_menu->enter();
}


PlaylistDialog::~PlaylistDialog()
{
    if (net_flasher)
    {
        delete net_flasher;
        net_flasher = NULL;
    }
}
