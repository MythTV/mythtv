#include <iostream>
#include <cstdlib>

// qt
#include <QKeyEvent>

// MythTV
#include <libmythbase/mythdb.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// mythmusic
#include "musiccommon.h"
#include "musicdata.h"
#include "searchview.h"

SearchView::SearchView(MythScreenStack *parent, MythScreenType *parentScreen)
         :MusicCommon(parent, parentScreen,"searchview")
{
    m_currentView = MV_SEARCH;
}

bool SearchView::Create(void)
{
    // Load the theme for this screen
    bool err = LoadWindowFromXML("music-ui.xml", "searchview", this);

    if (!err)
        return false;

    // find common widgets available on any view
    err = CreateCommon();

    // find widgets specific to this view
    UIUtilE::Assign(this, m_fieldList,    "field_list",    &err);
    UIUtilE::Assign(this, m_criteriaEdit, "criteria_edit", &err);
    UIUtilW::Assign(this, m_matchesText,  "matches_text",  &err);
    UIUtilE::Assign(this, m_tracksList,   "tracks_list",   &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'searchview'");
        return false;
    }

    BuildFocusList();

    SetFocusWidget(m_criteriaEdit);

    new MythUIButtonListItem(m_fieldList, tr("All Fields"),
                                QVariant::fromValue(0));
    new MythUIButtonListItem(m_fieldList, tr("Artist"),
                                QVariant::fromValue(1));
    new MythUIButtonListItem(m_fieldList, tr("Album"),
                                QVariant::fromValue(2));
    new MythUIButtonListItem(m_fieldList, tr("Title"),
                                QVariant::fromValue(3));
    new MythUIButtonListItem(m_fieldList, tr("Genre"),
                                QVariant::fromValue(4));
    //new MythUIButtonListItem(m_fieldList, tr("Tags"),
    //                            QVariant::fromValue(5));

    connect(m_fieldList, &MythUIButtonList::itemSelected,
            this, &SearchView::fieldSelected);

    connect(m_tracksList, &MythUIButtonList::itemClicked,
            this, &SearchView::trackClicked);

    connect(m_tracksList, &MythUIButtonList::itemVisible,
            this, &SearchView::trackVisible);

    connect(m_criteriaEdit, &MythUITextEdit::valueChanged, this, &SearchView::criteriaChanged);

    updateTracksList();

    return true;
}

void SearchView::customEvent(QEvent *event)
{
    bool handled = false;

    if (event->type() == MusicPlayerEvent::kTrackRemovedEvent ||
        event->type() == MusicPlayerEvent::kTrackAddedEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackID = mpe->m_trackID;

        for (int x = 0; x < m_tracksList->GetCount(); x++)
        {
            MythUIButtonListItem *item = m_tracksList->GetItemAt(x);
            auto *mdata = item->GetData().value<MusicMetadata*>();
            if (mdata && (mdata->ID() == (MusicMetadata::IdType) trackID || trackID == -1))
            {
                if (gPlayer->getCurrentPlaylist() && gPlayer->getCurrentPlaylist()->checkTrack(mdata->ID()))
                    item->DisplayState("on", "selectedstate");
                else
                    item->DisplayState("off", "selectedstate");
            }
        }

        // call the default handler in MusicCommon so the playlist and UI is updated
        MusicCommon::customEvent(event);
        handled = true;

        if (m_playTrack == 1 || (m_playTrack == -1 && MusicPlayer::getPlayNow()))
        {
            if (event->type() == MusicPlayerEvent::kTrackAddedEvent)
            {
                // make the added track current and play it
                m_currentPlaylist->SetItemCurrent(m_currentPlaylist->GetCount() - 1);
                playlistItemClicked(m_currentPlaylist->GetItemCurrent());
            }
        }
        m_playTrack = -1;     // use PlayNow preference or menu option
    }
    else if (event->type() == MusicPlayerEvent::kAllTracksRemovedEvent)
    {
        for (int x = 0; x < m_tracksList->GetCount(); x++)
        {
            MythUIButtonListItem *item = m_tracksList->GetItemAt(x);
            if (item)
                item->DisplayState("off", "selectedstate");
        }
    }
    else if (event->type() == MusicPlayerEvent::kMetadataChangedEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        uint trackID = mpe->m_trackID;

        for (int x = 0; x < m_tracksList->GetCount(); x++)
        {
            MythUIButtonListItem *item = m_tracksList->GetItemAt(x);
            auto *mdata = item->GetData().value<MusicMetadata*>();
            if (mdata && mdata->ID() == trackID)
            {
                InfoMap metadataMap;
                mdata->toMap(metadataMap);
                item->SetTextFromMap(metadataMap);
            }
        }

//        if (trackID == gPlayer->getCurrentMetadata()->ID())
//            updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent *>(event);

        // make sure the user didn't ESCAPE out of the menu
        if ((dce == nullptr) || (dce->GetResult() < 0))
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        if (resultid == "searchviewmenu")
        {
            if (resulttext == tr("Add To Playlist") || resulttext == tr("Remove From Playlist"))
            {
                if (GetFocusWidget() == m_tracksList)
                {
                    MythUIButtonListItem *item = m_tracksList->GetItemCurrent();
                    if (item)
                    {
                        m_playTrack = 0;
                        trackClicked(item);
                    }
                }
            }
            else if (resulttext == tr("Play Now"))
            {
                if (GetFocusWidget() == m_tracksList)
                {
                    MythUIButtonListItem *item = m_tracksList->GetItemCurrent();
                    if (item)
                    {
                        m_playTrack = 1;
                        trackClicked(item);
                    }
                }
            }
            else if (resulttext == tr("Prefer Play Now"))
            {
                MusicPlayer::setPlayNow(true);
            }
            else if (resulttext == tr("Prefer Add Tracks"))
            {
                MusicPlayer::setPlayNow(false);
            }
            else if (resulttext == tr("Search List..."))
            {
                searchButtonList();
            }
        }
    }

    if (!handled)
        MusicCommon::customEvent(event);
}

