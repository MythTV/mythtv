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
#include <qevent.h>

#include "metadata.h"
#include "databasebox.h"
#include "treecheckitem.h"
#include "cddecoder.h"
#include "playlist.h"

#include <mythtv/mythcontext.h>
#include <mythtv/lcddevice.h>

#define LCD_MAX_MENU_ITEMS 5

DatabaseBox::DatabaseBox(PlaylistsContainer *all_playlists,
                         AllMusic *music_ptr,
                         MythMainWindow *parent, const char *name)
           : MythDialog(parent, name)
{
    the_playlists = all_playlists;
    active_playlist = NULL;

    if (!music_ptr)
    {
        cerr << "databasebox.o: We are not going to get very far with a null "
                "pointer to metadata" << endl;
    }
    all_music = music_ptr;

    //  Do we check the CD?
    cd_checking_flag = false;
    cd_checking_flag = gContext->GetNumSetting("AutoLookupCD");

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    listview = new MythListView(this);
    listview->DontFixSpaceBar();
    listview->addColumn(tr("Select music to be played:"));
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
    connect(listview, SIGNAL(unhandledKeyPress(QKeyEvent *)), this,
            SLOT(updateLCDMenu(QKeyEvent *)));

    active_popup = NULL;
    active_pl_edit = NULL;
 
    playlist_popup = NULL;

    cditem = NULL;
    holding_track = false;

    // Make the first few nodes in the tree that everything else hangs off
    // as children

    QString templevel, temptitle;
    temptitle  = tr("Active Play Queue");
    allcurrent = new PlaylistTitle(listview, temptitle);
    templevel = "genre";
    temptitle = tr("All My Playlists");
    alllists = new TreeCheckItem(listview, temptitle, templevel, 0);
    if(cd_checking_flag)
    {
        temptitle = all_music->getCDTitle();
        temptitle = tr("Blechy Blech Blah");
        templevel = "cd";
        cditem = new CDCheckItem(listview, temptitle, templevel, 0);
    }
    templevel = "genre";
    temptitle = tr("All My Music");
    allmusic = new TreeCheckItem(listview, temptitle, templevel, 0);

    vbox->addWidget(listview, 1);

    listview->setCurrentItem(listview->firstChild());

    cd_reader_thread = NULL;
    if(cd_checking_flag)
    {
        // Start the CD checking thread, and set up a timer to make it check 
        // occasionally

        cd_reader_thread = new ReadCDThread(the_playlists, all_music);
        cd_reader_thread->start();
    
        cd_watcher = new QTimer(this);
        connect(cd_watcher, SIGNAL(timeout()), this, SLOT(occasionallyCheckCD()));
        cd_watcher->start(1000); // Every second?
        fillCD();
    }
    
    // Set a timer to keep redoing the fillList stuff until the metadata and 
    // playlist loading threads are done

    wait_counter = 0;
    numb_wait_dots = 0;
    fill_list_timer = new QTimer(this);
    connect(fill_list_timer, SIGNAL(timeout()), this, SLOT(keepFilling()));
    fill_list_timer->start(20);

    listview->setFocus();
}

DatabaseBox::~DatabaseBox()
{
    if (cd_reader_thread)
    {
        cd_watcher->stop();

        cd_reader_thread->wait();
        delete cd_reader_thread;
    }
    all_music->resetListings();

    gContext->GetLCDDevice()->switchToTime();
}

void DatabaseBox::showWaiting()
{
    wait_counter++;
    if (wait_counter > 10)
    {
        wait_counter = 0;
        ++numb_wait_dots; 
        if (numb_wait_dots > 3)
            numb_wait_dots = 1;

        QString a_string = tr("All My Music ~ Loading Music Data ");

        // Set Loading Message on the LCD
        QPtrList<LCDTextItem> textItems;
        textItems.setAutoDelete(true);

        textItems.append(new LCDTextItem(1, ALIGN_CENTERED, 
                         tr("Loading Music Data"), "Generic", false));
        gContext->GetLCDDevice()->switchToGeneric(&textItems);

        for (int i = 0; i < numb_wait_dots; i++)
            a_string += ".";

        allmusic->setText(0, a_string);
    }
}

