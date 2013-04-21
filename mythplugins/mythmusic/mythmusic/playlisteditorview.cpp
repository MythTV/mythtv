#include <iostream>
#include <cstdlib>

// qt
#include <QKeyEvent>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythmainwindow.h>
#include <mythuibuttontree.h>
#include <mythuitext.h>
#include <mythuiutils.h>
#include <mythdialogbox.h>
#include <mythdirs.h>

// mythmusic
#include "playlistcontainer.h"
#include "musiccommon.h"
#include "playlisteditorview.h"
#include "smartplaylist.h"
#include "musicutils.h"

MusicGenericTree::MusicGenericTree(MusicGenericTree *parent,
                                   const QString &name, const QString &action,
                                   MythUIButtonListItem::CheckState check,
                                   bool showArrow)
                 : MythGenericTree(name)
{
    m_check = check;
    m_action = action;
    m_showArrow = showArrow;

    SetSortText(name.toLower());

    if (!action.isEmpty() && !action.isNull())
        setSelectable(true);

    if (parent)
    {
        parent->addNode(this);
        parent->setDrawArrow(true);
    }
}

MusicGenericTree::~MusicGenericTree(void)
{
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
    MusicButtonItem *item = new MusicButtonItem(list, GetText());
    item->SetData(qVariantFromValue((MythGenericTree*) this));

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

PlaylistEditorView::PlaylistEditorView(MythScreenStack *parent, const QString &layout, bool restorePosition)
         :MusicCommon(parent, "playlisteditorview"),
            m_layout(layout), m_restorePosition(restorePosition),
            m_rootNode(NULL), m_playlistTree(NULL), m_breadcrumbsText(NULL),
            m_positionText(NULL)
{
    gCoreContext->SaveSetting("MusicPlaylistEditorView", layout);
}

PlaylistEditorView::~PlaylistEditorView()
{
    saveTreePosition();

    for (int x = 0; x < m_deleteList.count(); x++)
        delete m_deleteList.at(x);
    m_deleteList.clear();

    if (m_rootNode)
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

    if (m_restorePosition)
    {
        QStringList route = gCoreContext->GetSetting("MusicTreeLastActive", "").split("\n");
        restoreTreePosition(route);
    }

    connect(m_playlistTree, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(treeItemClicked(MythUIButtonListItem*)));
    connect(m_playlistTree, SIGNAL(nodeChanged(MythGenericTree*)),
            this, SLOT(treeNodeChanged(MythGenericTree*)));
    connect(m_playlistTree, SIGNAL(itemVisible(MythUIButtonListItem*)),
            this, SLOT(treeItemVisible(MythUIButtonListItem*)));

    BuildFocusList();

    return true;
}

void PlaylistEditorView::customEvent(QEvent *event)
{
    if (event->type() == MusicPlayerEvent::MetadataChangedEvent)
    {
        // TODO: this could be more efficient
        reloadTree();
    }
    else if (event->type() == MusicPlayerEvent::AlbumArtChangedEvent)
    {
        // TODO: this could be more efficient
        reloadTree();
    }
    else if (event->type() == MusicPlayerEvent::TrackRemovedEvent)
    {
        updateSelectedTracks();
    }
    else if (event->type() == MusicPlayerEvent::TrackAddedEvent)
    {
        updateSelectedTracks();
    }
    else if (event->type() == MusicPlayerEvent::AllTracksRemovedEvent)
    {
        updateSelectedTracks();
    }
    else if (event->type() == MusicPlayerEvent::PlaylistChangedEvent)
    {
        //TODO should just update the relevent playlist here
        reloadTree();
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = static_cast<DialogCompletionEvent*>(event);

        // make sure the user didn't ESCAPE out of the menu
        if (dce->GetResult() < 0)
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

            MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);
            if (!mnode)
                return;

            if (resulttext == tr("New Smart Playlist"))
            {
                QString category;
                if (mnode->getAction() == "smartplaylistcategory")
                    category = mnode->GetText();

                MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                SmartPlaylistEditor* editor = new SmartPlaylistEditor(mainStack);

                if (!editor->Create())
                {
                    delete editor;
                    return;
                }

                editor->newSmartPlaylist(category);

                connect(editor, SIGNAL(smartPLChanged(const QString&, const QString&)),
                        this, SLOT(smartPLChanged(QString, QString)));

                mainStack->AddScreen(editor);
            }
            else if (resulttext == tr("Remove Smart Playlist"))
            {
                QString category = mnode->getParent()->GetText();
                QString name = mnode->GetText();

                ShowOkPopup(tr("Are you sure you want to delete this Smart Playlist?\n"
                               "Category: %1 - Name: %2").arg(category).arg(name),
                            this, SLOT(deleteSmartPlaylist(bool)), true);
            }
            else if (resulttext == tr("Edit Smart Playlist"))
            {
                QString category = mnode->getParent()->GetText();
                QString name = mnode->GetText();

                MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                SmartPlaylistEditor* editor = new SmartPlaylistEditor(mainStack);

                if (!editor->Create())
                {
                    delete editor;
                    return;
                }

                editor->editSmartPlaylist(category, name);

                connect(editor, SIGNAL(smartPLChanged(const QString&, const QString&)),
                        this, SLOT(smartPLChanged(QString, QString)));

                mainStack->AddScreen(editor);
            }
            else if (resulttext == tr("Replace Tracks"))
            {
                m_playlistOptions.playPLOption = PL_CURRENT;
                m_playlistOptions.insertPLOption = PL_REPLACE;
                doUpdatePlaylist(false);
            }
            else if (resulttext == tr("Add Tracks"))
            {
                m_playlistOptions.playPLOption = PL_CURRENT;
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                doUpdatePlaylist(false);
            }
        }
        else if (resultid == "playlistmenu")
        {
            if (GetFocusWidget() != m_playlistTree)
                return;

            MythGenericTree *node = m_playlistTree->GetCurrentNode();
            if (!node)
                return;

            MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);
            if (!mnode)
                return;

            if (resulttext == tr("Remove Playlist"))
            {
                QString name = mnode->GetText();

                ShowOkPopup(tr("Are you sure you want to delete this Playlist?\n"
                               "Name: %1").arg(name),
                            this, SLOT(deletePlaylist(bool)), true);
            }
            else if (resulttext == tr("Replace Tracks"))
            {
                m_playlistOptions.playPLOption = PL_CURRENT;
                m_playlistOptions.insertPLOption = PL_REPLACE;
                doUpdatePlaylist(false);
            }
            else if (resulttext == tr("Add Tracks"))
            {
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                doUpdatePlaylist(false);
            }
        }
    }

    MusicCommon::customEvent(event);
}

