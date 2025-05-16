// C++
#include <cstdlib>
#include <iostream>

// qt
#include <QKeyEvent>

// MythTV
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythlogging.h>
#include <libmythmetadata/musicutils.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibuttontree.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuiutils.h>

// mythmusic
#include "mainvisual.h"
#include "musiccommon.h"
#include "musicdata.h"
#include "playlistcontainer.h"
#include "playlisteditorview.h"
#include "smartplaylist.h"

MusicGenericTree::MusicGenericTree(MusicGenericTree *parent,
                                   const QString &name, const QString &action,
                                   MythUIButtonListItem::CheckState check,
                                   bool showArrow)
                 : MythGenericTree(name),
                   m_action(action),
                   m_check(check),
                   m_showArrow(showArrow)
{
    if (!action.isEmpty())
        setSelectable(true);

    if (parent)
    {
        parent->addNode(this);
        parent->setDrawArrow(true);
    }
}

void MusicGenericTree::setDrawArrow(bool flag)
{
    m_showArrow = flag;
    if (m_buttonItem)
        m_buttonItem->setDrawArrow(true);
}

void MusicGenericTree::setCheck(MythUIButtonListItem::CheckState state)
{
    m_check = state;
    if (m_buttonItem)
    {
        m_buttonItem->setCheckable(m_check != MythUIButtonListItem::CantCheck);
        m_buttonItem->setChecked(m_check);
    }
}

MythUIButtonListItem *MusicGenericTree::CreateListButton(MythUIButtonList *list)
{
    auto *item = new MusicButtonItem(list, GetText());
    item->SetData(QVariant::fromValue((MythGenericTree*) this));

    if (visibleChildCount() > 0)
        item->setDrawArrow(true);

    if (m_showArrow)
        item->setDrawArrow(true);

    item->setCheckable(m_check != MythUIButtonListItem::CantCheck);
    item->setChecked(m_check);

    m_buttonItem = item;

    return item;
}

///////////////////////////////////////////////////////////////////////////////

#define LOC      QString("PlaylistEditorView: ")
#define LOC_WARN QString("PlaylistEditorView, Warning: ")
#define LOC_ERR  QString("PlaylistEditorView, Error: ")

PlaylistEditorView::PlaylistEditorView(MythScreenStack *parent, MythScreenType *parentScreen,
                                       const QString &layout, bool restorePosition)
         :MusicCommon(parent, parentScreen, "playlisteditorview"),
            m_layout(layout), m_restorePosition(restorePosition)
{
    gCoreContext->addListener(this);
    gCoreContext->SaveSetting("MusicPlaylistEditorView", layout);
}

PlaylistEditorView::~PlaylistEditorView()
{
    gCoreContext->removeListener(this);

    saveTreePosition();

    for (int x = 0; x < m_deleteList.count(); x++)
        delete m_deleteList.at(x);
    m_deleteList.clear();

    delete m_rootNode;
}

bool PlaylistEditorView::Create(void)
{
    bool err = false;

    QString windowName;

    if (m_layout == "gallery")
    {
        windowName = "playlisteditorview_gallery";
        m_currentView = MV_PLAYLISTEDITORGALLERY;
    }
    else
    {
        windowName = "playlisteditorview_tree";
        m_currentView = MV_PLAYLISTEDITORTREE;
    }

    // Load the theme for this screen
    err = LoadWindowFromXML("music-ui.xml", windowName, this);

    if (!err)
    {
        gPlayer->removeListener(this);
        return false;
    }

    // find common widgets available on any view
    err = CreateCommon();

    // find widgets specific to this view
    UIUtilE::Assign(this, m_playlistTree   , "playlist_tree", &err);
    UIUtilW::Assign(this, m_breadcrumbsText, "breadcrumbs",   &err);
    UIUtilW::Assign(this, m_positionText,    "position",      &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot load screen '%1'").arg(windowName));
        gPlayer->removeListener(this);
        return false;
    }

    createRootNode();

    treeNodeChanged(m_rootNode->getChildAt(0));

    m_playlistTree->AssignTree(m_rootNode);

    QStringList route = gCoreContext->GetSetting("MusicTreeLastActive", "").split("\n");
    restoreTreePosition(route);

    connect(m_playlistTree, &MythUIButtonTree::itemClicked,
            this, &PlaylistEditorView::treeItemClicked);
    connect(m_playlistTree, &MythUIButtonTree::nodeChanged,
            this, &PlaylistEditorView::treeNodeChanged);

    if (m_currentView == MV_PLAYLISTEDITORGALLERY)
        connect(m_playlistTree, &MythUIButtonTree::itemVisible,
                this, &PlaylistEditorView::treeItemVisible);

    BuildFocusList();

    return true;
}

