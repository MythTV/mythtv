// C++ includes
#include <cstdlib>
#include <iostream>

// Qt includes
#include <QApplication>
#include <QLocale>

// mythtv
#include <libmyth/audio/audiooutput.h>
#include <libmythbase/lcddevice.h>
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythrandom.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuibuttontree.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuistatetype.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>
#include <libmythui/mythuivideo.h>

// MythMusic includes
#include "constants.h"
#include "decoder.h"
#include "editmetadata.h"
#include "lyricsview.h"
#include "mainvisual.h"
#include "musiccommon.h"
#include "musicdata.h"
#include "playlist.h"
#include "playlistcontainer.h"
#include "playlisteditorview.h"
#include "playlistview.h"
#include "search.h"
#include "searchview.h"
#include "smartplaylist.h"
#include "streamview.h"
#include "visualizerview.h"

MusicCommon::MusicCommon(MythScreenStack *parent, MythScreenType *parentScreen,
                         const QString &name)
            : MythScreenType(parent, name),
    m_parentScreen(parentScreen)
{
    m_cycleVisualizer = gCoreContext->GetBoolSetting("VisualCycleOnSongChange", false);

    if (LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
        lcd->setFunctionLEDs(FUNC_MUSIC, true);
    }
}

MusicCommon::~MusicCommon(void)
{
    gPlayer->removeListener(this);

    if (m_mainvisual)
    {
        stopVisualizer();
        delete m_mainvisual;
        m_mainvisual = nullptr;
    }

    if (LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
        lcd->setFunctionLEDs(FUNC_MUSIC, false);
    }
}

bool MusicCommon::CreateCommon(void)
{
    bool err = false;
    UIUtilW::Assign(this, m_timeText,      "time", &err);
    UIUtilW::Assign(this, m_infoText,      "info", &err);
    UIUtilW::Assign(this, m_visualText,    "visualizername", &err);
    UIUtilW::Assign(this, m_noTracksText,  "notracks", &err);

    UIUtilW::Assign(this, m_shuffleState,      "shufflestate", &err);
    UIUtilW::Assign(this, m_repeatState,       "repeatstate", &err);
    UIUtilW::Assign(this, m_movingTracksState, "movingtracksstate", &err);

    UIUtilW::Assign(this, m_ratingState,       "ratingstate", &err);

    UIUtilW::Assign(this, m_trackProgress,     "progress", &err);
    UIUtilW::Assign(this, m_trackProgressText, "trackprogress", &err);
    UIUtilW::Assign(this, m_trackSpeedText,    "trackspeed", &err);
    UIUtilW::Assign(this, m_trackState,        "trackstate", &err);

    UIUtilW::Assign(this, m_volumeText,        "volume", &err);
    UIUtilW::Assign(this, m_muteState,         "mutestate", &err);

    UIUtilW::Assign(this, m_playlistProgress,     "playlistprogress", &err);

    UIUtilW::Assign(this, m_prevButton,    "prev", &err);
    UIUtilW::Assign(this, m_rewButton,     "rew", &err);
    UIUtilW::Assign(this, m_pauseButton,   "pause", &err);
    UIUtilW::Assign(this, m_playButton,    "play", &err);
    UIUtilW::Assign(this, m_stopButton,    "stop", &err);
    UIUtilW::Assign(this, m_ffButton,      "ff", &err);
    UIUtilW::Assign(this, m_nextButton,    "next", &err);

    UIUtilW::Assign(this, m_coverartImage, "coverart", &err);

    UIUtilW::Assign(this, m_currentPlaylist,  "currentplaylist", &err);
    UIUtilW::Assign(this, m_playedTracksList, "playedtrackslist", &err);

    UIUtilW::Assign(this, m_visualizerVideo, "visualizer", &err);

    if (m_prevButton)
        connect(m_prevButton, &MythUIButton::Clicked, this, &MusicCommon::previous);

    if (m_rewButton)
        connect(m_rewButton, &MythUIButton::Clicked, this, &MusicCommon::seekback);

    if (m_pauseButton)
    {
        m_pauseButton->SetLockable(true);
        connect(m_pauseButton, &MythUIButton::Clicked, this, &MusicCommon::pause);
    }

    if (m_playButton)
    {
        m_playButton->SetLockable(true);
        connect(m_playButton, &MythUIButton::Clicked, this, &MusicCommon::play);
    }

    if (m_stopButton)
    {
        m_stopButton->SetLockable(true);
        connect(m_stopButton, &MythUIButton::Clicked, this, &MusicCommon::stop);
    }

    if (m_ffButton)
        connect(m_ffButton, &MythUIButton::Clicked, this, &MusicCommon::seekforward);

    if (m_nextButton)
        connect(m_nextButton, &MythUIButton::Clicked, this, &MusicCommon::next);

    if (m_currentPlaylist)
    {
        connect(m_currentPlaylist, &MythUIButtonList::itemClicked,
                this, &MusicCommon::playlistItemClicked);
        connect(m_currentPlaylist, &MythUIButtonList::itemVisible,
                this, &MusicCommon::playlistItemVisible);

        m_currentPlaylist->SetSearchFields("**search**");
    }

    init();

    return err;
}

void MusicCommon::init(bool startPlayback)
{
    gPlayer->addListener(this);

    if (startPlayback)
    {
        if (!gPlayer->isPlaying())
        {
            if (m_currentView == MV_VISUALIZER ||
                m_currentView == MV_VISUALIZERINFO) // keep playmode
                gPlayer->setPlayMode(gPlayer->getPlayMode());
            else if (m_currentView == MV_RADIO)
                gPlayer->setPlayMode(MusicPlayer::PLAYMODE_RADIO);
            else if (m_currentView == MV_PLAYLISTEDITORGALLERY || m_currentView == MV_PLAYLISTEDITORTREE)
                gPlayer->setPlayMode(MusicPlayer::PLAYMODE_TRACKSEDITOR);
            else
                gPlayer->setPlayMode(MusicPlayer::PLAYMODE_TRACKSPLAYLIST);

            gPlayer->restorePosition();
        }
        else
        {
            // if we are playing but we are switching to a view from a different playmode
            // we need to restart playback in the new mode
            if (m_currentView == MV_VISUALIZER || m_currentView == MV_MINIPLAYER || m_currentView == MV_LYRICS)
            {
                // these views can be used in both play modes
            }
            else if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO && m_currentView != MV_RADIO)
            {
                gPlayer->stop(true);

                if (m_currentView == MV_PLAYLISTEDITORGALLERY || m_currentView == MV_PLAYLISTEDITORTREE)
                    gPlayer->setPlayMode(MusicPlayer::PLAYMODE_TRACKSEDITOR);
                else
                    gPlayer->setPlayMode(MusicPlayer::PLAYMODE_TRACKSPLAYLIST);

                gPlayer->restorePosition();
            }
            else if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO && m_currentView == MV_RADIO)
            {
                gPlayer->stop(true);
                gPlayer->setPlayMode(MusicPlayer::PLAYMODE_RADIO);
                gPlayer->restorePosition();
            }
        }
    }

    m_currentTrack = gPlayer->getCurrentTrackPos();

    MusicMetadata *curMeta = gPlayer->getCurrentMetadata();
    if (curMeta)
        updateTrackInfo(curMeta);

    updateProgressBar();

    if (m_currentPlaylist)
        updateUIPlaylist();

    if (m_visualizerVideo)
    {
        m_mainvisual = new MainVisual(m_visualizerVideo);

        m_visualModes = m_mainvisual->getVisualizations();

        m_fullscreenBlank = false;

        m_randomVisualizer = gCoreContext->GetBoolSetting("VisualRandomize", false);

        m_currentVisual = m_mainvisual->getCurrentVisual();

        // sanity check
        if (m_currentVisual >= static_cast<uint>(m_visualModes.count()))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("MusicCommon: Got a bad saved visualizer: %1").arg(m_currentVisual));
            m_currentVisual = 0;
        }

        switchVisualizer(m_currentVisual);

        if (gPlayer->isPlaying())
            startVisualizer();
    }

    m_controlVolume = gCoreContext->GetBoolSetting("MythControlsVolume", false);
    updateVolume();

    if (m_movingTracksState)
        m_movingTracksState->DisplayState("off");

    if (m_stopButton)
        m_stopButton->SetLocked(gPlayer->isStopped());
    if (m_playButton)
        m_playButton->SetLocked(gPlayer->isPlaying());
    if (m_pauseButton)
        m_pauseButton->SetLocked(gPlayer->isPaused());
    if (m_trackState)
    {
        if (gPlayer->isPlaying())
            m_trackState->DisplayState("playing");
        else if (gPlayer->isPaused())
            m_trackState->DisplayState("paused");
        else
            m_trackState->DisplayState("stopped");

    }

    updateShuffleMode();
    updateRepeatMode();

    if (gPlayer->getCurrentPlaylist())
        gPlayer->getCurrentPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                                 m_currentTrack, &m_playlistPlayedTime);

    if (m_playlistProgress)
    {
        m_playlistProgress->SetTotal(m_playlistMaxTime.count());
        m_playlistProgress->SetUsed(0);
    }

    updatePlaylistStats();

    updateUIPlayedList();
}