void DatabaseBox::keepFilling()
{
    if (all_music->doneLoading() &&
        the_playlists->doneLoading())
    {
        //  Good, now lets grab some QListItems
        //
        //  Say ... I dunno ... 100 at a time?

        if (all_music->putYourselfOnTheListView(allmusic, 100))
        {
            allmusic->setText(0, tr("All My Music"));
            fill_list_timer->stop();
            the_playlists->setActiveWidget(allcurrent);
            active_playlist = the_playlists->getActive();
            active_playlist->putYourselfOnTheListView(allcurrent);
            the_playlists->showRelevantPlaylists(alllists);
            listview->setOpen(allmusic, true);
            listview->ensureItemVisible(listview->currentItem());
            checkTree();

            //Display the menu when this opens up
            QKeyEvent *fakeKey = new QKeyEvent(QEvent::None, Qt::Key_sterling,
                                               0, Qt::NoButton);
            updateLCDMenu(fakeKey);
            delete fakeKey;
        }
        else
            showWaiting();
    }
    else
        showWaiting(); 
}

void DatabaseBox::occasionallyCheckCD()
{
    if (cd_reader_thread->statusChanged())
    {
        if (active_playlist)
        {
            active_playlist->ripOutAllCDTracksNow();
            fillCD();
        }
    }
    if (!cd_reader_thread->running())
        cd_reader_thread->start();
}

void DatabaseBox::copyNewPlaylist()
{
    if (!active_popup)
        return;

    if (active_pl_edit->text().length() < 1)
    {
        closeActivePopup();
        return;
    }

    if (the_playlists->nameIsUnique(active_pl_edit->text(), 0))
    {
        the_playlists->copyNewPlaylist(active_pl_edit->text());
        the_playlists->showRelevantPlaylists(alllists);
        checkTree();
        closeActivePopup();
    }
    else
    {
        //  Oh to beep
    }
}


void DatabaseBox::renamePlaylist()
{
    if (!playlist_popup)
        return;
 
    if (playlist_rename->text().length() < 1)
    {
        closePlaylistPopup();
        return;
    }

    QListViewItem *item = listview->currentItem();
    
    if (TreeCheckItem *rename_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if (rename_item->getID() < 0)
        {
            if (the_playlists->nameIsUnique(playlist_rename->text(), 
                                            rename_item->getID() * -1))
            {
                the_playlists->renamePlaylist(rename_item->getID() * -1, 
                                              playlist_rename->text());
                rename_item->setText(0, playlist_rename->text());
                closePlaylistPopup();
            }
            else
            {
                // I'd really like to beep at the user here
            }
        }
        else
        {
            cerr << "databasebox.o: Trying to rename something that doesn't "
                    "seem to be a playlist" << endl;
        }
    }
}

void DatabaseBox::popBackPlaylist()
{
    if (!active_popup)
        return;

    the_playlists->popBackPlaylist();
    the_playlists->showRelevantPlaylists(alllists);
    checkTree();

    closeActivePopup();
}

void DatabaseBox::clearActive()
{
    if (!active_popup)
        return;

    closeActivePopup();

    the_playlists->clearActive();
    the_playlists->showRelevantPlaylists(alllists);
    checkTree();
}

