// C++ headers
#include <chrono>
#include <cstdlib>
#include <iostream>

// qt
#include <QDomDocument>
#include <QKeyEvent>
#include <QThread>

// MythTV
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythdownloadmanager.h>
#include <libmythbase/mythlogging.h>
#include <libmythmetadata/musicutils.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuiutils.h>

// mythmusic
#include "musiccommon.h"
#include "musicdata.h"
#include "musicplayer.h"
#include "streamview.h"

StreamView::StreamView(MythScreenStack *parent, MythScreenType *parentScreen)
    : MusicCommon(parent, parentScreen, "streamview")
{
    m_currentView = MV_RADIO;
}

bool StreamView::Create(void)
{
    // Load the theme for this screen
    bool err = LoadWindowFromXML("stream-ui.xml", "streamview", this);

    if (!err)
        return false;

    // find common widgets available on any view
    err = CreateCommon();

    // find widgets specific to this view
    UIUtilE::Assign(this, m_streamList,     "streamlist", &err);
    UIUtilW::Assign(this, m_bufferStatus,   "bufferstatus", &err);
    UIUtilW::Assign(this, m_bufferProgress, "bufferprogress", &err);
    UIUtilW::Assign(this, m_noStreams,      "nostreams", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'streamview'");
        return false;
    }

    connect(m_streamList, &MythUIButtonList::itemClicked,
            this, &StreamView::streamItemClicked);
    connect(m_streamList, &MythUIButtonList::itemVisible,
            this, &StreamView::streamItemVisible);

    gPlayer->setPlayMode(MusicPlayer::PLAYMODE_RADIO);

    updateStreamList();
    updateUIPlayedList();

    BuildFocusList();

    return true;
}

