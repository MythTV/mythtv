/*
	mfedialog.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include "mfedialog.h"

MfeDialog::MfeDialog(
                        MythMainWindow *parent, 
                        QString window_name,
                        QString theme_filename,
                        MfdInterface*an_mfd_interface
                    )
          :MythThemedDialog(parent, window_name, theme_filename)
{
    
    //
    //  Pointer to the mfd client library
    //
        
    mfd_interface = an_mfd_interface;


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
    //  Redraw ourselves
    //

    updateForeground();
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
        
            menu->MoveLeft();
            handled = true;
            break;
            
        case Key_Right:
        
            menu->MoveRight();
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
        //
        //  It's a pure metadata item
        //

        mfd_interface->playAudio(
                                    current_mfd->getId(),
                                    node->getAttribute(0),
                                    node->getAttribute(1),
                                    node->getInt()
                                );
        //
        //  See if we can get everything about this puppy
        //
    
        AudioMetadata *current_item = current_mfd->getAudioMetadata( node->getAttribute(0), node->getInt());
        if(current_item)
        {
            cout << "this item exists: " << current_item->getTitle() << endl;
        }
        else
        {
            cerr << "mfedialog.o: weird ... couldn't find an audio item"
                 << endl;
        }

        
    }
    else if(node->getAttribute(1) == 2)
    {
        //
        //  It's an entry in a playlist
        //
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
 
    //
    // Make the first few nodes in the tree that everything else hangs off
    // as children
    //

    menu_root_node = new UIListGenericTree(NULL, "Menu Root Node");

    browse_node =  new UIListGenericTree(menu_root_node, "Browse");
    manage_node =  new UIListGenericTree(menu_root_node, "Manage");
    connect_node = new UIListGenericTree(menu_root_node, "Connect");
    setup_node   = new UIListGenericTree(menu_root_node, "Setup");
    
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

    connect(mfd_interface, SIGNAL(audioPaused(int, bool)),
            this, SLOT(paused(int, bool)));

    connect(mfd_interface, SIGNAL(audioStopped(int)),
            this, SLOT(stopped(int)));
    
    connect(mfd_interface, SIGNAL(audioPlaying(int, int, int, int, int, int, int, int, int)),
            this, SLOT(playing(int, int, int, int, int, int, int, int, int)));

    connect(mfd_interface, SIGNAL(metadataChanged(int, MfdContentCollection *)),
            this, SLOT(changeMetadata(int, MfdContentCollection *)));

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
                current_mfd_text->SetText(name);
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
        }
    }
}

void MfeDialog::changeMetadata(int which_mfd, MfdContentCollection *new_collection)
{
    if(available_mfds.find(which_mfd))
    {
        available_mfds[which_mfd]->setMfdContentCollection(new_collection);

        if(current_mfd->getId() == which_mfd)
        {

            QStringList route_to_current = menu->getRouteToCurrent();
            //menu->GoHome();

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
            menu->tryToSetCurrent(route_to_current);
            menu->refresh();
    
        }
    }
    else
    {
        cerr << "mfedialog.o: seem to be getting metadata for an mfd that "
             << "does not exist"
             << endl;
    }
}

void MfeDialog::cycleMfd()
{
    if(current_mfd && available_mfds.count() > 1)
    {
        QStringList route_to_current = menu->getRouteToCurrent();
        current_mfd->setPreviousTreePosition(route_to_current);

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
                current_mfd = it.current();
                break;
            }
        }
        
        //
        //  Ok, we now have a new mfd set as current. Update the display
        //
        
        current_mfd_text->SetText(current_mfd->getName());
        
        //
        //  
        //
        
        MfdContentCollection *collection = current_mfd->getMfdContentCollection();
        browse_node->deleteAllChildren();
        browse_node->addNode(collection->getAudioArtistTree());
        browse_node->addNode(collection->getAudioGenreTree());
        browse_node->addNode(collection->getAudioPlaylistTree());
        browse_node->addNode(collection->getAudioCollectionTree());
        
        
    }
}

void MfeDialog::stopAudio()
{
    if(current_mfd)
    {
        mfd_interface->stopAudio(current_mfd->getId());
    }
}

void MfeDialog::paused(int which_mfd, bool paused)
{

    if(paused)
    {
        cout << "mfd #" << which_mfd << " is paused" << endl;
    }
    else
    {
        cout << "mfd #" << which_mfd << " is unpaused" << endl;
    }

}

void MfeDialog::stopped(int which_mfd)
{
    if(current_mfd)
    {
        if(which_mfd == current_mfd->getId())
        {
            //
            //  Clear out all the audio-related text entries
            //
            //playlist_text->SetText("");
            //container_text->SetText("");
            //metadata_text->SetText("");
            //audio_text->SetText("");
        }
    }
}

void MfeDialog::playing(
                            int which_mfd, 
                            int playlist,
                            int playlist_index,
                            int container_id,
                            int metadata_id,
                            int seconds,
                            int channels,
                            int bitrate,
                            int frequency
                         )
{
    if(current_mfd)
    {
        if(which_mfd == current_mfd->getId())
        {
            //
            //  Set all the audio related text entries
            //
            
            /*
            playlist_text->SetText(QString("%1 (index=%2)").arg(playlist).arg(playlist_index));
            container_text->SetText(QString("%1").arg(container_id));
            metadata_text->SetText(QString("%1").arg(metadata_id));
            audio_text->SetText(QString("%1 seconds, %2 ch, %3 bps, %4 freq")
                                .arg(seconds)
                                .arg(channels)
                                .arg(bitrate)
                                .arg(frequency)
                                );
            */
        }
    }
}



MfeDialog::~MfeDialog()
{
    available_mfds.clear();
    if(menu_root_node)
    {
        delete menu_root_node;
        menu_root_node = NULL;
    }
}

