// C
#include <cstdlib>

// C++
#include <iostream>
using namespace std;

// qt
#include <QApplication>
#include <QKeyEvent>
#include <QRegExp>
#include <QLayout>
#include <QEvent>

// mythtv
#include <dialogbox.h>
#include <mythcontext.h>
#include <lcddevice.h>
#include <uitypes.h>
#include <uilistbtntype.h>
#include <mythmediamonitor.h>
#include <mythsystem.h>
#include <exitcodes.h>
#include "mythlogging.h"

// mythmusic
#include "metadata.h"
#include "databasebox.h"
#include "treecheckitem.h"
#include "cddecoder.h"
#include "playlistcontainer.h"
#include "musicplayer.h"
#ifndef USING_MINGW
#include "cddecoder.h"
#endif

DatabaseBox::DatabaseBox(MythMainWindow *parent,
                         const QString dev, const QString &window_name,
                         const QString &theme_filename, const char *name)
           : MythThemedDialog(parent, window_name, theme_filename, name)
{
    m_CDdevice = dev;
    active_playlist = NULL;

    if (!gMusicData->all_music)
    {
        LOG(VB_GENERAL, LOG_ERR, "We are not going to get very far with a null "
                                 "pointer to metadata");
        // TODO: is this OK?
        exit(0);
    }

    //  Do we check the CD?
    cd_checking_flag = false;
    cd_checking_flag = gCoreContext->GetNumSetting("AutoLookupCD");

    QString treelev = gCoreContext->GetSetting("TreeLevels", "artist album title");
    QStringList treelevels = treelev.toLower().split(" ", QString::SkipEmptyParts);

    active_popup = NULL;
    active_pl_edit = NULL;

    playlist_popup = NULL;

    cditem = NULL;
    holding_track = false;

    tree = getUIListTreeType("musictree");
    if (!tree)
    {
        DialogBox *dlg = new DialogBox(
            GetMythMainWindow(),
            QObject::tr(
                "The theme you are using does not contain the "
                "%1 element. Please contact the theme creator "
                "and ask if they could please update it.<br><br>"
                "The next screen will be empty. "
                "Escape out of it to return to the menu.")
            .arg("'musictree'"));

        dlg->AddButton(tr("OK"));
        dlg->exec();
        dlg->deleteLater();

        return;
    }

    UITextType *line = NULL;

    for (int i = 1; i <= 6; i++)
    {
        QString linename = QString("line%1").arg(i);
        if ((line = getUITextType(linename)))
            m_lines.push_back(line);
    }

    if (m_lines.size() < 6)
    {
        DialogBox *dlg = new DialogBox(
            GetMythMainWindow(),
            tr("The theme you are using does not contain enough info "
               "lines in the music element. Please contact the theme "
               "creator and ask if they could please update it."));

        dlg->AddButton(tr("OK"));
        dlg->exec();
        dlg->deleteLater();
    }

    connect(tree, SIGNAL(itemEntered(UIListTreeType *, UIListGenericTree *)),
            this, SLOT(entered(UIListTreeType *, UIListGenericTree *)));

    // Make the first few nodes in the tree that everything else hangs off
    // as children

    rootNode = new UIListGenericTree(NULL, "Root Music Node");

    allmusic = new TreeCheckItem(rootNode, tr("All My Music"), "genre", 0);
    if (cd_checking_flag)
        cditem = new CDCheckItem(rootNode, tr("Blechy Blech Blah"), "cd", 0);
    alllists = new TreeCheckItem(rootNode, tr("All My Playlists"), "genre", 0);
    allcurrent = new PlaylistTitle(rootNode, tr("Active Play Queue"));

    tree->SetTree(rootNode);

    cd_reader_thread = NULL;
    if (cd_checking_flag)
    {
        // Start the CD checking thread, and set up a timer to make it check
        // occasionally

        cd_reader_thread = new ReadCDThread(m_CDdevice);

        // filling initialy before thread running
        fillCD();

        cd_reader_thread->start();

        cd_watcher = new QTimer(this);
        connect(cd_watcher, SIGNAL(timeout()), this, SLOT(occasionallyCheckCD()));
        cd_watcher->start(1000); // Every second?
    }

    // Set a timer to keep redoing the fillList stuff until the metadata and
    // playlist loading threads are done

    wait_counter = 0;
    numb_wait_dots = 0;
    fill_list_timer = new QTimer(this);
    connect(fill_list_timer, SIGNAL(timeout()), this, SLOT(keepFilling()));
    fill_list_timer->start(20);
}