void StreamView::ShowMenu(void)
{
    auto *menu = new MythMenu(tr("Stream Actions"), this, "mainmenu");
    menu->AddItemV(MusicCommon::tr("Fullscreen Visualizer"), QVariant::fromValue((int)MV_VISUALIZER));
    menu->AddItem(tr("Add Stream"));

    if (m_streamList->GetItemCurrent())
    {
        menu->AddItem(tr("Edit Stream"));
        menu->AddItem(tr("Remove Stream"));
    }

    menu->AddItemV(MusicCommon::tr("Lyrics"), QVariant::fromValue((int)MV_LYRICS));

    menu->AddItem(tr("More Options"), nullptr, createSubMenu());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(menu, popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menu;
}

void StreamView::customEvent(QEvent *event)
{
    bool handled = true;

    if (event->type() == MusicPlayerEvent::kPlayedTracksChangedEvent)
    {
        if (!gPlayer->getPlayedTracksList().isEmpty())
            updateTrackInfo(gPlayer->getCurrentMetadata());

        // add the new track to the list
        if (m_playedTracksList && !gPlayer->getPlayedTracksList().isEmpty())
        {
            MusicMetadata *mdata = gPlayer->getPlayedTracksList().last();

            auto *item = new MythUIButtonListItem(m_playedTracksList, "",
                                                  QVariant::fromValue(mdata), 0);

            InfoMap metadataMap;
            mdata->toMap(metadataMap);
            item->SetTextFromMap(metadataMap);
            item->SetFontState("normal");
            item->DisplayState("default", "playstate");
            item->SetImage(mdata->getAlbumArtFile());

            m_playedTracksList->SetItemCurrent(item);
        }
    }
    else if (event->type() == MusicPlayerEvent::kTrackChangeEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        if (m_streamList)
        {
            MythUIButtonListItem *item = m_streamList->GetItemByData(QVariant::fromValue(m_currStream));
            if (item)
            {
                item->SetFontState("normal");
                item->DisplayState("default", "playstate");
            }

            if (m_currStream != gPlayer->getCurrentMetadata())
            {
                m_lastStream = m_currStream;
                m_currStream = gPlayer->getCurrentMetadata();
            }

            item = m_streamList->GetItemByData(QVariant::fromValue(m_currStream));
            if (item)
            {
                if (gPlayer->isPlaying())
                {
                    item->SetFontState("running");
                    item->DisplayState("playing", "playstate");
                }
                else if (gPlayer->isPaused())
                {
                    item->SetFontState("idle");
                    item->DisplayState("paused", "playstate");
                }
                else
                {
                    item->SetFontState("normal");
                    item->DisplayState("stopped", "playstate");
                }
            }
        }

        updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == OutputEvent::kPlaying)
    {
        if (gPlayer->isPlaying())
        {
            if (m_streamList)
            {
                MythUIButtonListItem *item = m_streamList->GetItemByData(QVariant::fromValue(m_currStream));
                if (item)
                {
                    item->SetFontState("running");
                    item->DisplayState("playing", "playstate");
                }
            }
        }

        // pass it on to the default handler in MusicCommon
        handled = false;
    }
    else if (event->type() == OutputEvent::kStopped)
    {
        if (m_streamList)
        {
            MythUIButtonListItem *item = m_streamList->GetItemByData(QVariant::fromValue(m_currStream));
            if (item)
            {
                item->SetFontState("normal");
                item->DisplayState("stopped", "playstate");
            }
        }

        // pass it on to the default handler in MusicCommon
        handled = false;
    }
    else if (event->type() == OutputEvent::kBuffering)
    {
    }
    else if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;
        QStringList tokens = me->Message().split(" ", Qt::SkipEmptyParts);

        if (tokens.isEmpty())
            return;

        if (tokens[0] == "DOWNLOAD_FILE")
        {
            QStringList args = me->ExtraDataList();

            if (tokens[1] == "UPDATE")
            {
            }
            else if (tokens[1] == "FINISHED")
            {
                const QString& url = args[0];
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();
                const QString& filename = args[1];

                if ((errorCode != 0) || (fileSize == 0))
                    LOG(VB_GENERAL, LOG_ERR, QString("StreamView: failed to download radio icon from '%1'").arg(url));
                else
                {
                    for (int x = 0; x < m_streamList->GetCount(); x++)
                    {
                        MythUIButtonListItem *item = m_streamList->GetItemAt(x);
                        auto *mdata = item->GetData().value<MusicMetadata *>();
                        if (mdata && mdata->LogoUrl() == url)
                            item->SetImage(filename);
                    }
                }
            }
        }
    }
    else if (event->type() == DecoderHandlerEvent::kOperationStart)
    {
        auto *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;
        if (dhe->getMessage() && m_bufferStatus)
        {
            m_bufferStatus->SetText(*dhe->getMessage());
        }
    }
    else if (event->type() == DecoderHandlerEvent::kBufferStatus)
    {
        auto *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;

        int available = 0;
        int maxSize = 0;
        dhe->getBufferStatus(&available, &maxSize);

        if (m_bufferStatus)
        {
            QString status = QString("%1%").arg((int)(100.0 / ((double)maxSize / (double)available)));
            m_bufferStatus->SetText(status);
        }

        if (m_bufferProgress)
        {
            m_bufferProgress->SetTotal(maxSize);
            m_bufferProgress->SetUsed(available);
        }
    }
    else if (event->type() == DecoderHandlerEvent::kOperationStop)
    {
        if (m_bufferStatus)
            m_bufferStatus->Reset();
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);

        // make sure the user didn't ESCAPE out of the menu
        if ((dce == nullptr) || (dce->GetResult() < 0))
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "mainmenu")
        {
            if (resulttext == tr("Add Stream"))
            {
                MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                MythScreenType *screen = new EditStreamMetadata(mainStack, this, nullptr);

                if (screen->Create())
                    mainStack->AddScreen(screen);
                else
                    delete screen;
            }
            else if (resulttext == tr("Remove Stream"))
            {
                removeStream();
            }
            else if (resulttext == tr("Edit Stream"))
            {
                editStream();
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
    else
    {
        handled = false;
    }


    if (!handled)
        MusicCommon::customEvent(event);
}

bool StreamView::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "EDIT")
        {
            editStream();
        }
        else if (action == "DELETE")
        {
            removeStream();
        }
        else if (action == "TOGGLELAST")
        {
            if (m_lastStream && m_lastStream != m_currStream)
            {
                m_streamList->SetValueByData(QVariant::fromValue(m_lastStream));

                MythUIButtonListItem *item = m_streamList->GetItemCurrent();
                if (item)
                    streamItemClicked(item);
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

void StreamView::editStream(void)
{
    MythUIButtonListItem *item = m_streamList->GetItemCurrent();
    if (item)
    {
        auto *mdata = item->GetData().value<MusicMetadata *>();
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        MythScreenType *screen = new EditStreamMetadata(mainStack, this, mdata);

        if (screen->Create())
            mainStack->AddScreen(screen);
        else
            delete screen;
    }
}

void StreamView::removeStream(void)
{
    MythUIButtonListItem *item = m_streamList->GetItemCurrent();
    if (item)
    {
        auto *mdata = item->GetData().value<MusicMetadata *>();

        if (mdata)
        {
            ShowOkPopup(tr("Are you sure you want to delete this Stream?\n"
                           "Broadcaster: %1 - Channel: %2")
                           .arg(mdata->Broadcaster(), mdata->Channel()),
                        this, &StreamView::doRemoveStream, true);
        }
    }
}

void StreamView::doRemoveStream(bool ok)
{
    if (!ok)
        return;

    MythUIButtonListItem *item = m_streamList->GetItemCurrent();
    if (item)
    {
        auto *mdata = item->GetData().value<MusicMetadata *>();

        if (mdata)
            deleteStream(mdata);
    }
}

void StreamView::updateStreamList(void)
{
    Playlist *playlist = gPlayer->getCurrentPlaylist();
    if (nullptr == playlist)
        return;

    m_streamList->Reset();

    bool foundActiveStream = false;

    for (int x = 0; x < playlist->getTrackCount(); x++)
    {
        MusicMetadata *mdata = playlist->getSongAt(x);
        auto *item = new MythUIButtonListItem(m_streamList, "",
                                              QVariant::fromValue(mdata));
        InfoMap metadataMap;
        if (mdata)
            mdata->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);
        item->SetText("", "imageloaded");
        item->SetFontState("normal");
        item->DisplayState("default", "playstate");

        // if this is the current radio stream update its play state to match the player
        if (gPlayer->getCurrentMetadata() && mdata &&
            mdata->ID() == gPlayer->getCurrentMetadata()->ID())
        {
            if (gPlayer->isPlaying())
            {
                item->SetFontState("running");
                item->DisplayState("playing", "playstate");
            }
            else if (gPlayer->isPaused())
            {
                item->SetFontState("idle");
                item->DisplayState("paused", "playstate");
            }
            else
            {
                item->SetFontState("normal");
                item->DisplayState("stopped", "playstate");
            }

            m_streamList->SetItemCurrent(item);

            m_currStream = gPlayer->getCurrentMetadata();

            foundActiveStream = true;
        }
    }

    if (m_streamList->GetCount() > 0 && !foundActiveStream)
    {
        m_streamList->SetItemCurrent(0);
        gPlayer->stop(true);
    }

    if (m_noStreams)
        m_noStreams->SetVisible((m_streamList->GetCount() == 0));

    if (m_streamList->GetCount() == 0)
        LOG(VB_GENERAL, LOG_ERR, "StreamView hasn't found any streams!");
}

void StreamView::streamItemClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    gPlayer->setCurrentTrackPos(m_streamList->GetCurrentPos());
}