void MusicCommon::updateRepeatMode(void)
{
    if (m_repeatState)
    {
        switch (gPlayer->getRepeatMode())
        {
            case MusicPlayer::REPEAT_OFF:
                m_repeatState->DisplayState("off");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicRepeat (LCD::MUSIC_REPEAT_NONE);
                break;
            case MusicPlayer::REPEAT_TRACK:
                m_repeatState->DisplayState("track");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicRepeat (LCD::MUSIC_REPEAT_TRACK);
                break;
            case MusicPlayer::REPEAT_ALL:
                m_repeatState->DisplayState("all");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicRepeat (LCD::MUSIC_REPEAT_ALL);
                break;
            default:
                m_repeatState->DisplayState("off");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicRepeat (LCD::MUSIC_REPEAT_NONE);
                break;
        }
    }

    // need this to update the next track info
    MusicMetadata *curMeta = gPlayer->getCurrentMetadata();
    if (curMeta)
        updateTrackInfo(curMeta);
}

void MusicCommon::updateShuffleMode(bool updateUIList)
{
    if (m_shuffleState)
    {
        switch (gPlayer->getShuffleMode())
        {
            case MusicPlayer::SHUFFLE_OFF:
                m_shuffleState->DisplayState("off");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_NONE);
                break;
            case MusicPlayer::SHUFFLE_RANDOM:
                m_shuffleState->DisplayState("random");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_RAND);
                break;
            case MusicPlayer::SHUFFLE_INTELLIGENT:
                m_shuffleState->DisplayState("intelligent");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_SMART);
                break;
            case MusicPlayer::SHUFFLE_ALBUM:
                m_shuffleState->DisplayState("album");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_ALBUM);
                break;
            case MusicPlayer::SHUFFLE_ARTIST:
                m_shuffleState->DisplayState("artist");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_ARTIST);
                break;
            default:
                m_shuffleState->DisplayState("off");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_NONE);
                break;
        }
    }

    if (updateUIList)
    {
        updateUIPlaylist();

        if (gPlayer->getCurrentPlaylist())
            gPlayer->getCurrentPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                                     gPlayer->getCurrentTrackPos(), &m_playlistPlayedTime);
        updatePlaylistStats();

        // need this to update the next track info
        MusicMetadata *curMeta = gPlayer->getCurrentMetadata();
        if (curMeta)
            updateTrackInfo(curMeta);
    }
}

void MusicCommon::switchView(MusicView view)
{
    // can we switch to this view from the current view?
    switch (m_currentView)
    {
        case MV_PLAYLIST:
        {
            if (view != MV_PLAYLISTEDITORTREE && view != MV_PLAYLISTEDITORGALLERY &&
                    view != MV_SEARCH && view != MV_VISUALIZER && view != MV_LYRICS)
                return;
            break;
        }

        case MV_PLAYLISTEDITORTREE:
        {
            if (view != MV_PLAYLISTEDITORGALLERY && view != MV_SEARCH && view != MV_VISUALIZER && view != MV_LYRICS)
                return;
            break;
        }

        case MV_PLAYLISTEDITORGALLERY:
        {
            if (view != MV_PLAYLISTEDITORTREE && view != MV_SEARCH && view != MV_VISUALIZER && view != MV_LYRICS)
                return;
            break;
        }

        case MV_SEARCH:
        {
            if (view != MV_VISUALIZER && view != MV_LYRICS)
                return;
            break;
        }

        case MV_VISUALIZER:
        {
            return;
        }

        case MV_LYRICS:
        {
            if (view != MV_VISUALIZER && view != MV_SEARCH)
                return;
            break;
        }

        case MV_RADIO:
        {
            if (view != MV_VISUALIZER && view != MV_LYRICS)
                return;
            break;
        }

        default:
            return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    stopVisualizer();

    if (m_mainvisual)
    {
        delete m_mainvisual;
        m_mainvisual = nullptr;
    }

    gPlayer->removeListener(this);
    gPlayer->setAllowRestorePos(false);

    switch (view)
    {
        case MV_PLAYLIST:
        {
            auto *plview = new PlaylistView(mainStack, this);

            if (plview->Create())
            {
                mainStack->AddScreen(plview);
                connect(plview, &MythScreenType::Exiting, this, &MusicCommon::viewExited);
            }
            else
            {
                delete plview;
            }

            break;
        }

        case MV_PLAYLISTEDITORTREE:
        {
            // if we are switching playlist editor views save and restore
            // the current position in the tree
            bool restorePos = (m_currentView == MV_PLAYLISTEDITORGALLERY);
            auto *oldView = qobject_cast<PlaylistEditorView *>(this);
            if (oldView)
                oldView->saveTreePosition();

            MythScreenType *parentScreen = (oldView != nullptr ? m_parentScreen : this);

            auto *pleview = new PlaylistEditorView(mainStack, parentScreen, "tree", restorePos);

            if (pleview->Create())
            {
                mainStack->AddScreen(pleview);
                connect(pleview, &MythScreenType::Exiting, this, &MusicCommon::viewExited);
            }
            else
            {
                delete pleview;
            }

            if (oldView)
                Close();

            break;
        }

        case MV_PLAYLISTEDITORGALLERY:
        {
            // if we are switching playlist editor views save and restore
            // the current position in the tree
            bool restorePos = (m_currentView == MV_PLAYLISTEDITORTREE);
            auto *oldView = qobject_cast<PlaylistEditorView *>(this);
            if (oldView)
                oldView->saveTreePosition();

            MythScreenType *parentScreen = (oldView != nullptr ? m_parentScreen : this);

            auto *pleview = new PlaylistEditorView(mainStack, parentScreen, "gallery", restorePos);

            if (pleview->Create())
            {
                mainStack->AddScreen(pleview);
                connect(pleview, &MythScreenType::Exiting, this, &MusicCommon::viewExited);
            }
            else
            {
                delete pleview;
            }

            if (oldView)
                Close();

            break;
        }

        case MV_SEARCH:
        {
            auto *sview = new SearchView(mainStack, this);

            if (sview->Create())
            {
                mainStack->AddScreen(sview);
                connect(sview, &MythScreenType::Exiting, this, &MusicCommon::viewExited);
            }
            else
            {
                delete sview;
            }

            break;
        }

        case MV_VISUALIZER:
        {
            auto *vview = new VisualizerView(mainStack, this);

            if (vview->Create())
            {
                mainStack->AddScreen(vview);
                connect(vview, &MythScreenType::Exiting, this, &MusicCommon::viewExited);
            }
            else
            {
                delete vview;
            }

            break;
        }

        case MV_LYRICS:
        {
            auto *lview = new LyricsView(mainStack, this);

            if (lview->Create())
            {
                mainStack->AddScreen(lview);
                connect(lview, &MythScreenType::Exiting, this, &MusicCommon::viewExited);
            }
            else
            {
                delete lview;
            }

            break;
        }

        default:
            break;
    }

    gPlayer->setAllowRestorePos(true);
}

void MusicCommon::viewExited(void)
{
    init(false);
}

bool MusicCommon::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    // if there is a pending jump point pass the key press to the default handler
    if (GetMythMainWindow()->IsExitingToMain())
    {
        gPlayer->savePosition();

        // do we need to stop playing?
        if (gPlayer->isPlaying() && gCoreContext->GetSetting("MusicJumpPointAction", "stop") == "stop")
            gPlayer->stop(true);

        return MythScreenType::keyPressEvent(e);
    }

    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", e, actions, true);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        // if we are currently moving an item,
        // we only accept UP/DOWN/SELECT/ESCAPE
        if (m_moveTrackMode && m_movingTrack)
        {
            MythUIButtonListItem *item = m_currentPlaylist->GetItemCurrent();
            if (!item)
                return false;

            if (action == "SELECT" || action == "ESCAPE")
            {
                m_movingTrack = false;
                item->DisplayState("off", "movestate");
            }
            else if (action == "UP")
            {
                gPlayer->moveTrackUpDown(true, m_currentPlaylist->GetCurrentPos());
                item->MoveUpDown(true);
                m_currentTrack = gPlayer->getCurrentTrackPos();
                if (gPlayer->getCurrentPlaylist())
                    gPlayer->getCurrentPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                                             m_currentTrack, &m_playlistPlayedTime);
                updatePlaylistStats();
                updateTrackInfo(gPlayer->getCurrentMetadata());
            }
            else if (action == "DOWN")
            {
                gPlayer->moveTrackUpDown(false, m_currentPlaylist->GetCurrentPos());
                item->MoveUpDown(false);
                m_currentTrack = gPlayer->getCurrentTrackPos();
                if (gPlayer->getCurrentPlaylist())
                    gPlayer->getCurrentPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                                             m_currentTrack, &m_playlistPlayedTime);
                updatePlaylistStats();
                updateTrackInfo(gPlayer->getCurrentMetadata());
            }

            return true;
        }

        if (action == "ESCAPE")
        {
            // if we was started from another music view screen return to it
            if (m_parentScreen)
            {
                handled = false;
            }
            else
            {
                // this is the top music view screen so prompt to continue playing
                QString exit_action = gCoreContext->GetSetting("MusicExitAction", "prompt");

                if (!gPlayer->isPlaying())
                {
                    gPlayer->savePosition();
                    stopAll();
                    Close();
                }
                else
                {
                    if (exit_action == "stop")
                    {
                        gPlayer->savePosition();
                        stopAll();
                        Close();
                    }
                    else if (exit_action == "play")
                    {
                        Close();
                    }
                    else
                    {
                        showExitMenu();
                    }
                }
            }
        }
        else if (action == "THMBUP")
        {
            changeRating(true);
        }
        else if (action == "THMBDOWN")
        {
            changeRating(false);
        }
        else if (action == "NEXTTRACK")
        {
            if (m_nextButton)
                m_nextButton->Push();
            else
                next();
        }
        else if (action == "PREVTRACK")
        {
            if (m_prevButton)
                m_prevButton->Push();
            else
                previous();
        }
        else if (action == "FFWD")
        {
            if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
            {
                if (m_ffButton)
                    m_ffButton->Push();
                else
                    seekforward();
            }
        }
        else if (action == "RWND")
        {
            if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
            {
                if (m_rewButton)
                    m_rewButton->Push();
                else
                    seekback();
            }
        }
        else if (action == "PAUSE")
        {
            if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO && gPlayer->isPlaying())
            {
                // ignore if we are already playing a radio stream
            }
            else if (gPlayer->isPlaying() || (gPlayer->getOutput() && gPlayer->getOutput()->IsPaused()))
            {
                // if we are playing or are paused PAUSE will toggle the pause state
                if (m_pauseButton)
                    m_pauseButton->Push();
                else
                    pause();
            }
            else
            {
                // not playing or paused so PAUSE acts the same as PLAY
                if (m_playButton)
                    m_playButton->Push();
                else
                    play();
            }
        }
        else if (action == "PLAY")
        {
            if (m_playButton)
                m_playButton->Push();
            else
                play();
        }
        else if (action == "STOP")
        {
            if (m_stopButton)
                m_stopButton->Push();
            else
                stop();
            m_currentTime = 0s;
        }
        else if (action == "CYCLEVIS")
        {
            cycleVisualizer();
        }
        else if (action == "BLANKSCR")
        {
            // change to the blank visualizer
            if (m_mainvisual)
                switchVisualizer("Blank");

            // switch to the full screen visualiser view
            if (m_currentView != MV_VISUALIZER)
                switchView(MV_VISUALIZER);
        }
        else if (action == "VOLUMEDOWN")
        {
            changeVolume(false);
        }
        else if (action == "VOLUMEUP")
        {
            changeVolume(true);
        }
        else if (action == "SPEEDDOWN")
        {
            changeSpeed(false);
        }
        else if (action == "SPEEDUP")
        {
            changeSpeed(true);
        }
        else if (action == "MUTE")
        {
            toggleMute();
        }
        else if (action == "TOGGLEUPMIX")
        {
            toggleUpmix();
        }
        else if (action == "INFO" || action == "EDIT")
        {
            if (m_currentPlaylist && GetFocusWidget() == m_currentPlaylist)
            {
                if (m_currentPlaylist->GetItemCurrent())
                {
                    auto *mdata = m_currentPlaylist->GetItemCurrent()->GetData().value<MusicMetadata*>();
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
                if (action == "INFO")
                    showTrackInfo(gPlayer->getCurrentMetadata());
                else
                    editTrackInfo(gPlayer->getCurrentMetadata());
            }
        }
        else if (action == "DELETE" && m_currentPlaylist && GetFocusWidget() == m_currentPlaylist)
        {
            MythUIButtonListItem *item = m_currentPlaylist->GetItemCurrent();
            if (item)
            {
                auto *mdata = item->GetData().value<MusicMetadata*>();
                if (mdata)
                    gPlayer->removeTrack(mdata->ID());
            }
        }
        else if (action == "MENU")
        {
            ShowMenu();
        }
        else if (action == "REFRESH")
        {
            if (m_currentPlaylist)
                m_currentPlaylist->SetItemCurrent(m_currentTrack);
        }
        else if (action == "MARK")
        {
            if (!m_moveTrackMode)
            {
                m_moveTrackMode = true;
                m_movingTrack = false;

                if (m_movingTracksState)
                    m_movingTracksState->DisplayState((m_moveTrackMode ? "on" : "off"));
            }
            else
            {
                m_moveTrackMode = false;

                if (m_currentPlaylist && m_movingTrack)
                {
                    MythUIButtonListItem *item = m_currentPlaylist->GetItemCurrent();
                    if (item)
                        item->DisplayState("off", "movestate");

                    m_movingTrack = false;
                }

                if (m_movingTracksState)
                    m_movingTracksState->DisplayState((m_moveTrackMode ? "on" : "off"));
            }
        }
        else if (action == "SWITCHTOPLAYLIST" && m_currentView != MV_PLAYLIST)
        {
            switchView(MV_PLAYLIST);
        }
        else if (action == "SWITCHTOPLAYLISTEDITORTREE" && m_currentView != MV_PLAYLISTEDITORTREE)
        {
            switchView(MV_PLAYLISTEDITORTREE);
        }
        else if (action == "SWITCHTOPLAYLISTEDITORGALLERY" && m_currentView != MV_PLAYLISTEDITORGALLERY)
        {
            switchView(MV_PLAYLISTEDITORGALLERY);
        }
        else if (action == "SWITCHTOSEARCH" && m_currentView != MV_SEARCH)
        {
            switchView(MV_SEARCH);
        }
        else if (action == "SWITCHTOVISUALISER" && m_currentView != MV_VISUALIZER)
        {
            switchView(MV_VISUALIZER);
        }
        else if (action == "SWITCHTORADIO" && m_currentView != MV_RADIO)
        {
            switchView(MV_RADIO);
        }
        else if (action == "TOGGLESHUFFLE")
        {
            gPlayer->toggleShuffleMode();
            updateShuffleMode(true);
        }
        else if (action == "TOGGLEREPEAT")
        {
            gPlayer->toggleRepeatMode();
            updateRepeatMode();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(e))
        handled = true;
    return handled;
}