DatabaseBox::~DatabaseBox()
{
    if (cd_reader_thread)
    {
        cd_watcher->stop();

        cd_reader_thread->wait();
        delete cd_reader_thread;
    }

    gMusicData->all_music->cleanOutThreads();
    gMusicData->all_playlists->cleanOutThreads();

    gMusicData->all_music->resetListings();

    gMusicData->all_playlists->getActive()->removeAllWidgets();
    gMusicData->all_playlists->getActive()->fillSonglistFromSongs();

    if (LCD *lcd = LCD::Get())
        lcd->switchToTime();

    delete rootNode;

    gCoreContext->SaveSetting("MusicBookmark", "");
    gCoreContext->SaveSetting("MusicBookmarkPosition", 0);
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

        if (LCD *lcd = LCD::Get())
        {
            // Set Loading Message on the LCD
            QList<LCDTextItem> textItems;

            textItems.append(LCDTextItem(1, ALIGN_CENTERED,
                             tr("Loading Music Data"), "Generic", false));
            lcd->switchToGeneric(textItems);
        }

        for (int i = 0; i < numb_wait_dots; i++)
            a_string += ".";

        allmusic->setText(a_string);
    }
}

void DatabaseBox::keepFilling()
{
    if (gMusicData->all_music->doneLoading() &&
        gMusicData->all_playlists->doneLoading())
    {
        //  Good, now lets grab some QListItems
        if (gMusicData->all_music->putYourselfOnTheListView(allmusic))
        {
            allmusic->setText(tr("All My Music"));
            fill_list_timer->stop();
            gMusicData->all_playlists->setActiveWidget(allcurrent);
            active_playlist = gMusicData->all_playlists->getActive();
            active_playlist->putYourselfOnTheListView(allcurrent);
            gMusicData->all_playlists->showRelevantPlaylists(alllists);
            // XXX listview->ensureItemVisible(listview->currentItem());
            checkTree();

            // make sure we get rid of the 'Loading Music Data' message
            if (LCD *lcd = LCD::Get())
                lcd->switchToTime();
        }
        else
            showWaiting();
    }
    else
        showWaiting();
}

void DatabaseBox::occasionallyCheckCD()
{
    if (!cd_reader_thread->getLock()->tryLock())
        return;
    cd_reader_thread->getLock()->unlock();

    if (cd_reader_thread->statusChanged())
    {
        if (active_playlist)
        {
            active_playlist->ripOutAllCDTracksNow();
            fillCD();
        }
    }
    if (!cd_reader_thread->isRunning())
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

    if (gMusicData->all_playlists->nameIsUnique(active_pl_edit->text(), 0))
    {
        gMusicData->all_playlists->copyNewPlaylist(active_pl_edit->text());
        gMusicData->all_playlists->showRelevantPlaylists(alllists);
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

    UIListGenericTree *item = tree->GetCurrentPosition();

    if (TreeCheckItem *rename_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if (rename_item->getID() < 0)
        {
            if (gMusicData->all_playlists->nameIsUnique(playlist_rename->text(),
                                            rename_item->getID() * -1))
            {
                gMusicData->all_playlists->renamePlaylist(rename_item->getID() * -1,
                                              playlist_rename->text());
                rename_item->setText(playlist_rename->text());
                tree->Redraw();
                closePlaylistPopup();
            }
            else
            {
                // I'd really like to beep at the user here
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Trying to rename something that doesn't "
                                     "seem to be a playlist");
        }
    }
}

void DatabaseBox::popBackPlaylist()
{
    if (!active_popup)
        return;

    gMusicData->all_playlists->popBackPlaylist();
    gMusicData->all_playlists->showRelevantPlaylists(alllists);
    checkTree();

    closeActivePopup();
}

void DatabaseBox::clearActive()
{
    if (!active_popup)
        return;

    closeActivePopup();

    gMusicData->all_playlists->clearActive();
    gMusicData->all_playlists->showRelevantPlaylists(alllists);
    checkTree();
}

void DatabaseBox::CreateCDAudio()
{
    int error;

    if (!active_popup)
        return;

    closeActivePopup();

    // Begin CD Audio Creation

    error = active_playlist->CreateCDAudio();

    error_popup = NULL;

    if (error)
        ErrorPopup(tr("Couldn't create CD"));
    else
        ErrorPopup(tr("CD Created"));
}

void DatabaseBox::CreateCDMP3()
{
    int error;
    if (!active_popup)
        return;

    closeActivePopup();

    // Begin MP3 Creation

    error = active_playlist->CreateCDMP3();

    error_popup=NULL;

    if (error)
        ErrorPopup(tr("Couldn't create CD"));
    else
        ErrorPopup(tr("CD Created"));
}

void DatabaseBox::ErrorPopup(const QString &msg)
{
    if (error_popup)
        return;

    error_popup = new MythPopupBox(GetMythMainWindow(),
                                   "playlist_popup");

    error_popup->addLabel(msg);

    QAbstractButton *mac_b =error_popup->addButton(tr("OK"), this,
                                           SLOT(closeErrorPopup()));

    int x = (int)(100 * wmult);
    int y = (int)(100 * hmult);

    error_popup->ShowPopupAtXY(x, y, this, SLOT(closeErrorPopup()));
    mac_b->setFocus();
}

void DatabaseBox::closeErrorPopup(void)
{
    if (!error_popup)
        return;

    error_popup->hide();
    error_popup->deleteLater();
    error_popup = NULL;
}

void DatabaseBox::BlankCDRW()
{
    if (!active_popup)
        return;

    closeActivePopup();

    // Check & get global settings
    if (!gCoreContext->GetNumSetting("CDWriterEnabled"))
    {
        LOG(VB_GENERAL, LOG_ERR, "Writer is not enabled. We cannot be here!");
        return;
    }

    QString scsidev = MediaMonitor::defaultCDWriter();
    if (scsidev.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "No CD Writer device defined.");
        return;
    }

    // Begin Blanking
    MythProgressDialog *record_progress;
    record_progress = new MythProgressDialog(tr("CD-RW Blanking Progress"), 10);

    // Run CD Record
    QString blanktype=gCoreContext->GetSetting("CDBlankType");

    record_progress->setProgress(1);

    QString cmd = QString("cdrecord -v dev=%1 -blank=%2")
        .arg(scsidev).arg(blanktype);
    uint flags = kMSRunShell | kMSDontBlockInputDevs | kMSDontDisableDrawing;

    if (myth_system(cmd, flags) != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("DatabaseBox::BlankCDRW() cmd: '%1' Failed!").arg(cmd));
    }

    record_progress->Close();
    record_progress->deleteLater();
}