void  StreamView::streamItemVisible(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (!item->GetText("imageloaded").isEmpty())
        return;

    auto *mdata = item->GetData().value<MusicMetadata *>();
    if (mdata)
    {
        if (!mdata->LogoUrl().isEmpty())
            item->SetImage(mdata->getAlbumArtFile());
        else
            item->SetImage("");
    }

    item->SetText(" ", "imageloaded");
}

void StreamView::addStream(MusicMetadata *mdata)
{
    // sanity check this is a radio stream
    int repo = ID_TO_REPO(mdata->ID());
    if (repo != RT_Radio)
    {
        LOG(VB_GENERAL, LOG_ERR, "StreamView asked to add a stream but it isn't a radio stream!");
        return;
    }

    QString url = mdata->Url();

    gMusicData->m_all_streams->addStream(mdata);

    gPlayer->loadStreamPlaylist();

    updateStreamList();

    // find the new stream and make it the active item
    for (int x = 0; x < m_streamList->GetCount(); x++)
    {
        MythUIButtonListItem *item = m_streamList->GetItemAt(x);
        auto *itemsdata = item->GetData().value<MusicMetadata *>();
        if (itemsdata)
        {
            if (url == itemsdata->Url())
            {
                m_streamList->SetItemCurrent(item);
                break;
            }
        }
    }
}