void DatabaseBox::deletePlaylist()
{
    if (!playlist_popup)
        return;

    //  Delete currently selected
   
    closePlaylistPopup(); 

    QListViewItem *item = listview->currentItem(); 
    
    if (TreeCheckItem *check_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if (check_item->getID() < 0)
        {
            if (check_item->itemBelow())
            {
                listview->ensureItemVisible(check_item->itemBelow());
                listview->setCurrentItem(check_item->itemBelow());
            }
            else if (check_item->itemAbove())
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
     
    cerr << "databasebox.o: Some crazy user managed to get a playlist popup "
            "from a non-playlist item" << endl;
}


void DatabaseBox::copyToActive()
{
    if (!playlist_popup)
        return;

    closePlaylistPopup();

    QListViewItem *item = listview->currentItem();
    
    if (TreeCheckItem *check_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if (check_item->getID() < 0)
        {
            the_playlists->copyToActive(check_item->getID() * -1);
            the_playlists->showRelevantPlaylists(alllists);
            checkTree();
            listview->setOpen(allcurrent, true);
            listview->setCurrentItem(allcurrent);
            return;
        }
    }
    cerr << "databasebox.o: Some crazy user managed to get a playlist popup "
            "from a non-playlist item in another way" << endl;
}


void DatabaseBox::fillCD(void)
{
    if (cditem)
    {
        // Delete anything that might be there  
    
        while (cditem->firstChild())
        {
            delete cditem->firstChild();
        }
    
        // Put on whatever all_music tells us is there
    
        cditem->setText(0, all_music->getCDTitle());
        cditem->setOn(false);
        cditem->setCheckable(false);
        qApp->lock();
        all_music->putCDOnTheListView(cditem);
        
        //  reflect selections in cd playlist

        QListViewItemIterator it(listview);
        
        for (it = cditem->firstChild(); it.current(); ++it)
        {
            if (CDCheckItem *track_ptr = dynamic_cast<CDCheckItem*>(it.current()))
            {
                track_ptr->setOn(false);
                if (the_playlists->checkCDTrack(track_ptr->getID()))
                    track_ptr->setOn(true);
            }
        }
        qApp->unlock();
    }
    // Can't check what ain't there
    
    if (cditem->childCount() > 0)
    {
        cditem->setCheckable(true);
        checkParent(cditem);
    }
}

void DatabaseBox::doMenus(QListViewItem *item)
{
    if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        if (item_ptr->getID() < 0)
            doPlaylistPopup(item_ptr);
    }
    else if (PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
        doActivePopup(item_ptr);
}

void DatabaseBox::alternateDoMenus(QListViewItem *item, int keypad_number)
{
    if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        if (item_ptr->getID() < 0)
            doPlaylistPopup(item_ptr);
        else if (item->parent())
        {
            int a_number = item->parent()->childCount();
            a_number = (int) (a_number * ( keypad_number / 10.0));
            
            QListViewItem *temp = item->parent()->firstChild();
            for (int i = 0; i < a_number; i++)
            {
                if(temp)
                    temp = temp->nextSibling();
            }
            if (temp)
            {
                listview->ensureItemVisible(temp);
                listview->setCurrentItem(temp);
            }
        }
    }
    else if (PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
        doActivePopup(item_ptr);
}

void DatabaseBox::selected(QListViewItem *item)
{
    // Try a dynamic cast to see if this is a TreeCheckItem, PlaylistItem, etc,
    // or something else (Effectice C++ 2nd Ed on my lap, pg. 181, which I am 
    // partially disregarding due to laziness)
    
    if (CDCheckItem *item_ptr = dynamic_cast<CDCheckItem*>(item))
    {
        // Something to do with a CD
        if (active_playlist)
        {
            item_ptr->setOn(!item_ptr->isOn());
            doSelected(item_ptr, true);
            if (CDCheckItem *item_ptr = dynamic_cast<CDCheckItem*>(item->parent()))
                checkParent(item_ptr);
        }
        
    }
    else if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        if (active_playlist)
        {
            item_ptr->setOn(!item_ptr->isOn());
            doSelected(item_ptr, false);
            if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item->parent()))
                checkParent(item_ptr);
        }
    } 
    else if (PlaylistItem *item_ptr = dynamic_cast<PlaylistTrack*>(item))
        dealWithTracks(item_ptr);
    else if (PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
        doActivePopup(item_ptr);
    else
    {
        cerr << "databasebox.o: That's odd ... there's something I don't "
                "recognize on a ListView" << endl;
    }

    // Update the menu
    // Fake a key event, Key_sterling is a dummy
    QKeyEvent *fakeKey = new QKeyEvent(QEvent::None, Qt::Key_sterling, 0, 
                                       Qt::NoButton);
    updateLCDMenu(fakeKey);
    delete fakeKey;
}