void DatabaseBox::deletePlaylist()
{
    if (!playlist_popup)
        return;

    //  Delete currently selected

    closePlaylistPopup();

    UIListGenericTree *item = tree->GetCurrentPosition();

    if (TreeCheckItem *check_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if (check_item->getID() < 0)
        {
            if (check_item->nextSibling(1))
                tree->MoveDown();
            else if (check_item->prevSibling(1))
                tree->MoveUp();

            gMusicData->all_playlists->deletePlaylist(check_item->getID() * -1);
            //gMusicData->all_playlists->showRelevantPlaylists(alllists);
            item->RemoveFromParent();
            //delete item; will be deleted by generic tree
            gMusicData->all_playlists->refreshRelevantPlaylists(alllists);
            checkTree();
            return;
        }
    }

    LOG(VB_GENERAL, LOG_ERR, "deletePlaylist() - Playlist popup on a "
                             "non-playlist item");
}


void DatabaseBox::copyToActive()
{
    if (!playlist_popup)
        return;

    closePlaylistPopup();

    UIListGenericTree *item = tree->GetCurrentPosition();

    if (TreeCheckItem *check_item = dynamic_cast<TreeCheckItem*>(item) )
    {
        if (check_item->getID() < 0)
        {
            gMusicData->all_playlists->copyToActive(check_item->getID() * -1);
            gMusicData->all_playlists->refreshRelevantPlaylists(alllists);
            tree->RefreshCurrentLevel();
            checkTree();
            // XXX listview->setCurrentItem(allcurrent);
            return;
        }
    }
    LOG(VB_GENERAL, LOG_ERR, "copyToActive() - Playlist popup on a "
                             "non-playlist item");
}