void MusicCommon::changeVolume(bool up) const
{
    if (m_controlVolume && gPlayer->getOutput())
    {
        if (up)
            gPlayer->incVolume();
        else
            gPlayer->decVolume();
        showVolume();
    }
}

void MusicCommon::changeSpeed(bool up)
{
    if (gPlayer->getOutput() && gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
    {
        if (up)
            gPlayer->incSpeed();
        else
            gPlayer->decSpeed();
        showSpeed(true);
        updatePlaylistStats(); // update trackspeed and map for templates
    }
}

void MusicCommon::toggleMute() const
{
    if (m_controlVolume)
    {
        gPlayer->toggleMute();
        showVolume();
    }
}

void MusicCommon::toggleUpmix()
{
    if (gPlayer->getOutput())
        gPlayer->getOutput()->ToggleUpmix();
}

void MusicCommon::updateProgressBar()
{
    if (!m_trackProgress)
        return;

    if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO)
    {
        // radio mode so show the buffer fill level since we don't know the track length
        int available = 0;
        int maxSize = 0;
        gPlayer->getBufferStatus(&available, &maxSize);

        if (m_infoText)
        {
            QString status = QString("%1%").arg((int)(100.0 / ((double)maxSize / (double)available)));
            m_infoText->SetText(status);
        }

        if (m_trackProgress)
        {
            m_trackProgress->SetTotal(maxSize);
            m_trackProgress->SetUsed(available);
        }
    }
    else
    {
        // show the track played time
        int percentplayed = 1;
        if (m_maxTime > 0s)
            percentplayed = m_currentTime * 100 / m_maxTime;
        m_trackProgress->SetTotal(100);
        m_trackProgress->SetUsed(percentplayed);
    }
}

void MusicCommon::showVolume(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *vol = new MythMusicVolumeDialog(popupStack, "volumepopup");

    if (!vol->Create())
    {
        delete vol;
        return;
    }

    popupStack->AddScreen(vol);
}

void MusicCommon::showSpeed([[maybe_unused]] bool show)
{
}

void MusicCommon::switchVisualizer(const QString &visual)
{
    switchVisualizer(m_visualModes.indexOf(visual));
}

void MusicCommon::switchVisualizer(int visual)
{
    if (!m_mainvisual)
        return;

    if (visual < 0 || visual > m_visualModes.count() - 1)
        visual = 0;

    m_currentVisual = visual;

    m_mainvisual->setVisual(m_visualModes[m_currentVisual]);

    if (m_visualText)
        m_visualText->SetText(m_visualModes[m_currentVisual]);
}

void MusicCommon::cycleVisualizer(void)
{
    if (!m_mainvisual)
        return;

    // Only change the visualizer if there is more than 1 visualizer
    // and the user currently has a visualizer active
    if (m_visualModes.count() > 1)
    {
        if (m_randomVisualizer)
        {
            unsigned int next_visualizer = m_currentVisual;

            //Find a visual thats not like the previous visual
            while (next_visualizer == m_currentVisual)
                next_visualizer = MythRandom(0, m_visualModes.count() - 1);
            m_currentVisual = next_visualizer;
        }
        else
        {
            //Change to the next selected visual
            m_currentVisual = (m_currentVisual + 1) % m_visualModes.count();
        }

        //Change to the new visualizer
        switchVisualizer(m_currentVisual);
    }
}

void MusicCommon::startVisualizer(void)
{
    if (!m_visualizerVideo || !m_mainvisual)
        return;

    gPlayer->addVisual(m_mainvisual);
}

void MusicCommon::stopVisualizer(void )
{
    if (!m_visualizerVideo || !m_mainvisual)
        return;

    gPlayer->removeVisual(m_mainvisual);
}

void MusicCommon::setTrackOnLCD(MusicMetadata *mdata)
{
    LCD *lcd = LCD::Get();
    if (!lcd || !mdata)
        return;

    // Set the Artist and Tract on the LCD
    lcd->switchToMusic(mdata->Artist(),
                       mdata->Album(),
                       mdata->Title());
}

void MusicCommon::stopAll()
{
    if (LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
    }

    stopVisualizer();

    gPlayer->stop(true);
}

void MusicCommon::play()
{
    gPlayer->play();
}

