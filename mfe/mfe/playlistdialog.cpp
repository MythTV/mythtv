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
                                UIListGenericTree *all_content_tree
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
    //  Wire up the theme
    //

    wireUpTheme();

/*
    //
    //  Connect up signals from the mfd client library
    //
    
    current_mfd = NULL;
    available_mfds.setAutoDelete(true);
    connectUpMfd();

    //
    //  Redraw ourselves
    //

    updateForeground();

*/
} 

void PlaylistDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

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
            


/*
        case Key_M:

            if(menu->toggleShow())
            {
                //
                //  true means it will now be drawn ... we should (?) move
                //  to top menu node (maybe turn this off in some kind of
                //  "expert" mode ?)
                //
                
                menu->GoHome();
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
        
            if(menu->isShown())
            {
                menu->toggleShow();
                handled = true;
            }
            break;
*/            
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
                                "Regular", //   Someday, this could be "Smart" as well
                                QString("%1").arg(playlist->getCount())
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

void PlaylistDialog::handlePlaylistSelected(UIListTreeType*, UIListGenericTree*)
{
    cout << "selected playlist something or other" << endl;
}

void PlaylistDialog::handleContentEntered(UIListTreeType*, UIListGenericTree*)
{
    clearDisplayInfo();
}

void PlaylistDialog::handleContentSelected(UIListTreeType*, UIListGenericTree*)
{
    cout << "selected content something or other" << endl;
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