void DatabaseBox::doPlaylistPopup(TreeCheckItem *item_ptr)
{
    if (playlist_popup)
        return;

    MythPushButton *playlist_mac_b;
    MythPushButton *playlist_del_b;
    MythPushButton *playlist_rename_button;

    // Popup for all other playlists (up top)
    playlist_popup = new MythPopupBox(gContext->GetMainWindow(), 
                                      "playlist_popup");

    playlist_mac_b = new MythPushButton(tr("Move to Active Play Queue"),
                                        playlist_popup);
    playlist_popup->addWidget(playlist_mac_b);
    connect(playlist_mac_b, SIGNAL(clicked()), this, SLOT(copyToActive()));

    playlist_del_b = new MythPushButton(tr("Delete This Playlist"),
                                        playlist_popup);
    playlist_popup->addWidget(playlist_del_b);
    connect(playlist_del_b, SIGNAL(clicked()), this, SLOT(deletePlaylist()));

    playlist_rename = new MythRemoteLineEdit(playlist_popup);
    playlist_popup->addWidget(playlist_rename);

    playlist_rename_button = new MythPushButton(tr("Rename This Playlist"),
                                                playlist_popup);
    playlist_popup->addWidget(playlist_rename_button);
    connect(playlist_rename_button, SIGNAL(clicked()), this,
            SLOT(renamePlaylist()));

    QAccel *plistaccel = new QAccel(playlist_popup);
    plistaccel->connectItem(plistaccel->insertItem(Key_Escape), this,
                            SLOT(closePlaylistPopup()));

    playlist_mac_b->adjustSize();
    playlist_del_b->adjustSize();
    playlist_rename->adjustSize();
    playlist_rename_button->adjustSize();

    playlist_popup->polish();

    int x, y, maxw, poph;
    QRect r;

    poph = playlist_mac_b->height() + playlist_del_b->height() +
           playlist_rename->height() + playlist_rename_button->height() +
           (int)(60 * hmult);

    playlist_popup->setMaximumHeight(poph);

    maxw = 0;

    if (playlist_mac_b->width() > maxw)
        maxw = playlist_mac_b->width();
    if (playlist_del_b->width() > maxw)
        maxw = playlist_del_b->width();
    if (playlist_rename_button->width() > maxw)
        maxw = playlist_rename_button->width();

    maxw += (int)(80 * wmult);

    x = item_ptr->width(listview->fontMetrics(), listview, 0) +
        (int)(40 * wmult);
    r = item_ptr->listView()->itemRect(item_ptr);
    y = r.top() + listview->header()->height() + (int)(24 * hmult);

    // If there isn't enough room to show it, move it up the hight of the frame

    if (poph + y > height())
        y = height() - poph - (int)(8 * hmult);

    playlist_popup->setFixedSize(maxw, poph);
    playlist_popup->setGeometry(x, y, maxw, poph);
    playlist_popup->Show();

    playlist_rename->setText(item_ptr->text(0));
    playlist_mac_b->setFocus();

    listview->setFocusPolicy(NoFocus);
}

void DatabaseBox::closePlaylistPopup(void)
{
    if (!playlist_popup)
        return;

    playlist_popup->hide();
    delete playlist_popup;
    playlist_popup = NULL;

    listview->setFocusPolicy(TabFocus);
    listview->setFocus();
}

