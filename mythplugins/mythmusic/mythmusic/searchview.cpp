#include <iostream>
#include <cstdlib>

// qt
#include <QKeyEvent>

// myth
#include <mythdialogbox.h>
#include <mythuitextedit.h>
#include <mythuibuttonlist.h>
#include <mythdb.h>

// mythmusic
#include "musicdata.h"
#include "musiccommon.h"
#include "searchview.h"

SearchView::SearchView(MythScreenStack *parent)
         :MusicCommon(parent, "searchview"),
            m_playTrack(false), m_fieldList(NULL), m_criteriaEdit(NULL),
            m_matchesText(NULL), m_tracksList(NULL)
{
    m_currentView = MV_SEARCH;
}

SearchView::~SearchView()
{
}

bool SearchView::Create(void)
{
    bool err = false;

    // Load the theme for this screen
    err = LoadWindowFromXML("music-ui.xml", "searchview", this);

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
                                qVariantFromValue(0));
    new MythUIButtonListItem(m_fieldList, tr("Artist"),
                                qVariantFromValue(1));
    new MythUIButtonListItem(m_fieldList, tr("Album"),
                                qVariantFromValue(2));
    new MythUIButtonListItem(m_fieldList, tr("Title"),
                                qVariantFromValue(3));
    new MythUIButtonListItem(m_fieldList, tr("Genre"),
                                qVariantFromValue(4));
    //new MythUIButtonListItem(m_fieldList, tr("Tags"),
    //                            qVariantFromValue(5));

    connect(m_fieldList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(fieldSelected(MythUIButtonListItem*)));

    connect(m_tracksList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(trackClicked(MythUIButtonListItem*)));

    connect(m_tracksList, SIGNAL(itemVisible(MythUIButtonListItem*)),
            this, SLOT(trackVisible(MythUIButtonListItem*)));

    connect(m_criteriaEdit, SIGNAL(valueChanged()), this, SLOT(criteriaChanged()));

    updateTracksList();

    return true;
}

void SearchView::customEvent(QEvent *event)
{
    bool handled = false;

    if (event->type() == MusicPlayerEvent::TrackRemovedEvent ||
        event->type() == MusicPlayerEvent::TrackAddedEvent)
    {
        MusicPlayerEvent *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackID = mpe->TrackID;

        for (int x = 0; x < m_tracksList->GetCount(); x++)
        {
            MythUIButtonListItem *item = m_tracksList->GetItemAt(x);
            MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
            if (mdata && (mdata->ID() == (MusicMetadata::IdType) trackID || trackID == -1))
            {
                if (gPlayer->getPlaylist()->checkTrack(mdata->ID()))
                    item->DisplayState("on", "selectedstate");
                else
                    item->DisplayState("off", "selectedstate");
            }
        }

        // call the default handler in MusicCommon so the playlist and UI is updated
        MusicCommon::customEvent(event);
        handled = true;

        if (m_playTrack)
        {
            m_playTrack = false;

            if (event->type() == MusicPlayerEvent::TrackAddedEvent)
            {
                // make the added track current and play it
                m_currentPlaylist->SetItemCurrent(m_currentPlaylist->GetCount() - 1);
                playlistItemClicked(m_currentPlaylist->GetItemCurrent());
            }
        }
    }
    else if (event->type() == MusicPlayerEvent::AllTracksRemovedEvent)
    {
        for (int x = 0; x < m_tracksList->GetCount(); x++)
        {
            MythUIButtonListItem *item = m_tracksList->GetItemAt(x);
            if (item)
                item->DisplayState("off", "selectedstate");
        }
    }
    else if (event->type() == MusicPlayerEvent::MetadataChangedEvent)
    {
        MusicPlayerEvent *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        uint trackID = mpe->TrackID;

        for (int x = 0; x < m_tracksList->GetCount(); x++)
        {
            MythUIButtonListItem *item = m_tracksList->GetItemAt(x);
            MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
            if (mdata && mdata->ID() == trackID)
            {
                MetadataMap metadataMap;
                mdata->toMap(metadataMap);
                item->SetTextFromMap(metadataMap);
            }
        }

//        if (trackID == gPlayer->getCurrentMetadata()->ID())
//            updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = static_cast<DialogCompletionEvent *>(event);

        // make sure the user didn't ESCAPE out of the menu
        if (dce->GetResult() < 0)
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
                        m_playTrack = false;
                        trackClicked(item);
                    }
                }
            }
            else if (resulttext == tr("Add To Playlist And Play"))
            {
                if (GetFocusWidget() == m_tracksList)
                {
                    MythUIButtonListItem *item = m_tracksList->GetItemCurrent();
                    if (item)
                    {
                        m_playTrack = true;
                        trackClicked(item);
                    }
                }
            }
            else if (resulttext == tr("Search List..."))
                searchButtonList();
        }
    }

    if (!handled)
        MusicCommon::customEvent(event);
}