void StreamView::updateStream(MusicMetadata *mdata)
{
    // sanity check this is a radio stream
    int repo = ID_TO_REPO(mdata->ID());
    if (repo != RT_Radio)
    {
        LOG(VB_GENERAL, LOG_ERR, "StreamView asked to update a stream but it isn't a radio stream!");
        return;
    }

    MusicMetadata::IdType id = mdata->ID();

    gMusicData->m_all_streams->updateStream(mdata);

    gPlayer->loadStreamPlaylist();

    // update mdata to point to the new item
    mdata = gMusicData->m_all_streams->getMetadata(id);

    if (!mdata)
        return;

    // force the icon to reload incase it changed
    QFile::remove(mdata->getAlbumArtFile());
    mdata->reloadAlbumArtImages();

    updateStreamList();

    // if we just edited the currently playing stream update the current metadata to match
    MusicMetadata *currentMetadata = gPlayer->getCurrentMetadata();
    if (id == currentMetadata->ID())
    {
        currentMetadata->setBroadcaster(mdata->Broadcaster());
        currentMetadata->setChannel(mdata->Channel());
    }

    // update the played tracks list to match the new metadata
    if (m_playedTracksList)
    {
        for (int x = 0; x < m_playedTracksList->GetCount(); x++)
        {
            MythUIButtonListItem *item = m_playedTracksList->GetItemAt(x);
            auto *playedmdata = item->GetData().value<MusicMetadata *>();

            if (playedmdata && playedmdata->ID() == id)
            {
                playedmdata->setBroadcaster(mdata->Broadcaster());
                playedmdata->setChannel(mdata->Channel());

                InfoMap metadataMap;
                playedmdata->toMap(metadataMap);
                item->SetTextFromMap(metadataMap);
            }
        }
    }

    // find the stream and make it the active item
    for (int x = 0; x < m_streamList->GetCount(); x++)
    {
        MythUIButtonListItem *item = m_streamList->GetItemAt(x);
        auto *itemsdata = item->GetData().value<MusicMetadata *>();
        if (itemsdata)
        {
            if (mdata->ID() == itemsdata->ID())
            {
                m_streamList->SetItemCurrent(item);
                break;
            }
        }
    }
}

void StreamView::deleteStream(MusicMetadata *mdata)
{
    // sanity check this is a radio stream
    int repo = ID_TO_REPO(mdata->ID());
    if (repo != RT_Radio)
    {
        LOG(VB_GENERAL, LOG_ERR, "StreamView asked to delete a stream but it isn't a radio stream!");
        return;
    }

    int currPos = m_streamList->GetCurrentPos();
    int topPos = m_streamList->GetTopItemPos();

    // if we are playing this radio stream stop playing
    if (gPlayer->getCurrentMetadata() == mdata)
        gPlayer->stop(true);

    gMusicData->m_all_streams->removeStream(mdata);

    gPlayer->loadStreamPlaylist();

    updateStreamList();

    m_streamList->SetItemCurrent(currPos, topPos);
}

///////////////////////////////////////////////////////////////////////////////

bool EditStreamMetadata::Create()
{
    if (!LoadWindowFromXML("stream-ui.xml", "editstreammetadata", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_broadcasterEdit, "broadcasteredit",  &err);
    UIUtilE::Assign(this, m_channelEdit,     "channeledit",  &err);
    UIUtilE::Assign(this, m_descEdit,        "descriptionedit", &err);
    UIUtilE::Assign(this, m_url1Edit,        "url1edit",     &err);
    UIUtilE::Assign(this, m_url2Edit,        "url2edit",     &err);
    UIUtilE::Assign(this, m_url3Edit,        "url3edit",     &err);
    UIUtilE::Assign(this, m_url4Edit,        "url4edit",     &err);
    UIUtilE::Assign(this, m_url5Edit,        "url5edit",     &err);
    UIUtilE::Assign(this, m_logourlEdit,     "logourledit" , &err);
    UIUtilE::Assign(this, m_genreEdit,       "genreedit",    &err);
    UIUtilE::Assign(this, m_languageEdit,    "languageedit", &err);
    UIUtilE::Assign(this, m_countryEdit,     "countryedit",  &err);
    UIUtilE::Assign(this, m_formatEdit,      "formatedit",   &err);
    UIUtilE::Assign(this, m_saveButton,      "savebutton",   &err);
    UIUtilE::Assign(this, m_cancelButton,    "cancelbutton", &err);
    UIUtilE::Assign(this, m_searchButton,    "searchbutton", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'editstreampopup'");
        return false;
    }

    if (m_streamMeta)
    {
        m_broadcasterEdit->SetText(m_streamMeta->Broadcaster());
        m_channelEdit->SetText(m_streamMeta->Channel());
        m_descEdit->SetText(m_streamMeta->Description());
        m_url1Edit->SetText(m_streamMeta->Url(0));
        m_url2Edit->SetText(m_streamMeta->Url(1));
        m_url3Edit->SetText(m_streamMeta->Url(2));
        m_url4Edit->SetText(m_streamMeta->Url(3));
        m_url5Edit->SetText(m_streamMeta->Url(4));
        m_logourlEdit->SetText(m_streamMeta->LogoUrl());
        m_genreEdit->SetText(m_streamMeta->Genre());
        m_countryEdit->SetText(m_streamMeta->Country());
        m_languageEdit->SetText(m_streamMeta->Language());
        m_formatEdit->SetText(m_streamMeta->MetadataFormat());
    }
    else
    {
        m_formatEdit->SetText("%a - %t");
    }

    connect(m_searchButton, &MythUIButton::Clicked, this, &EditStreamMetadata::searchClicked);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);
    connect(m_saveButton, &MythUIButton::Clicked, this, &EditStreamMetadata::saveClicked);

    BuildFocusList();

    return true;
}