void DatabaseBox::doActivePopup(PlaylistTitle *item_ptr)
{
    if (active_popup)
        return;

    //  Popup for active playlist
    active_popup = new MythPopupBox(gContext->GetMainWindow(),
                                    "active_popup");
    active_pl_edit = new MythRemoteLineEdit(active_popup);
    active_popup->addWidget(active_pl_edit);
    active_pl_edit->setFocus();
    MythPushButton *active_b = new MythPushButton(tr("Copy To New Playlist"),
                                                  active_popup);
    active_popup->addWidget(active_b);
    connect(active_b, SIGNAL(clicked()), this, SLOT(copyNewPlaylist()));
    MythPushButton *active_clear_b = new MythPushButton(tr("Clear the Active "
                                                           "Play Queue"),
                                                        active_popup);
    active_popup->addWidget(active_clear_b);
    connect(active_clear_b, SIGNAL(clicked()), this, SLOT(clearActive()));
    MythPushButton *pop_back_button;
    pop_back_button = new MythPushButton(tr("Save Back to Playlist Tree"),
                                         active_popup);
    active_popup->addWidget(pop_back_button);
    connect(pop_back_button, SIGNAL(clicked()), this, SLOT(popBackPlaylist()));

    QAccel *activeaccel = new QAccel(active_popup);
    activeaccel->connectItem(activeaccel->insertItem(Key_Escape), this,
                             SLOT(closeActivePopup()));

    active_pl_edit->adjustSize();
    active_b->adjustSize();
    active_clear_b->adjustSize();
    pop_back_button->adjustSize();
    
    active_popup->polish();

    int x, y, maxw, poph;
    QRect r;

    poph = active_pl_edit->height() + active_b->height() + 
           active_clear_b->height() + pop_back_button->height() + 
           (int)(60 * hmult);
 
    active_popup->setMaximumHeight(poph);

    maxw = 0;
 
    if (active_b->width() > maxw)
        maxw = active_b->width();
    if (active_clear_b->width() > maxw)
        maxw = active_clear_b->width();
    if (pop_back_button->width() > maxw)
        maxw = pop_back_button->width();

    maxw += (int)(80 * wmult);

    x = item_ptr->width(listview->fontMetrics(), listview, 0) + 
        (int)(40 * wmult);
    r = item_ptr->listView()->itemRect(item_ptr);
    y = r.top() + listview->header()->height() + (int)(24 * hmult);
    
    // If there isn't enough room to show it, move it up the hight of the frame
    
    if (poph + y > height())
        y = height() - poph - (int)(8 * hmult);

    active_pl_edit->setText("");

    active_popup->setFixedSize(maxw, poph);
    active_popup->setGeometry(x, y, maxw, poph);
    active_popup->Show();

    if(the_playlists->pendingWriteback())
        pop_back_button->setEnabled(true);
    else
        pop_back_button->setEnabled(false);

    listview->setFocusPolicy(NoFocus);
}

void DatabaseBox::closeActivePopup(void)
{
    if (!active_popup)
        return;

    active_popup->hide();
    delete active_popup;
    active_popup = NULL;

    listview->setFocusPolicy(TabFocus);
    listview->setFocus();
}

void DatabaseBox::dealWithTracks(PlaylistItem *item_ptr)
{
    // Logic to handle the start of moving/deleting songs from playlist
    
    if (holding_track)
    {
        cerr << "databasebox.o: Oh crap, this is not supposed to happen " 
             << endl;
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

    if (tcitem->childCount() > 0)
    {
        keep_going = true;
        if (PlaylistItem *check_item = dynamic_cast<PlaylistItem*>(tcitem->firstChild()))
        {
            (void)check_item;
            keep_going = false;
        }
    }
    
    if (keep_going)
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
            active_playlist->addTrack(tcitem->getID(), true, cd_flag);
        else
            active_playlist->removeTrack(tcitem->getID(), cd_flag);
    }
}

void DatabaseBox::checkParent(QListViewItem *item)
{
    if (!item)
        return;

    bool do_check = false;

    if (TreeCheckItem *tcitem = dynamic_cast<TreeCheckItem*>(item))
    {
        (void)tcitem;
        do_check = true;   
    }
    else if (CDCheckItem *tcitem = dynamic_cast<CDCheckItem*>(item))
    {
        (void)tcitem;
        do_check = true;
    }

    if (do_check); // XXX This can't be right...
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
            checkParent(tcitem->parent());
    }
}    