void DatabaseBox::fillCD(void)
{
    QMutexLocker locker(cd_reader_thread->getLock());

    if (cditem)
    {

        // Close leaf before delete if opened

        UIListGenericTree *curItem = tree->GetCurrentPosition();
        if (dynamic_cast<CDCheckItem*>(curItem))
        {
            int depth = curItem->calculateDepth(0);
            while(depth--)
                tree->MoveLeft();
        }

        // Delete anything that might be there

        while (cditem->childCount())
        {
            CDCheckItem *track_ptr =
                static_cast<CDCheckItem*>(cditem->getChildAt(0));
            track_ptr->RemoveFromParent();
        }

        // Put on whatever all_music tells us is there

        cditem->setText(gMusicData->all_music->getCDTitle());
        cditem->setCheck(0);
        cditem->setCheckable(false);

        gMusicData->all_music->putCDOnTheListView(cditem);

        //  reflect selections in cd playlist

        GenericTree::iterator it;
        for (it = cditem->begin(); it != cditem->end(); ++it)
        {
            UIListGenericTree *uit = (UIListGenericTree*)(*it);

            if (CDCheckItem *track_ptr = dynamic_cast<CDCheckItem*>(uit))
            {
                track_ptr->setCheck(0);
                if (gMusicData->all_playlists->checkCDTrack(
                        track_ptr->getID() * -1))
                {
                    track_ptr->setCheck(2);
                }
            }
        }

        // Can't check what ain't there

        if (cditem->childCount() > 0)
        {
            cditem->setCheckable(true);
            cditem->setCheck(0);
            checkParent(cditem);
        }

        tree->Redraw();
    }
}

void DatabaseBox::doMenus(UIListGenericTree *item)
{
    if (dynamic_cast<CDCheckItem*>(item))
        return;
    else if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        if (item_ptr->getID() < 0)
            doPlaylistPopup(item_ptr);
    }
    else if (PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
        doActivePopup(item_ptr);
}

void DatabaseBox::alternateDoMenus(UIListGenericTree *item, int keypad_number)
{
    if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        bool is_cd = dynamic_cast<CDCheckItem*>(item) != 0;
        if (item_ptr->getID() < 0 && !is_cd)
            doPlaylistPopup(item_ptr);
        else if (item->getParent())
        {
            int a_number = item->getParent()->childCount();
            a_number = (int)(a_number * ( keypad_number / 10.0));

            tree->MoveUp(UIListTreeType::MoveMax);
            tree->MoveDown(a_number);
        }
    }
    else if (PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
        doActivePopup(item_ptr);
}

void DatabaseBox::entered(UIListTreeType *treetype, UIListGenericTree *item)
{
    if (!item || !treetype)
        return;

    // Determin if this is a CD entry
    bool cd = false;
    if (dynamic_cast<CDCheckItem*>(item))
      cd = true;

    TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item);

    if (item_ptr
        && item->childCount() == 0
        && item_ptr->getLevel() == "title")
    {
        int id = item_ptr->getID();

        Metadata *mdata;
        if (!cd)
        {
            mdata = gMusicData->all_music->getMetadata(id);
            if (!mdata)
                return;
        }
        else
        {
            // Need to allocate storage for CD Metadata
            mdata = new Metadata;
            if (!gMusicData->all_music->getCDMetadata(id, mdata))
            {
                delete mdata;
                return;
            }
        }

        unsigned int line = 0;
        QString tmpstr;

        if (mdata->Compilation())
        {
            tmpstr = tr("Compilation Artist:\t") + mdata->CompilationArtist();
            if (m_lines.at(line))
                m_lines.at(line++)->SetText(tmpstr);
        }

        tmpstr = tr("Artist:\t") + mdata->Artist();
        if (m_lines.at(line))
            m_lines.at(line++)->SetText(tmpstr);

        tmpstr = tr("Album:\t") + mdata->Album();
        if (m_lines.at(line))
            m_lines.at(line++)->SetText(tmpstr);

        tmpstr = tr("Title:\t") + mdata->Title();
        if (m_lines.at(line))
            m_lines.at(line++)->SetText(tmpstr);

        if (m_lines.at(line))
        {
            int maxTime = mdata->Length() / 1000;

            int maxh = maxTime / 3600;
            int maxm = (maxTime / 60) % 60;
            int maxs = maxTime % 60;

            QString timeStr;
            if (maxh > 0)
                timeStr.sprintf("%02d:%02d:%02d", maxh, maxm, maxs);
            else
                timeStr.sprintf("%02d:%02d", maxm, maxs);

            tmpstr = tr("Length:\t") + timeStr;

            m_lines.at(line++)->SetText(tmpstr);
        }

        tmpstr = tr("Genre: ") + mdata->Genre();

        if (m_lines.at(line))
        {
            m_lines.at(line)->SetText(tmpstr);
        }
        else
        {
            QString prevvalue = m_lines.at(line-1)->GetText();
            tmpstr = prevvalue + "            " + tmpstr;
            m_lines.at(line-1)->SetText(tmpstr);
        }

        // Pre increment as not incremented from previous use.
        while (++line < (unsigned) m_lines.size())
          m_lines.at(line)->SetText("");

        // Don't forget to delete the mdata storage if we allocated it.
        if (cd)
          delete mdata;

        return;
    }

    QStringList pathto = treetype->getRouteToCurrent();

    int linelen = 0;
    int dispat = 0;
    QString data;

    for (QStringList::Iterator it = pathto.begin();
         it != pathto.end(); ++it)
    {
        if (it == pathto.begin())
            continue;

        if (!data.isEmpty())
            data += "  /  ";

        data += *it;
        linelen++;
        if (linelen == 2)
        {
            if (m_lines.at(dispat))
            {
                m_lines.at(dispat)->SetText(data);
            }

            data.clear();
            linelen = 0;
            dispat++;
        }
    }

    if (linelen != 0)
    {
        if (m_lines.at(dispat))
        {
            m_lines.at(dispat)->SetText(data);
        }
        dispat++;
    }

    for (unsigned int i = dispat; i < (unsigned) m_lines.size(); i++)
        m_lines.at(i)->SetText("");
}