void EditStreamMetadata::searchClicked(void )
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    MythScreenType *screen = new SearchStream(mainStack, this);

    if (screen->Create())
        mainStack->AddScreen(screen);
    else
        delete screen;

}

void EditStreamMetadata::saveClicked(void)
{
    bool doUpdate = true;

    if (!m_streamMeta)
    {
        m_streamMeta = new MusicMetadata;
        m_streamMeta->setRepo(RT_Radio);
        doUpdate = false;
    }

    m_streamMeta->setBroadcaster(m_broadcasterEdit->GetText());
    m_streamMeta->setChannel(m_channelEdit->GetText());
    m_streamMeta->setUrl(m_url1Edit->GetText(), 0);
    m_streamMeta->setUrl(m_url2Edit->GetText(), 1);
    m_streamMeta->setUrl(m_url3Edit->GetText(), 2);
    m_streamMeta->setUrl(m_url4Edit->GetText(), 3);
    m_streamMeta->setUrl(m_url5Edit->GetText(), 4);
    m_streamMeta->setFormat("cast");
    m_streamMeta->setMetadataFormat(m_formatEdit->GetText());
    m_streamMeta->setLogoUrl(m_logourlEdit->GetText());
    m_streamMeta->setGenre(m_genreEdit->GetText());
    m_streamMeta->setDescription(m_descEdit->GetText());
    m_streamMeta->setLanguage(m_languageEdit->GetText());
    m_streamMeta->setCountry(m_countryEdit->GetText());

    if (doUpdate)
        m_parent->updateStream(m_streamMeta);
    else
        m_parent->addStream(m_streamMeta);

    Close();
}

void EditStreamMetadata::changeStreamMetadata(MusicMetadata* mdata)
{
    if (mdata)
    {
        m_broadcasterEdit->SetText(mdata->Broadcaster());
        m_channelEdit->SetText(mdata->Channel());
        m_url1Edit->SetText(mdata->Url(0));
        m_url2Edit->SetText(mdata->Url(1));
        m_url3Edit->SetText(mdata->Url(2));
        m_url4Edit->SetText(mdata->Url(3));
        m_url5Edit->SetText(mdata->Url(4));
        m_logourlEdit->SetText(mdata->LogoUrl());
        m_genreEdit->SetText(mdata->Genre());
        m_formatEdit->SetText(mdata->MetadataFormat());
        m_descEdit->SetText(mdata->Description());
        m_countryEdit->SetText(mdata->Country());
        m_languageEdit->SetText(mdata->Language());
    }
}

///////////////////////////////////////////////////////////////////////////////