void PlaylistEditorView::customEvent(QEvent *event)
{
    if ((event->type() == MusicPlayerEvent::kMetadataChangedEvent) ||
        (event->type() == MusicPlayerEvent::kAlbumArtChangedEvent))
    { // NOLINT(bugprone-branch-clone)
        // TODO: this could be more efficient
        reloadTree();
    }
    else if ((event->type() == MusicPlayerEvent::kTrackRemovedEvent) ||
             (event->type() == MusicPlayerEvent::kTrackAddedEvent)   ||
             (event->type() == MusicPlayerEvent::kAllTracksRemovedEvent))
    {
        updateSelectedTracks();
    }
    else if (event->type() == MusicPlayerEvent::kPlaylistChangedEvent)
    {
        //TODO should just update the relevent playlist here
        reloadTree();
    }
    else if (event->type() == MusicPlayerEvent::kCDChangedEvent)
    {
        //TODO should just update the cd node
        reloadTree();
    }
    else if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent*>(event);
        if (!me)
            return;

        if (me->Message().startsWith("MUSIC_RESYNC_FINISHED"))
        {
            QStringList list = me->Message().simplified().split(' ');
            if (list.size() == 4)
            {
                int added = list[1].toInt();
                int removed = list[2].toInt();
                int changed = list[3].toInt();

                // if something changed reload the tree
                if (added || removed || changed)
                    reloadTree();
            }
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);

        // make sure the user didn't ESCAPE out of the menu
        if ((dce == nullptr) || (dce->GetResult() < 0))
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "smartplaylistmenu")
        {
            if (GetFocusWidget() != m_playlistTree)
                return;

            MythGenericTree *node = m_playlistTree->GetCurrentNode();
            if (!node)
                return;

            auto *mnode = dynamic_cast<MusicGenericTree*>(node);
            if (!mnode)
                return;

            if (resulttext == tr("New Smart Playlist"))
            {
                QString category;
                if (mnode->getAction() == "smartplaylistcategory")
                    category = mnode->GetText();

                MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                auto* editor = new SmartPlaylistEditor(mainStack);

                if (!editor->Create())
                {
                    delete editor;
                    return;
                }

                editor->newSmartPlaylist(category);

                connect(editor, &SmartPlaylistEditor::smartPLChanged,
                        this, &PlaylistEditorView::smartPLChanged);

                mainStack->AddScreen(editor);
            }
            else if (resulttext == tr("Remove Smart Playlist"))
            {
                QString category = mnode->getParent()->GetText();
                QString name = mnode->GetText();

                ShowOkPopup(tr("Are you sure you want to delete this Smart Playlist?\n"
                               "Category: %1 - Name: %2").arg(category, name),
                            this, &PlaylistEditorView::deleteSmartPlaylist, true);
            }
            else if (resulttext == tr("Edit Smart Playlist"))
            {
                QString category = mnode->getParent()->GetText();
                QString name = mnode->GetText();

                MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                auto* editor = new SmartPlaylistEditor(mainStack);

                if (!editor->Create())
                {
                    delete editor;
                    return;
                }

                editor->editSmartPlaylist(category, name);

                connect(editor, &SmartPlaylistEditor::smartPLChanged,
                        this, &PlaylistEditorView::smartPLChanged);

                mainStack->AddScreen(editor);
            }
            else if (resulttext == tr("Replace Tracks"))
            {
                m_playlistOptions.insertPLOption = PL_REPLACE;
                doUpdatePlaylist();
            }
            else if (resulttext == tr("Add Tracks"))
            {
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                doUpdatePlaylist();
            }
            else if (resulttext == tr("Play Now"))
            {               // cancel shuffles and repeats to play now
                gPlayer->setShuffleMode(MusicPlayer::SHUFFLE_OFF);
                gPlayer->setRepeatMode(MusicPlayer::REPEAT_OFF);
                updateShuffleMode(true);
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                m_playlistOptions.playPLOption = PL_FIRSTNEW;
                doUpdatePlaylist();
            }
        }
        else if (resultid == "treeplaylistmenu")
        {
            if (GetFocusWidget() != m_playlistTree)
                return;

            MythGenericTree *node = m_playlistTree->GetCurrentNode();
            if (!node)
                return;

            auto *mnode = dynamic_cast<MusicGenericTree*>(node);
            if (!mnode)
                return;

            if (resulttext == tr("Remove Playlist"))
            {
                QString name = mnode->GetText();

                ShowOkPopup(tr("Are you sure you want to delete this Playlist?\n"
                               "Name: %1").arg(name),
                            this, &PlaylistEditorView::deletePlaylist, true);
            }
            else if (resulttext == tr("Replace Tracks"))
            {
                m_playlistOptions.insertPLOption = PL_REPLACE;
                doUpdatePlaylist();
            }
            else if (resulttext == tr("Add Tracks"))
            {
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                doUpdatePlaylist();
            }
            else if (resulttext == tr("Play Now"))
            {               // cancel shuffles and repeats to play now
                gPlayer->setShuffleMode(MusicPlayer::SHUFFLE_OFF);
                gPlayer->setRepeatMode(MusicPlayer::REPEAT_OFF);
                updateShuffleMode(true);
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                m_playlistOptions.playPLOption = PL_FIRSTNEW;
                doUpdatePlaylist();
            }
        }
    }

    MusicCommon::customEvent(event);
}

