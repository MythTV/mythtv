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
    //  Build our backstop GenericTree data to show when there is no mfd
    //  available anywhere
    //
    
    no_mfds_tree = new GenericTree("", 0, false);
    no_mfds_tree->addNode("", 0, false);
    music_tree_list->assignTreeData(no_mfds_tree);
        
    
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
        
            music_tree_list->select();
            handled = true;
            break;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void MfeDialog::clearPlayingDisplay()
{
    playlist_text->SetText("");
    container_text->SetText("");
    metadata_text->SetText("");
    audio_text->SetText("");

}

void MfeDialog::handleTreeListSignals(int node_int, QValueVector<int> *attributes)
{
    if(attributes->at(1) == 1)
    {
        //
        //  Item
        //

        mfd_interface->playAudio(
                                    current_mfd->getId(),
                                    attributes->at(0),
                                    attributes->at(1),
                                    node_int
                                );
    }
    else if(attributes->at(1) == 2)
    {
        //
        //  playlist item
        //

        mfd_interface->playAudio(
                                    current_mfd->getId(),
                                    attributes->at(0),
                                    attributes->at(1),
                                    attributes->at(2),
                                    node_int
                                );
    }


}


void MfeDialog::wireUpTheme()
{

    music_tree_list = getUIManagedTreeListType("musictreelist");
    if (!music_tree_list)
    {
        cerr << "mfedialog.o: Couldn't find a music tree list in your theme"
             << endl;
        exit(0);
    }
    music_tree_list->showWholeTree(true);
    connect(music_tree_list, SIGNAL(nodeSelected(int, IntVector*)),
                this, SLOT(handleTreeListSignals(int, IntVector*)));

    //
    //  Basic textual displays
    //
    
    mfd_text = getUITextType("mfd_text");
    playlist_text = getUITextType("playlist_text");
    container_text = getUITextType("container_text");
    metadata_text = getUITextType("metadata_text");
    audio_text = getUITextType("audio_text");
    
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

    connect(mfd_interface, SIGNAL(metadataChanged(int, GenericTree *)),
            this, SLOT(changeMetadata(int, GenericTree *)));

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
            }
            mfd_text->SetText(QString("%1 (%2) [%3]")
                                .arg(current_mfd->getName())
                                .arg(current_mfd->getHost())
                                .arg(available_mfds.count()));
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
                cout << "doin dat" << endl;
                current_mfd = available_mfds[0];
                mfd_text->SetText(QString("%1 (%2) [%3]")
                                  .arg(current_mfd->getName())
                                  .arg(current_mfd->getHost())
                                  .arg(available_mfds.count()));
            }
            else
            {
                current_mfd = NULL;
                mfd_text->SetText("--none--");
                music_tree_list->assignTreeData(no_mfds_tree);
            }
            updateForeground();
        }
    }
}

void MfeDialog::changeMetadata(int which_mfd, GenericTree *new_tree)
{
    if(available_mfds.find(which_mfd))
    {
        available_mfds[which_mfd]->setMetadata(new_tree);
        if(current_mfd->getId() == which_mfd)
        {
            int current_bin = music_tree_list->getActiveBin();
            QStringList route_to_current = music_tree_list->getRouteToCurrent();
            music_tree_list->assignTreeData(new_tree);
            if( route_to_current.count() > 0)
            {
                if(music_tree_list->tryToSetCurrent(route_to_current))
                {
                    music_tree_list->setActiveBin(current_bin);
                }
                else
                {
                    music_tree_list->setCurrentNode(new_tree->findLeaf());
                }
            }
            updateForeground();
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
    clearPlayingDisplay();
    if(current_mfd && available_mfds.count() > 1)
    {
        QStringList route_to_current = music_tree_list->getRouteToCurrent();
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
                GenericTree *switched_metadata = current_mfd->getMetadata();
                if(switched_metadata)
                {
                    music_tree_list->assignTreeData(current_mfd->getMetadata());
                    QStringList route_to_current = current_mfd->getPreviousTreePosition();
                    if( route_to_current.count() > 0)
                    {
                        if(!music_tree_list->tryToSetCurrent(route_to_current))
                        {
                            //music_tree_list->setActiveBin(current_bin);
                        }
                        else
                        {
                            music_tree_list->setCurrentNode(current_mfd->getMetadata()->findLeaf());
                        }
                    }
                }
                else
                {
                    music_tree_list->assignTreeData(no_mfds_tree);
                }
                break;
            }
        }
    }

    mfd_text->SetText(QString("%1 (%2) [%3]")
                      .arg(current_mfd->getName())
                      .arg(current_mfd->getHost())
                      .arg(available_mfds.count()));

    updateForeground();
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
            playlist_text->SetText("");
            container_text->SetText("");
            metadata_text->SetText("");
            audio_text->SetText("");
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
            
            playlist_text->SetText(QString("%1 (index=%2)").arg(playlist).arg(playlist_index));
            container_text->SetText(QString("%1").arg(container_id));
            metadata_text->SetText(QString("%1").arg(metadata_id));
            audio_text->SetText(QString("%1 seconds, %2 ch, %3 bps, %4 freq")
                                .arg(seconds)
                                .arg(channels)
                                .arg(bitrate)
                                .arg(frequency)
                                );
        }
    }
}



MfeDialog::~MfeDialog()
{
    available_mfds.clear();
    if(no_mfds_tree)
    {
        delete no_mfds_tree;
        no_mfds_tree = NULL;
    }
}