bool SearchStream::Create()
{
    if (!LoadWindowFromXML("stream-ui.xml", "searchstream", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_broadcasterList, "broadcasterlist", &err);
    UIUtilE::Assign(this, m_genreList,       "genrelist",       &err);
    UIUtilW::Assign(this, m_languageList,    "languagelist",    &err);
    UIUtilW::Assign(this, m_countryList,     "countrylist",     &err);
    UIUtilE::Assign(this, m_streamList,      "streamlist",      &err);
    UIUtilE::Assign(this, m_channelEdit,     "channeledit",     &err);
    UIUtilE::Assign(this, m_matchesText,     "matchestext",     &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'searchstream'");
        return false;
    }

    new MythUIButtonListItem(m_broadcasterList, "");
    new MythUIButtonListItem(m_genreList, "");
    m_matchesText->SetText("");

    connect(m_streamList, &MythUIButtonList::itemClicked,
            this, &SearchStream::streamClicked);
    connect(m_streamList, &MythUIButtonList::itemVisible,
            this, &SearchStream::streamVisible);
    connect(m_broadcasterList, &MythUIButtonList::itemSelected,
            this, &SearchStream::updateStreams);
    connect(m_genreList, &MythUIButtonList::itemSelected,
            this, &SearchStream::updateStreams);


    if (m_countryList)
    {
        connect(m_countryList, &MythUIButtonList::itemSelected,
                this, &SearchStream::updateStreams);

        new MythUIButtonListItem(m_countryList, "");
        connect(m_languageList, &MythUIButtonList::itemSelected,
                this, &SearchStream::updateStreams);
    }

    if (m_languageList)
    {
        new MythUIButtonListItem(m_languageList, "");
        connect(m_channelEdit, &MythUITextEdit::valueChanged,
                this, &SearchStream::updateStreams);
    }

    connect(&m_updateTimer, &QTimer::timeout, this, &SearchStream::doUpdateStreams);

    LoadInBackground("Loading Streams...");

    BuildFocusList();

    return true;
}

void SearchStream::Load(void)
{
    loadStreams();
    QTimer::singleShot(0, this, &SearchStream::doneLoading);
}

void SearchStream::doneLoading(void)
{
    updateBroadcasters();
    updateGenres();
    updateLanguages();
    updateCountries();
    doUpdateStreams();
}

void SearchStream::streamClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto mdata = item->GetData().value<MusicMetadata>();
    m_parent->changeStreamMetadata(&mdata);

    Close();
}

void SearchStream::streamVisible(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto mdata =  item->GetData().value<MusicMetadata>();
    if (!mdata.LogoUrl().isEmpty() && mdata.LogoUrl() != "-")
    {
        if (item->GetText("dummy") == " ")
        {
            item->SetImage(mdata.LogoUrl());
            item->SetText("", "dummy");
        }
    }
}

void SearchStream::loadStreams(void)
{
    MusicMetadata::updateStreamList();
}

void SearchStream::updateBroadcasters(void )
{
    m_broadcasterList->Reset();

    new MythUIButtonListItem(m_broadcasterList, tr("<All Broadcasters>"));

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT DISTINCT broadcaster FROM music_streams ORDER BY broadcaster;");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("get broadcaster", query);
        return;
    }

    while (query.next())
    {
        new MythUIButtonListItem(m_broadcasterList, query.value(0).toString());
    }

    m_broadcasterList->SetValue(tr("<All Broadcasters>"));
}

void SearchStream::updateGenres(void )
{
    m_genreList->Reset();

    new MythUIButtonListItem(m_genreList, tr("<All Genres>"));

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT DISTINCT genre FROM music_streams ORDER BY genre;");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("get genres", query);
        return;
    }

    while (query.next())
    {
        new MythUIButtonListItem(m_genreList, query.value(0).toString());
    }

    m_genreList->SetValue(tr("<All Genres>"));
}

void SearchStream::updateCountries(void )
{
    if (m_countryList)
    {
        m_countryList->Reset();

        new MythUIButtonListItem(m_countryList, tr("<All Countries>"));

        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT DISTINCT country FROM music_streams ORDER BY country;");

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("get countries", query);
            return;
        }

        while (query.next())
        {
            new MythUIButtonListItem(m_countryList, query.value(0).toString());
        }

        m_countryList->SetValue(tr("<All Countries>"));
    }
}

void SearchStream::updateLanguages(void )
{
    if (m_languageList)
    {
        m_languageList->Reset();

        new MythUIButtonListItem(m_languageList, tr("<All Languages>"));

        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT DISTINCT language FROM music_streams ORDER BY language;");

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("get languages", query);
            return;
        }

        while (query.next())
        {
            new MythUIButtonListItem(m_languageList, query.value(0).toString());
        }

        m_languageList->SetValue(tr("<All Languages>"));
    }
}

void SearchStream::updateStreams(void)
{
    if (m_updateTimer.isActive())
        m_updateTimer.stop();

    m_updateTimer.start(500ms);
}