bool PlaylistEditorView::keyPressEvent(QKeyEvent *event)
{
    if (!m_moveTrackMode && GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if ((action == "EDIT" || action == "INFO") && GetFocusWidget() == m_playlistTree)
        {
            handled = false;

            MythGenericTree *node = m_playlistTree->GetCurrentNode();
            if (node)
            {
                MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);
                if (mnode)
                {
                    if (mnode->getAction() == "smartplaylist" && action == "EDIT")
                    {
                        QString category = mnode->getParent()->GetText();
                        QString name = mnode->GetText();

                        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                        SmartPlaylistEditor* editor = new SmartPlaylistEditor(mainStack);

                        if (!editor->Create())
                        {
                            delete editor;
                            return true;
                        }

                        editor->editSmartPlaylist(category, name);
                        connect(editor, SIGNAL(smartPLChanged(const QString&, const QString&)),
                                this, SLOT(smartPLChanged(QString, QString)));

                        mainStack->AddScreen(editor);

                        handled = true;
                    }
                    else if (mnode->getAction() == "smartplaylistcategory" && action == "EDIT")
                    {
                        QString category = mnode->GetText();

                        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                        SmartPlaylistEditor* editor = new SmartPlaylistEditor(mainStack);

                        if (!editor->Create())
                        {
                            delete editor;
                            return true;
                        }

                        editor->newSmartPlaylist(category);

                        connect(editor, SIGNAL(smartPLChanged(const QString&, const QString&)),
                                this, SLOT(smartPLChanged(QString, QString)));

                        mainStack->AddScreen(editor);

                        handled = true;
                    }
                    else if (mnode->getAction() == "trackid")
                    {
                        Metadata *mdata = gMusicData->all_music->getMetadata(mnode->getInt());
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
                MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);
                if (mnode)
                {
                    if (mnode->getAction() == "smartplaylist")
                    {
                        QString category = mnode->getParent()->GetText();
                        QString name = mnode->GetText();

                        ShowOkPopup(tr("Are you sure you want to delete this Smart Playlist?\n"
                                       "Category: %1 - Name: %2").arg(category).arg(name),
                                    this, SLOT(deleteSmartPlaylist(bool)), true);
                        handled = true;
                    }
                    else if (mnode->getAction() == "playlist")
                    {
                        QString name = mnode->GetText();

                        ShowOkPopup(tr("Are you sure you want to delete this Playlist?\n"
                                       "Name: %1").arg(name),
                                    this, SLOT(deletePlaylist(bool)), true);
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
                 MythGenericTree *node = qVariantValue<MythGenericTree*> (item->GetData());
                 MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);

                 if (mnode)
                 {
                     updateSonglist(mnode);

                     if (m_songList.count() > 0)
                     {
                         m_playlistOptions.playPLOption = PL_FIRST;
                         m_playlistOptions.insertPLOption = PL_REPLACE;
                         doUpdatePlaylist(true);
                     }
                     else
                     {
                         handled = false;
                     }
                 }
             }
        }
        else
            handled = false;
    }

    if (!handled && MusicCommon::keyPressEvent(event))
        handled = true;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void PlaylistEditorView::updateSonglist(MusicGenericTree *node)
{
    m_songList.clear();

    if (node->getAction() == "playlists" ||
        node->getAction() == "smartplaylists" ||
        node->getAction() == "smartplaylistcategory")
    {
    }
    else if (node->getAction() == "trackid")
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
        MusicGenericTree *allTracksNode = dynamic_cast<MusicGenericTree*>(node->getParent()->getChildAt(0));
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
        MusicGenericTree *allTracksNode = dynamic_cast<MusicGenericTree*>(node->getChildAt(0));
        if (allTracksNode)
        {
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
        Playlist *playlist = gMusicData->all_playlists->getPlaylist(playlistID);

        if (playlist)
        {
            SongList songlist = playlist->getSongs();

            for (int x = 0; x < songlist.count(); x++)
            {
                m_songList.append(songlist.at(x)->ID());
            }
        }
    }
    else
    {
        // fall back to getting the tracks from the MetadataPtrList
        MetadataPtrList *tracks = qVariantValue<MetadataPtrList*> (node->GetData());
        for (int x = 0; x < tracks->count(); x++)
        {
            Metadata *mdata = tracks->at(x);
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

        MythMenu *menu = NULL;
        MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(m_playlistTree->GetCurrentNode());

        if (!mnode)
        {
            MusicCommon::ShowMenu();
            return;
        }

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
        else if (mnode->getAction() == "trackid")
        {
        }
        else
        {
            menu = createPlaylistOptionsMenu();
        }

        updateSonglist(mnode);

        if (menu)
        {
            menu->AddItem(tr("More Options"), NULL, createMainMenu());

            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

            MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack, "actionmenu");

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
    MythMenu *menu = NULL;

    if (GetFocusWidget() == m_playlistTree)
    {
        MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(m_playlistTree->GetCurrentNode());

        if (!mnode)
            return NULL;

        if (mnode->getAction() == "playlist")
        {
            menu = new MythMenu(tr("Playlist Actions"), this, "playlistmenu");
            menu->AddItem(tr("Replace Tracks"));
            menu->AddItem(tr("Add Tracks"));
            menu->AddItem(tr("Remove Playlist"));
        }
    }

    return menu;
}

MythMenu* PlaylistEditorView::createSmartPlaylistMenu(void)
{
    MythMenu *menu = NULL;

    if (GetFocusWidget() == m_playlistTree)
    {
        MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(m_playlistTree->GetCurrentNode());

        if (!mnode)
            return NULL;

        if (mnode->getAction() == "smartplaylists" || mnode->getAction() == "smartplaylistcategory")
        {
            QString label = tr("Smart Playlist Actions");

            menu = new MythMenu(label, this, "smartplaylistmenu");

            menu->AddItem(tr("New Smart Playlist"));
        }
        else if (mnode->getAction() == "smartplaylist")
        {
            menu = new MythMenu(tr("Smart Playlist Actions"), this, "smartplaylistmenu");

            menu->AddItem(tr("Replace Tracks"));
            menu->AddItem(tr("Add Tracks"));

            menu->AddItem(tr("Edit Smart Playlist"));
            menu->AddItem(tr("New Smart Playlist"));
            menu->AddItem(tr("Remove Smart Playlist"));
        }
    }

    return menu;
}

void PlaylistEditorView::createRootNode(void )
{
    if (!m_rootNode)
        m_rootNode = new MusicGenericTree(NULL, "Root Music Node");

    MusicGenericTree *node = new MusicGenericTree(m_rootNode, tr("All Tracks"), "all tracks");
    node->setDrawArrow(true);
    node->SetData(qVariantFromValue(gMusicData->all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Albums"), "albums");
    node->setDrawArrow(true);
    node->SetData(qVariantFromValue(gMusicData->all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Artists"), "artists");
    node->setDrawArrow(true);
    node->SetData(qVariantFromValue(gMusicData->all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Genres"), "genres");
    node->setDrawArrow(true);
    node->SetData(qVariantFromValue(gMusicData->all_music->getAllMetadata()));
#if 0
    node = new MusicGenericTree(m_rootNode, tr("Tags"), "tags");
    node->setDrawArrow(true);
    node->SetData(qVariantFromValue(gMusicData->all_music->getAllMetadata()));
#endif
    node = new MusicGenericTree(m_rootNode, tr("Ratings"), "ratings");
    node->setDrawArrow(true);
    node->SetData(qVariantFromValue(gMusicData->all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Years"), "years");
    node->setDrawArrow(true);
    node->SetData(qVariantFromValue(gMusicData->all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Compilations"), "compilations");
    node->setDrawArrow(true);

    MetadataPtrList *alltracks = gMusicData->all_music->getAllMetadata();
    MetadataPtrList *compTracks = new MetadataPtrList;
    m_deleteList.append(compTracks);

    for (int x = 0; x < alltracks->count(); x++)
    {
        Metadata *mdata = alltracks->at(x);
        if (mdata)
        {
            if (mdata->Compilation())
                compTracks->append(mdata);
        }
    }
    node->SetData(qVariantFromValue(compTracks));

    node = new MusicGenericTree(m_rootNode, tr("Directory"), "directory");
    node->setDrawArrow(true);
    node->SetData(qVariantFromValue(gMusicData->all_music->getAllMetadata()));

    node = new MusicGenericTree(m_rootNode, tr("Playlists"), "playlists");
    node->setDrawArrow(true);

    node = new MusicGenericTree(m_rootNode, tr("Smart Playlists"), "smartplaylists");
    node->setDrawArrow(true);
}

void PlaylistEditorView::treeItemClicked(MythUIButtonListItem *item)
{
    MythGenericTree *node = qVariantValue<MythGenericTree*> (item->GetData());
    MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);

    if (!mnode)
        return;

    if (mnode->getAction() == "trackid")
    {
        if (gPlayer->getPlaylist()->checkTrack(mnode->getInt()))
        {
            // remove track from the current playlist
            gPlayer->removeTrack(mnode->getInt());
            mnode->setCheck(MythUIButtonListItem::NotChecked);
        }
        else
        {
            // add track to the current playlist
            gPlayer->addTrack(mnode->getInt(), true);
            mnode->setCheck(MythUIButtonListItem::FullChecked);
        }
    }
    else
        ShowMenu();
}


void PlaylistEditorView::treeItemVisible(MythUIButtonListItem *item)
{
    MythGenericTree *node = qVariantValue<MythGenericTree*> (item->GetData());;
    MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);

    if (!mnode)
        return;

    if (item->GetImageFilename().isEmpty())
    {
        QString artFile;

        if (mnode->getAction() == "trackid")
        {
            Metadata *mdata = gMusicData->all_music->getMetadata(mnode->getInt());
            if (mdata)
                artFile = mdata->getAlbumArtFile();
        }
        else if (mnode->getAction() == "album")
        {
            // hunt for a coverart image for the album
            MetadataPtrList *tracks = qVariantValue<MetadataPtrList*> (node->GetData());
            for (int x = 0; x < tracks->count(); x++)
            {
                Metadata *mdata = tracks->at(x);
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
        else
        {
            artFile = findIcon(mnode->getAction(), mnode->GetText().toLower());
        }

        QString state = "default";

        if (mnode->getAction() == "all tracks")
        {
            state = "alltracks";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "genres")
        {
            state = "genres";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "albums")
        {
            state = "albums";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "artists")
        {
            state = "artists";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "compartists")
        {
            state = "compartists";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "ratings")
        {
            state = "ratings";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "years")
        {
            state = "years";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "compilations")
        {
            state = "compilations";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "directory")
        {
            state = "directory";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "playlists")
        {
            state = "playlists";
            artFile="blank.png";
        }
        else if (mnode->getAction() == "smartplaylists")
        {
            state = "smartplaylists";
            artFile="blank.png";
        }

        item->DisplayState(state, "nodetype");

        if (artFile.isEmpty())
            item->SetImage("");
        else
            item->SetImage(artFile);
    }
}

void PlaylistEditorView::treeNodeChanged(MythGenericTree *node)
{
    MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);
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
    else
        filterTracks(mnode);
}

void PlaylistEditorView::filterTracks(MusicGenericTree *node)
{
    MetadataPtrList *tracks = qVariantValue<MetadataPtrList*> (node->GetData());

    if (!tracks)
        return;

    if (node->getAction() == "all tracks")
    {
        QMap<QString, int> map;
        QStringList list;
        bool isAlbum = false;
        MusicGenericTree *parentNode = dynamic_cast<MusicGenericTree*>(node->getParent());

        if (parentNode)
            isAlbum = parentNode->getAction() == "album";

        for (int x = 0; x < tracks->count(); x++)
        {
            Metadata *mdata = tracks->at(x);
            if (mdata)
            {
                QString key = mdata->Title();

                // Add the track number if an album is selected
                if (isAlbum && mdata->Track() > 0)
                {
                    key.prepend(QString::number(mdata->Track()) + " - ");
                    if (mdata->Track() < 10)
                        key.prepend("0");
                }

                map.insertMulti(key, mdata->ID());
            }
        }

        QMap<QString, int>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            MusicGenericTree *newnode = new MusicGenericTree(node, i.key(), "trackid");
            newnode->setInt(i.value());
            newnode->setDrawArrow(false);
            bool hasTrack = gPlayer->getPlaylist()->checkTrack(newnode->getInt());
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
            Metadata *mdata = tracks->at(x);
            if (mdata)
            {
                if (map.contains(mdata->Artist()))
                {
                    MetadataPtrList *filteredTracks = map.value(mdata->Artist());
                    filteredTracks->append(mdata);
                }
                else
                {
                    MetadataPtrList *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(mdata->Artist(), filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            MusicGenericTree *newnode = new MusicGenericTree(node, i.key(), "artist");
            newnode->SetData(qVariantFromValue(i.value()));
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "compartists")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            Metadata *mdata = tracks->at(x);
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
                        MetadataPtrList *filteredTracks = new MetadataPtrList;
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
            MusicGenericTree *newnode = new MusicGenericTree(node, i.key(), "compartist");
            newnode->SetData(qVariantFromValue(i.value()));
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "albums")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            Metadata *mdata = tracks->at(x);
            if (mdata)
            {
                if (map.contains(mdata->Album()))
                {
                    MetadataPtrList *filteredTracks = map.value(mdata->Album());
                    filteredTracks->append(mdata);
                }
                else
                {
                    MetadataPtrList *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(mdata->Album(), filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            MusicGenericTree *newnode = new MusicGenericTree(node, i.key(), "album");
            newnode->SetData(qVariantFromValue(i.value()));
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "genres")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            Metadata *mdata = tracks->at(x);
            if (mdata)
            {
                if (map.contains(mdata->Genre()))
                {
                    MetadataPtrList *filteredTracks = map.value(mdata->Genre());
                    filteredTracks->append(mdata);
                }
                else
                {
                    MetadataPtrList *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(mdata->Genre(), filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            MusicGenericTree *newnode = new MusicGenericTree(node, i.key(), "genre");
            newnode->SetData(qVariantFromValue(i.value()));
            ++i;
        }

        node->sortByString(); // Case-insensitive sort
    }
    else if (node->getAction() == "ratings")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            Metadata *mdata = tracks->at(x);
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
                    MetadataPtrList *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(ratingStr, filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            MusicGenericTree *newnode = new MusicGenericTree(node, i.key(), "rating");
            newnode->SetData(qVariantFromValue(i.value()));
            ++i;
        }
    }
    else if (node->getAction() == "years")
    {
        QMap<QString, MetadataPtrList*> map;

        for (int x = 0; x < tracks->count(); x++)
        {
            Metadata *mdata = tracks->at(x);
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
                    MetadataPtrList *filteredTracks = new MetadataPtrList;
                    m_deleteList.append(filteredTracks);
                    filteredTracks->append(mdata);
                    map.insert(yearStr, filteredTracks);
                }
            }
        }

        QMap<QString, MetadataPtrList*>::const_iterator i = map.constBegin();
        while (i != map.constEnd())
        {
            MusicGenericTree *newnode = new MusicGenericTree(node, i.key(), "year");
            newnode->SetData(qVariantFromValue(i.value()));
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
            climber = (MusicGenericTree *) climber->getParent();
        }

        // remove the top two nodes
        QString top2 = "Root Music Node/" + tr("Directory") + '/';
        if (dir.startsWith(top2))
            dir = dir.mid(top2.length());

        for (int x = 0; x < tracks->count(); x++)
        {
            Metadata *mdata = tracks->at(x);
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
                    MetadataPtrList *filteredTracks = new MetadataPtrList;
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
                MusicGenericTree *newnode = new MusicGenericTree(node, i.key(), "directory");
                newnode->SetData(qVariantFromValue(i.value()));
            }
            ++i;
        }

        // now add tracks
        i = map.constBegin();
        while (i != map.constEnd())
        {
            if (i.key().startsWith("[TRACK]"))
            {
                MusicGenericTree *newnode = new MusicGenericTree(node, i.key().mid(7), "trackid");
                newnode->setInt(i.value()->at(0)->ID());
                newnode->setDrawArrow(false);
                bool hasTrack = gPlayer->getPlaylist()->checkTrack(newnode->getInt());
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
            climber = (MusicGenericTree *) climber->getParent();
        }

        MusicGenericTree *newnode = new MusicGenericTree(node, tr("All Tracks"), "all tracks");
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
            do
            {
                if (mnode->getAction() == "compilations")
                {
                    showCompArtists = true;
                    break;
                }

                mnode = (MusicGenericTree *) mnode->getParent();

            } while (mnode);

            // only show the Comp. Artist if it differs from the Artist
            bool found = false;
            MetadataPtrList *tracks = qVariantValue<MetadataPtrList*> (node->GetData());
            for (int x = 0; x < tracks->count(); x++)
            {
                Metadata *mdata = tracks->at(x);
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
            while (query.next())
            {
                MusicGenericTree *newnode =
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
            while (query.next())
            {
                MusicGenericTree *newnode =
                    new MusicGenericTree(node, query.value(1).toString(), "smartplaylist");
                newnode->setInt(query.value(0).toInt());
            }
        }
    }
    else
        MythDB::DBError("Load smartplaylist names", query);
}

void PlaylistEditorView::getSmartPlaylistTracks(MusicGenericTree *node, int playlistID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // find smartplaylist
    QString matchType;
    QString orderBy;
    int limitTo;

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
                whereClause += matchType + getCriteriaSQL(fieldName,
                                           operatorName, value1, value2);
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
        MusicGenericTree *newnode =
                new MusicGenericTree(node, query.value(1).toString(), "trackid");
        newnode->setInt(query.value(0).toInt());
        newnode->setDrawArrow(false);
        bool hasTrack = gPlayer->getPlaylist()->checkTrack(newnode->getInt());
        newnode->setCheck( hasTrack ? MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
    }

    // check we found some tracks if not add something to let the user know
    if (node->childCount() == 0)
    {
        MusicGenericTree *newnode =
                new MusicGenericTree(node, tr("** No matching tracks **"), "error");
        newnode->setDrawArrow(false);
    }
}

void PlaylistEditorView::getPlaylists(MusicGenericTree *node)
{
    QList<Playlist*> *playlists = gMusicData->all_playlists->getPlaylists();

    for (int x =0; x < playlists->count(); x++)
    {
        Playlist *playlist = playlists->at(x);
        MusicGenericTree *newnode =
                new MusicGenericTree(node, playlist->getName(), "playlist");
        newnode->setInt(playlist->getID());
    }
}

void PlaylistEditorView::getPlaylistTracks(MusicGenericTree *node, int playlistID)
{
    Playlist *playlist = gMusicData->all_playlists->getPlaylist(playlistID);
    QList<Metadata*> songs = playlist->getSongs();

    for (int x = 0; x < songs.count(); x++)
    {
        Metadata *mdata = songs.at(x);
        if (mdata)
        {
            MusicGenericTree *newnode = new MusicGenericTree(node, mdata->Title(), "trackid");
            newnode->setInt(mdata->ID());
            newnode->setDrawArrow(false);
            bool hasTrack = gPlayer->getPlaylist()->checkTrack(mdata->ID());
            newnode->setCheck(hasTrack ? MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
        }
    }

    // check we found some tracks if not add something to let the user know
    if (node->childCount() == 0)
    {
        MusicGenericTree *newnode =
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
        MusicGenericTree *mnode =  dynamic_cast<MusicGenericTree*>(node->getChildAt(x));
        if (mnode)
        {
            if (mnode->getAction() == "trackid")
            {
                bool hasTrack = gPlayer->getPlaylist()->checkTrack(mnode->getInt());
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
        MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);
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
        MusicGenericTree *mnode = dynamic_cast<MusicGenericTree*>(node);
        if (mnode)
        {
            if (mnode->getAction() == "playlist")
            {
                int id = mnode->getInt();

                gMusicData->all_playlists->deletePlaylist(id);
                m_playlistTree->RemoveCurrentItem(true);
            }
        }
    }
}