void MusicCommon::pause(void)
{
    gPlayer->pause();
}

void MusicCommon::stop(void)
{
    gPlayer->stop();

    if (m_timeText)
        m_timeText->SetText(getTimeString(m_maxTime, 0s));
    if (m_infoText)
        m_infoText->Reset();
}

void MusicCommon::next()
{
    if (m_cycleVisualizer)
        cycleVisualizer();


    gPlayer->next();
}

void MusicCommon::previous()
{
    if (m_cycleVisualizer)
        cycleVisualizer();

    gPlayer->previous();
}

void MusicCommon::seekforward()
{
    std::chrono::seconds nextTime = m_currentTime + 5s;
    nextTime = std::clamp(nextTime, 0s, m_maxTime);
    seek(nextTime);
}

void MusicCommon::seekback()
{
    // I don't know why, but seeking before 00:05 fails.  Repeated
    // rewind under 00:05 can hold time < 5s while the music plays.
    // Time starts incrementing from zero but is now several seconds
    // behind.  Finishing a track after this records a truncated
    // length.  We can workaround this by limiting rewind to 1s.
    std::chrono::seconds nextTime = m_currentTime - 5s;
    nextTime = std::clamp(nextTime, 1s, m_maxTime); // #787
    seek(nextTime);
}

void MusicCommon::seek(std::chrono::seconds pos)
{
    if (gPlayer->getOutput())
    {
        Decoder *decoder = gPlayer->getDecoder();
        if (decoder && decoder->isRunning())
        {
            decoder->lock();
            decoder->seek(pos.count());

            if (m_mainvisual)
            {
                m_mainvisual->mutex()->lock();
                m_mainvisual->prepare();
                m_mainvisual->mutex()->unlock();
            }

            decoder->unlock();
        }

        gPlayer->getOutput()->SetTimecode(pos);

        if (!gPlayer->isPlaying())
        {
            m_currentTime = pos;
            if (m_timeText)
                m_timeText->SetText(getTimeString(pos, m_maxTime));

            updateProgressBar();

            if (LCD *lcd = LCD::Get())
            {
                float percent_heard = m_maxTime <= 0s ? 0.0F : ((float)pos.count() /
                                      (float)m_maxTime.count());

                QString lcd_time_string = getTimeString(pos, m_maxTime);

                // if the string is longer than the LCD width, remove all spaces
                if (lcd_time_string.length() > lcd->getLCDWidth())
                    lcd_time_string.remove(' ');

                lcd->setMusicProgress(lcd_time_string, percent_heard);
            }
        }
    }
}

void MusicCommon::changeRating(bool increase)
{
    if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO)
        return;

    // Rationale here is that if you can't get visual feedback on ratings
    // adjustments, you probably should not be changing them
    // TODO: should check if the rating is visible in the playlist buttontlist
    //if (!m_ratingState)
    //    return;

    MusicMetadata *curMeta = gPlayer->getCurrentMetadata();
    if (!curMeta)
        return;


    if (increase)
        curMeta->incRating();
    else
        curMeta->decRating();

    gPlayer->sendTrackStatsChangedEvent(curMeta->ID());
}