bool PlaylistEditorView::keyPressEvent(QKeyEvent *event)
{
    // if there is a pending jump point pass the key press to the default handler
    if (GetMythMainWindow()->IsExitingToMain())
    {
        // Just call the parent handler.  It will properly clean up.
        return MusicCommon::keyPressEvent(event);
    }

    if (!m_moveTrackMode && GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if ((action == "EDIT" || action == "INFO") && GetFocusWidget() == m_playlistTree)
        {
            handled = false;

            MythGenericTree *node = m_playlistTree->GetCurrentNode();
            if (node)
            {
                auto *mnode = dynamic_cast<MusicGenericTree*>(node);
                if (mnode)
                {
                    if (mnode->getAction() == "smartplaylist" && action == "EDIT")
                    {
                        QString category = mnode->getParent()->GetText();
                        QString name = mnode->GetText();

                        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                        auto* editor = new SmartPlaylistEditor(mainStack);

                        if (!editor->Create())
                        {
                            delete editor;
                            return true;
                        }

                        editor->editSmartPlaylist(category, name);
                        connect(editor, &SmartPlaylistEditor::smartPLChanged,
                                this, &PlaylistEditorView::smartPLChanged);

                        mainStack->AddScreen(editor);

                        handled = true;
                    }
                    else if (mnode->getAction() == "smartplaylistcategory" && action == "EDIT")
                    {
                        QString category = mnode->GetText();

                        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                        auto* editor = new SmartPlaylistEditor(mainStack);

                        if (!editor->Create())
                        {
                            delete editor;
                            return true;
                        }

                        editor->newSmartPlaylist(category);

                        connect(editor, &SmartPlaylistEditor::smartPLChanged,
                                this, &PlaylistEditorView::smartPLChanged);

                        mainStack->AddScreen(editor);

                        handled = true;
                    }
                    else if (mnode->getAction() == "trackid")
                    {
                        MusicMetadata *mdata = gMusicData->m_all_music->getMetadata(mnode->getInt());
                        if (mdata)
                        {
                            if (action == "INFO")
                                showTrackInfo(mdata);
                            else
                                editTrackInfo(mdata);
                        }

                        handled = true;
                    }
                }
            }
        }
        else if (action == "DELETE" && GetFocusWidget() == m_playlistTree)
        {
            handled = false;

            MythGenericTree *node = m_playlistTree->GetCurrentNode();
            if (node)
            {
                auto *mnode = dynamic_cast<MusicGenericTree*>(node);
                if (mnode)
                {
                    if (mnode->getAction() == "smartplaylist")
                    {
                        QString category = mnode->getParent()->GetText();
                        QString name = mnode->GetText();

                        ShowOkPopup(tr("Are you sure you want to delete this Smart Playlist?\n"
                                       "Category: %1 - Name: %2").arg(category, name),
                                    this, &PlaylistEditorView::deleteSmartPlaylist, true);
                        handled = true;
                    }
                    else if (mnode->getAction() == "playlist")
                    {
                        QString name = mnode->GetText();

                        ShowOkPopup(tr("Are you sure you want to delete this Playlist?\n"
                                       "Name: %1").arg(name),
                                    this, &PlaylistEditorView::deletePlaylist, true);
                        handled = true;
                    }
                }
            }
        }
        else if (action == "MARK")
        {
            MythUIButtonListItem *item = m_playlistTree->GetItemCurrent();
            if (item)
                treeItemClicked(item);
        }
        else if ((action == "PLAY") && (GetFocusWidget() == m_playlistTree))
        {
            MythUIButtonListItem *item = m_playlistTree->GetItemCurrent();
            if (item)
            {
                auto *node = item->GetData().value<MythGenericTree*>();
                auto *mnode = dynamic_cast<MusicGenericTree*>(node);

                if (mnode)
                {
                    updateSonglist(mnode);

                    if (m_songList.count() > 0)
                    {
                        m_playlistOptions.playPLOption = PL_FIRST;
                        m_playlistOptions.insertPLOption = PL_REPLACE;

                        stopVisualizer();

                        doUpdatePlaylist();

                        if (m_mainvisual)
                        {
                            m_mainvisual->mutex()->lock();
                            m_mainvisual->prepare();
                            m_mainvisual->mutex()->unlock();
                        }
                    }
                    else
                    {
                        handled = false;
                    }
                 }
             }
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MusicCommon::keyPressEvent(event))
        handled = true;

    return handled;
}

void PlaylistEditorView::updateSonglist(MusicGenericTree *node)
{
    m_songList.clear();

    if (node->getAction() == "playlists" ||
        node->getAction() == "smartplaylists" ||
        node->getAction() == "smartplaylistcategory")
    { // NOLINT(bugprone-branch-clone)
    }
    else if (node->getAction() == "trackid" || node->getAction() == "cdtrack")
    {
        m_songList.append(node->getInt());
    }
    else if (node->getAction() == "all tracks" ||
             node->getAction() == "albums" ||
             node->getAction() == "compartists" ||
             node->getAction() == "artists" ||
             node->getAction() == "genres" ||
             node->getAction() == "ratings" ||
             node->getAction() == "years")
    {
        // get the list of tracks from the previous 'All Tracks' node
        auto *allTracksNode = dynamic_cast<MusicGenericTree*>(node->getParent()->getChildAt(0));
        if (allTracksNode)
        {
            for (int x = 0; x < allTracksNode->childCount(); x++)
            {
                MythGenericTree *trackNode = allTracksNode->getChildAt(x);
                if (trackNode)
                    m_songList.append(trackNode->getInt());
            }
        }
    }
    else if (node->getAction() == "album" ||
             node->getAction() == "artist" ||
             node->getAction() == "genre" ||
             node->getAction() == "rating" ||
             node->getAction() == "year" ||
             node->getAction() == "compilations" ||
             node->getAction() == "compartist")
    {
        // get the list of tracks from the 'All Tracks' node
        auto *allTracksNode = dynamic_cast<MusicGenericTree*>(node->getChildAt(0));
        if (allTracksNode)
        {
            if (allTracksNode->childCount() == 0)
                filterTracks(allTracksNode);

            for (int x = 0; x < allTracksNode->childCount(); x++)
            {
                MythGenericTree *trackNode = allTracksNode->getChildAt(x);
                if (trackNode)
                    m_songList.append(trackNode->getInt());
            }
        }
    }
    else if (node->getAction() == "smartplaylist")
    {
        // add the selected smart playlist's tracks to the song list
        QList<MythGenericTree*> *children = node->getAllChildren();
        for (int x = 0; x < children->count(); x++)
        {
            MythGenericTree *childnode = children->at(x);
            m_songList.append(childnode->getInt());
        }
    }
    else if (node->getAction() == "playlist")
    {
        // get list of tracks to add from the playlist
        int playlistID = node->getInt();
        Playlist *playlist = gMusicData->m_all_playlists->getPlaylist(playlistID);

        if (playlist)
        {
            for (int x = 0; x < playlist->getTrackCount(); x++)
            {
                MusicMetadata *mdata = playlist->getSongAt(x);
                if (mdata)
                    m_songList.append((int) mdata->ID());
            }
        }
    }
    else if (node->getAction() == "error")
    {
        // a smart playlist has returned no tracks etc.
    }
    else
    {
        // fall back to getting the tracks from the MetadataPtrList
        auto *tracks = node->GetData().value<MetadataPtrList*>();
        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
                m_songList.append((int)mdata->ID());
        }
    }
}

void PlaylistEditorView::ShowMenu(void)
{
    if (GetFocusWidget() == m_playlistTree)
    {
        m_playlistOptions.playPLOption = PL_CURRENT;
        m_playlistOptions.insertPLOption = PL_REPLACE;

        MythMenu *menu = nullptr;
        auto *mnode = dynamic_cast<MusicGenericTree*>(m_playlistTree->GetCurrentNode());

        if (mnode)
        {
            if (mnode->getAction() == "smartplaylists" ||
                mnode->getAction() == "smartplaylistcategory" ||
                mnode->getAction() == "smartplaylist")
            {
                menu = createSmartPlaylistMenu();
            }
            else if (mnode->getAction() == "playlists" ||
                        mnode->getAction() == "playlist")
            {
                menu = createPlaylistMenu();
            }
            else if ( // (mnode->getAction() == "trackid") ||
                     (mnode->getAction() == "error"))
            {
            }
            else
            {
                menu = createPlaylistOptionsMenu();
            }

            updateSonglist(mnode);
        }

        if (menu)
        {
            menu->AddItem(tr("More Options"), nullptr, createMainMenu());

            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

            auto *menuPopup = new MythDialogBox(menu, popupStack, "actionmenu");

            if (menuPopup->Create())
                popupStack->AddScreen(menuPopup);
            else
                delete menu;

            return;
        }
    }

    MusicCommon::ShowMenu();
}