void DatabaseBox::selected(UIListGenericTree *item)
{
    if (!item)
        return;

    UIListGenericTree *parent = (UIListGenericTree *)item->getParent();

    if (CDCheckItem *item_ptr = dynamic_cast<CDCheckItem*>(item))
    {
        // Something to do with a CD
        if (active_playlist)
        {
            if (item_ptr->getCheck() > 0)
                item_ptr->setCheck(0);
            else
                item_ptr->setCheck(2);
            doSelected(item_ptr, true);
            if (CDCheckItem *item_ptr = dynamic_cast<CDCheckItem*>(parent))
                checkParent(item_ptr);
            tree->Redraw();
        }

    }
    else if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(item))
    {
        if (active_playlist)
        {
            if (item_ptr->getCheck() > 0)
                item_ptr->setCheck(0);
            else
                item_ptr->setCheck(2);
            doSelected(item_ptr, false);
            if (TreeCheckItem *item_ptr = dynamic_cast<TreeCheckItem*>(parent))
                checkParent(item_ptr);
            tree->Redraw();
        }
    }
    else if (PlaylistItem *item_ptr = dynamic_cast<PlaylistTrack*>(item))
        dealWithTracks(item_ptr);
    else if (PlaylistTitle *item_ptr = dynamic_cast<PlaylistTitle*>(item))
        doActivePopup(item_ptr);
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "That's odd ... there's something I don't "
                                 "recognize on a ListView");
    }
}


void DatabaseBox::doPlaylistPopup(TreeCheckItem *item_ptr)
{
    if (playlist_popup)
        return;

    // Popup for all other playlists (up top)
    playlist_popup = new MythPopupBox(GetMythMainWindow(),
                                      "playlist_popup");

    QAbstractButton *mac_b = playlist_popup->addButton(tr("Move to Active Play Queue"),
                                               this, SLOT(copyToActive()));

    playlist_popup->addButton(tr("Delete This Playlist"), this,
                              SLOT(deletePlaylist()));

    playlist_rename = new MythRemoteLineEdit(playlist_popup);
    playlist_rename->setText(item_ptr->getString());
    playlist_popup->addWidget(playlist_rename);

    playlist_popup->addButton(tr("Rename This Playlist"), this,
                              SLOT(renamePlaylist()));

    playlist_popup->ShowPopup(this, SLOT(closePlaylistPopup()));

    mac_b->setFocus();
}

void DatabaseBox::closePlaylistPopup(void)
{
    if (!playlist_popup)
        return;

    playlist_popup->hide();
    playlist_popup->deleteLater();
    playlist_popup = NULL;
}