void MusicCommon::customEvent(QEvent *event)
{
    QString statusString;

    if (event->type() == OutputEvent::kPlaying)
    {
        MusicMetadata *curMeta = gPlayer->getCurrentMetadata();
        if (curMeta)
            updateTrackInfo(curMeta);

        statusString = tr("Playing stream.");
        if (gPlayer->isPlaying())
        {
            if (m_stopButton)
                m_stopButton->SetLocked(false);
            if (m_playButton)
                m_playButton->SetLocked(true);
            if (m_pauseButton)
                m_pauseButton->SetLocked(false);
            if (m_trackState)
                m_trackState->DisplayState("playing");

            if (m_currentPlaylist)
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(m_currentTrack);
                if (item)
                {
                    item->SetFontState("running");
                    item->DisplayState("playing", "playstate");
                }
            }

            startVisualizer();

            updateVolume();
        }
    }
    else if (event->type() == OutputEvent::kBuffering)
    {
        statusString = tr("Buffering stream.");
    }
    else if (event->type() == OutputEvent::kPaused)
    {
        statusString = tr("Stream paused.");

        if (m_stopButton)
            m_stopButton->SetLocked(false);
        if (m_playButton)
            m_playButton->SetLocked(false);
        if (m_pauseButton)
            m_pauseButton->SetLocked(true);
        if (m_trackState)
            m_trackState->DisplayState("paused");

        if (m_currentPlaylist)
        {
            MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(m_currentTrack);
            if (item)
            {
                item->SetFontState("idle");
                item->DisplayState("paused", "playstate");
            }
        }
    }
    else if (event->type() == OutputEvent::kInfo)
    {

        auto *oe = dynamic_cast<OutputEvent *>(event);

        if (!oe)
            return;

        std::chrono::seconds rs = 0s;
        MusicMetadata *curMeta = gPlayer->getCurrentMetadata();

        if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO)
        {
            if (curMeta)
                m_currentTime = rs = duration_cast<std::chrono::seconds>(curMeta->Length());
            else
                m_currentTime = 0s;
        }
        else
        {
            m_currentTime = rs = oe->elapsedSeconds();
        }

        QString time_string = getTimeString(rs, m_maxTime);

        updateProgressBar();

        if (curMeta)
        {
            if (LCD *lcd = LCD::Get())
            {
                float percent_heard = m_maxTime <= 0s ?
                    0.0F:((float)rs.count() / (float)curMeta->Length().count()) * 1000.0F;

                QString lcd_time_string = time_string;

                // if the string is longer than the LCD width, remove all spaces
                if (time_string.length() > lcd->getLCDWidth())
                    lcd_time_string.remove(' ');

                lcd->setMusicProgress(lcd_time_string, percent_heard);
            }
        }

        QString info_string;

        //  Hack around for cd bitrates
        if (oe->bitrate() < 2000)
        {
            info_string = QString("%1 "+tr("kbps")+ "   %2 "+ tr("kHz")+ "   %3 "+ tr("ch"))
                .arg(oe->bitrate())
                .arg(static_cast<double>(oe->frequency()) / 1000.0,0,'f',1,QChar('0'))
                .arg(oe->channels() > 1 ? "2" : "1");
        }
        else
        {
            info_string = QString("%1 "+ tr("kHz")+ "   %2 "+ tr("ch"))
                .arg(static_cast<double>(oe->frequency()) / 1000.0,0,'f',1,QChar('0'))
                .arg(oe->channels() > 1 ? "2" : "1");
        }

        if (curMeta)
        {
            if (m_timeText)
                m_timeText->SetText(time_string);
            if (m_infoText)
                m_infoText->SetText(info_string);
        }

        // TODO only need to update the playlist times here
        updatePlaylistStats();
    }
    else if (event->type() == OutputEvent::kStopped)
    {
        statusString = tr("Stream stopped.");
        if (m_stopButton)
            m_stopButton->SetLocked(true);
        if (m_playButton)
            m_playButton->SetLocked(false);
        if (m_pauseButton)
            m_pauseButton->SetLocked(false);
        if (m_trackState)
            m_trackState->DisplayState("stopped");

        if (m_currentPlaylist)
        {
            MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(m_currentTrack);
            if (item)
            {
                item->SetFontState("normal");
                item->DisplayState("stopped", "playstate");
            }
        }

        stopVisualizer();
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
            if (resulttext == tr("Fullscreen Visualizer"))
                switchView(MV_VISUALIZER);
            else if (resulttext == tr("Playlist Editor") ||
                     resulttext == tr("Browse Music Library"))
            {
                if (gCoreContext->GetSetting("MusicPlaylistEditorView", "tree") ==  "tree")
                    switchView(MV_PLAYLISTEDITORTREE);
                else
                    switchView(MV_PLAYLISTEDITORGALLERY);
            }
            else if (resulttext == tr("Search for Music"))
            {
                switchView(MV_SEARCH);
            }
            else if (resulttext == tr("Switch To Gallery View"))
            {
                switchView(MV_PLAYLISTEDITORGALLERY);
            }
            else if (resulttext == tr("Switch To Tree View"))
            {
                switchView(MV_PLAYLISTEDITORTREE);
            }
            else if (resulttext == tr("Lyrics"))
            {
                switchView(MV_LYRICS);
            }
        }
        else if (resultid == "submenu")
        {
            if (resulttext == tr("Search List..."))
                searchButtonList();
        }
        else if (resultid == "playlistmenu")
        {
            if (resulttext == tr("Sync List With Current Track"))
            {
                m_currentPlaylist->SetItemCurrent(m_currentTrack);
            }
            else if (resulttext == tr("Remove Selected Track"))
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemCurrent();
                if (item)
                {
                    auto *mdata = item->GetData().value<MusicMetadata*>();
                    if (mdata)
                        gPlayer->removeTrack(mdata->ID());
                }
            }
            else if (resulttext == tr("Remove All Tracks"))
            {
                if (gPlayer->getCurrentPlaylist())
                {
                    gPlayer->getCurrentPlaylist()->removeAllTracks();
                    gPlayer->activePlaylistChanged(-1, true);
                }
            }
            else if (resulttext == tr("Save To New Playlist"))
            {
                QString message = tr("Enter new playlist name");

                MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

                auto *inputdialog = new MythTextInputDialog(popupStack, message);

                if (inputdialog->Create())
                {
                    inputdialog->SetReturnEvent(this, "addplaylist");
                    popupStack->AddScreen(inputdialog);
                }
                else
                {
                    delete inputdialog;
                }
            }
            else if (resulttext == tr("Save To Existing Playlist"))
            {
                QString message = tr("Select the playlist to save to");
                QStringList playlists = gMusicData->m_all_playlists->getPlaylistNames();

                MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

                auto *searchdialog = new MythUISearchDialog(popupStack, message, playlists);

                if (searchdialog->Create())
                {
                    searchdialog->SetReturnEvent(this, "updateplaylist");
                    popupStack->AddScreen(searchdialog);
                }
            }
            else if (resulttext == tr("Switch To Move Mode"))
            {
                m_moveTrackMode = true;
                m_movingTrack = false;

                if (m_movingTracksState)
                    m_movingTracksState->DisplayState((m_moveTrackMode ? "on" : "off"));
            }
            else if (resulttext == tr("Switch To Select Mode"))
            {
                m_moveTrackMode = false;

                if (m_currentPlaylist && m_movingTrack)
                {
                    MythUIButtonListItem *item = m_currentPlaylist->GetItemCurrent();
                    if (item)
                        item->DisplayState("off", "movestate");

                    m_movingTrack = false;
                }

                if (m_movingTracksState)
                    m_movingTracksState->DisplayState((m_moveTrackMode ? "on" : "off"));
            }
        }
        else if (resultid == "repeatmenu")
        {
            int mode = dce->GetData().toInt();
            gPlayer->setRepeatMode((MusicPlayer::RepeatMode) mode);
            updateRepeatMode();
        }
        else if (resultid == "shufflemenu")
        {
            int mode = dce->GetData().toInt();
            gPlayer->setShuffleMode((MusicPlayer::ShuffleMode) mode);
            updateShuffleMode(true);
        }
        else if (resultid == "exitmenu")
        {
            if (resulttext == tr("No - Exit, Stop Playing"))
            {
                gPlayer->savePosition();
                stopAll();
                Close();
            }
            else if (resulttext == tr("Yes - Exit, Continue Playing"))
            {
                Close();
            }
        }
        else if (resultid == "playermenu")
        {
            if (resulttext == tr("Change Volume"))
                showVolume();
            else if (resulttext ==  tr("Mute"))
                toggleMute();
            else if (resulttext == tr("Previous Track"))
                previous();
            else if (resulttext == tr("Next Track"))
                next();
            else if (resulttext == tr("Jump Back"))
                seekback();
            else if (resulttext == tr("Jump Forward"))
                seekforward();
            else if (resulttext == tr("Play"))
                play();
            else if (resulttext == tr("Stop"))
                stop();
            else if (resulttext == tr("Pause"))
                pause();
        }
        else if (resultid == "quickplaylistmenu")
        {
            if (resulttext == tr("All Tracks"))
                allTracks();
            else if (resulttext == tr("From CD"))
                fromCD();
            else if (resulttext ==  tr("Tracks By Current Artist"))
                byArtist();
            else if (resulttext == tr("Tracks From Current Genre"))
                byGenre();
            else if (resulttext == tr("Tracks From Current Album"))
                byAlbum();
            else if (resulttext == tr("Tracks From Current Year"))
                byYear();
            else if (resulttext == tr("Tracks With Same Title"))
                byTitle();
        }
        else if (resultid == "playlistoptionsmenu")
        {
            if (resulttext == tr("Replace Tracks"))
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
            {     // cancel shuffles and repeats to play only this now
                gPlayer->setShuffleMode(MusicPlayer::SHUFFLE_OFF);
                updateShuffleMode();
                gPlayer->setRepeatMode(MusicPlayer::REPEAT_OFF);
                updateRepeatMode();
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                m_playlistOptions.playPLOption = PL_FIRSTNEW;
                doUpdatePlaylist();
            }
            else if (resulttext == tr("Prefer Play Now"))
            {
                MusicPlayer::setPlayNow(true);
            }
            else if (resulttext == tr("Prefer Add Tracks"))
            {
                MusicPlayer::setPlayNow(false);
            }
        }
        else if (resultid == "visualizermenu")
        {
            if (dce->GetResult() >= 0)
            {
                m_currentVisual = dce->GetData().toInt();

                //Change to the new visualizer
                switchVisualizer(m_currentVisual);
            }
        }
        else if (resultid == "addplaylist")
        {
            gMusicData->m_all_playlists->copyNewPlaylist(resulttext);
            Playlist *playlist = gMusicData->m_all_playlists->getPlaylist(resulttext);
            gPlayer->playlistChanged(playlist->getID());
        }
        else if (resultid == "updateplaylist")
        {
            if (gPlayer->getCurrentPlaylist())
            {
                Playlist *playlist = gMusicData->m_all_playlists->getPlaylist(resulttext);
                QString songList = gPlayer->getCurrentPlaylist()->toRawSonglist();
                playlist->removeAllTracks();
                playlist->fillSongsFromSonglist(songList);
                playlist->changed();
                gPlayer->playlistChanged(playlist->getID());
            }
        }
    }
    else if (event->type() == MusicPlayerEvent::kTrackChangeEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackNo = mpe->m_trackID;

        if (m_currentPlaylist)
        {
            if (m_currentTrack >= 0 && m_currentTrack < m_currentPlaylist->GetCount())
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(m_currentTrack);
                if (item)
                {
                    item->SetFontState("normal");
                    item->DisplayState("default", "playstate");
                }
            }

            if (trackNo >= 0 && trackNo < m_currentPlaylist->GetCount())
            {
                if (m_currentTrack == m_currentPlaylist->GetCurrentPos())
                    m_currentPlaylist->SetItemCurrent(trackNo);

                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(trackNo);
                if (item)
                {
                    item->SetFontState("running");
                    item->DisplayState("playing", "playstate");
                }
            }
        }

        m_currentTrack = trackNo;

        if (gPlayer->getCurrentPlaylist())
            gPlayer->getCurrentPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                                     m_currentTrack, &m_playlistPlayedTime);
        if (m_playlistProgress)
        {
            m_playlistProgress->SetTotal(m_playlistMaxTime.count());
            m_playlistProgress->SetUsed(0);
        }

        updatePlaylistStats();
        updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == MusicPlayerEvent::kVolumeChangeEvent)
    {
        updateVolume();
    }
    else if (event->type() == MusicPlayerEvent::kTrackRemovedEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackID = mpe->m_trackID;

        if (m_currentPlaylist)
        {
            // find and remove the list item for the removed track
            for (int x = 0; x < m_currentPlaylist->GetCount(); x++)
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(x);
                auto *mdata = item->GetData().value<MusicMetadata*>();
                if (mdata && mdata->ID() == (MusicMetadata::IdType) trackID)
                {
                    m_currentPlaylist->RemoveItem(item);
                    x -= 1;  // remove all entries, or:
                    // break; // remove only first entry
                }
            }
        }

        m_currentTrack = gPlayer->getCurrentTrackPos();

        // if we have just removed the playing track from the playlist
        // move to the next track
        if (gPlayer->getCurrentMetadata())
        {
            if (gPlayer->getCurrentMetadata()->ID() == (MusicMetadata::IdType) trackID)
                gPlayer->next();
        }

        if (gPlayer->getCurrentPlaylist())
            gPlayer->getCurrentPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                                     m_currentTrack, &m_playlistPlayedTime);
        updatePlaylistStats();
        updateTrackInfo(gPlayer->getCurrentMetadata());

        if (m_noTracksText && gPlayer->getCurrentPlaylist())
            m_noTracksText->SetVisible((gPlayer->getCurrentPlaylist()->getTrackCount() == 0));
    }
    else if (event->type() == MusicPlayerEvent::kTrackAddedEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackID = mpe->m_trackID;

        if (m_currentPlaylist)
        {
            if (trackID == -1)
            {
                // more than one track has been added so easier to just reload the list
                updateUIPlaylist();
            }
            else
            {
                // just one track was added so just add that track to the end of the list
                MusicMetadata *mdata = gMusicData->m_all_music->getMetadata(trackID);

                if (mdata)
                {
                    InfoMap metadataMap;
                    mdata->toMap(metadataMap);

                    auto *item = new MythUIButtonListItem(m_currentPlaylist, "",
                                                          QVariant::fromValue(mdata));

                    item->SetTextFromMap(metadataMap);

                    if (gPlayer->isPlaying() && gPlayer->getCurrentMetadata() &&
                        mdata->ID() == gPlayer->getCurrentMetadata()->ID())
                    {
                        item->SetFontState("running");
                        item->DisplayState("playing", "playstate");
                    }
                    else
                    {
                        item->SetFontState("normal");
                        item->DisplayState("default", "playstate");
                    }

                    if (!gPlayer->getCurrentMetadata())
                        gPlayer->changeCurrentTrack(0);
                }

                if (m_noTracksText && gPlayer->getCurrentPlaylist())
                    m_noTracksText->SetVisible((gPlayer->getCurrentPlaylist()->getTrackCount() == 0));
            }
        }

        if (gPlayer->getCurrentPlaylist())
            gPlayer->getCurrentPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                                     m_currentTrack, &m_playlistPlayedTime);

        updateUIPlaylist();     // else album art doesn't update
        updatePlaylistStats();
        updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == MusicPlayerEvent::kAllTracksRemovedEvent)
    {
        updateUIPlaylist();
        updatePlaylistStats();
        updateTrackInfo(nullptr);
    }
    else if (event->type() == MusicPlayerEvent::kMetadataChangedEvent ||
             event->type() == MusicPlayerEvent::kTrackStatsChangedEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        uint trackID = mpe->m_trackID;

        if (m_currentPlaylist)
        {
            for (int x = 0; x < m_currentPlaylist->GetCount(); x++)
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(x);
                auto *mdata = item->GetData().value<MusicMetadata*>();

                if (mdata && mdata->ID() == trackID)
                {
                    InfoMap metadataMap;
                    mdata->toMap(metadataMap);
                    item->SetTextFromMap(metadataMap);

                    item->DisplayState(QString("%1").arg(mdata->Rating()), "ratingstate");
                }
            }
        }

        if (m_playedTracksList)
        {
            for (int x = 0; x < m_playedTracksList->GetCount(); x++)
            {
                MythUIButtonListItem *item = m_playedTracksList->GetItemAt(x);
                auto *mdata = item->GetData().value<MusicMetadata*>();

                if (mdata && mdata->ID() == trackID)
                {
                    InfoMap metadataMap;
                    mdata->toMap(metadataMap);
                    item->SetTextFromMap(metadataMap);
                }
            }
        }

        if (gPlayer->getCurrentMetadata() && trackID == gPlayer->getCurrentMetadata()->ID())
            updateTrackInfo(gPlayer->getCurrentMetadata());

        // this will ensure the next track info gets updated
        if (gPlayer->getNextMetadata() && trackID == gPlayer->getNextMetadata()->ID())
            updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == MusicPlayerEvent::kAlbumArtChangedEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        uint trackID = mpe->m_trackID;

        if (m_currentPlaylist)
        {
            for (int x = 0; x < m_currentPlaylist->GetCount(); x++)
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(x);
                auto *mdata = item->GetData().value<MusicMetadata*>();
                if (mdata && mdata->ID() == trackID)
                {
                    // reload the albumart image if one has already been loaded for this track
                    if (!item->GetImageFilename().isEmpty())
                    {
                        QString artFile = mdata->getAlbumArtFile();
                        if (artFile.isEmpty())
                        {
                            item->SetImage("");
                            item->SetImage("", "coverart");
                        }
                        else
                        {
                            item->SetImage(mdata->getAlbumArtFile());
                            item->SetImage(mdata->getAlbumArtFile(), "coverart");
                        }
                    }
                }
            }
        }

        if (gPlayer->getCurrentMetadata() && trackID == gPlayer->getCurrentMetadata()->ID())
            updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == MusicPlayerEvent::kTrackUnavailableEvent)
    {
        auto *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        uint trackID = mpe->m_trackID;

        if (m_currentPlaylist)
        {
            for (int x = 0; x < m_currentPlaylist->GetCount(); x++)
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(x);
                auto *mdata = item->GetData().value<MusicMetadata*>();
                if (mdata && mdata->ID() == trackID)
                {
                    item->SetFontState("disabled");
                    item->DisplayState("unavailable", "playstate");
                }
            }
        }
    }
}