bool SearchView::keyPressEvent(QKeyEvent *event)
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

        if (action == "INFO" || action == "EDIT")
        {
            if (GetFocusWidget() == m_tracksList)
            {
                if (m_tracksList->GetItemCurrent())
                {
                    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (m_tracksList->GetItemCurrent()->GetData());
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
                handled = false;
        }
        else if (action == "PLAY")
        {
            if (GetFocusWidget() == m_tracksList)
            {
                MythUIButtonListItem *item = m_tracksList->GetItemCurrent();
                if (item)
                {
                    m_playTrack = true;
                    trackClicked(item);
                }
            }
            else
                handled = false;
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

void SearchView::ShowMenu(void)
{
    if (GetFocusWidget() == m_tracksList)
    {
        QString label = tr("Search Actions");

        MythMenu *menu = new MythMenu(label, this, "searchviewmenu");

        MythUIButtonListItem *item = m_tracksList->GetItemCurrent();
        if (item)
        {
            MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
            if (mdata)
            {
                if (gPlayer->getPlaylist()->checkTrack(mdata->ID()))
                    menu->AddItem(tr("Remove From Playlist"));
                else
                {
                    menu->AddItem(tr("Add To Playlist"));
                    menu->AddItem(tr("Add To Playlist And Play"));
                }
            }
        }

        if (GetFocusWidget() == m_tracksList || GetFocusWidget() == m_currentPlaylist)
            menu->AddItem(tr("Search List..."));

        menu->AddItem(tr("More Options"), NULL, createMainMenu());

        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack, "actionmenu");

        if (menuPopup->Create())
            popupStack->AddScreen(menuPopup);
        else
            delete menu;
    }
    else
        MusicCommon::ShowMenu();
}

void SearchView::fieldSelected(MythUIButtonListItem *item)
{
    (void) item;
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
                //TODO add tag query
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

        MusicMetadata *mdata = gMusicData->all_music->getMetadata(trackid);
        if (mdata)
        {
            MythUIButtonListItem *newitem = new MythUIButtonListItem(m_tracksList, "");
            newitem->SetData(qVariantFromValue(mdata));
            MetadataMap metadataMap;
            mdata->toMap(metadataMap);
            newitem->SetTextFromMap(metadataMap);

            if (gPlayer->getPlaylist()->checkTrack(mdata->ID()))
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
    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
    if (mdata)
    {
        if (gPlayer->getPlaylist()->checkTrack(mdata->ID()))
            gPlayer->getPlaylist()->removeTrack(mdata->ID());
        else
            gPlayer->getPlaylist()->addTrack(mdata->ID(), true);
    }
}

void SearchView::trackVisible(MythUIButtonListItem *item)
{

    if (!item)
        return;

    if (item->GetImageFilename().isEmpty())
    {
        MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
        if (mdata)
        {
            QString artFile = mdata->getAlbumArtFile();
            if (artFile.isEmpty())
                item->SetImage("mm_nothumb.png");
            else
                item->SetImage(mdata->getAlbumArtFile());
        }
        else
            item->SetImage("mm_nothumb.png");
    }
}