void DatabaseBox::doActivePopup(PlaylistTitle *item_ptr)
{
    if (active_popup)
        return;

    //  Popup for active playlist
    active_popup = new MythPopupBox(GetMythMainWindow(),
                                    "active_popup");
    active_pl_edit = new MythRemoteLineEdit(active_popup);
    active_popup->addWidget(active_pl_edit);
    active_pl_edit->setFocus();

    active_popup->addButton(tr("Copy To New Playlist"), this,
                            SLOT(copyNewPlaylist()));

    active_popup->addButton(tr("Clear the Active Play Queue"), this,
                            SLOT(clearActive()));

    QAbstractButton *pb = active_popup->addButton(tr("Save Back to Playlist Tree"),
                                          this, SLOT(popBackPlaylist()));

    // CD writing

    bool cdwriter = false;

    if (gCoreContext->GetNumSetting("CDWriterEnabled"))
    {
        QString scsidev = MediaMonitor::defaultCDWriter();
        if (!scsidev.isEmpty())
            cdwriter = true;
    }

    QAbstractButton *cdaudiob = NULL;
    QAbstractButton *cdmp3b = NULL;

    if (cdwriter)
    {
#if 0
        /*
         Disabled as Playlist::CreateCDAudio() is empty,
         it is called by CreateCDAudio()
        */
        cdaudiob = active_popup->addButton(tr("Create Audio CD from "
                                               "Playlist"), this,
                                               SLOT(CreateCDAudio()));
#endif

        cdmp3b = active_popup->addButton(tr("Create MP3 CD from Playlist"),
                                         this, SLOT(CreateCDMP3()));

        active_popup->addButton(tr("Clear CD-RW Disk"), this,
                                SLOT(BlankCDRW()));

        double size_in_MB = 0.0;
        double size_in_sec = 0.0;
        active_playlist->computeSize(size_in_MB, size_in_sec);

        int disksize = gCoreContext->GetNumSetting("CDDiskSize", 2);

        double max_size_in_MB;
        double max_size_in_min;

        if (disksize == 1)
        {
            max_size_in_MB = 650;
            max_size_in_min = 75;
        }
        else
        {
            max_size_in_MB = 700;
            max_size_in_min = 80;
        }

        double ratio_MB = 100.0 * size_in_MB / max_size_in_MB;
        double ratio_sec = 100.0 * size_in_sec / 60.0 / 1000.0 / max_size_in_min;

        QString label1;
        QString label2;

        label1.sprintf("Size: %dMB (%02d%%)", (int)(size_in_MB),
                       (int)(ratio_MB));
        label2.sprintf("Duration: %3dmin (%02d%%)",
                       (int)(size_in_sec / 60.0 / 1000.0), (int)(ratio_sec));

        active_popup->addLabel(label1);
        active_popup->addLabel(label2);

        cdmp3b->setEnabled((ratio_MB <= 100.0));

        if (cdaudiob)
        {
            cdaudiob->setEnabled((ratio_sec <= 100.0));
            cdaudiob->setEnabled(false);
        }
    }

    (void)item_ptr;

    active_pl_edit->setText("");

    active_popup->ShowPopup(this, SLOT(closeActivePopup()));

    if (gMusicData->all_playlists->pendingWriteback())
        pb->setEnabled(true);
    else
        pb->setEnabled(false);
}

void DatabaseBox::closeActivePopup(void)
{
    if (!active_popup)
        return;

    active_popup->hide();
    active_popup->deleteLater();
    active_popup = NULL;
}

void DatabaseBox::dealWithTracks(PlaylistItem *item_ptr)
{
    // Logic to handle the start of moving/deleting songs from playlist

    if (holding_track)
    {
        LOG(VB_GENERAL, LOG_ERR, "dealWithTracks() - Holding track. This is "
                                 "not supposed to happen");
        holding_track = false;
        track_held->beMoving(false);
        releaseKeyboard();
        tree->RedrawCurrent();
    }
    else
    {
        holding_track = true;
        track_held = dynamic_cast<PlaylistTrack*>(item_ptr);
        track_held->beMoving(true);
        grabKeyboard();
        tree->RedrawCurrent();
    }
}

void DatabaseBox::doSelected(UIListGenericTree *item, bool cd_flag)
{
    bool keep_going = false;

    TreeCheckItem *tcitem = (TreeCheckItem *)item;

    if (tcitem->childCount() > 0)
    {
        keep_going = true;
        UIListGenericTree *test = (UIListGenericTree *)tcitem->getChildAt(0);
        if (PlaylistItem *check_item = dynamic_cast<PlaylistItem*>(test))
        {
            (void)check_item;
            keep_going = false;
        }
    }

    if (keep_going)
    {
        GenericTree::iterator it;
        for (it = tcitem->begin(); it != tcitem->end(); ++it)
        {
            TreeCheckItem *child = (TreeCheckItem*)(*it);
            if (child->getCheck() != tcitem->getCheck())
            {
                child->setCheck(tcitem->getCheck());
                doSelected(child, cd_flag);
            }
        }
    }
    else
    {
        if (tcitem->getCheck() == 2)
            active_playlist->addTrack(tcitem->getID(), true, cd_flag);
        else
            active_playlist->removeTrack(tcitem->getID(), cd_flag);
    }
}