void MusicCommon::updateVolume(void)
{
    if (!m_controlVolume)
    {
        if (m_volumeText)
            m_volumeText->Hide();

        if (m_muteState)
            m_muteState->Hide();

        return;
    }

    if (m_volumeText)
    {
        InfoMap map;
        gPlayer->toMap(map);
        m_volumeText->SetTextFromMap(map);
    }

    if (m_muteState)
    {
        bool muted = gPlayer->isMuted();
        m_muteState->DisplayState(muted ? "on" : "off");
    }
}

void MusicCommon::editTrackInfo(MusicMetadata *mdata)
{
    if (!mdata)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *editDialog = new EditMetadataDialog(mainStack, mdata);

    if (!editDialog->Create())
    {
        delete editDialog;
        return;
    }

    mainStack->AddScreen(editDialog);
}

void MusicCommon::updateTrackInfo(MusicMetadata *mdata)
{
    if (!mdata)
    {
        InfoMap metadataMap;
        MusicMetadata metadata;
        metadata.toMap(metadataMap);
        metadata.toMap(metadataMap, "next");
        ResetMap(metadataMap);

        if (m_coverartImage)
            m_coverartImage->Reset();
        if (m_ratingState)
            m_ratingState->DisplayState("0");
        if (m_timeText)
            m_timeText->Reset();
        if (m_infoText)
            m_infoText->Reset();
        if (m_trackProgress)
            m_trackProgress->SetUsed(0);

        if (m_mainvisual)
            m_mainvisual->setVisual(m_visualModes[m_currentVisual]);

        return;
    }

    if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO)
        m_maxTime = 0s;
    else
        m_maxTime = duration_cast<std::chrono::seconds>(mdata->Length());

    // get map for current track
    InfoMap metadataMap;
    mdata->toMap(metadataMap);

    // add the map from the next track
    MusicMetadata *nextMetadata = gPlayer->getNextMetadata();
    if (nextMetadata)
        nextMetadata->toMap(metadataMap, "next");

    // now set text using the map
    SetTextFromMap(metadataMap);

    if (m_coverartImage)
    {
        QString filename = mdata->getAlbumArtFile();
        if (!filename.isEmpty())
        {
            m_coverartImage->SetFilename(filename);
            m_coverartImage->Load();
        }
        else
        {
            m_coverartImage->Reset();
        }
    }

    if (m_ratingState)
        m_ratingState->DisplayState(QString("%1").arg(mdata->Rating()));

    setTrackOnLCD(mdata);
}

void MusicCommon::showTrackInfo(MusicMetadata *mdata)
{
    if (!mdata)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *dlg = new TrackInfoDialog(popupStack, mdata, "trackinfopopup");

    if (!dlg->Create())
    {
        delete dlg;
        return;
    }

    popupStack->AddScreen(dlg);
}

void MusicCommon::playlistItemClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_moveTrackMode)
    {
        m_movingTrack = !m_movingTrack;

        if (m_movingTrack)
            item->DisplayState("on", "movestate");
        else
            item->DisplayState("off", "movestate");
    }
    else
    {
        gPlayer->setCurrentTrackPos(m_currentPlaylist->GetCurrentPos());
    }

    if (m_cycleVisualizer)
        cycleVisualizer();
}

void MusicCommon::playlistItemVisible(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *mdata = item->GetData().value<MusicMetadata*>();
    if (mdata && item->GetText() == " ")
    {
        if (item->GetImageFilename().isEmpty())
        {
            QString artFile = mdata->getAlbumArtFile();
            if (artFile.isEmpty())
            {
                item->SetImage("");
                item->SetImage("", "coverart");
            }
            else
            {
                item->SetImage(mdata->getAlbumArtFile());
                item->SetImage(mdata->getAlbumArtFile(), "coverart");
            }
        }

        InfoMap metadataMap;
        mdata->toMap(metadataMap);
        item->SetText("");
        item->SetTextFromMap(metadataMap);
        item->DisplayState(QString("%1").arg(mdata->Rating()), "ratingstate");
    }
}

void MusicCommon::updateUIPlaylist(void)
{
    if (m_noTracksText && gPlayer->getCurrentPlaylist())
        m_noTracksText->SetVisible((gPlayer->getCurrentPlaylist()->getTrackCount() == 0));

    if (!m_currentPlaylist)
        return;

    m_currentPlaylist->Reset();

    m_currentTrack = -1;

    Playlist *playlist = gPlayer->getCurrentPlaylist();

    if (!playlist)
        return;

    for (int x = 0; x < playlist->getTrackCount(); x++)
    {
        MusicMetadata *mdata = playlist->getSongAt(x);
        if (mdata)
        {
            auto *item = new MythUIButtonListItem(m_currentPlaylist, " ",
                                                  QVariant::fromValue(mdata));

            item->SetText(mdata->Artist() + mdata->Album() + mdata->Title(), "**search**");
            item->SetFontState("normal");
            item->DisplayState("default", "playstate");

            // if this is the current track update its play state to match the player
            if (gPlayer->getCurrentMetadata() && mdata->ID() == gPlayer->getCurrentMetadata()->ID())
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

                m_currentPlaylist->SetItemCurrent(item);
                m_currentTrack = m_currentPlaylist->GetCurrentPos();
            }
        }
    }
}

void MusicCommon::updateUIPlayedList(void)
{
    if (!m_playedTracksList)
        return;

    m_playedTracksList->Reset();

    QList<MusicMetadata*> playedList = gPlayer->getPlayedTracksList();

    for (int x = playedList.count(); x > 0; x--)
    {
        MusicMetadata *mdata = playedList[x-1];
        auto *item = new MythUIButtonListItem(m_playedTracksList, "",
                                              QVariant::fromValue(mdata));

        InfoMap metadataMap;
        mdata->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);

        item->SetFontState("normal");
        item->DisplayState("default", "playstate");

        item->SetImage(mdata->getAlbumArtFile());
    }
}

