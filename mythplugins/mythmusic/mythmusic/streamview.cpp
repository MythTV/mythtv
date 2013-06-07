#include <iostream>
#include <cstdlib>

// qt
#include <QKeyEvent>
#include <QRegExp>
#include <QThread>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythmainwindow.h>
#include <mythuibuttonlist.h>
#include <mythuibutton.h>
#include <mythuitext.h>
#include <mythuiprogressbar.h>
#include <mythuiutils.h>
#include <mythdialogbox.h>
#include <mythuihelper.h>
#include <mythdownloadmanager.h>
#include <mythdirs.h>
#include <musicutils.h>

// mythmusic
#include "musicdata.h"
#include "musiccommon.h"
#include "streamview.h"
#include "musicplayer.h"

using namespace std;

StreamView::StreamView(MythScreenStack *parent)
           :MusicCommon(parent, "streamview"),
            m_streamList(NULL), m_noStreams(NULL), m_bufferStatus(NULL),
            m_bufferProgress(NULL)
{
    m_currentView = MV_RADIO;
}

StreamView::~StreamView()
{
}

bool StreamView::Create(void)
{
    bool err = false;

    // Load the theme for this screen
    err = LoadWindowFromXML("stream-ui.xml", "streamview", this);

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

    connect(m_streamList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(streamItemClicked(MythUIButtonListItem*)));
    connect(m_streamList, SIGNAL(itemVisible(MythUIButtonListItem*)),
            this, SLOT(streamItemVisible(MythUIButtonListItem*)));

    gPlayer->setPlayMode(MusicPlayer::PLAYMODE_RADIO);

    updateStreamList();
    updateUIPlayedList();

    BuildFocusList();

    return true;
}

void StreamView::ShowMenu(void)
{
    MythMenu *menu = NULL;

    menu = new MythMenu(tr("Stream Actions"), this, "streammenu");
    menu->AddItem(tr("Add Stream"));

    if (m_streamList->GetItemCurrent())
    {
        menu->AddItem(tr("Edit Stream"));
        menu->AddItem(tr("Remove Stream"));
    }

    menu->AddItem(tr("More Options"), NULL, createMainMenu());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menu;
}

void StreamView::customEvent(QEvent *event)
{
    bool handled = true;

    if (event->type() == MusicPlayerEvent::PlayedTracksChangedEvent)
    {
        if (gPlayer->getPlayedTracksList().count())
            updateTrackInfo(gPlayer->getCurrentMetadata());

        // add the new track to the list
        if (m_playedTracksList && gPlayer->getPlayedTracksList().count())
        {
            MusicMetadata *mdata = gPlayer->getPlayedTracksList().last();

            MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_playedTracksList, "", qVariantFromValue(mdata), 0);

            MetadataMap metadataMap;
            mdata->toMap(metadataMap);
            item->SetTextFromMap(metadataMap);
            item->SetFontState("normal");
            item->DisplayState("default", "playstate");
            item->SetImage(mdata->getAlbumArtFile());

            m_playedTracksList->SetItemCurrent(item);
        }
    }
    else if (event->type() == MusicPlayerEvent::TrackChangeEvent)
    {
        MusicPlayerEvent *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackNo = mpe->TrackID;

        if (m_streamList)
        {
            if (m_currentTrack >= 0 && m_currentTrack < m_streamList->GetCount())
            {
                MythUIButtonListItem *item = m_streamList->GetItemAt(m_currentTrack);
                if (item)
                {
                    item->SetFontState("normal");
                    item->DisplayState("default", "playstate");
                }
            }

            if (trackNo >= 0 && trackNo < m_streamList->GetCount())
            {
                if (m_currentTrack == m_streamList->GetCurrentPos())
                    m_streamList->SetItemCurrent(trackNo);

                MythUIButtonListItem *item = m_streamList->GetItemAt(trackNo);
                if (item)
                {
                    item->SetFontState("running");
                    item->DisplayState("playing", "playstate");
                }
            }
        }

        m_currentTrack = trackNo;

        updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == OutputEvent::Playing)
    {
        if (gPlayer->isPlaying())
        {
            if (m_streamList)
            {
                MythUIButtonListItem *item = m_streamList->GetItemAt(m_currentTrack);
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
    else if (event->type() == OutputEvent::Stopped)
    {
        if (m_streamList)
        {
            MythUIButtonListItem *item = m_streamList->GetItemAt(m_currentTrack);
            if (item)
            {
                item->SetFontState("normal");
                item->DisplayState("stopped", "playstate");
            }
        }

        // pass it on to the default handler in MusicCommon
        handled = false;
    }
    else if (event->type() == OutputEvent::Buffering)
    {
    }
    else if (event->type() == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QStringList tokens = me->Message().split(" ", QString::SkipEmptyParts);

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
                QString url = args[0];
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();
                QString filename = args[1];

                if ((errorCode != 0) || (fileSize == 0))
                    LOG(VB_GENERAL, LOG_ERR, QString("StreamView: failed to download radio icon from '%1'").arg(url));
                else
                {
                    for (int x = 0; x < m_streamList->GetCount(); x++)
                    {
                        MythUIButtonListItem *item = m_streamList->GetItemAt(x);
                        MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
                        if (mdata && mdata->LogoUrl() == url)
                            item->SetImage(filename);
                    }
                }
            }
        }
    }
    else if (event->type() == DecoderHandlerEvent::OperationStart)
    {
        DecoderHandlerEvent *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;
        if (dhe->getMessage() && m_bufferStatus)
        {
            m_bufferStatus->SetText(*dhe->getMessage());
        }
    }
    else if (event->type() == DecoderHandlerEvent::BufferStatus)
    {
        DecoderHandlerEvent *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;

        int available, maxSize;
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
    else if (event->type() == DecoderHandlerEvent::OperationStop)
    {
        if (m_bufferStatus)
            m_bufferStatus->Reset();
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = static_cast<DialogCompletionEvent*>(event);

        // make sure the user didn't ESCAPE out of the menu
        if (dce->GetResult() < 0)
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "streammenu")
        {
            if (resulttext == tr("Add Stream"))
            {
                MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                MythScreenType *screen = new EditStreamMetadata(mainStack, this, NULL);

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
        }
        else
            handled = false;
    }
    else
        handled = false;


    if (!handled)
        MusicCommon::customEvent(event);
}

bool StreamView::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "EDIT" && GetFocusWidget() == m_streamList)
        {
            editStream();
        }
        else if (action == "DELETE" && GetFocusWidget() == m_streamList)
        {
            removeStream();
        }
        else if (action == "INFO")
        {
            //TODO show stream info popup
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

void StreamView::editStream(void)
{
    MythUIButtonListItem *item = m_streamList->GetItemCurrent();
    if (item)
    {
        MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
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
        MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());

        if (mdata)
            ShowOkPopup(tr("Are you sure you want to delete this Stream?\n"
                           "Station: %1 - Channel: %2")
                           .arg(mdata->Station()).arg(mdata->Channel()),
                        this, SLOT(doRemoveStream(bool)), true);
    }
}

