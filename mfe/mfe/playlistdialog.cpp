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
                                MfdInfo *an_mfd,
                                UIListGenericTree *a_playlist_tree,
                                UIListGenericTree *all_content_tree,
                                const QString& playlist_name
                              )
          :MythThemedDialog(parent, window_name, theme_filename)
{
    
    //
    //  Pointer to the mfd client library
    //
        
    current_mfd = an_mfd;

    //
    //  We need the tree data for the playlist
    //

    playlist_tree = a_playlist_tree;
    content_tree = all_content_tree;

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

} 

void PlaylistDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    if(holding_track)
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
        
            if(current_menu == content_tree_menu)
            {
                if(current_menu->getDepth() > 0)
                {
                    current_menu->MoveLeft();
                }
                else
                {
                    current_menu = playlist_tree_menu;
                    playlist_tree_menu->setActive(true);
                    content_tree_menu->setActive(false);
                    current_menu->enter();
                }
            }
            else
            {
                this->close();
            }
            handled = true;
            break;
            
        case Key_Right:
        
            if(current_menu == playlist_tree_menu)
            {
                current_menu = content_tree_menu;
                playlist_tree_menu->setActive(false);
                content_tree_menu->setActive(true);
                current_menu->enter();
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
    
    if(current_menu == playlist_tree_menu)
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
        if(current_menu->getDepth() < 1)
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

void PlaylistDialog::handlePlaylistEntered(UIListTreeType*, UIListGenericTree *node)
{
    if(current_menu != playlist_tree_menu)
    {
        return;
    }

    if(node)
    {
        if(node->getAttribute(3) < 0)
        {
            ClientPlaylist *playlist = current_mfd->getAudioPlaylist(
                                                                        node->getAttribute(0),
                                                                        node->getInt()
                                                                     );
            if(playlist)
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
        else if(node->getAttribute(3) > 0)
        {
            AudioMetadata *audio_item = current_mfd->getAudioMetadata(
                                                                        node->getAttribute(0),
                                                                        node->getInt()
                                                                     );
            if(audio_item)
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
    //  Deal with moving tracks up and down within the playlist column
    //
    
    if(holding_track)
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
    if(holding_track)
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
    if(held_track)
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
/*

    if(up_or_down)
    {
        cout << "moving it up" << endl;
    }
    else
    {
        cout << "moving it down" << endl;
    }
*/

}

void PlaylistDialog::handleContentEntered(UIListTreeType*, UIListGenericTree *node)
{
    if(node)
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
    
        if(node->getInt() < 0)
        {
            ClientPlaylist *playlist = current_mfd->getAudioPlaylist(
                                                                        node->getAttribute(0),
                                                                        node->getInt() * -1
                                                                     );
            if(playlist)
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
        else if(node->getInt() > 0)
        {
            AudioMetadata *audio_item = current_mfd->getAudioMetadata(
                                                                        node->getAttribute(0),
                                                                        node->getInt()
                                                                     );
            if(audio_item)
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

            if(
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
    if(!node)
    {
        cerr << "PlaylistDialog::handleContentSelected() called with a NULL node!" << endl;
    }

    if(!node->getActive())
    {
        return;
    }

    //
    //  User selected this node. First step is to figure out what kind of
    //  node it is.
    //
    
    if(node->getInt() != 0)
    {
    
        //
        //  Are we turning on or off?
        //
        
        int current_state = node->getCheck();
        
        if(current_state == 0)
        {
            toggleItem(node, true);
        }
        else if(current_state == 2)
        {
            toggleItem(node, false);
        }
        else if(current_state == 1)
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
    
        if(current_state == 0)
        {
            playlist_tree_menu->SetTree(playlist_tree);
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

            if(playlist_tree->childCount() <= playlist_tree_menu->getNumbItemsVisible())
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
        
        if(current_state == 0 || current_state == 1)
        {
            toggleTree(node, true);
        }
        else if(current_state == 2)
        {
            toggleTree(node, false);
        }
        else
        {
            cerr << "intermediate content node with a bizarre getCheck() of " << current_state << endl;
        }

        content_tree_menu->refresh();

        if(current_state == 0 || current_state == 1)
        {
            playlist_tree_menu->SetTree(playlist_tree);
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

            if(playlist_tree->childCount() <= playlist_tree_menu->getNumbItemsVisible())
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
}    


void PlaylistDialog::setDisplayInfo(
                                bool is_a_playlist,
                                const QString &string1, 
                                const QString &string2,
                                const QString &string3,
                                const QString &string4
                            )
{
    if(is_a_playlist)
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

    if(clear_only_right)
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
    if(node)
    {
        if(node->getAttribute(3) == 1)
        {
            genre_string = node->getString();
        }
        else if(node->getAttribute(3) == 2)
        {
            artist_string = node->getString();
        }
        else if(node->getAttribute(3) == 3)
        {
            album_string = node->getString();
        }
        GenericTree *parent = node->getParent();
        if(parent)
        {
            if( UIListGenericTree *parent_node = dynamic_cast<UIListGenericTree*>(parent))
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

    current_mfd->alterPlaylist(playlist_tree_menu, playlist_tree, node, turn_on);
    
}                                   

void PlaylistDialog::toggleTree(UIListGenericTree *node, bool turn_on)
{

    //
    //  Ask the mfd client library to turn this on/off (because it knows about
    //  other nodes in the tree that are actually the same item, and can
    //  turn those on/off as well). It will also update the playlist with added
    //  or deleted entries
    //

    current_mfd->toggleTree(playlist_tree_menu, playlist_tree, node, turn_on);
    
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


    playlist_tree_menu->SetTree(playlist_tree);
    playlist_tree_menu->setActive(true);
    current_menu = playlist_tree_menu;
    leftarrow_on->hide();
    leftarrow_off->show();
    rightarrow_on->show();
    rightarrow_off->hide();
    
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
    
    content_tree_menu->SetTree(content_tree);
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
}