void DatabaseBox::checkParent(UIListGenericTree *item)
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

    if (do_check)
    {
        TreeCheckItem *tcitem = dynamic_cast<TreeCheckItem*>(item);
        TreeCheckItem *child = (TreeCheckItem *)tcitem->getChildAt(0);
        if (!child)
            return;

        bool allon = true;
        bool oneon = false;

        GenericTree::iterator it;
        for (it = tcitem->begin(); it != tcitem->end(); ++it)
        {
            child = (TreeCheckItem*)(*it);
            if (child->getCheck() > 0)
                oneon = true;
            if (child->getCheck() == 0)
                allon = false;
        }

        if (allon)
            tcitem->setCheck(2);
        else if (oneon)
            tcitem->setCheck(1);
        else
            tcitem->setCheck(0);

        if (tcitem->getParent())
            checkParent((UIListGenericTree *)tcitem->getParent());
    }
}

void DatabaseBox::deleteTrack(UIListGenericTree *item)
{
    if (PlaylistTrack *delete_item = dynamic_cast<PlaylistCD*>(item) )
    {
        if (delete_item->nextSibling(1))
            tree->MoveDown();
        else if (delete_item->prevSibling(1))
            tree->MoveUp();

        UIListGenericTree *newItem = (UIListGenericTree *)delete_item->getParent();
        if (TreeCheckItem *item_owner = dynamic_cast<TreeCheckItem*>(newItem))
        {
            Playlist *owner =
                gMusicData->all_playlists->getPlaylist(item_owner->getID() * -1);

            if (owner)
                owner->removeTrack(delete_item->getID(), true);
        }
        else if (PlaylistTitle *item_owner = dynamic_cast<PlaylistTitle*>(newItem))
        {
            (void)item_owner;
            active_playlist->removeTrack(delete_item->getID(), true);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "deleteTrack() - I don't know how to "
                                     "delete whatever you're trying to get "
                                     "rid of");
        }

        gMusicData->all_playlists->refreshRelevantPlaylists(alllists);
        checkTree();
    }
    else if (PlaylistTrack *delete_item = dynamic_cast<PlaylistTrack*>(item))
    {
        if (delete_item->nextSibling(1))
            tree->MoveDown();
        else if (delete_item->prevSibling(1))
            tree->MoveUp();

        UIListGenericTree *newItem = (UIListGenericTree *)delete_item->getParent();
        if (TreeCheckItem *item_owner = dynamic_cast<TreeCheckItem*>(newItem))
        {
            Playlist *owner =
                gMusicData->all_playlists->getPlaylist(item_owner->getID() * -1);

            if (owner)
                owner->removeTrack(delete_item->getID(), false);
        }
        else if (PlaylistTitle *item_owner = dynamic_cast<PlaylistTitle*>(newItem))
        {
            (void)item_owner;
            active_playlist->removeTrack(delete_item->getID(), false);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "deleteTrack() - I don't know how to "
                                     "delete whatever you're trying to get "
                                     "rid of");
        }
        gMusicData->all_playlists->refreshRelevantPlaylists(alllists);
        checkTree();
    }
}

void DatabaseBox::moveHeldUpDown(bool flag)
{
    track_held->moveUpDown(flag);
    tree->RedrawCurrent();
    //XXX listview->ensureItemVisible(track_held);
    //XXX listview->setCurrentItem(track_held);
}

void DatabaseBox::keyPressEvent(QKeyEvent *e)
{
    // This is a bit wonky, but it works (more or less)

    if (holding_track)
    {
        bool handled = false;
        QStringList actions;
        handled = GetMythMainWindow()->TranslateKeyPress("Qt", e, actions);
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "SELECT" || action == "ESCAPE")
            {
                //  Done holding this track
                holding_track = false;
                track_held->beMoving(false);
                tree->RedrawCurrent();
                releaseKeyboard();
            }
            else if (action == "UP")
                moveHeldUpDown(true);
            else if (action == "DOWN")
                moveHeldUpDown(false);
            else
                handled = false;
        }
        return;
    }

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", e, actions);

    UIListGenericTree *curItem = tree->GetCurrentPosition();

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "DELETE")
            deleteTrack(curItem);
        else if (action == "MENU" || action == "INFO")
            doMenus(curItem);
        else if (action == "SELECT")
            selected(curItem);
        else if (action == "0" || action == "1" || action == "2" ||
                 action == "3" || action == "4" || action == "5" ||
                 action == "6" || action == "7" || action == "8" ||
                 action == "9")
        {
            alternateDoMenus(curItem, action.toInt());
        }
        else if (action == "UP")
            tree->MoveUp();
        else if (action == "DOWN")
            tree->MoveDown();
        else if (action == "LEFT")
            tree->MoveLeft();
        else if (action == "RIGHT")
            tree->MoveRight();
        else if (action == "PAGEUP")
            tree->MoveUp(UIListTreeType::MovePage);
        else if (action == "PAGEDOWN")
            tree->MoveDown(UIListTreeType::MovePage);
        else if (action == "INCSEARCH")
            tree->incSearchStart();
        else if (action == "INCSEARCHNEXT")
            tree->incSearchNext();
        else
            handled = false;
    }

    if (handled)
        return;

    MythDialog::keyPressEvent(e);
}