void MusicCommon::updatePlaylistStats(void)
{
    int trackCount = 0;

    if (gPlayer->getCurrentPlaylist())
        trackCount = gPlayer->getCurrentPlaylist()->getTrackCount();

    InfoMap map;
    if (gPlayer->isPlaying() && trackCount > 0)
    {
        QString playlistcurrent = QLocale::system().toString(m_currentTrack + 1);
        QString playlisttotal = QLocale::system().toString(trackCount);

        map["playlistposition"] = tr("%1 of %2").arg(playlistcurrent, playlisttotal);
        map["playlistcurrent"] = playlistcurrent;
        map["playlistcount"] = playlisttotal;
        map["playlisttime"] = getTimeString(m_playlistPlayedTime + m_currentTime, m_playlistMaxTime);
        map["playlistplayedtime"] = getTimeString(m_playlistPlayedTime + m_currentTime, 0s);
        map["playlisttotaltime"] = getTimeString(m_playlistMaxTime, 0s);
        QString playlistName = gPlayer->getCurrentPlaylist() ? gPlayer->getCurrentPlaylist()->getName() : "";
        if (playlistName == "default_playlist_storage")
            playlistName = tr("Default Playlist");
        else if (playlistName ==  "stream_playlist")
            playlistName = tr("Stream Playlist");
        map["playlistname"] = playlistName;
        map["playedtime"] = getTimeString(m_currentTime, 0s); // v34 - parts
        map["totaltime"] = getTimeString(m_maxTime, 0s);
        map["trackspeed"] = getTimeString(-1s, 0s);
    }
    else
    {
        map["playlistposition"] = "";
        map["playlistcurrent"] = "";
        map["playlistcount"] = "";
        map["playlisttime"] = "";
        map["playlistplayedtime"] = "";
        map["playlisttotaltime"] = "";
        map["playlistname"] = "";
        map["playedtime"] = ""; // v34 - parts for track templates
        map["totaltime"] = "";
        map["trackspeed"] = "";
    }

    SetTextFromMap(map);

    if (m_playlistProgress)
        m_playlistProgress->SetUsed((m_playlistPlayedTime + m_currentTime).count());
}

QString MusicCommon::getTimeString(std::chrono::seconds exTime, std::chrono::seconds maxTime)
{
    if (exTime > -1s && maxTime <= 0s)
        return MythDate::formatTime(exTime,
                                    (exTime >= 1h) ? "H:mm:ss" : "mm:ss");

    QString fmt = (maxTime >= 1h) ? "H:mm:ss" : "mm:ss";
    QString out = MythDate::formatTime(exTime, fmt)
        + " / " + MythDate::formatTime(maxTime, fmt);
    float speed = gPlayer->getSpeed(); // v34 - show altered speed
    QString speedstr = "";
    if (lroundf(speed * 100.0F) != 100.0F)
    {
        speedstr = QString("%1").arg(speed);
        out += ", " + speedstr + "X";
    }
    if (exTime <= -1s)
        return speedstr;
    return out;
}

void MusicCommon::searchButtonList(void)
{
    auto *buttonList = dynamic_cast<MythUIButtonList *>(GetFocusWidget());
    if (buttonList)
        buttonList->ShowSearchDialog();

    auto *buttonTree = dynamic_cast<MythUIButtonTree *>(GetFocusWidget());
    if (buttonTree)
        buttonTree->ShowSearchDialog();
}