bool SearchView::keyPressEvent(QKeyEvent *event)
{
    if (!m_moveTrackMode && GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "INFO" || action == "EDIT")
        {
            if (GetFocusWidget() == m_tracksList)
            {
                if (m_tracksList->GetItemCurrent())
                {
                    auto *mdata = m_tracksList->GetItemCurrent()->GetData().value<MusicMetadata*>();
                    if (mdata)
                    {
                        if (action == "INFO")
                            showTrackInfo(mdata);
                        else
                            editTrackInfo(mdata);
                    }
                }
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "PLAY")
        {
            if (GetFocusWidget() == m_tracksList)
            {
                MythUIButtonListItem *item = m_tracksList->GetItemCurrent();
                if (item)
                {
                    m_playTrack = 1;
                    trackClicked(item);
                }
            }
            else
            {
                handled = false;
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

void SearchView::ShowMenu(void)
{
    if (GetFocusWidget() == m_tracksList)
    {
        QString label = tr("Search Actions");

        auto *menu = new MythMenu(label, this, "searchviewmenu");

        MythUIButtonListItem *item = m_tracksList->GetItemCurrent();
        if (item)
        {
            auto *mdata = item->GetData().value<MusicMetadata*>();
            if (mdata)
            {
                if (gPlayer->getCurrentPlaylist() && gPlayer->getCurrentPlaylist()->checkTrack(mdata->ID()))
                    menu->AddItem(tr("Remove From Playlist"));
                else
                {
                    if (MusicPlayer::getPlayNow())
                    {
                        menu->AddItem(tr("Play Now"));
                        menu->AddItem(tr("Add To Playlist"));
                        menu->AddItem(tr("Prefer Add To Playlist"));
                    }
                    else
                    {
                        menu->AddItem(tr("Add To Playlist"));
                        menu->AddItem(tr("Play Now"));
                        menu->AddItem(tr("Prefer Play Now"));
                    }
                }
            }
        }

        if (GetFocusWidget() == m_tracksList || GetFocusWidget() == m_currentPlaylist)
            menu->AddItem(tr("Search List..."));

        menu->AddItem(tr("More Options"), nullptr, createSubMenu());

        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        auto *menuPopup = new MythDialogBox(menu, popupStack, "actionmenu");

        if (menuPopup->Create())
            popupStack->AddScreen(menuPopup);
        else
            delete menu;
    }
    else
    {
        MusicCommon::ShowMenu();
    }
}

void SearchView::fieldSelected([[maybe_unused]] MythUIButtonListItem *item)
{
    updateTracksList();
}

void SearchView::criteriaChanged(void)
{
    updateTracksList();
}

void SearchView::updateTracksList(void)
{
    m_tracksList->Reset();

    MythUIButtonListItem *item = m_fieldList->GetItemCurrent();

    if (!item)
        return;

    QString searchStr = m_criteriaEdit->GetText();
    int field = item->GetData().toInt();

    QString sql;
    MSqlQuery query(MSqlQuery::InitCon());

    if (searchStr.isEmpty())
    {
        sql = "SELECT song_id "
              "FROM music_songs ";

        query.prepare(sql);
    }
    else
    {
        switch(field)
        {
            case 1: // artist
            {
                sql = "SELECT song_id "
                      "FROM music_songs "
                      "LEFT JOIN music_artists ON "
                      "    music_songs.artist_id=music_artists.artist_id "
                      "WHERE music_artists.artist_name LIKE '%" + searchStr + "%' ";
                query.prepare(sql);
                break;
            }
            case 2: // album
            {
                sql = "SELECT song_id "
                      "FROM music_songs "
                      "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                      "WHERE music_albums.album_name LIKE '%" + searchStr + "%' ";
                query.prepare(sql);
                break;
            }
            case 3: // title
            {
                sql = "SELECT song_id "
                      "FROM music_songs "
                      "WHERE music_songs.name LIKE '%" + searchStr + "%' ";
                query.prepare(sql);
                break;
            }
            case 4: // genre
            {
                sql = "SELECT song_id "
                      "FROM music_songs "
                      "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
                      "WHERE music_genres.genre LIKE '%" + searchStr + "%' ";
                query.prepare(sql);
                break;
            }
            case 5: // tags
            {
                //TODO add tag query.  Remove fallthrough once added.
                [[fallthrough]];
            }
            case 0: // all fields
            default:
            {
                sql = "SELECT song_id "
                      "FROM music_songs "
                      "LEFT JOIN music_artists ON "
                      "    music_songs.artist_id=music_artists.artist_id "
                      "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                      "LEFT JOIN music_artists AS music_comp_artists ON "
                      "    music_albums.artist_id=music_comp_artists.artist_id "
                      "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
                      "WHERE music_songs.name LIKE '%" + searchStr + "%' "
                      "OR music_artists.artist_name LIKE '%" + searchStr + "%' "
                      "OR music_albums.album_name LIKE '%" + searchStr + "%' "
                      "OR music_genres.genre LIKE '%" + searchStr + "%' ";

                query.prepare(sql);
            }
        }
    }

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Search music database", query);
        return;
    }

    while (query.next())
    {
        int trackid = query.value(0).toInt();

        MusicMetadata *mdata = gMusicData->m_all_music->getMetadata(trackid);
        if (mdata)
        {
            auto *newitem = new MythUIButtonListItem(m_tracksList, "");
            newitem->SetData(QVariant::fromValue(mdata));
            InfoMap metadataMap;
            mdata->toMap(metadataMap);
            newitem->SetTextFromMap(metadataMap);

            if (gPlayer->getCurrentPlaylist() && gPlayer->getCurrentPlaylist()->checkTrack(mdata->ID()))
                newitem->DisplayState("on", "selectedstate");
            else
                newitem->DisplayState("off", "selectedstate");

            // TODO rating state etc
        }
    }

    trackVisible(m_tracksList->GetItemCurrent());

    if (m_matchesText)
        m_matchesText->SetText(QString("%1").arg(m_tracksList->GetCount()));
}

void SearchView::trackClicked(MythUIButtonListItem *item)
{
    if (!gPlayer->getCurrentPlaylist())
        return;

    auto *mdata = item->GetData().value<MusicMetadata*>();
    if (mdata)
    {
        if (gPlayer->getCurrentPlaylist()->checkTrack(mdata->ID()))
            gPlayer->getCurrentPlaylist()->removeTrack(mdata->ID());
        else
            gPlayer->getCurrentPlaylist()->addTrack(mdata->ID(), true);
    }
}

void SearchView::trackVisible(MythUIButtonListItem *item)
{

    if (!item)
        return;

    if (item->GetImageFilename().isEmpty())
    {
        auto *mdata = item->GetData().value<MusicMetadata*>();
        if (mdata)
        {
            QString artFile = mdata->getAlbumArtFile();
            if (artFile.isEmpty())
                item->SetImage("");
            else
                item->SetImage(mdata->getAlbumArtFile());
        }
        else
        {
            item->SetImage("");
        }
    }
}