MythMenu* PlaylistEditorView::createPlaylistMenu(void)
{
    MythMenu *menu = nullptr;

    if (GetFocusWidget() == m_playlistTree)
    {
        auto *mnode = dynamic_cast<MusicGenericTree*>(m_playlistTree->GetCurrentNode());

        if (!mnode)
            return nullptr;

        if (mnode->getAction() == "playlist")
        {
            menu = new MythMenu(tr("Playlist Actions"), this, "treeplaylistmenu");
            if (MusicPlayer::getPlayNow())
            {
                menu->AddItem(tr("Play Now"));
                menu->AddItem(tr("Add Tracks"));
            }
            else
            {
                menu->AddItem(tr("Add Tracks"));
                menu->AddItem(tr("Play Now"));
            }
            menu->AddItem(tr("Replace Tracks"));
            menu->AddItem(tr("Remove Playlist"));
        }
    }

    return menu;
}

MythMenu* PlaylistEditorView::createSmartPlaylistMenu(void)
{
    MythMenu *menu = nullptr;

    if (GetFocusWidget() == m_playlistTree)
    {
        auto *mnode = dynamic_cast<MusicGenericTree*>(m_playlistTree->GetCurrentNode());

        if (!mnode)
            return nullptr;

        if (mnode->getAction() == "smartplaylists" || mnode->getAction() == "smartplaylistcategory")
        {
            QString label = tr("Smart Playlist Actions");

            menu = new MythMenu(label, this, "smartplaylistmenu");

            menu->AddItem(tr("New Smart Playlist"));
        }
        else if (mnode->getAction() == "smartplaylist")
        {
            menu = new MythMenu(tr("Smart Playlist Actions"), this, "smartplaylistmenu");

            if (MusicPlayer::getPlayNow())
            {
                menu->AddItem(tr("Play Now"));
                menu->AddItem(tr("Add Tracks"));
            }
            else
            {
                menu->AddItem(tr("Add Tracks"));
                menu->AddItem(tr("Play Now"));
            }
            menu->AddItem(tr("Replace Tracks"));

            menu->AddItem(tr("Edit Smart Playlist"));
            menu->AddItem(tr("New Smart Playlist"));
            menu->AddItem(tr("Remove Smart Playlist"));
        }
    }

    return menu;
}

// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void PlaylistEditorView::createRootNode(void )
{
    if (!m_rootNode)
        m_rootNode = new MusicGenericTree(nullptr, "Root Music Node");

    auto *node = new MusicGenericTree(m_rootNode, tr("All Tracks"), "all tracks");
    node->setDrawArrow(true);
    node->SetData(QVariant::fromValue(gMusicData->m_all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Albums"), "albums");
    node->setDrawArrow(true);
    node->SetData(QVariant::fromValue(gMusicData->m_all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Artists"), "artists");
    node->setDrawArrow(true);
    node->SetData(QVariant::fromValue(gMusicData->m_all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Genres"), "genres");
    node->setDrawArrow(true);
    node->SetData(QVariant::fromValue(gMusicData->m_all_music->getAllMetadata()));
#if 0
    node = new MusicGenericTree(m_rootNode, tr("Tags"), "tags");
    node->setDrawArrow(true);
    node->SetData(QVariant::fromValue(gMusicData->all_music->getAllMetadata()));
#endif
    node = new MusicGenericTree(m_rootNode, tr("Ratings"), "ratings");
    node->setDrawArrow(true);
    node->SetData(QVariant::fromValue(gMusicData->m_all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Years"), "years");
    node->setDrawArrow(true);
    node->SetData(QVariant::fromValue(gMusicData->m_all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Compilations"), "compilations");
    node->setDrawArrow(true);

    MetadataPtrList *alltracks = gMusicData->m_all_music->getAllMetadata();
    auto *compTracks = new MetadataPtrList;
    m_deleteList.append(compTracks);

    for (int x = 0; x < alltracks->count(); x++)
    {
        MusicMetadata *mdata = alltracks->at(x);
        if (mdata)
        {
            if (mdata->Compilation())
                compTracks->append(mdata);
        }
    }
    node->SetData(QVariant::fromValue(compTracks));

    if (gMusicData->m_all_music->getCDTrackCount())
    {
        node = new MusicGenericTree(m_rootNode, tr("CD - %1").arg(gMusicData->m_all_music->getCDTitle()), "cd");
        node->setDrawArrow(true);
        node->SetData(QVariant::fromValue(gMusicData->m_all_music->getAllCDMetadata()));
    }

    node = new MusicGenericTree(m_rootNode, tr("Directory"), "directory");
    node->setDrawArrow(true);
    node->SetData(QVariant::fromValue(gMusicData->m_all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Playlists"), "playlists");
    node->setDrawArrow(true);

    node = new MusicGenericTree(m_rootNode, tr("Smart Playlists"), "smartplaylists");
    node->setDrawArrow(true);
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void PlaylistEditorView::treeItemClicked(MythUIButtonListItem *item)
{
    auto *node = item->GetData().value<MythGenericTree*>();
    auto *mnode = dynamic_cast<MusicGenericTree*>(node);

    if (!mnode || !gPlayer->getCurrentPlaylist() || mnode->getAction() == "error")
        return;

    if (mnode->getAction() == "trackid")
    {
        if (gPlayer->getCurrentPlaylist()->checkTrack(mnode->getInt()))
        {
            // remove track from the current playlist
            gPlayer->removeTrack(mnode->getInt());
            mnode->setCheck(MythUIButtonListItem::NotChecked);
        }
        else if (MusicPlayer::getPlayNow())
        {
            gPlayer->addTrack(mnode->getInt(), false);
            gPlayer->setCurrentTrackPos(gPlayer->getCurrentPlaylist()->getTrackCount() - 1);
            updateUIPlaylist();
            mnode->setCheck(MythUIButtonListItem::FullChecked);
        }
        else
        {
            // add track to the current playlist
            gPlayer->addTrack(mnode->getInt(), true);
            mnode->setCheck(MythUIButtonListItem::FullChecked);
        }
    }
    else
    {
        ShowMenu();
    }
}


void PlaylistEditorView::treeItemVisible(MythUIButtonListItem *item)
{
    auto *node = item->GetData().value<MythGenericTree*>();;
    auto *mnode = dynamic_cast<MusicGenericTree*>(node);

    if (!mnode)
        return;

    if (item->GetText("*").isEmpty())
    {
        item->SetText(" ", "*");

        QString artFile;

        if (mnode->getAction() == "trackid")
        {
            MusicMetadata *mdata = gMusicData->m_all_music->getMetadata(mnode->getInt());
            if (mdata)
                artFile = mdata->getAlbumArtFile();
        }
        else if (mnode->getAction() == "album")
        {
            // hunt for a coverart image for the album
            auto *tracks = node->GetData().value<MetadataPtrList*>();
            for (int x = 0; x < tracks->count(); x++)
            {
                MusicMetadata *mdata = tracks->at(x);
                if (mdata)
                {
                    artFile = mdata->getAlbumArtFile();
                    if (!artFile.isEmpty())
                        break;
                }
            }
        }
        else if (mnode->getAction() == "compartist")
        {
            artFile = findIcon("artist", mnode->GetText().toLower());
        }
        else if (mnode->getAction() == "all tracks" || mnode->getAction() == "genres" ||
                 mnode->getAction() == "albums"     || mnode->getAction() == "artists" ||
                 mnode->getAction() == "compartists"|| mnode->getAction() == "ratings" ||
                 mnode->getAction() == "years"      || mnode->getAction() == "compilations" ||
                 mnode->getAction() == "cd"         || mnode->getAction() == "directory" ||
                 mnode->getAction() == "playlists"  || mnode->getAction() == "smartplaylists")
        {
            artFile = "blank.png";
        }
        else
        {
            artFile = findIcon(mnode->getAction(), mnode->GetText().toLower());
        }

        QString state = "default";

        if (mnode->getAction() == "all tracks")
            state = "alltracks";
        else if (mnode->getAction() == "genres")
            state = "genres";
        else if (mnode->getAction() == "albums")
            state = "albums";
        else if (mnode->getAction() == "artists")
            state = "artists";
        else if (mnode->getAction() == "compartists")
            state = "compartists";
        else if (mnode->getAction() == "ratings")
            state = "ratings";
        else if (mnode->getAction() == "years")
            state = "years";
        else if (mnode->getAction() == "compilations")
            state = "compilations";
        else if (mnode->getAction() == "cd")
            state = "cd";
        else if (mnode->getAction() == "directory")
            state = "directory";
        else if (mnode->getAction() == "playlists")
            state = "playlists";
        else if (mnode->getAction() == "smartplaylists")
            state = "smartplaylists";

        item->DisplayState(state, "nodetype");

        if (artFile.isEmpty())
            item->SetImage("");
        else
            item->SetImage(artFile);
    }
}

void PlaylistEditorView::treeNodeChanged(MythGenericTree *node)
{
    auto *mnode = dynamic_cast<MusicGenericTree*>(node);
    if (!mnode)
        return;

    if (m_breadcrumbsText)
    {
        QString route = node->getRouteByString().join(" -> ");
        route = route.remove("Root Music Node -> ");
        m_breadcrumbsText->SetText(route);
    }

    if (m_positionText)
    {
        m_positionText->SetText(tr("%1 of %2").arg(node->getPosition() + 1)
                                              .arg(node->siblingCount()));
    }

    if (mnode->childCount() > 0 || mnode->getAction() == "trackid")
        return;

    if (mnode->getAction() == "smartplaylists")
    {
        getSmartPlaylistCategories(mnode);
    }
    else if (mnode->getAction() == "smartplaylistcategory")
    {
        getSmartPlaylists(mnode);
    }
    else if (mnode->getAction() == "smartplaylist")
    {
        getSmartPlaylistTracks(mnode, mnode->getInt());
    }
    else if (mnode->getAction() == "playlists")
    {
        getPlaylists(mnode);
    }
    else if (mnode->getAction() == "playlist")
    {
        getPlaylistTracks(mnode, mnode->getInt());
    }
    else if (mnode->getAction() == "cd")
    {
        getCDTracks(mnode);
    }
    else
    {
        filterTracks(mnode);
    }
}

void PlaylistEditorView::filterTracks(MusicGenericTree *node)
{
    auto *tracks = node->GetData().value<MetadataPtrList*>();

    if (!tracks)
        return;

    if (node->getAction() == "all tracks")
    {
        QMultiMap<QString, int> map;
        bool isAlbum = false;
        auto *parentNode = dynamic_cast<MusicGenericTree*>(node->getParent());

        if (parentNode)
            isAlbum = parentNode->getAction() == "album";

        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
            {
                QString key = mdata->Title();

                // Add the track number if an album is selected
                if (isAlbum && mdata->Track() > 0)
                {
                    key.prepend(QString::number(mdata->Track()) + " - ");
                    if (mdata->Track() < 10)
                        key.prepend("0");

                    // Add the disc number if this is a multi-disc album
                    if (mdata->DiscNumber() > 0)
                    {
                        key.prepend(QString::number(mdata->DiscNumber()) + "/");
                        if (mdata->DiscNumber() < 10)
                            key.prepend("0");
                    }
                }
                map.insert(key, mdata->ID());
            }
        }

        auto i = map.constBegin();
        while (i != map.constEnd())
        {
            auto *newnode = new MusicGenericTree(node, i.key(), "trackid");
            newnode->setInt(i.value());
            newnode->setDrawArrow(false);
            bool hasTrack = gPlayer->getCurrentPlaylist() ? gPlayer->getCurrentPlaylist()->checkTrack(newnode->getInt()) : false;
            newnode->setCheck( hasTrack ? MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "artists")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
            {
                if (map.contains(mdata->Artist()))
                {
                    MetadataPtrList *filteredTracks = map.value(mdata->Artist());
                    filteredTracks->append(mdata);
                }
                else
                {
                    auto *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(mdata->Artist(), filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            auto *newnode = new MusicGenericTree(node, i.key(), "artist");
            newnode->SetData(QVariant::fromValue(i.value()));
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "compartists")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
            {
                if (mdata->CompilationArtist() != mdata->Artist())
                {
                    if (map.contains(mdata->CompilationArtist()))
                    {
                        MetadataPtrList *filteredTracks = map.value(mdata->CompilationArtist());
                        filteredTracks->append(mdata);
                    }
                    else
                    {
                        auto *filteredTracks = new MetadataPtrList;
                        m_deleteList.append(filteredTracks);
                        filteredTracks->append(mdata);
                        map.insert(mdata->CompilationArtist(), filteredTracks);
                    }
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            auto *newnode = new MusicGenericTree(node, i.key(), "compartist");
            newnode->SetData(QVariant::fromValue(i.value()));
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "albums")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
            {
                if (map.contains(mdata->Album()))
                {
                    MetadataPtrList *filteredTracks = map.value(mdata->Album());
                    filteredTracks->append(mdata);
                }
                else
                {
                    auto *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(mdata->Album(), filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            auto *newnode = new MusicGenericTree(node, i.key(), "album");
            newnode->SetData(QVariant::fromValue(i.value()));
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "genres")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
            {
                if (map.contains(mdata->Genre()))
                {
                    MetadataPtrList *filteredTracks = map.value(mdata->Genre());
                    filteredTracks->append(mdata);
                }
                else
                {
                    auto *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(mdata->Genre(), filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            auto *newnode = new MusicGenericTree(node, i.key(), "genre");
            newnode->SetSortText(i.key()); // No manipulation of prefixes on genres
            newnode->SetData(QVariant::fromValue(i.value()));
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "ratings")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
            {
                QString ratingStr = tr("%n Star(s)", "", mdata->Rating());
                if (map.contains(ratingStr))
                {
                    MetadataPtrList *filteredTracks = map.value(ratingStr);
                    filteredTracks->append(mdata);
                }
                else
                {
                    auto *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(ratingStr, filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            auto *newnode = new MusicGenericTree(node, i.key(), "rating");
            newnode->SetData(QVariant::fromValue(i.value()));
            ++i;
        }
    }
    else if (node->getAction() == "years")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
            {
                QString yearStr = QString("%1").arg(mdata->Year());
                if (map.contains(yearStr))
                {
                    MetadataPtrList *filteredTracks = map.value(yearStr);
                    filteredTracks->append(mdata);
                }
                else
                {
                    auto *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(yearStr, filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            auto *newnode = new MusicGenericTree(node, i.key(), "year");
            newnode->SetData(QVariant::fromValue(i.value()));
            ++i;
        }
    }
    else if (node->getAction() == "directory")
    {
        QMap<QString, MetadataPtrList*> map;

        // which directories have we already filtered by
        QString dir;
        MusicGenericTree *climber = node;
        while (climber)
        {
            dir = climber->GetText() + '/' + dir;
            climber = dynamic_cast<MusicGenericTree *>(climber->getParent());
        }

        // remove the top two nodes
        QString top2 = "Root Music Node/" + tr("Directory") + '/';
        if (dir.startsWith(top2))
            dir = dir.mid(top2.length());

        for (int x = 0; x < tracks->count(); x++)
        {
            MusicMetadata *mdata = tracks->at(x);
            if (mdata)
            {
                QString filename = mdata->Filename(false);

                if (filename.startsWith(dir))
                    filename = filename.mid(dir.length());

                QStringList dirs = filename.split("/");

                QString key = dirs.count() > 1 ? dirs[0] : "[TRACK]" + dirs[0];
                if (map.contains(key))
                {
                    MetadataPtrList *filteredTracks = map.value(key);
                    filteredTracks->append(mdata);
                }
                else
                {
                    auto *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(key, filteredTracks);
                }
            }
        }

        // add directories first
        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            if (!i.key().startsWith("[TRACK]"))
            {
                auto *newnode = new MusicGenericTree(node, i.key(), "directory");
                newnode->SetData(QVariant::fromValue(i.value()));
            }
            ++i;
        }

        // now add tracks
        i = map.constBegin();
        while (i != map.constEnd())
        {
            if (i.key().startsWith("[TRACK]"))
            {
                auto *newnode = new MusicGenericTree(node, i.key().mid(7), "trackid");
                newnode->setInt(i.value()->at(0)->ID());
                newnode->setDrawArrow(false);
                bool hasTrack = gPlayer->getCurrentPlaylist() ? gPlayer->getCurrentPlaylist()->checkTrack(newnode->getInt()) : false;
                newnode->setCheck( hasTrack ? MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
            }
            ++i;
        }
    }
    else if (node->getAction() == "artist" || node->getAction() == "compartist" ||
             node->getAction() == "album" || node->getAction() == "genre" ||
             node->getAction() == "rating" || node->getAction() == "year" ||
             node->getAction() == "compilations")
    {
        // which fields have we already filtered by
        QStringList fields;
        MusicGenericTree *climber = node;
        while (climber)
        {
            fields.append(climber->getAction());
            climber = dynamic_cast<MusicGenericTree *>(climber->getParent());
        }

        auto *newnode = new MusicGenericTree(node, tr("All Tracks"), "all tracks");
        newnode->setDrawArrow(true);
        newnode->SetData(node->GetData());

        if (!fields.contains("albums"))
        {
            newnode = new MusicGenericTree(node, tr("Albums"), "albums");
            newnode->setDrawArrow(true);
            newnode->SetData(node->GetData());
        }

        if (!fields.contains("artists"))
        {
            newnode = new MusicGenericTree(node, tr("Artists"), "artists");
            newnode->setDrawArrow(true);
            newnode->SetData(node->GetData());
        }

        if (!fields.contains("compartists"))
        {
            // only show the Compilation Artists node if we are one the Compilations branch
            bool showCompArtists = false;
            MusicGenericTree *mnode = node;
            while (mnode)
            {
                if (mnode->getAction() == "compilations")
                {
                    showCompArtists = true;
                    break;
                }

                mnode = dynamic_cast<MusicGenericTree *>(mnode->getParent());
            }

            // only show the Comp. Artist if it differs from the Artist
            bool found = false;
            auto *tracks2 = node->GetData().value<MetadataPtrList*>();
            for (int x = 0; x < tracks2->count(); x++)
            {
                MusicMetadata *mdata = tracks2->at(x);
                if (mdata)
                {
                    if (mdata->Artist() != mdata->CompilationArtist())
                    {
                        found = true;
                        break;
                    }
                }
            }

            if (showCompArtists && found)
            {
                newnode = new MusicGenericTree(node, tr("Compilation Artists"), "compartists");
                newnode->setDrawArrow(true);
                newnode->SetData(node->GetData());
            }
        }

        if (!fields.contains("genres"))
        {
            newnode = new MusicGenericTree(node, tr("Genres"), "genres");
            newnode->setDrawArrow(true);
            newnode->SetData(node->GetData());
        }
#if 0
        if (!fields.contains("tags"))
        {
            newnode = new MusicGenericTree(node, tr("Tags"), "tags");
            newnode->setDrawArrow(true);
            newnode->SetData(node->GetData());
        }
#endif
        if (!fields.contains("ratings"))
        {
            newnode = new MusicGenericTree(node, tr("Ratings"), "ratings");
            newnode->setDrawArrow(true);
            newnode->SetData(node->GetData());
        }

        if (!fields.contains("years"))
        {
            newnode = new MusicGenericTree(node, tr("Years"), "years");
            newnode->setDrawArrow(true);
            newnode->SetData(node->GetData());
        }
    }
}

void PlaylistEditorView::getSmartPlaylistCategories(MusicGenericTree *node)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (query.exec("SELECT categoryid, name FROM music_smartplaylist_categories ORDER BY name;"))
    {
        if (query.isActive() && query.size() > 0)
        {
            // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
            while (query.next())
            {
                // No memory leak. MusicGenericTree adds the new node
                // into a list of nodes maintained by its parent.
                auto *newnode =
                    new MusicGenericTree(node, query.value(1).toString(), "smartplaylistcategory");
                newnode->setInt(query.value(0).toInt());
            }
        }
    }
    else
    {
        MythDB::DBError("Load smartplaylist categories", query);
    }
}

void PlaylistEditorView::getSmartPlaylists(MusicGenericTree *node)
{
    int categoryid = node->getInt();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT smartplaylistid, name FROM music_smartplaylists WHERE categoryid = :CATEGORYID "
                  "ORDER BY name;");
    query.bindValue(":CATEGORYID", categoryid);
    if (query.exec())
    {
        if (query.isActive() && query.size() > 0)
        {
            // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
            while (query.next())
            {
                // No memory leak. MusicGenericTree adds the new node
                // into a list of nodes maintained by its parent.
                auto *newnode =
                    new MusicGenericTree(node, query.value(1).toString(), "smartplaylist");
                newnode->setInt(query.value(0).toInt());
            }
        }
    }
    else
    {
        MythDB::DBError("Load smartplaylist names", query);
    }
}

void PlaylistEditorView::getSmartPlaylistTracks(MusicGenericTree *node, int playlistID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // find smartplaylist
    QString matchType;
    QString orderBy;
    int limitTo = 0;

    query.prepare("SELECT smartplaylistid, matchtype, orderby, limitto "
                  "FROM music_smartplaylists "
                  "WHERE smartplaylistid = :SMARTPLAYLISTID;");
    query.bindValue(":SMARTPLAYLISTID", playlistID);

    if (query.exec())
    {
        if (query.isActive() && query.size() > 0)
        {
            query.first();
            matchType = (query.value(1).toString() == "All") ? " AND " : " OR ";
            orderBy = query.value(2).toString();
            limitTo = query.value(3).toInt();
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING,
                LOC + QString("Cannot find smartplaylist: %1").arg(playlistID));
            return;
        }
    }
    else
    {
        MythDB::DBError("Find SmartPlaylist", query);
        return;
    }

    // get smartplaylist items
    QString whereClause = "WHERE ";

    query.prepare("SELECT field, operator, value1, value2 "
                  "FROM music_smartplaylist_items "
                  "WHERE smartplaylistid = :ID;");
    query.bindValue(":ID", playlistID);
    if (query.exec())
    {
        bool bFirst = true;
        while (query.next())
        {
            QString fieldName = query.value(0).toString();
            QString operatorName = query.value(1).toString();
            QString value1 = query.value(2).toString();
            QString value2 = query.value(3).toString();
            if (!bFirst)
            {
                whereClause += matchType + getCriteriaSQL(fieldName,
                                           operatorName, value1, value2);
            }
            else
            {
               bFirst = false;
               whereClause += " " + getCriteriaSQL(fieldName, operatorName,
                                                   value1, value2);
            }
        }
    }

    // add order by clause
    whereClause += getOrderBySQL(orderBy);

    // add limit
    if (limitTo > 0)
        whereClause +=  " LIMIT " + QString::number(limitTo);


    // find the tracks for this smartplaylist
    QString theQuery;

    theQuery = "SELECT song_id, name FROM music_songs "
               "LEFT JOIN music_directories ON"
               " music_songs.directory_id=music_directories.directory_id "
               "LEFT JOIN music_artists ON"
               " music_songs.artist_id=music_artists.artist_id "
               "LEFT JOIN music_albums ON"
               " music_songs.album_id=music_albums.album_id "
               "LEFT JOIN music_genres ON"
               " music_songs.genre_id=music_genres.genre_id "
               "LEFT JOIN music_artists AS music_comp_artists ON "
               "music_albums.artist_id=music_comp_artists.artist_id ";
    if (whereClause.length() > 0)
      theQuery += whereClause;

    if (!query.exec(theQuery))
    {
        MythDB::DBError("Load songlist from query", query);
        return;
    }

    while (query.next())
    {
        auto *newnode =
                new MusicGenericTree(node, query.value(1).toString(), "trackid");
        newnode->setInt(query.value(0).toInt());
        newnode->setDrawArrow(false);
        bool hasTrack = gPlayer->getCurrentPlaylist() ? gPlayer->getCurrentPlaylist()->checkTrack(newnode->getInt()) : false;
        newnode->setCheck( hasTrack ? MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
    }

    // check we found some tracks if not add something to let the user know
    if (node->childCount() == 0)
    {
        auto *newnode =
                new MusicGenericTree(node, tr("** No matching tracks **"), "error");
        newnode->setDrawArrow(false);
    }
}

void PlaylistEditorView::getPlaylists(MusicGenericTree *node)
{
    QList<Playlist*> *playlists = gMusicData->m_all_playlists->getPlaylists();

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    for (int x =0; x < playlists->count(); x++)
    {
        Playlist *playlist = playlists->at(x);
        // No memory leak. MusicGenericTree adds the new node
        // into a list of nodes maintained by its parent.
        auto *newnode =
                new MusicGenericTree(node, playlist->getName(), "playlist");
        newnode->setInt(playlist->getID());
    }
}

void PlaylistEditorView::getCDTracks(MusicGenericTree *node)
{
    MetadataPtrList *tracks = gMusicData->m_all_music->getAllCDMetadata();

    for (int x = 0; x < tracks->count(); x++)
    {
        MusicMetadata *mdata = tracks->at(x);
        QString title = QString("%1 - %2").arg(mdata->Track()).arg(mdata->FormatTitle());
        auto *newnode = new MusicGenericTree(node, title, "trackid");
        newnode->setInt(mdata->ID());
        newnode->setDrawArrow(false);
        bool hasTrack = gPlayer->getCurrentPlaylist() ? gPlayer->getCurrentPlaylist()->checkTrack(mdata->ID()) : false;
        newnode->setCheck(hasTrack ? MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
    }
}

void PlaylistEditorView::getPlaylistTracks(MusicGenericTree *node, int playlistID)
{
    Playlist *playlist = gMusicData->m_all_playlists->getPlaylist(playlistID);

    if (playlist)
    {
        for (int x = 0; x < playlist->getTrackCount(); x++)
        {
            MusicMetadata *mdata = playlist->getSongAt(x);
            if (mdata)
            {
                auto *newnode = new MusicGenericTree(node, mdata->Title(), "trackid");
                newnode->setInt(mdata->ID());
                newnode->setDrawArrow(false);
                bool hasTrack = gPlayer->getCurrentPlaylist() ? gPlayer->getCurrentPlaylist()->checkTrack(mdata->ID()) : false;
                newnode->setCheck(hasTrack ? MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
            }
        }
    }

    // check we found some tracks if not add something to let the user know
    if (node->childCount() == 0)
    {
        auto *newnode =
                new MusicGenericTree(node, tr("** Empty Playlist!! **"), "error");
        newnode->setDrawArrow(false);
    }
}

void PlaylistEditorView::updateSelectedTracks(void)
{
    updateSelectedTracks(m_rootNode);
}

void PlaylistEditorView::updateSelectedTracks(MusicGenericTree *node)
{
    for (int x = 0; x < node->childCount(); x++)
    {
        auto *mnode =  dynamic_cast<MusicGenericTree*>(node->getChildAt(x));
        if (mnode)
        {
            if (mnode->getAction() == "trackid")
            {
                bool hasTrack = gPlayer->getCurrentPlaylist() ? gPlayer->getCurrentPlaylist()->checkTrack(mnode->getInt()) : false;
                mnode->setCheck(hasTrack ? MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
            }
            else
            {
                if (mnode->childCount())
                    updateSelectedTracks(mnode);
            }
        }
    }
}

void PlaylistEditorView::reloadTree(void)
{
    QStringList route = m_playlistTree->GetCurrentNode()->getRouteByString();

    m_playlistTree->Reset();

    for (int x = 0; x < m_deleteList.count(); x++)
        delete m_deleteList.at(x);
    m_deleteList.clear();

    m_rootNode->deleteAllChildren();
    createRootNode();
    m_playlistTree->AssignTree(m_rootNode);

    restoreTreePosition(route);
}

void PlaylistEditorView::restoreTreePosition(const QStringList &route)
{
    if (route.count() < 2)
        return;

    // traverse up the tree creating each nodes children as we go
    MythGenericTree *node = m_rootNode;
    for (int x = 1 ; x < route.count(); x++)
    {
        node = node->getChildByName(route.at(x));

        if (node)
            treeNodeChanged(node);
        else
            break;
    }

    m_playlistTree->SetNodeByString(route);
}

void PlaylistEditorView::saveTreePosition(void)
{
    if (m_playlistTree)
    {
        QString route = m_playlistTree->GetCurrentNode()->getRouteByString().join("\n");
        gCoreContext->SaveSetting("MusicTreeLastActive", route);
    }
}

void PlaylistEditorView::smartPLChanged(const QString &category, const QString &name)
{
    reloadTree();

    // move to the smart playlist in tree
    QStringList route;
    route << "Root Music Node" << tr("Smart Playlists") << category << name;
    restoreTreePosition(route);
}

void PlaylistEditorView::deleteSmartPlaylist(bool ok)
{
    if (!ok)
        return;

    MythGenericTree *node = m_playlistTree->GetCurrentNode();
    if (node)
    {
        auto *mnode = dynamic_cast<MusicGenericTree*>(node);
        if (mnode)
        {
            if (mnode->getAction() == "smartplaylist")
            {
                QString category = mnode->getParent()->GetText();
                QString name = mnode->GetText();

                SmartPlaylistEditor::deleteSmartPlaylist(category, name);
                reloadTree();
            }
        }
    }
}

void PlaylistEditorView::deletePlaylist(bool ok)
{
    if (!ok)
        return;

    MythGenericTree *node = m_playlistTree->GetCurrentNode();
    if (node)
    {
        auto *mnode = dynamic_cast<MusicGenericTree*>(node);
        if (mnode)
        {
            if (mnode->getAction() == "playlist")
            {
                int id = mnode->getInt();

                gMusicData->m_all_playlists->deletePlaylist(id);
                m_playlistTree->RemoveCurrentItem(true);
            }
        }
    }
}