void MusicCommon::ShowMenu(void)
{
    MythMenu *mainMenu = createMainMenu();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(mainMenu, popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete mainMenu;
}

MythMenu* MusicCommon::createMainMenu(void)
{
    QString label = tr("View Actions");

    auto *menu = new MythMenu(label, this, "mainmenu");

    if (m_currentView == MV_PLAYLISTEDITORTREE)
    {
        menu->AddItem(tr("Switch To Gallery View"));
    }
    else if (m_currentView == MV_PLAYLISTEDITORGALLERY)
    {
        menu->AddItem(tr("Switch To Tree View"));
    }

    QStringList screenList;
    MythScreenType *screen = this;
    while (screen)
    {
        screenList.append(screen->objectName());
        screen = qobject_cast<MusicCommon*>(screen)->m_parentScreen;
    }

    if (!screenList.contains("visualizerview"))
        menu->AddItem(tr("Fullscreen Visualizer"));

    if (m_currentView == MV_PLAYLIST)
        menu->AddItem(tr("Browse Music Library")); // v33- was "Playlist Editor"

    if (m_currentView != MV_VISUALIZER) {
        if (!screenList.contains("searchview") && !screenList.contains("streamview"))
            menu->AddItem(tr("Search for Music"));

        if (!screenList.contains("lyricsview"))
            menu->AddItem(tr("Lyrics"));
    }

    menu->AddItem(tr("More Options"), nullptr, createSubMenu());

    return menu;
}

MythMenu* MusicCommon::createSubMenu(void)
{
    QString label = tr("Actions");

    auto *menu = new MythMenu(label, this, "submenu");

    if (GetFocusWidget() && (GetFocusWidget()->inherits("MythUIButtonList") ||
                             GetFocusWidget()->inherits("MythUIButtonTree")))
        menu->AddItem(tr("Search List..."));

    if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
    {
        menu->AddItem(tr("Playlist Options"), nullptr, createPlaylistMenu());
        menu->AddItem(tr("Set Shuffle Mode"), nullptr, createShuffleMenu());
        menu->AddItem(tr("Set Repeat Mode"),  nullptr, createRepeatMenu());
    }

    menu->AddItem(tr("Player Options"),   nullptr, createPlayerMenu());

    if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
        menu->AddItem(tr("Quick Playlists"),  nullptr, createQuickPlaylistsMenu());

    if (m_visualizerVideo)
        menu->AddItem(tr("Change Visualizer"), nullptr, createVisualizerMenu());

    return menu;
}

MythMenu* MusicCommon::createPlaylistMenu(void)
{
    QString label = tr("Playlist Options");

    auto *menu = new MythMenu(label, this, "playlistmenu");

    if (m_currentPlaylist)
    {
        menu->AddItem(tr("Sync List With Current Track"));
        menu->AddItem(tr("Remove Selected Track"));
    }

    menu->AddItem(tr("Remove All Tracks"));

    if (m_currentPlaylist)
    {
        menu->AddItem(tr("Save To New Playlist"));
        menu->AddItem(tr("Save To Existing Playlist"));

        if (m_moveTrackMode)
            menu->AddItem(tr("Switch To Select Mode"));
        else
            menu->AddItem(tr("Switch To Move Mode"));
    }

    return menu;
}

void MusicCommon::showExitMenu(void)
{
    QString label = tr("Exiting Music Player.\n\n"
                       "Do you want to continue playing in the background?");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox(label, popupStack, "exitmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "exitmenu");

    menu->AddButton(tr("No - Exit, Stop Playing"));
    menu->AddButton(tr("Yes - Exit, Continue Playing"));
    menu->AddButton(tr("Cancel"));

    popupStack->AddScreen(menu);
}

MythMenu* MusicCommon::createPlayerMenu(void)
{
    QString label = tr("Player Actions");

    auto *menu = new MythMenu(label, this, "playermenu");

    menu->AddItem(tr("Change Volume"));
    menu->AddItem(tr("Mute"));
    menu->AddItem(tr("Previous Track"));
    menu->AddItem(tr("Next Track"));

    if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
    {
        menu->AddItem(tr("Jump Back"));
        menu->AddItem(tr("Jump Forward"));
    }

    menu->AddItem(tr("Play"));
    menu->AddItem(tr("Stop"));

    if (gPlayer->getPlayMode() != MusicPlayer::PLAYMODE_RADIO)
        menu->AddItem(tr("Pause"));

    return menu;
}

MythMenu* MusicCommon::createRepeatMenu(void)
{
    QString label = tr("Set Repeat Mode");

    auto *menu = new MythMenu(label, this, "repeatmenu");

    menu->AddItemV(tr("None"),  QVariant::fromValue((int)MusicPlayer::REPEAT_OFF));
    menu->AddItemV(tr("Track"), QVariant::fromValue((int)MusicPlayer::REPEAT_TRACK));
    menu->AddItemV(tr("All"),   QVariant::fromValue((int)MusicPlayer::REPEAT_ALL));

    menu->SetSelectedByData(static_cast<int>(gPlayer->getRepeatMode()));

    return menu;
}

MythMenu* MusicCommon::createShuffleMenu(void)
{
    QString label = tr("Set Shuffle Mode");

    auto *menu = new MythMenu(label, this, "shufflemenu");

    menu->AddItemV(tr("None"),   QVariant::fromValue((int)MusicPlayer::SHUFFLE_OFF));
    menu->AddItemV(tr("Random"), QVariant::fromValue((int)MusicPlayer::SHUFFLE_RANDOM));
    menu->AddItemV(tr("Smart"),  QVariant::fromValue((int)MusicPlayer::SHUFFLE_INTELLIGENT));
    menu->AddItemV(tr("Album"),  QVariant::fromValue((int)MusicPlayer::SHUFFLE_ALBUM));
    menu->AddItemV(tr("Artist"), QVariant::fromValue((int)MusicPlayer::SHUFFLE_ARTIST));

    menu->SetSelectedByData(static_cast<int>(gPlayer->getShuffleMode()));

    return menu;
}

MythMenu* MusicCommon::createQuickPlaylistsMenu(void)
{
    QString label = tr("Quick Playlists");

    auto *menu = new MythMenu(label, this, "quickplaylistmenu");

    menu->AddItem(tr("All Tracks"));

    if (gMusicData->m_all_music->getCDTrackCount() > 0)
        menu->AddItem(tr("From CD"));

    if (gPlayer->getCurrentMetadata())
    {
        menu->AddItem(tr("Tracks By Current Artist"));
        menu->AddItem(tr("Tracks From Current Album"));
        menu->AddItem(tr("Tracks From Current Genre"));
        menu->AddItem(tr("Tracks From Current Year"));
        menu->AddItem(tr("Tracks With Same Title"));
    }

    return menu;
}

MythMenu* MusicCommon::createVisualizerMenu(void)
{
    QString label = tr("Choose Visualizer");

    auto *menu = new MythMenu(label, this, "visualizermenu");

    for (uint x = 0; x < static_cast<uint>(m_visualModes.count()); x++)
        menu->AddItemV(m_visualModes.at(x), QVariant::fromValue(x));

    menu->SetSelectedByData(m_currentVisual);

    return menu;
}

MythMenu* MusicCommon::createPlaylistOptionsMenu(void)
{
    QString label = tr("Add to Playlist Options");

    auto *menu = new MythMenu(label, this, "playlistoptionsmenu");

    if (MusicPlayer::getPlayNow())
    {
        menu->AddItem(tr("Play Now"));
        menu->AddItem(tr("Add Tracks"));
        menu->AddItem(tr("Replace Tracks"));
        menu->AddItem(tr("Prefer Add Tracks"));
    }
    else
    {
        menu->AddItem(tr("Add Tracks"));
        menu->AddItem(tr("Play Now"));
        menu->AddItem(tr("Replace Tracks"));
        menu->AddItem(tr("Prefer Play Now"));
    }

    return menu;
}

void MusicCommon::allTracks()
{
   m_whereClause = "ORDER BY music_artists.artist_name, album_name, disc_number, track";
   showPlaylistOptionsMenu();
}

void MusicCommon::fromCD(void)
{
    m_whereClause = "";
    m_songList.clear();

    // get the list of cd tracks
    for (int x = 1; x <= gMusicData->m_all_music->getCDTrackCount(); x++)
    {
        MusicMetadata *mdata = gMusicData->m_all_music->getCDMetadata(x);
        if (mdata)
        {
            m_songList.append((mdata)->ID());
        }
    }

    showPlaylistOptionsMenu();
}

void MusicCommon::byArtist(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Artist().toUtf8().constData());
    m_whereClause = "WHERE music_artists.artist_name = " + value +
                    " ORDER BY album_name, disc_number, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byAlbum(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Album().toUtf8().constData());
    m_whereClause = "WHERE album_name = " + value +
                    " ORDER BY disc_number, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byGenre(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Genre().toUtf8().constData());
    m_whereClause = "WHERE genre = " + value +
                    " ORDER BY music_artists.artist_name, album_name, disc_number, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byYear(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Year());
    m_whereClause = "WHERE music_songs.year = " + value +
                    " ORDER BY music_artists.artist_name, album_name, disc_number, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byTitle(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Title().toUtf8().constData());
    m_whereClause = "WHERE music_songs.name = " + value +
                    " ORDER BY music_artists.artist_name, album_name, disc_number, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::showPlaylistOptionsMenu(bool addMainMenu)
{
    if (!gPlayer->getCurrentPlaylist())
        return;

    m_playlistOptions.playPLOption = PL_CURRENT;

    // Don't bother showing the dialog if the current playlist is empty
    if (gPlayer->getCurrentPlaylist()->getTrackCount() == 0)
    {
        m_playlistOptions.insertPLOption = PL_REPLACE;
        doUpdatePlaylist();
        return;
    }

    MythMenu *menu = createPlaylistOptionsMenu();

    if (addMainMenu)
        menu->AddItem(tr("More Options"), nullptr, createMainMenu());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(menu, popupStack, "playlistoptionsmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menu;
}

void MusicCommon::doUpdatePlaylist(void)
{
    int curTrackID = -1;
    int added = 0;
    int curPos = gPlayer->getCurrentTrackPos();

    // store id of current track
    if (gPlayer->getCurrentMetadata())
        curTrackID = gPlayer->getCurrentMetadata()->ID();

    if (!m_whereClause.isEmpty())
    {
        // update playlist from quick playlist
        added = gMusicData->m_all_playlists->getActive()->fillSonglistFromQuery(
            m_whereClause, true,
            m_playlistOptions.insertPLOption, curTrackID);
        m_whereClause.clear();
    }
    else if (!m_songList.isEmpty())
    {
        // update playlist from song list (from the playlist editor)
        added = gMusicData->m_all_playlists->getActive()->fillSonglistFromList(
            m_songList, true,
            m_playlistOptions.insertPLOption, curTrackID);

        m_songList.clear();
    }

    m_currentTrack = gPlayer->getCurrentTrackPos();

    updateUIPlaylist();
    Playlist *playlist = gPlayer->getCurrentPlaylist();
    if (nullptr == playlist)
        return;

    // if (m_currentTrack == -1) // why? non-playing should also
    //     playFirstTrack();     // start playing per options -twitham
    // else
    {
        switch (m_playlistOptions.playPLOption)
        {
            case PL_CURRENT:
            {
                if (!restorePosition(curTrackID))
                    playFirstTrack();

                break;
            }

            case PL_FIRST:
                playFirstTrack();
                break;

            case PL_FIRSTNEW:
            {
                switch (m_playlistOptions.insertPLOption)
                {
                    case PL_REPLACE:
                        playFirstTrack();
                        break;

                    case PL_INSERTATEND:
                    {
                        pause();
                        if (!gPlayer->setCurrentTrackPos(playlist->getTrackCount() - added))
                            playFirstTrack();
                        break;
                    }

                    case PL_INSERTAFTERCURRENT:
                        if (!gPlayer->setCurrentTrackPos(curPos + 1))
                            playFirstTrack();
                        break;

                    default:
                        playFirstTrack();
                }

                break;
            }
        }
    }

    playlist->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                       m_currentTrack, &m_playlistPlayedTime);
    updatePlaylistStats();
    updateTrackInfo(gPlayer->getCurrentMetadata());
}

bool MusicCommon::restorePosition(int trackID)
{
    // try to move to the current track
    bool foundTrack = false;

    Playlist *playlist = gPlayer->getCurrentPlaylist();
    if (nullptr == playlist)
        return false;

    if (trackID != -1)
    {
        for (int x = 0; x < playlist->getTrackCount(); x++)
        {
            MusicMetadata *mdata = playlist->getSongAt(x);
            if (mdata && mdata->ID() == (MusicMetadata::IdType) trackID)
            {
                m_currentTrack = x;
                if (m_currentPlaylist)
                {
                    m_currentPlaylist->SetItemCurrent(m_currentTrack);
                    MythUIButtonListItem *item = m_currentPlaylist->GetItemCurrent();
                    if (item)
                    {
                        item->SetFontState("running");
                        item->DisplayState("playing", "playstate");
                    }
                }

                foundTrack = true;

                break;
            }
        }
    }

    return foundTrack;
}

void MusicCommon::playFirstTrack()
{
    gPlayer->setCurrentTrackPos(0);
}

//---------------------------------------------------------
// MythMusicVolumeDialog
//---------------------------------------------------------
static constexpr std::chrono::milliseconds MUSICVOLUMEPOPUPTIME { 4s };

MythMusicVolumeDialog::~MythMusicVolumeDialog(void)
{
    if (m_displayTimer)
    {
        m_displayTimer->stop();
        delete m_displayTimer;
        m_displayTimer = nullptr;
    }
}

bool MythMusicVolumeDialog::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "volume_popup", this))
        return false;

    UIUtilW::Assign(this, m_volText,     "volume");
    UIUtilW::Assign(this, m_volProgress, "volumeprogress");
    UIUtilW::Assign(this, m_muteState,   "mutestate");

    if (m_volProgress)
         m_volProgress->SetTotal(100);

    updateDisplay();

    m_displayTimer = new QTimer(this);
    connect(m_displayTimer, &QTimer::timeout, this, &MythScreenType::Close);
    m_displayTimer->setSingleShot(true);
    m_displayTimer->start(MUSICVOLUMEPOPUPTIME);

    return true;
}

bool MythMusicVolumeDialog::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions, false);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "UP" || action == "VOLUMEUP")
            increaseVolume();
        else if (action == "DOWN" || action == "VOLUMEDOWN")
            decreaseVolume();
        else if (action == "MUTE" || action == "SELECT")
            toggleMute();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    // Restart the display timer only if we handled this keypress, if nothing
    // has changed there's no need to keep the volume on-screen
    if (handled)
        m_displayTimer->start(MUSICVOLUMEPOPUPTIME);

    return handled;
}

void MythMusicVolumeDialog::increaseVolume(void)
{
    gPlayer->incVolume();
    updateDisplay();
}

void MythMusicVolumeDialog::decreaseVolume(void)
{
    gPlayer->decVolume();
    updateDisplay();
}

void MythMusicVolumeDialog::toggleMute(void)
{
    gPlayer->toggleMute();
    updateDisplay();
}

void MythMusicVolumeDialog::updateDisplay()
{
    if (m_muteState)
        m_muteState->DisplayState(gPlayer->isMuted() ? "on" : "off");

    if (m_volProgress)
        m_volProgress->SetUsed(gPlayer->getVolume());

    if (m_volText)
    {
        InfoMap map;
        gPlayer->toMap(map);
        m_volText->SetTextFromMap(map);
    }
}

//---------------------------------------------------------
// TrackInfoDialog
//---------------------------------------------------------
bool TrackInfoDialog::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "trackdetail_popup", this))
        return false;

    InfoMap metadataMap;
    m_metadata->toMap(metadataMap);
    SetTextFromMap(metadataMap);

    MythUIStateType *ratingState = dynamic_cast<MythUIStateType *>(GetChild("rating_state"));
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

    // hide the song ID by default
    MythUIText *songID = dynamic_cast<MythUIText *>(GetChild("songid"));
    if (songID)
        songID->Hide();

    return true;
}

bool TrackInfoDialog::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions, false);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "INFO")
            Close();
        if (action == "0")
        {
            // if it's available show the song ID
            MythUIText *songID = dynamic_cast<MythUIText *>(GetChild("songid"));
            if (songID)
                songID->Show();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