void SearchStream::doUpdateStreams(void)
{
    if (m_updating)
        return;

    QString broadcaster = m_broadcasterList->GetValue();
    QString genre = m_genreList->GetValue();
    QString language = m_languageList ? m_languageList->GetValue() : tr("<All Languages>");
    QString country = m_countryList ? m_countryList->GetValue() : tr("<All Countries>");
    QString channel = m_channelEdit->GetText();

    // only update the buttonlist if something changed
    if (m_oldBroadcaster == broadcaster && m_oldGenre == genre && m_oldChannel == channel &&
        m_oldLanguage == language && m_oldCountry == country)
        return;

    m_oldBroadcaster = broadcaster;
    m_oldGenre = genre;
    m_oldChannel = channel;
    m_oldLanguage = language;
    m_oldCountry = country;

    bool searchBroadcaster = (broadcaster != tr("<All Broadcasters>"));
    bool searchGenre = (genre != tr("<All Genres>"));
    bool searchLanguage = (language != tr("<All Languages>"));
    bool searchCountry = (country != tr("<All Countries>"));
    bool searchChannel = !channel.isEmpty();

    m_streams.clear();
    m_streamList->Reset();

    MSqlQuery query(MSqlQuery::InitCon());

    QString sql = "SELECT broadcaster, channel, description, genre, url1, url2, url3, url4, url5, "
                  "logourl, metaformat, country, language "
                  "FROM music_streams ";
    bool doneWhere = false;

    if (searchBroadcaster)
    {
        sql += "WHERE broadcaster = :BROADCASTER ";
        doneWhere = true;
    }

    if (searchGenre)
    {
        if (!doneWhere)
        {
            sql += "WHERE genre = :GENRE ";
            doneWhere = true;
        }
        else
        {
            sql += "AND genre = :GENRE ";
        }
    }

    if (searchLanguage)
    {
        if (!doneWhere)
        {
            sql += "WHERE language = :LANGUAGE ";
            doneWhere = true;
        }
        else
        {
            sql += "AND language = :LANGUAGE ";
        }
    }

    if (searchCountry)
    {
        if (!doneWhere)
        {
            sql += "WHERE country = :COUNTRY ";
            doneWhere = true;
        }
        else
        {
            sql += "AND country = :COUNTRY ";
        }
    }

    if (searchChannel)
    {
        if (!doneWhere)
        {
            sql += "WHERE channel LIKE " + QString("'%%1%'").arg(channel);
            // doneWhere = true;
        }
        else
        {
            sql += "AND channel LIKE " + QString("'%%1%' ").arg(channel);
        }
    }

    sql += "ORDER BY broadcaster, channel;";

    query.prepare(sql);

    if (searchBroadcaster)
        query.bindValue(":BROADCASTER", broadcaster);

    if (searchGenre)
        query.bindValue(":GENRE", genre);

    if (searchLanguage)
        query.bindValue(":LANGUAGE", language);

    if (searchCountry)
        query.bindValue(":COUNTRY", country);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("search streams", query);
        return;
    }

    if (query.size() > 500)
    {
        QString message = tr("Updating stream list. Please Wait ...");
        OpenBusyPopup(message);
    }

    int count = 0;
    while (query.next())
    {
        MusicMetadata mdata;
        mdata.setBroadcaster(query.value(0).toString());
        mdata.setChannel(query.value(1).toString());
        mdata.setDescription(query.value(2).toString());
        mdata.setGenre(query.value(3).toString());

        for (size_t x = 0; x < STREAMURLCOUNT; x++)
            mdata.setUrl(query.value(4 + x).toString(), x);

        mdata.setLogoUrl(query.value(9).toString());
        mdata.setMetadataFormat(query.value(10).toString());
        mdata.setCountry(query.value(11).toString());
        mdata.setLanguage(query.value(12).toString());

        auto *item = new MythUIButtonListItem(m_streamList, "",
                                              QVariant::fromValue(mdata));
        InfoMap metadataMap;
        mdata.toMap(metadataMap);

        item->SetTextFromMap(metadataMap);

        item->SetText(" ", "dummy");
        count++;

        if ((count % 500) == 0)
        {
            qApp->processEvents();
        }
    }

    m_matchesText->SetText(QString("%1").arg(m_streamList->GetCount()));

    if (query.size() > 500)
        CloseBusyPopup();

    m_updating = false;
}