void StreamView::doRemoveStream(bool ok)
{
    if (!ok)
        return;

    MythUIButtonListItem *item = m_streamList->GetItemCurrent();
    if (item)
    {
        MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());

        if (mdata)
            deleteStream(mdata);
    }
}

void StreamView::updateStreamList(void)
{
    m_streamList->Reset();

    bool foundActiveStream = false;

    for (int x = 0; x < gPlayer->getPlaylist()->getSongs().count(); x++)
    {
        MusicMetadata *mdata = gPlayer->getPlaylist()->getSongs().at(x);
        MythUIButtonListItem *item = new MythUIButtonListItem(m_streamList, "", qVariantFromValue(mdata));
        MetadataMap metadataMap;
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

    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
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

    gMusicData->all_streams->addStream(mdata);

    gPlayer->loadStreamPlaylist();

    updateStreamList();

    // find the new stream and make it the active item
    for (int x = 0; x < m_streamList->GetCount(); x++)
    {
        MythUIButtonListItem *item = m_streamList->GetItemAt(x);
        MusicMetadata *itemsdata = qVariantValue<MusicMetadata*> (item->GetData());
        if (itemsdata)
        {
            if (url == itemsdata->Url())
            {
                m_streamList->SetItemCurrent(item);
                m_currentTrack = x;
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

    gMusicData->all_streams->updateStream(mdata);

    gPlayer->loadStreamPlaylist();

    // update mdata to point to the new item
    mdata = gMusicData->all_streams->getMetadata(id);

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
        currentMetadata->setStation(mdata->Station());
        currentMetadata->setChannel(mdata->Channel());
    }

    // update the played tracks list to match the new metadata
    if (m_playedTracksList)
    {
        for (int x = 0; x < m_playedTracksList->GetCount(); x++)
        {
            MythUIButtonListItem *item = m_playedTracksList->GetItemAt(x);
            MusicMetadata *playedmdata = qVariantValue<MusicMetadata*> (item->GetData());

            if (playedmdata && playedmdata->ID() == id)
            {
                playedmdata->setStation(mdata->Station());
                playedmdata->setChannel(mdata->Channel());

                MetadataMap metadataMap;
                playedmdata->toMap(metadataMap);
                item->SetTextFromMap(metadataMap);
            }
        }
    }

    // find the stream and make it the active item
    for (int x = 0; x < m_streamList->GetCount(); x++)
    {
        MythUIButtonListItem *item = m_streamList->GetItemAt(x);
        MusicMetadata *itemsdata = qVariantValue<MusicMetadata*> (item->GetData());
        if (itemsdata)
        {
            if (mdata->ID() == itemsdata->ID())
            {
                m_streamList->SetItemCurrent(item);
                m_currentTrack = x;
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

    gMusicData->all_streams->removeStream(mdata);

    gPlayer->loadStreamPlaylist();

    updateStreamList();
}

///////////////////////////////////////////////////////////////////////////////

EditStreamMetadata::EditStreamMetadata(MythScreenStack *parentStack,
                                 StreamView *parent,
                                 MusicMetadata *mdata)
    : MythScreenType(parentStack, "editstreampopup"),
        m_parent(parent), m_streamMeta(mdata),
    m_stationEdit(NULL),  m_channelEdit(NULL), m_urlEdit(NULL),
    m_logourlEdit(NULL),  m_formatEdit(NULL),  m_genreEdit(NULL),
    m_searchButton(NULL), m_cancelButton(NULL), m_saveButton(NULL)
{
}

bool EditStreamMetadata::Create()
{
    if (!LoadWindowFromXML("stream-ui.xml", "editstreammetadata", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_stationEdit,  "stationedit", &err);
    UIUtilE::Assign(this, m_channelEdit,  "channeledit", &err);
    UIUtilE::Assign(this, m_urlEdit,      "urledit",     &err);
    UIUtilE::Assign(this, m_logourlEdit,  "logourledit", &err);
    UIUtilE::Assign(this, m_genreEdit,    "genreedit",   &err);
    UIUtilE::Assign(this, m_formatEdit,   "formatedit",  &err);
    UIUtilE::Assign(this, m_saveButton,   "savebutton",  &err);
    UIUtilE::Assign(this, m_cancelButton, "cancelbutton",  &err);
    UIUtilE::Assign(this, m_searchButton, "searchbutton",  &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'editstreampopup'");
        return false;
    }

    if (m_streamMeta)
    {
        m_stationEdit->SetText(m_streamMeta->Station());
        m_channelEdit->SetText(m_streamMeta->Channel());
        m_urlEdit->SetText(m_streamMeta->Url());
        m_logourlEdit->SetText(m_streamMeta->LogoUrl());
        m_genreEdit->SetText(m_streamMeta->Genre());
        m_formatEdit->SetText(m_streamMeta->MetadataFormat());
    }
    else
        m_formatEdit->SetText("%a - %t");

    connect(m_searchButton, SIGNAL(Clicked()), this, SLOT(searchClicked()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));
    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(saveClicked()));

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

    m_streamMeta->setStation(m_stationEdit->GetText());
    m_streamMeta->setChannel(m_channelEdit->GetText());
    m_streamMeta->setUrl(m_urlEdit->GetText());
    m_streamMeta->setFormat("cast");
    m_streamMeta->setMetadataFormat(m_formatEdit->GetText());
    m_streamMeta->setLogoUrl(m_logourlEdit->GetText());
    m_streamMeta->setGenre(m_genreEdit->GetText());

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
        m_stationEdit->SetText(mdata->Station());
        m_channelEdit->SetText(mdata->Channel());
        m_urlEdit->SetText(mdata->Url());
        m_logourlEdit->SetText(mdata->LogoUrl());
        m_genreEdit->SetText(mdata->Genre());
        m_formatEdit->SetText(mdata->MetadataFormat());
    }
}

///////////////////////////////////////////////////////////////////////////////

SearchStream::SearchStream(MythScreenStack *parentStack,
                           EditStreamMetadata *parent)
    : MythScreenType(parentStack, "searchstream"),
    m_parent(NULL),  m_stationList(NULL), m_genreList(NULL),
    m_channelEdit(NULL), m_streamList(NULL), m_matchesText(NULL)
{
    m_parent = parent;
}

bool SearchStream::Create()
{
    if (!LoadWindowFromXML("stream-ui.xml", "searchstream", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_stationList, "stationlist", &err);
    UIUtilE::Assign(this, m_genreList,   "genrelist",   &err);
    UIUtilE::Assign(this, m_streamList,  "streamlist",  &err);
    UIUtilE::Assign(this, m_channelEdit, "channeledit", &err);
    UIUtilE::Assign(this, m_matchesText, "matchestext", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'searchstream'");
        return false;
    }

    connect(m_streamList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(streamClicked(MythUIButtonListItem*)));
    connect(m_streamList, SIGNAL(itemVisible(MythUIButtonListItem*)),
            this, SLOT(streamVisible(MythUIButtonListItem*)));
    connect(m_stationList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(updateStreams()));
    connect(m_genreList, SIGNAL(itemSelected(MythUIButtonListItem*)), 
            this, SLOT(updateStreams()));
    connect(m_channelEdit, SIGNAL(valueChanged()), 
            this, SLOT(updateStreams()));

    loadStreams();
    updateStations();
    updateGenres();
    updateStreams();

    BuildFocusList();

    return true;
}

void SearchStream::streamClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
    if (mdata)
        m_parent->changeStreamMetadata(mdata);

    Close();
}

void SearchStream::streamVisible(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
    if (mdata)
    {
        if (item->GetText("dummy") == " ")
        {
            item->SetImage(mdata->LogoUrl());
            item->SetText("", "dummy");
        }
    }
}

void SearchStream::loadStreams(void)
{
    m_streams.clear();
    m_stations.clear();
    m_genres.clear();

    m_stations.append(tr("<All Stations>"));
    m_genres.append(tr("<All Genres>"));

    QString filename = QString("%1%2").arg(GetShareDir()).arg("mythmusic/streams.xml");

    QFile xmlFile(filename);

    if (!xmlFile.exists() || !xmlFile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, "SearchStream: Cannot open streams.xml");
        return;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    QDomDocument domDoc;

    if (!domDoc.setContent(&xmlFile, false, &errorMsg,
                           &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "SearchStream: Could not read content of streams.xml" +
                QString("\n\t\t\tError parsing %1").arg(filename) +
                QString("\n\t\t\tat line: %1  column: %2 msg: %3")
                .arg(errorLine).arg(errorColumn).arg(errorMsg));
        return;
    }

    QDomNodeList itemList = domDoc.elementsByTagName("item");

    QDomNode itemNode;
    for (int i = 0; i < itemList.count(); i++)
    {
        itemNode = itemList.item(i);

        MusicMetadata mdata;
        mdata.setStation(itemNode.namedItem(QString("station")).toElement().text());
        mdata.setChannel(itemNode.namedItem(QString("channel")).toElement().text());
        mdata.setUrl(itemNode.namedItem(QString("url")).toElement().text());
        mdata.setLogoUrl(itemNode.namedItem(QString("logourl")).toElement().text());
        mdata.setGenre(itemNode.namedItem(QString("genre")).toElement().text());
        mdata.setMetadataFormat(itemNode.namedItem(QString("metadataformat")).toElement().text());

        m_streams.insert(mdata.Station() + '-' + mdata.Channel(), mdata);

        if (!m_stations.contains(mdata.Station()))
            m_stations.append(mdata.Station());

        QStringList genreList = mdata.Genre().split(',');

        for (int x = 0; x < genreList.count(); x++)
        {
            if (!m_genres.contains(genreList[x].trimmed()))
                m_genres.append(genreList[x].trimmed());
        }
    }

    xmlFile.close();

    m_stations.sort();
    m_genres.sort();
}

void SearchStream::updateStations(void )
{
    m_stationList->Reset();

    for (int x = 0; x < m_stations.count(); x++)
        new MythUIButtonListItem(m_stationList, m_stations.at(x));

    m_stationList->SetValue(tr("<All Stations>"));
}

void SearchStream::updateGenres(void )
{
    m_genreList->Reset();

    for (int x = 0; x < m_genres.count(); x++)
        new MythUIButtonListItem(m_genreList, m_genres.at(x));

    m_genreList->SetValue(tr("<All Genres>"));
}

void SearchStream::updateStreams(void)
{
    m_streamList->Reset();

    QString station = m_stationList->GetValue();
    QString genre = m_genreList->GetValue();
    QString channel = m_channelEdit->GetText();

    bool searchStation = (station != tr("<All Stations>"));
    bool searchGenre = (genre != tr("<All Genres>"));
    bool searchChannel = !channel.isEmpty();

    QMap<QString, MusicMetadata>::iterator it;

    for (it = m_streams.begin(); it != m_streams.end(); ++it)
    {
        MusicMetadata *mdata = &(*it);

        if (searchStation && station != mdata->Station())
            continue;

        if (searchGenre && !mdata->Genre().contains(genre, Qt::CaseInsensitive))
            continue;

        if (searchChannel && !mdata->Channel().contains(channel, Qt::CaseInsensitive))
            continue;

        // if we got here we must have a match so add it to the list
        MythUIButtonListItem *item = new MythUIButtonListItem(m_streamList, 
                "", qVariantFromValue(mdata));

        MetadataMap metadataMap;
        mdata->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);

        item->SetText(" ", "dummy");
    }

    m_matchesText->SetText(QString("%1").arg(m_streamList->GetCount()));
}
