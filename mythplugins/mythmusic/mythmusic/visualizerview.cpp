// C++
#include <cstdlib>
#include <iostream>

// qt
#include <QKeyEvent>
#include <QTimer>

// myth
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdbcon.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuistatetype.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuiutils.h>
#include <libmythui/mythdialogbox.h>

// mythmusic
#include "musiccommon.h"
#include "visualizerview.h"

VisualizerView::VisualizerView(MythScreenStack *parent, MythScreenType *parentScreen)
         :MusicCommon(parent, parentScreen, "visualizerview")
{
    m_currentView = MV_VISUALIZER;
}

bool VisualizerView::Create(void)
{
    // Load the theme for this screen
    bool err = LoadWindowFromXML("music-ui.xml", "visualizerview", this);

    if (!err)
        return false;

    // find common widgets available on any view
    err = CreateCommon();

    // find widgets specific to this view

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'lyricsview'");
        return false;
    }

    BuildFocusList();

    return true;
}

void VisualizerView::customEvent(QEvent *event)
{
    if (event->type() == MusicPlayerEvent::TrackChangeEvent ||
        event->type() == MusicPlayerEvent::PlayedTracksChangedEvent)
        showTrackInfoPopup();

    MusicCommon::customEvent(event);
}

bool VisualizerView::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
        {
            showTrackInfoPopup();
        }
        else
            handled = false;
    }

    if (!handled && MusicCommon::keyPressEvent(event))
        handled = true;

    return handled;
}

void VisualizerView::ShowMenu(void)
{
    QString label = tr("Actions");

    auto *menu = new MythMenu(label, this, "menu");

    menu->AddItem(tr("Change Visualizer"), nullptr, createVisualizerMenu());
    menu->AddItem(tr("Show Track Info"), &VisualizerView::showTrackInfoPopup);
    menu->AddItem(tr("Other Options"), nullptr, createMainMenu());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(menu, popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

void VisualizerView::showTrackInfoPopup(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *popup = new TrackInfoPopup(popupStack, gPlayer->getCurrentMetadata());

    if (!popup->Create())
    {
        delete popup;
        return;
    }

    popupStack->AddScreen(popup);
}

//---------------------------------------------------------
// TrackInfoPopup
//---------------------------------------------------------
static constexpr std::chrono::seconds MUSICINFOPOPUPTIME { 8s };

TrackInfoPopup::~TrackInfoPopup(void)
{
    if (m_displayTimer)
    {
        m_displayTimer->stop();
        delete m_displayTimer;
        m_displayTimer = nullptr;
    }
}

bool TrackInfoPopup::Create(void)
{
    bool err = LoadWindowFromXML("music-ui.xml", "trackinfo_popup", this);

    if (!err)
        return false;

    // get map for current track
    InfoMap metadataMap;
    m_metadata->toMap(metadataMap); 

    // add the map from the next track
    MusicMetadata *nextMetadata = gPlayer->getNextMetadata();
    if (nextMetadata)
        nextMetadata->toMap(metadataMap, "next");

    SetTextFromMap(metadataMap);

    MythUIStateType *ratingState = dynamic_cast<MythUIStateType *>(GetChild("ratingstate"));
    if (ratingState)
        ratingState->DisplayState(QString("%1").arg(m_metadata->Rating()));

    MythUIImage *albumImage = dynamic_cast<MythUIImage *>(GetChild("coverart"));
    if (albumImage)
    {
        if (!m_metadata->getAlbumArtFile().isEmpty())
        {
            albumImage->SetFilename(m_metadata->getAlbumArtFile());
            albumImage->Load();
        }
    }

    m_displayTimer = new QTimer(this);
    connect(m_displayTimer, &QTimer::timeout, this, &MythScreenType::Close);
    m_displayTimer->setSingleShot(true);
    m_displayTimer->start(MUSICINFOPOPUPTIME);

    return true;
}

bool TrackInfoPopup::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions, false);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
            Close();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
