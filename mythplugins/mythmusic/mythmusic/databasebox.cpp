#include <iostream>
using namespace std;

#include <qlistview.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qheader.h>
#include <qpixmap.h>
#include <qregexp.h>
#include <qframe.h>
#include <qlayout.h>
#include <qaccel.h>

#include "metadata.h"
#include "databasebox.h"
#include "treecheckitem.h"
#include "cddecoder.h"
#include "playlist.h"

#include <mythtv/mythcontext.h>

DatabaseBox::DatabaseBox(PlaylistsContainer *all_playlists,
                         AllMusic *music_ptr,
                         QWidget *parent, const char *name)
           : MythDialog(parent, name)
{
    the_playlists = all_playlists;
    if(!music_ptr)
    {
        cerr << "databasebox.o: We are not going to get very far with a null pointer to metadata" << endl;
    }
    all_music = music_ptr;


    //  Do we check the CD?
    cd_checking_flag = false;
    cd_checking_flag = gContext->GetNumSetting("AutoLookupCD");

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    listview = new MythListView(this);
    listview->DontFixSpaceBar();
    listview->addColumn("Select music to be played:");
    listview->setSorting(-1);
    listview->setRootIsDecorated(true);
    listview->setAllColumnsShowFocus(true);
    listview->setColumnWidth(0, (int)(730 * wmult));
    listview->setColumnWidthMode(0, QListView::Manual);

    connect(listview, SIGNAL(returnPressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(spacePressed(QListViewItem *)), this,
            SLOT(selected(QListViewItem *)));
    connect(listview, SIGNAL(infoPressed(QListViewItem *)), this,
            SLOT(doMenus(QListViewItem *)));
    connect(listview, SIGNAL(numberPressed(QListViewItem *, int)), this,
            SLOT(alternateDoMenus(QListViewItem *, int)));
    connect(listview, SIGNAL(deletePressed(QListViewItem *)), this,
            SLOT(deleteTrack(QListViewItem *)));

    //  Popup for active playlist
    active_popup = new MythPopupBox(this);
    active_pl_edit = new MythRemoteLineEdit(active_popup);
    active_popup->addWidget(active_pl_edit);
    active_pl_edit->setFocus();
    MythPushButton *active_b = new MythPushButton("Copy To New Playlist", 
                                                  active_popup);
    active_popup->addWidget(active_b);
    connect(active_b, SIGNAL(clicked()), this, SLOT(copyNewPlaylist()));
    MythPushButton *active_clear_b = new MythPushButton("Clear the Active Play "
                                                        "Queue", active_popup);
    active_popup->addWidget(active_clear_b);
    connect(active_clear_b, SIGNAL(clicked()), this, SLOT(clearActive()));
    pop_back_button = new MythPushButton("Save Back to Playlist Tree", 
                                         active_popup);
    active_popup->addWidget(pop_back_button);
    connect(pop_back_button, SIGNAL(clicked()), this, SLOT(popBackPlaylist()));

    QAccel *activeaccel = new QAccel(active_popup);
    activeaccel->connectItem(activeaccel->insertItem(Key_Escape), active_popup,
                             SLOT(hide()));

    //  Popup for all other playlists (up top)
    playlist_popup = new MythPopupBox(this);
    playlist_mac_b = new MythPushButton("Move to Active Play Queue", 
                                        playlist_popup);
    playlist_popup->addWidget(playlist_mac_b);
    connect(playlist_mac_b, SIGNAL(clicked()), this, SLOT(copyToActive()));
    playlist_del_b = new MythPushButton("Delete This Playlist", playlist_popup);
    playlist_popup->addWidget(playlist_del_b);
    connect(playlist_del_b, SIGNAL(clicked()), this, SLOT(deletePlaylist()));
    playlist_rename = new MythRemoteLineEdit(playlist_popup);
    playlist_popup->addWidget(playlist_rename);
    playlist_rename_button = new MythPushButton("Rename This Playlist", 
                                                playlist_popup);
    playlist_popup->addWidget(playlist_rename_button);
    connect(playlist_rename_button, SIGNAL(clicked()), this, 
            SLOT(renamePlaylist()));
    QAccel *plistaccel = new QAccel(playlist_popup);
    plistaccel->connectItem(plistaccel->insertItem(Key_Escape), playlist_popup,
                            SLOT(hide()));


    cditem = NULL;
    holding_track = false;

    //
    //  Make the first few nodes in the
    //  tree that everything else hangs off
    //  as children
    //
    QString templevel, temptitle;
    temptitle  = "Active Play Queue";
    allcurrent = new PlaylistTitle(listview, temptitle);
    the_playlists->setActiveWidget(allcurrent);
    templevel = "genre";
    temptitle = "All My Playlists";
    alllists = new TreeCheckItem(listview, temptitle, templevel, 0);
    if(cd_checking_flag)
    {
        temptitle = all_music->getCDTitle();
        temptitle = "Blechy Blech Blah";
        templevel = "cd";
        cditem = new CDCheckItem(listview, temptitle, templevel, 0);
    }
    templevel = "genre";
    temptitle = "All My Music";
    allmusic = new TreeCheckItem(listview, temptitle, templevel, 0);

    vbox->addWidget(listview, 1);

    listview->setCurrentItem(listview->firstChild());

    if(cd_checking_flag)
    {
        //  Start the CD checking thread, and set up a timer
        //  to make it check occasionally
        //  (thor pretends he understands how threads work)

        cd_reader_thread = new ReadCDThread(the_playlists, all_music);
        cd_reader_thread->start();
    
        cd_watcher = new QTimer();
        connect(cd_watcher, SIGNAL(timeout()), this, SLOT(occasionallyCheckCD()));
        cd_watcher->start(10000); // Every 10 seconds?
        fillCD();
    }
    
    //  If metadata and playlist loading is already done,
    //  then just show it

    if(all_music->doneLoading() &&
       the_playlists->doneLoading() )
    {
        active_playlist = the_playlists->getActive();
        active_playlist->putYourselfOnTheListView(allcurrent);
        all_music->putYourselfOnTheListView(allmusic);
        the_playlists->showRelevantPlaylists(alllists);
        listview->setOpen(allmusic, true);
        checkTree();
    }
    else
    {

        //
        //  Set a timer to keep redoing the fillList
        //  stuff until the metadata and playlist loading
        //  threads are done
        //

        wait_counter = 0;
        fill_list_timer = new QTimer();
        connect(fill_list_timer, SIGNAL(timeout()), this, SLOT(keepFilling()));
        fill_list_timer->start(300);
        //allmusic->setCheckable(false);
        //alllists->setCheckable(false);
    }

}

void DatabaseBox::keepFilling()
{

    if(all_music->doneLoading() &&
       the_playlists->doneLoading())
    {
        allmusic->setText(0, "All My Music");
        all_music->putYourselfOnTheListView(allmusic);
        fill_list_timer->stop();
        active_playlist = the_playlists->getActive();
        active_playlist->putYourselfOnTheListView(allcurrent);
        the_playlists->showRelevantPlaylists(alllists);
        listview->setOpen(allmusic, true);
        listview->ensureItemVisible(listview->currentItem());
        checkTree();
    }
    else
    {
        wait_counter++;
        QString a_string = "All My Music ~ Loading Music Data ";
        int numb_dots = wait_counter % 4;
        for(int i = 0; i < numb_dots; i++)
        {
            a_string += ".";
        }
        allmusic->setText(0, a_string);
    }
}

void DatabaseBox::occasionallyCheckCD()
{
    fillCD();
    if(!cd_reader_thread->running())
    {
        cd_reader_thread->start();
    }
}

void DatabaseBox::copyNewPlaylist()
{
    if(active_pl_edit->text().length() < 1)
    {
        active_popup->hide();
        return;
    }

    if(the_playlists->nameIsUnique(active_pl_edit->text(), 0))
    {
        active_popup->hide();
        the_playlists->copyNewPlaylist(active_pl_edit->text());
        the_playlists->showRelevantPlaylists(alllists);
        checkTree();
    }
    else
    {
        //  Oh to beep
    }
}


void DatabaseBox::renamePlaylist()
{
    
    if(playlist_rename->text().length() < 1)
    {
        playlist_popup->hide();
        return;
    }

    QListViewItem *item = listview->currentItem();
    
    if(TreeCheckItem *rename_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if(rename_item->getID() < 0)
        {
            if(the_playlists->nameIsUnique(playlist_rename->text(), rename_item->getID() * -1))
            {
                playlist_popup->hide();
                the_playlists->renamePlaylist(rename_item->getID() * -1, playlist_rename->text());
                rename_item->setText(0, playlist_rename->text());
            }
            else
            {
                // I'd really like to beep at the user here
            }
        }
        else
        {
            cerr << "databasebox.o: Trying to rename something that doesn't seem to be a playlist" << endl;
        }
    }
}

void DatabaseBox::popBackPlaylist()
{
    active_popup->hide();
    the_playlists->popBackPlaylist();
    the_playlists->showRelevantPlaylists(alllists);
    checkTree();
}

void DatabaseBox::clearActive()
{
    active_popup->hide();
    the_playlists->clearActive();
    the_playlists->showRelevantPlaylists(alllists);
    checkTree();
}

void DatabaseBox::deletePlaylist()
{
    //  Delete currently selected
    
    playlist_popup->hide();
    
    QListViewItem *item = listview->currentItem();
    
    
    if(TreeCheckItem *check_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if(check_item->getID() < 0)
        {
            if(check_item->itemBelow())
            {
                listview->ensureItemVisible(check_item->itemBelow());
                listview->setCurrentItem(check_item->itemBelow());
            }
            else if(check_item->itemAbove())
            {
                listview->ensureItemVisible(check_item->itemAbove());
                listview->setCurrentItem(check_item->itemAbove());
            }
            the_playlists->deletePlaylist(check_item->getID() * -1);
            //the_playlists->showRelevantPlaylists(alllists);
            delete item;
            the_playlists->refreshRelevantPlaylists(alllists);
            return;
        }
    }
     
    cerr << "databasebox.o: Some crazy user managed to get a playlist popup from a non-playlist item" << endl;
}


void DatabaseBox::copyToActive()
{
    playlist_popup->hide();

    QListViewItem *item = listview->currentItem();
    
    if(TreeCheckItem *check_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if(check_item->getID() < 0)
        {
            the_playlists->copyToActive(check_item->getID() * -1);
            the_playlists->showRelevantPlaylists(alllists);
            checkTree();
            listview->setOpen(allcurrent, true);
            listview->setCurrentItem(allcurrent);
            return;
        }
    }
    cerr << "databasebox.o: Some crazy user managed to get a playlist popup from a non-playlist item in another way" << endl;
}


void DatabaseBox::fillCD(void)
{
    
    if(cditem)
    {
        //  Delete anything that might be there  
    
        while (cditem->firstChild())
        {
            delete cditem->firstChild();
        }
    
        //  Put on whatever all_music tells us is
        //  there
    
        cditem->setText(0, all_music->getCDTitle());
        cditem->setOn(false);
        cditem->setCheckable(false);
        qApp->lock();
        all_music->putCDOnTheListView(cditem);
        
        //  reflect selections in cd playlist

        QListViewItemIterator it(listview);
        
        for(it = cditem->firstChild(); it.current(); ++it)
        {
            if(CDCheckItem *track_ptr = dynamic_cast<CDCheckItem*>(it.current()))
            {
                track_ptr->setOn(false);
                if(the_playlists->checkCDTrack(track_ptr->getID()))
                {
                    track_ptr->setOn(true);
                }
            }
        }
        qApp->unlock();
    }
    //
    //  Can't check what ain't there
    //
    
    if(cditem->childCount() > 0)
    {
        cditem->setCheckable(true);
        checkParent(cditem);
    }
}

void DatabaseBox::doMenus(QListViewItem *item)
{
    if(TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        if(item_ptr->getID() < 0)
        {
            doPlaylistPopup(item_ptr);
        }
    }
    else if(PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
    {
        doActivePopup(item_ptr);
    }
}

void DatabaseBox::alternateDoMenus(QListViewItem *item, int keypad_number)
{
    if(TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        if(item_ptr->getID() < 0)
        {
            doPlaylistPopup(item_ptr);
        }
        else if(item->parent())
        {
            int a_number = item->parent()->childCount();
            a_number = (int) (a_number * ( keypad_number / 10.0));
            
            QListViewItem *temp = item->parent()->firstChild();
            for(int i = 0; i < a_number; i++)
            {
                if(temp)
                {
                    temp = temp->nextSibling();
                }
            }
            if(temp)
            {
                listview->ensureItemVisible(temp);
                listview->setCurrentItem(temp);
            }
        }
    }
    else if(PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
    {
        doActivePopup(item_ptr);
    }
}

void DatabaseBox::selected(QListViewItem *item)
{
    //  Try a dynamic cast to see if this is a
    //  TreeCheckItem, PlaylistItem, etc, or something
    //  else (Effectice C++ 2nd Ed on my lap, pg. 181,
    //  which I am partially disregarding due to laziness)
    
    
    if(CDCheckItem *item_ptr = dynamic_cast<CDCheckItem*>(item))
    {
        //  Something to do with a CD
        doSelected(item_ptr, true);
        if (CDCheckItem *item_ptr = dynamic_cast<CDCheckItem*>(item->parent()))
        {
            checkParent(item_ptr);
        }
        
    }
    else if(TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        doSelected(item_ptr, false);
        if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item->parent()))
        {
            checkParent(item_ptr);
        }
    } 
    else if(PlaylistItem *item_ptr = dynamic_cast<PlaylistTrack*>(item))
    {
        dealWithTracks(item_ptr);
    }
    else if(PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
    {
        doActivePopup(item_ptr);
    }
    else
    {
        cerr << "databasebox.o: That's odd ... there's something I don't recognize on a ListView" << endl;
    }
}


void DatabaseBox::doPlaylistPopup(TreeCheckItem *item_ptr)
{
    int x, y;
    QRect   r;
    
    x = item_ptr->width(listview->fontMetrics(), listview, 0) + (int)(65 * wmult);
    r = item_ptr->listView()->itemRect(item_ptr);
    y = r.top() + listview->header()->height() + (int)(24 * hmult);
    
    //
    //  If there isn't enough room to show it, move it up 
    //  the hight of the frame
    //
    
    if (height() - y < playlist_popup->height())
    {
        y = height() - playlist_popup->height() - (int)(8 * hmult);
    } 
    playlist_rename->setText(item_ptr->text(0));

    playlist_popup->move(x,y);
    playlist_popup->show();
    
    playlist_mac_b->setFocus();
}

void DatabaseBox::doActivePopup(PlaylistTitle *item_ptr)
{
    int x, y;
    QRect   r;
    
    x = item_ptr->width(listview->fontMetrics(), listview, 0) + (int)(40 * wmult);
    r = item_ptr->listView()->itemRect(item_ptr);
    y = r.top() +  listview->header()->height() + (int)(24 * hmult);
    
    //
    //  If there isn't enough room to show it, move it up 
    //  the hight of the frame
    //
    
    if (height() - y < active_popup->height())
    {
        y = height() - active_popup->height() - (int)(8 * hmult);
    }  

    active_pl_edit->setText("");
    active_popup->move(x,y);
    active_popup->show();
    active_pl_edit->setFocus();
    if(the_playlists->pendingWriteback())
    {
        pop_back_button->setEnabled(true);
    }
    else
    {
        pop_back_button->setEnabled(false);
    }
}

void DatabaseBox::dealWithTracks(PlaylistItem *item_ptr)
{
    //  Logic to handle the start of
    //  moving/deleting songs from playlist
    
    if(holding_track)
    {
        cerr << "databasebox.o: Oh crap, this is not supposed to happen " << endl ;
        holding_track = false;
        track_held->beMoving(false);
        releaseKeyboard();
    }
    else
    {
        holding_track = true;
        track_held = dynamic_cast<PlaylistTrack*>(item_ptr);
        track_held->beMoving(true);
        grabKeyboard();
    }
}

void DatabaseBox::doSelected(QListViewItem *item, bool cd_flag)
{
    bool keep_going = false;

    TreeCheckItem *tcitem = (TreeCheckItem *)item;
    
    if(tcitem->childCount() > 0)
    {
        keep_going = true;
        if(PlaylistItem *check_item = dynamic_cast<PlaylistItem*>(tcitem->firstChild()))
        {
            (void)check_item;
            keep_going = false;
        }
    }
    
    if(keep_going)
    {
    
        TreeCheckItem *child = (TreeCheckItem *)tcitem->firstChild();
        while (child) 
        {
            if (child->isOn() != tcitem->isOn())
            {
                child->setOn(tcitem->isOn());
                doSelected(child, cd_flag);
            }
            child = (TreeCheckItem *)child->nextSibling();
        }
    }
    else 
    {
        if (tcitem->isOn())
        {
            if(cd_flag)
            {
                the_playlists->addCDTrack(tcitem->getID());
            }
            else
            {
                active_playlist->addTrack(tcitem->getID(), true);
            }
        }
        else
        {
            if(cd_flag)
            {
                the_playlists->removeCDTrack(tcitem->getID());                    
            }
            else
            {
                active_playlist->removeTrack(tcitem->getID());
            }
        }
    }
}

void DatabaseBox::checkParent(QListViewItem *item)
{
    if (!item)
        return;

    bool do_check = false;

    if(TreeCheckItem *tcitem = dynamic_cast<TreeCheckItem*>(item))
    {
        (void)tcitem;
        do_check = true;   
    }
    else if(CDCheckItem *tcitem = dynamic_cast<CDCheckItem*>(item))
    {
        (void)tcitem;
        do_check = true;
    }

    if(do_check);
    {
        TreeCheckItem *tcitem = dynamic_cast<TreeCheckItem*>(item);
        TreeCheckItem *child = (TreeCheckItem *)tcitem->firstChild();
        if (!child)
            return;

        bool state = child->isOn();
        bool same = true;
        while (child)
        {
            if (child->isOn() != state)
                same = false;
            child = (TreeCheckItem *)child->nextSibling();
        }

        if (same)
            tcitem->setOn(state);

        if (!same)
            tcitem->setOn(false);

        if (tcitem->parent())
        {
            checkParent(tcitem->parent());
        }
    }
}    

void DatabaseBox::deleteTrack(QListViewItem *item)
{
    if(PlaylistTrack *delete_item = dynamic_cast<PlaylistTrack*>(item) )
    {
        if(delete_item->itemBelow())
        {
            listview->ensureItemVisible(delete_item->itemBelow());
            listview->setCurrentItem(delete_item->itemBelow());
        }
        else if(delete_item->itemAbove())
        {
            listview->ensureItemVisible(delete_item->itemAbove());
            listview->setCurrentItem(delete_item->itemAbove());
        }
   
    
        QListViewItem *item = delete_item->parent();
    
        if(TreeCheckItem *item_owner = dynamic_cast<TreeCheckItem*>(item))
        {
            Playlist *owner = the_playlists->getPlaylist(item_owner->getID() * -1);
            owner->removeTrack(delete_item->getID());
        }
        else if(PlaylistTitle *item_owner = dynamic_cast<PlaylistTitle*>(item))
        {
            (void)item_owner;
            active_playlist->removeTrack(delete_item->getID());
        }
        else
        {
            cerr << "databasebox.o: I don't know how to delete whatever you're trying to get rid of" << endl;
        }
        the_playlists->refreshRelevantPlaylists(alllists);
        checkTree();
    }
}

void DatabaseBox::moveHeldUpDown(bool flag)
{
    track_held->moveUpDown(flag);  
    listview->ensureItemVisible(track_held);
    listview->setCurrentItem(track_held);
}

void DatabaseBox::keyPressEvent(QKeyEvent *e)
{
    //  NB: BAD
    //  this is the wrong way to be doing this
    //  fix soon

    if(holding_track)
    {
        if( e->key() == Key_Space || 
            e->key() == Key_Enter ||
            e->key() == Key_Escape)
        {
            //  Done holding this track
            holding_track = false;
            track_held->beMoving(false);
            releaseKeyboard();
        }
        else if(e->key() == Key_Up)
        {
            //  move track up
            moveHeldUpDown(true);
        }
        else if(e->key() == Key_Down)
        {
            //  move track down
            moveHeldUpDown(false);
        }
        /*
        else if(e->key() == Key_D && holding_track == true)
        {
            //  Delete this track
            deleteHeld();
        }
        */
    }
    else
    {
        MythDialog::keyPressEvent(e);
    }
}

void DatabaseBox::checkTree()
{
    QListViewItemIterator it(listview);

    //  Using the current playlist metadata, 
    //  check the boxes on the ListView tree appropriately.


    for (it = allmusic->firstChild(); it.current(); ++it )
    {
        //  Only check things which are TreeCheckItem's
        if(TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(it.current()))
        {
            //  Turn off
            item_ptr->setOn(false);
            if(active_playlist->checkTrack(item_ptr->getID()))
            {
                //  Turn on if it's on the current playlist
                item_ptr->setOn(true);
                checkParent(item_ptr->parent());
            }
        }
    }
}

void DatabaseBox::setCDTitle(const QString& title)
{
    if(cditem)
    {
        cditem->setText(0, title);
    }
}

ReadCDThread::ReadCDThread(PlaylistsContainer *all_the_playlists, AllMusic *all_the_music)
{
    the_playlists = all_the_playlists;
    all_music = all_the_music;
}

void ReadCDThread::run()
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    int tracknum = decoder->getNumTracks();

    bool setTitle = false;
    bool redo = false;

    if(tracknum == 0)
    {
        //  No CD, or no recognizable CD
        all_music->clearCDData();
        the_playlists->clearCDList();
    }

    if(tracknum > 0)
    {
        //  Check the last track to see if it's different
        //  than whatever it was before
        Metadata *checker = decoder->getMetadata(tracknum);
        if(!all_music->checkCDTrack(checker))
        {
            redo = true;
            all_music->clearCDData();
            the_playlists->clearCDList();
        }
    } 

    while (tracknum > 0 && redo) 
    {
        Metadata *track = decoder->getMetadata(tracknum);
        all_music->addCDTrack(track);

        if (!setTitle)
        {
            QString parenttitle = " " + track->Artist() + " ~ " + 
                                  track->Album();
            all_music->setCDTitle(parenttitle);
            setTitle = true;
        }

        QString title = QString(" %1").arg(tracknum);
        title += " - " + track->Title();

        QString level = "title";
        
        tracknum--;
    }
}
