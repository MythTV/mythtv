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
#include "mainvisual.h"
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
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'visualizerview'");
        return false;
    }

    BuildFocusList();

    m_currentView = MV_VISUALIZER;

    return true;
}

void VisualizerView::customEvent(QEvent *event)
{
    if (event->type() == MusicPlayerEvent::kTrackChangeEvent ||
        event->type() == MusicPlayerEvent::kPlayedTracksChangedEvent)
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

        if (m_mainvisual && m_mainvisual->visual())
            m_mainvisual->visual()->handleKeyPress(action);

        // unassgined arrow keys might as well be useful
        if (action == "UP")
        {
            action = "PREVTRACK";
            previous();
        }
        else if (action == "DOWN")
        {
            action = "NEXTTRACK";
            next();
        }
        else if (action == "LEFT")
        {
            action = "RWND";
            seekback();
        }
        else if (action == "RIGHT")
        {
            action = "FFWD";
            seekforward();
        }

        if (action == "INFO")
            showTrackInfoPopup();
        else if (
            action == "NEXTTRACK" ||
            action == "PREVTRACK" ||
            action == "FFWD" ||
            action == "RWND" ||
            action == "THMBUP" ||
            action == "THMBDOWN" ||
            action == "SPEEDUP" ||
            action == "SPEEDDOWN")
        {
            handled = MusicCommon::keyPressEvent(event);
            showTrackInfoPopup();
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
    if (m_currentView == MV_VISUALIZERINFO)
        return;
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *popup = new TrackInfoPopup(popupStack);

    if (popup->Create())
        popupStack->AddScreen(popup);
    else
        delete popup;
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

    // find common widgets available on any view
    err = CreateCommon();

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'trackinfo_popup'");
        return false;
    }
    m_currentView = MV_VISUALIZERINFO;

    // get map for current track
    MusicMetadata *metadata = gPlayer->getCurrentMetadata();
    InfoMap metadataMap;
    metadata->toMap(metadataMap);

    // add the map from the next track
    MusicMetadata *nextMetadata = gPlayer->getNextMetadata();
    if (nextMetadata)
        nextMetadata->toMap(metadataMap, "next");

    SetTextFromMap(metadataMap);

    MythUIStateType *ratingState = dynamic_cast<MythUIStateType *>(GetChild("ratingstate"));
    if (ratingState)
        ratingState->DisplayState(QString("%1").arg(metadata->Rating()));

    MythUIImage *albumImage = dynamic_cast<MythUIImage *>(GetChild("coverart"));
    if (albumImage)
    {
        if (!metadata->getAlbumArtFile().isEmpty())
        {
            albumImage->SetFilename(metadata->getAlbumArtFile());
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
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    m_currentView = MV_VISUALIZERINFO;
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions, false);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            if (m_displayTimer)
                m_displayTimer->stop();
            return true;
        }
        if (action == "ESCAPE")
            Close();
        else if (action == "INFO")
            showTrackInfo(gPlayer->getCurrentMetadata());
        else if (action == "MENU")
        {
            // menu over info misbehaves: if we close after 8 seconds,
            // then menu seg faults!  We could workaround that by
            // canceling our timer as shown here, but menu fails to
            // get the visualizer list (how does m_visualModes.count()
            // == 0?)  So just doing nothing forces user to ESCAPE out
            // of info to get to the working menu.  -twitham

            // if (m_displayTimer)
            // {
            //     m_displayTimer->stop();
            //     delete m_displayTimer;
            //     m_displayTimer = nullptr;
            // }
            // handled = false;
        }
        else
        {
            handled = false;
        }
    }
    // keep info up while seeking, theme should show progressbar/time
    if (m_displayTimer)
        m_displayTimer->start(MUSICINFOPOPUPTIME);

    if (!handled && VisualizerView::keyPressEvent(event))
        handled = true;

    return handled;
}