void DatabaseBox::checkTree(UIListGenericTree *startingpoint)
{
    bool toplevel = false;
    if (!startingpoint)
    {
        toplevel = true;
        startingpoint = rootNode;
    }

    GenericTree::iterator it;
    for (it = startingpoint->begin(); it != startingpoint->end(); ++it)
    {
        UIListGenericTree *uit = (UIListGenericTree*)(*it);
        //  Only check things which are TreeCheckItem's
        if (TreeCheckItem *item = dynamic_cast<TreeCheckItem*>(uit))
        {
            bool is_cd = dynamic_cast<CDCheckItem*>(uit) != 0;
            item->setCheck(0);
            if (active_playlist->checkTrack(item->getID(), is_cd))
            {
                //  Turn on if it's on the current playlist
                item->setCheck(2);
                checkParent((UIListGenericTree *)item->getParent());
            }

            if (item->childCount() > 0)
                checkTree(item);
        }
    }

    if (toplevel)
        tree->Redraw();
}

void DatabaseBox::setCDTitle(const QString& title)
{
    if (cditem)
        cditem->setText(title);
}

ReadCDThread::ReadCDThread(const QString &dev) :
    MThread("ReadCD"), m_CDdevice(dev), cd_status_changed(false)
{
}

void ReadCDThread::run()
{
    RunProlog();
#ifndef USING_MINGW
    // lock all_music and cd_status_changed while running thread
    QMutexLocker locker(getLock());

    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
    decoder->setDevice(m_CDdevice);
    int tracknum = decoder->getNumCDAudioTracks();

    bool redo = false;

    if (tracknum != gMusicData->all_music->getCDTrackCount())
    {
        cd_status_changed = true;
        LOG(VB_GENERAL, LOG_INFO, QString("CD status has changed."));
    }
    else
        cd_status_changed = false;

    if (tracknum == 0)
    {
        //  No CD, or no recognizable CD
        gMusicData->all_music->clearCDData();
        gMusicData->all_playlists->clearCDList();
    }
    else if (tracknum > 0)
    {
        // Check the last track to see if it's differen than whatever it was
        // before
        Metadata *checker = decoder->getLastMetadata();
        if (checker)
        {
            if (!gMusicData->all_music->checkCDTrack(checker))
            {
                redo = true;
                cd_status_changed = true;
                gMusicData->all_music->clearCDData();
                gMusicData->all_playlists->clearCDList();
            }
            else
                cd_status_changed = false;
            delete checker;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "The cddecoder said it had audio tracks, "
                                     "but it won't tell me about them");
        }
    }

    int tracks = decoder->getNumTracks();
    bool setTitle = false;

    for (int actual_tracknum = 1;
         redo && actual_tracknum <= tracks; actual_tracknum++)
    {
        Metadata *track = decoder->getMetadata(actual_tracknum);
        if (track)
        {
            gMusicData->all_music->addCDTrack(track);

            if (!setTitle)
            {

                QString parenttitle = " ";
                if (track->FormatArtist().length() > 0)
                {
                    parenttitle += track->FormatArtist();
                    parenttitle += " ~ ";
                }

                if (track->Album().length() > 0)
                    parenttitle += track->Album();
                else
                {
                    parenttitle = " " + QObject::tr("Unknown");
                    LOG(VB_GENERAL, LOG_ERR,
                        "Couldn't find your CD. It may not be in the freedb "
                        "database.\n"
                        "    More likely, however, is that you need to delete\n"
                        "    ~/.cddb and ~/.cdserverrc and restart mythmusic.");
                }
                gMusicData->all_music->setCDTitle(parenttitle);
                setTitle = true;
            }
            delete track;
        }
    }

    delete decoder;
#endif // USING_MINGW
    RunEpilog();
}