void DatabaseBox::deleteTrack(QListViewItem *item)
{
    if (PlaylistTrack *delete_item = dynamic_cast<PlaylistCD*>(item) )
    {
        if (delete_item->itemBelow())
        {
            listview->ensureItemVisible(delete_item->itemBelow());
            listview->setCurrentItem(delete_item->itemBelow());
        }
        else if (delete_item->itemAbove())
        {
            listview->ensureItemVisible(delete_item->itemAbove());
            listview->setCurrentItem(delete_item->itemAbove());
        }

        QListViewItem *item = delete_item->parent();
        if (TreeCheckItem *item_owner = dynamic_cast<TreeCheckItem*>(item))
        {
            Playlist *owner = the_playlists->getPlaylist(item_owner->getID() * 
                                                         -1);
            owner->removeTrack(delete_item->getID(), true);
        }
        else if (PlaylistTitle *item_owner = dynamic_cast<PlaylistTitle*>(item))
        {
            (void)item_owner;
            active_playlist->removeTrack(delete_item->getID(), true);
        }
        else
        {
            cerr << "databasebox.o: I don't know how to delete whatever you're "
                    "trying to get rid of" << endl;
        }
        the_playlists->refreshRelevantPlaylists(alllists);
        checkTree();
    }
    else if (PlaylistTrack *delete_item = dynamic_cast<PlaylistTrack*>(item))
    {
        if (delete_item->itemBelow())
        {
            listview->ensureItemVisible(delete_item->itemBelow());
            listview->setCurrentItem(delete_item->itemBelow());
        }
        else if (delete_item->itemAbove())
        {
            listview->ensureItemVisible(delete_item->itemAbove());
            listview->setCurrentItem(delete_item->itemAbove());
        }

        QListViewItem *item = delete_item->parent();
        if (TreeCheckItem *item_owner = dynamic_cast<TreeCheckItem*>(item))
        {
            Playlist *owner = the_playlists->getPlaylist(item_owner->getID() * 
                                                         -1);
            owner->removeTrack(delete_item->getID(), false);
        }
        else if (PlaylistTitle *item_owner = dynamic_cast<PlaylistTitle*>(item))
        {
            (void)item_owner;
            active_playlist->removeTrack(delete_item->getID(), false);
        }
        else
        {
            cerr << "databasebox.o: I don't know how to delete whatever you're "
                    "trying to get rid of" << endl;
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
    // This is a bit wonky, but it works (more or less)

    if(holding_track)
    {
        if (e->key() == Key_Space || e->key() == Key_Enter ||
            e->key() == Key_Return || e->key() == Key_Escape)
        {
            //  Done holding this track
            holding_track = false;
            track_held->beMoving(false);
            releaseKeyboard();
        }
        else if (e->key() == Key_Up)
        {
            //  move track up
            moveHeldUpDown(true);
        }
        else if (e->key() == Key_Down)
        {
            //  move track down
            moveHeldUpDown(false);
        }
    }
    else
    {
        MythDialog::keyPressEvent(e);
    }
}

void DatabaseBox::checkTree()
{
    QListViewItemIterator it(listview);

    // Using the current playlist metadata, check the boxes on the ListView 
    // tree appropriately.

    for (it = allmusic->firstChild(); it.current(); ++it )
    {
        //  Only check things which are TreeCheckItem's
        if (TreeCheckItem *item = dynamic_cast<TreeCheckItem*>(it.current()))
        {
            //  Turn off
            item->setOn(false);
            if (active_playlist->checkTrack(item->getID()))
            {
                //  Turn on if it's on the current playlist
                item->setOn(true);
                checkParent(item->parent());
            }
        }
    }
}

void DatabaseBox::setCDTitle(const QString& title)
{
    if (cditem)
        cditem->setText(0, title);
}

ReadCDThread::ReadCDThread(PlaylistsContainer *all_the_playlists, 
                           AllMusic *all_the_music)
{
    the_playlists = all_the_playlists;
    all_music = all_the_music;
    cd_status_changed = false;
}

void ReadCDThread::run()
{
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    int tracknum = decoder->getNumCDAudioTracks();

    bool setTitle = false;
    bool redo = false;

    if (tracknum != all_music->getCDTrackCount())
    {
        cd_status_changed = true;
        cout << "set cd_status_changed to true " << endl ;
    }
    else
        cd_status_changed = false;

    if (tracknum == 0)
    {
        //  No CD, or no recognizable CD
        all_music->clearCDData();
        the_playlists->clearCDList();
    }

    if (tracknum > 0)
    {
        // Check the last track to see if it's differen than whatever it was 
        // before
        Metadata *checker = decoder->getLastMetadata();
        if (checker)
        {
            if (!all_music->checkCDTrack(checker))
            {
                redo = true;
                cd_status_changed = true;
                all_music->clearCDData();
                the_playlists->clearCDList();
            }
            else
                cd_status_changed = false;
            delete checker;
        }
        else
        {
            cerr << "databasebox.o: The cddecoder said it had audio tracks, "
                    "but it won't tell me about them" << endl;
        }
    } 

    int actual_tracknum = decoder->getNumTracks();

    while (actual_tracknum > 0 && redo) 
    {
        Metadata *track = decoder->getMetadata(actual_tracknum);
        if (track)
        {
            all_music->addCDTrack(track);

            if (!setTitle)
            {
            
                QString parenttitle = " ";
                if (track->Artist().length() > 0)
                {
                    parenttitle += track->Artist();
                    parenttitle += " ~ "; 
                }

                if (track->Album().length() > 0)
                    parenttitle += track->Album();
                else
                {
                    parenttitle = " " + QObject::tr("Unknown");
                    cerr << "databasebox.o: Couldn't find your CD. It may not "
                            "be in the freedb database." << endl;
                    cerr << "               More likely, however, is that you "
                            "need to delete ~/.cddb and" << endl;
                    cerr << "               ~/.cdserverrc and restart "
                            "mythmusic. Have a nice day." << endl;
                }
                all_music->setCDTitle(parenttitle);
                setTitle = true;
            }
            delete track;
        }
        actual_tracknum--;
    }

    delete decoder;
}

void DatabaseBox::updateLCDMenu(QKeyEvent * e)
{
    // Update the LCD with a menu of items
    QListViewItem *curItem = listview->currentItem();

    // Add the current item, and a few items below
    if(!curItem)
        return;

    // Container
    QPtrList<LCDMenuItem> *menuItems = new QPtrList<LCDMenuItem>;

    // Let the pointer object take care of deleting things for us
    menuItems->setAutoDelete(true);

    if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(curItem))
        buildMenuTree(menuItems, item_ptr, 1);
    else if (QListViewItem *item_ptr = dynamic_cast<QListViewItem*>(curItem))
        buildMenuTree(menuItems, item_ptr, 1);

    if (!menuItems->isEmpty())
        gContext->GetLCDDevice()->switchToMenu(menuItems, "MythMusic", false);

    //release the container
    delete menuItems;

    //listview

    //Were done, so switch back to the time display
    if (e->key() == Key_Escape)
        gContext->GetLCDDevice()->switchToTime();
}

void DatabaseBox::buildMenuTree(QPtrList<LCDMenuItem> *menuItems, 
                                TreeCheckItem *item_ptr, int level)
{
    if(!item_ptr || level > LCD_MAX_MENU_ITEMS)
        return;

    //If this is the first time in, try to add a few previous items
    if (level == 1 && item_ptr->itemAbove())
    {
        TreeCheckItem *tcitem;
        QListViewItem *qlvitem;

        QListViewItem *testitem = item_ptr->itemAbove()->itemAbove();
        if (testitem)
        {
            if ((tcitem = dynamic_cast<TreeCheckItem*>(testitem)))
                menuItems->append(buildLCDMenuItem(tcitem, false));
            else if ((qlvitem = dynamic_cast<QListViewItem*>(testitem)))
                menuItems->append(buildLCDMenuItem(qlvitem, false));
        }

        testitem = item_ptr->itemAbove();
        if ((tcitem = dynamic_cast<TreeCheckItem*>(testitem)))
            menuItems->append(buildLCDMenuItem(tcitem, false));
        else if ((qlvitem = dynamic_cast<QListViewItem*>(testitem)))
            menuItems->append(buildLCDMenuItem(qlvitem, false));
    }

    // Is this the item to point at in the menu?
    // The first item ever comming into this method is expected to be
    // the currently highlighted item
    bool tf = (level == 1 ? true: false);

    // Add this item
    menuItems->append(buildLCDMenuItem(dynamic_cast<TreeCheckItem*>(item_ptr), 
                      tf));

    TreeCheckItem *tcitem;
    QListViewItem *qlvitem;

    // If there is an item below, add it to the list
    if ((tcitem = dynamic_cast<TreeCheckItem*>(item_ptr->itemBelow())))
        buildMenuTree(menuItems, tcitem, ++level);
    else if ((qlvitem = dynamic_cast<QListViewItem*>(item_ptr->itemBelow())))
        menuItems->append(buildLCDMenuItem(qlvitem, false));
}

LCDMenuItem *DatabaseBox::buildLCDMenuItem(TreeCheckItem *item_ptr, 
                                           bool curMenuItem)
{
    CHECKED_STATE check_state;

    if (item_ptr->isOn())
        check_state = CHECKED;
    else
        check_state = UNCHECKED;

    QString indent = indentMenuItem(item_ptr->getLevel());
    QString name =  indent + item_ptr->text().stripWhiteSpace();
    return new LCDMenuItem(curMenuItem, check_state, name, indent.length());
}

void DatabaseBox::buildMenuTree(QPtrList<LCDMenuItem> *menuItems, 
                                QListViewItem *item_ptr, int level)
{
    if (!item_ptr || level > LCD_MAX_MENU_ITEMS)
        return;

    // If this is the first time in, try to add a few previous items
    if (level == 1 && item_ptr->itemAbove())
    {
        TreeCheckItem *tcitem;
        QListViewItem *qlvitem;

        QListViewItem *testitem = item_ptr->itemAbove()->itemAbove();
        if (testitem)
        {
            if ((tcitem = dynamic_cast<TreeCheckItem*>(testitem)))
                menuItems->append(buildLCDMenuItem(tcitem, false));
            else if ((qlvitem = dynamic_cast<QListViewItem*>(testitem)))
                menuItems->append(buildLCDMenuItem(qlvitem, false));
        }

        testitem = item_ptr->itemAbove();
        if ((tcitem = dynamic_cast<TreeCheckItem*>(testitem)))
            menuItems->append(buildLCDMenuItem(tcitem, false));
        else if ((qlvitem = dynamic_cast<QListViewItem*>(testitem)))
            menuItems->append(buildLCDMenuItem(qlvitem, false));
    }

    // Is this the item to point at in the menu?
    // The first item ever comming into this method is expected to be
    // the currently highlighted item
    bool tf = (level == 1 ? true: false);

    // Add this item
    menuItems->append(buildLCDMenuItem(dynamic_cast<QListViewItem*>(item_ptr), 
                                       tf));

    TreeCheckItem *tcitem;
    QListViewItem *qlvitem;

    // If there is an item below, add it to the list

    if ((tcitem = dynamic_cast<TreeCheckItem*>(item_ptr->itemBelow())))
        buildMenuTree(menuItems, tcitem, ++level);
    else if ((qlvitem = dynamic_cast<QListViewItem*>(item_ptr->itemBelow())))
        menuItems->append(buildLCDMenuItem(qlvitem, false));
}

LCDMenuItem *DatabaseBox::buildLCDMenuItem(QListViewItem *item_ptr, 
                                           bool curMenuItem)
{
    CHECKED_STATE check_state = NOTCHECKABLE;
    QString name = "Danger Will Robinson";
    QString indent = "";

    if (PlaylistTitle *pl_ptr = dynamic_cast<PlaylistTitle*>(item_ptr))
    {
        check_state = NOTCHECKABLE;
        name = pl_ptr->getText().stripWhiteSpace();
    }
    else if (PlaylistItem *pli_ptr = dynamic_cast<PlaylistItem*>(item_ptr))
    {
        check_state = NOTCHECKABLE;

        indent = indentMenuItem("album");
        name = indent + pli_ptr->getText().stripWhiteSpace();
    }

    return new LCDMenuItem(curMenuItem, check_state, name, indent.length());
}

QString DatabaseBox::indentMenuItem(QString itemLevel)
{
    QString tree = gContext->GetSetting("TreeLevels", "artist album title");
    QStringList treelevels = QStringList::split(" ", tree.lower());

    // Find the index and indent that number of spaces
    int count = 1;
    QStringList::ConstIterator it;
    for (it = treelevels.begin(); it != treelevels.end(); ++it) 
    {
        if(*it == itemLevel)
            break;
        ++count;
    }

    // Reset the count if it hit the end without a match
    if (it == treelevels.end() || itemLevel == "cd")
        count = 0;

    // Try to do a few overrides for indentation
    if (itemLevel == "playlist")
        count = 1;

    QString returnvalue;
    if (count > 0)
        returnvalue = returnvalue.fill(' ', count);

    return returnvalue;
}

