// ANSI C includes
#include <cstdlib>

// C++ includes
#include <iostream>
using namespace std;

// Qt includes
#include <QApplication>
#include <QLocale>

// mythtv
#include <mythuitextedit.h>
#include <mythuistatetype.h>
#include <mythuiprogressbar.h>
#include <mythuibutton.h>
#include <mythuiimage.h>
#include <mythdialogbox.h>
#include <mythuibuttonlist.h>
#include <mythuibuttontree.h>
#include <mythuicheckbox.h>
#include <mythuivideo.h>
#include <audiooutput.h>
#include <compat.h>
#include <lcddevice.h>
#include <musicmetadata.h>

// MythMusic includes
#include "musicdata.h"
#include "constants.h"
#include "decoder.h"
#include "mainvisual.h"
#include "smartplaylist.h"
#include "playlistcontainer.h"
#include "search.h"
#include "editmetadata.h"
#include "playlist.h"

#include "musiccommon.h"
#include "playlistview.h"
#include "playlisteditorview.h"
#include "streamview.h"
#include "visualizerview.h"
#include "searchview.h"

MusicCommon::MusicCommon(MythScreenStack *parent, const QString &name)
            : MythScreenType(parent, name),
    m_currentView(),            m_mainvisual(NULL),
    m_fullscreenBlank(false),   m_randomVisualizer(false),
    m_currentVisual(0),         m_moveTrackMode(false),
    m_movingTrack(false),       m_controlVolume(true),
    m_currentTrack(0),          m_currentTime(0),
    m_maxTime(0),               m_playlistTrackCount(0),
    m_playlistPlayedTime(0),    m_playlistMaxTime(0),
    m_timeText(NULL),           m_infoText(NULL),
    m_visualText(NULL),         m_noTracksText(NULL),
    m_shuffleState(NULL),       m_repeatState(NULL),
    m_movingTracksState(NULL),  m_ratingState(NULL),
    m_trackProgress(NULL),      m_trackProgressText(NULL),
    m_trackSpeedText(NULL),     m_trackState(NULL),
    m_muteState(NULL),          m_volumeText(NULL),
    m_playlistProgress(NULL),   m_prevButton(NULL),
    m_rewButton(NULL),          m_pauseButton(NULL),
    m_playButton(NULL),         m_stopButton(NULL),
    m_ffButton(NULL),           m_nextButton(NULL),
    m_coverartImage(NULL),      m_currentPlaylist(NULL),
    m_playedTracksList(NULL),   m_visualizerVideo(NULL)
{
    m_cycleVisualizer = gCoreContext->GetNumSetting("VisualCycleOnSongChange", 0);

    if (LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
        lcd->setFunctionLEDs(FUNC_MUSIC, true);
    }

    m_playlistOptions.insertPLOption = PL_REPLACE;
    m_playlistOptions.playPLOption = PL_CURRENT;
}

MusicCommon::~MusicCommon(void)
{
    gPlayer->removeListener(this);

    if (m_mainvisual)
    {
        stopVisualizer();
        delete m_mainvisual;
        m_mainvisual = NULL;
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
        connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(previous()));

    if (m_rewButton)
        connect(m_rewButton, SIGNAL(Clicked()), this, SLOT(seekback()));

    if (m_pauseButton)
    {
        m_pauseButton->SetLockable(true);
        connect(m_pauseButton, SIGNAL(Clicked()), this, SLOT(pause()));
    }

    if (m_playButton)
    {
        m_playButton->SetLockable(true);
        connect(m_playButton, SIGNAL(Clicked()), this, SLOT(play()));
    }

    if (m_stopButton)
    {
        m_stopButton->SetLockable(true);
        connect(m_stopButton, SIGNAL(Clicked()), this, SLOT(stop()));
    }

    if (m_ffButton)
        connect(m_ffButton, SIGNAL(Clicked()), this, SLOT(seekforward()));

    if (m_nextButton)
        connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(next()));

    gPlayer->addListener(this);

    if (!gPlayer->isPlaying())
    {
        bool isRadioView = (m_currentView == MV_RADIO);
        if (isRadioView)
            gPlayer->setPlayMode(MusicPlayer::PLAYMODE_RADIO);
        else
            gPlayer->setPlayMode(MusicPlayer::PLAYMODE_TRACKS);

        gPlayer->restorePosition();
    }
    else
    {
        // if we are playing but we are switching to a view from a different playmode
        // we need to restart playback in the new mode
        if (m_currentView == MV_VISUALIZER || m_currentView == MV_MINIPLAYER)
        {
            // this view can be used in both play modes
        }
        else if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO && m_currentView != MV_RADIO)
        {
            //gPlayer->savePosition();
            gPlayer->stop(true);
            gPlayer->setPlayMode(MusicPlayer::PLAYMODE_TRACKS);
            gPlayer->restorePosition();
        }
        else if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_TRACKS && m_currentView == MV_RADIO)
        {
            //gPlayer->savePosition();
            gPlayer->stop(true);
            gPlayer->setPlayMode(MusicPlayer::PLAYMODE_RADIO);
            gPlayer->restorePosition();
        }
    }

    m_currentTrack = gPlayer->getCurrentTrackPos();

    MusicMetadata *curMeta = gPlayer->getCurrentMetadata();
    if (curMeta)
        updateTrackInfo(curMeta);

    updateProgressBar();

    if (m_currentPlaylist)
    {
        connect(m_currentPlaylist, SIGNAL(itemClicked(MythUIButtonListItem*)),
                this, SLOT(playlistItemClicked(MythUIButtonListItem*)));
        connect(m_currentPlaylist, SIGNAL(itemVisible(MythUIButtonListItem*)),
                this, SLOT(playlistItemVisible(MythUIButtonListItem*)));

        m_currentPlaylist->SetSearchFields("**search**");

        updateUIPlaylist();
    }

    if (m_visualizerVideo)
    {
        // Warm up the visualizer
        m_mainvisual = new MainVisual(m_visualizerVideo);
        m_visualModes = m_mainvisual->getVisualizations();

        m_fullscreenBlank = false;

        m_randomVisualizer = gCoreContext->GetNumSetting("VisualRandomize", 0);

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

    m_controlVolume = gCoreContext->GetNumSetting("MythControlsVolume", 0);
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

    gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                      m_currentTrack, &m_playlistPlayedTime);

    if (m_playlistProgress)
    {
        m_playlistProgress->SetTotal(m_playlistMaxTime);
        m_playlistProgress->SetUsed(0);
    }

    updatePlaylistStats();

    return err;
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

        gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
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
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    stopVisualizer();

    if (m_mainvisual)
    {
        delete m_mainvisual;
        m_mainvisual = NULL;
    }

    gPlayer->removeListener(this);
    gPlayer->setAllowRestorePos(false);

    switch (view)
    {
        case MV_PLAYLIST:
        {
            PlaylistView *view = new PlaylistView(mainStack);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }

        case MV_PLAYLISTEDITORTREE:
        {
            // if we are switching playlist editor views save and restore
            // the current position in the tree
            bool restorePos = (m_currentView == MV_PLAYLISTEDITORGALLERY);
            PlaylistEditorView *oldView = dynamic_cast<PlaylistEditorView *>(this);
            if (oldView)
                oldView->saveTreePosition();

            PlaylistEditorView *view = new PlaylistEditorView(mainStack, "tree", restorePos);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }

        case MV_PLAYLISTEDITORGALLERY:
        {
            // if we are switching playlist editor views save and restore
            // the current position in the tree
            bool restorePos = (m_currentView == MV_PLAYLISTEDITORTREE);
            PlaylistEditorView *oldView = dynamic_cast<PlaylistEditorView *>(this);
            if (oldView)
                oldView->saveTreePosition();

            PlaylistEditorView *view = new PlaylistEditorView(mainStack, "gallery", restorePos);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }

        case MV_SEARCH:
        {
            SearchView *view = new SearchView(mainStack);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }

        case MV_VISUALIZER:
        {
            VisualizerView *view = new VisualizerView(mainStack);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }

        case MV_RADIO:
        {
            StreamView *view = new StreamView(mainStack);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }

        default:
            return;
    }

    Close();

    gPlayer->setAllowRestorePos(true);
}

bool MusicCommon::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", e, actions, true);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
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
                gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                         m_currentTrack, &m_playlistPlayedTime);
                updatePlaylistStats();
                updateTrackInfo(gPlayer->getCurrentMetadata());
            }
            else if (action == "DOWN")
            {
                gPlayer->moveTrackUpDown(false, m_currentPlaylist->GetCurrentPos());
                item->MoveUpDown(false);
                m_currentTrack = gPlayer->getCurrentTrackPos();
                gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                         m_currentTrack, &m_playlistPlayedTime);
                updatePlaylistStats();
                updateTrackInfo(gPlayer->getCurrentMetadata());
            }

            return true;
        }

        if (action == "ESCAPE")
        {
            QString exit_action = gCoreContext->GetSetting("MusicExitAction", "prompt");

            if (!gPlayer->isPlaying() || GetMythMainWindow()->IsExitingToMain())
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
                    Close();
                else
                    showExitMenu();
            }
        }
        else if (action == "THMBUP")
            changeRating(true);
        else if (action == "THMBDOWN")
            changeRating(false);
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
            if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_TRACKS)
            {
                if (m_ffButton)
                    m_ffButton->Push();
                else
                    seekforward();
            }
        }
        else if (action == "RWND")
        {
            if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_TRACKS)
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
            m_currentTime = 0;
        }
        else if (action == "CYCLEVIS")
            cycleVisualizer();
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
            changeVolume(false);
        else if (action == "VOLUMEUP")
            changeVolume(true);
        else if (action == "SPEEDDOWN")
            changeSpeed(false);
        else if (action == "SPEEDUP")
            changeSpeed(true);
        else if (action == "MUTE")
            toggleMute();
        else if (action == "TOGGLEUPMIX")
            toggleUpmix();
        else if (action == "INFO" || action == "EDIT")
        {
            if (m_currentPlaylist && GetFocusWidget() == m_currentPlaylist)
            {
                if (m_currentPlaylist->GetItemCurrent())
                {
                    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (m_currentPlaylist->GetItemCurrent()->GetData());
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
                MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
                if (mdata)
                    gPlayer->removeTrack(mdata->ID());
            }
        }
        else if (action == "MENU")
            ShowMenu();
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
            switchView(MV_PLAYLIST);
        else if (action == "SWITCHTOPLAYLISTEDITORTREE" && m_currentView != MV_PLAYLISTEDITORTREE)
            switchView(MV_PLAYLISTEDITORTREE);
        else if (action == "SWITCHTOPLAYLISTEDITORGALLERY" && m_currentView != MV_PLAYLISTEDITORGALLERY)
            switchView(MV_PLAYLISTEDITORGALLERY);
        else if (action == "SWITCHTOSEARCH" && m_currentView != MV_SEARCH)
            switchView(MV_SEARCH);
        else if (action == "SWITCHTOVISUALISER" && m_currentView != MV_VISUALIZER)
            switchView(MV_VISUALIZER);
        else if (action == "SWITCHTORADIO" && m_currentView != MV_RADIO)
            switchView(MV_RADIO);
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
            handled = false;
    }

    return handled;
}

void MusicCommon::changeVolume(bool up)
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
    if (gPlayer->getOutput() && gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_TRACKS)
    {
        if (up)
            gPlayer->incSpeed();
        else
            gPlayer->decSpeed();
        showSpeed(true);
    }
}

void MusicCommon::toggleMute()
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
        int available, maxSize;
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
        if (m_maxTime)
            percentplayed = (int)(((double)m_currentTime / (double)m_maxTime) * 100);
        m_trackProgress->SetTotal(100);
        m_trackProgress->SetUsed(percentplayed);
    }
}

void MusicCommon::showVolume(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythMusicVolumeDialog *vol = new MythMusicVolumeDialog(popupStack, "volumepopup");

    if (!vol->Create())
    {
        delete vol;
        return;
    }

    popupStack->AddScreen(vol);
}

void MusicCommon::showSpeed(bool show)
{
    (void) show;
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
            unsigned int next_visualizer;

            //Find a visual thats not like the previous visual
            do
                next_visualizer = random() % m_visualModes.count();
            while (next_visualizer == m_currentVisual);
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

    QString time_string = getTimeString(m_maxTime, 0);

    if (m_timeText)
        m_timeText->SetText(time_string);
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
    int nextTime = m_currentTime + 5;
    if (nextTime > m_maxTime)
        nextTime = m_maxTime;
    seek(nextTime);
}

void MusicCommon::seekback()
{
    int nextTime = m_currentTime - 5;
    if (nextTime < 0)
        nextTime = 0;
    seek(nextTime);
}

void MusicCommon::seek(int pos)
{
    if (gPlayer->getOutput())
    {
        if (gPlayer->getDecoder() && gPlayer->getDecoder()->isRunning())
        {
            gPlayer->getDecoder()->lock();
            gPlayer->getDecoder()->seek(pos);

            if (m_mainvisual)
            {
                m_mainvisual->mutex()->lock();
                m_mainvisual->prepare();
                m_mainvisual->mutex()->unlock();
            }

            gPlayer->getDecoder()->unlock();
        }

        gPlayer->getOutput()->SetTimecode(pos*1000);

        if (!gPlayer->isPlaying())
        {
            m_currentTime = pos;
            if (m_timeText)
                m_timeText->SetText(getTimeString(pos, m_maxTime));

            updateProgressBar();

            if (LCD *lcd = LCD::Get())
            {
                float percent_heard = m_maxTime <= 0 ? 0.0 : ((float)pos /
                                      (float)m_maxTime);

                QString lcd_time_string = getTimeString(pos, m_maxTime);

                // if the string is longer than the LCD width, remove all spaces
                if (lcd_time_string.length() > (int)lcd->getLCDWidth())
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

    if (event->type() == OutputEvent::Playing)
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
    else if (event->type() == OutputEvent::Buffering)
    {
        statusString = tr("Buffering stream.");
    }
    else if (event->type() == OutputEvent::Paused)
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
    else if (event->type() == OutputEvent::Info)
    {

        OutputEvent *oe = dynamic_cast<OutputEvent *>(event);

        if (!oe)
            return;

        int rs = 0;
        MusicMetadata *curMeta = gPlayer->getCurrentMetadata();

        if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO)
        {
            if (curMeta)
                m_currentTime = rs = curMeta->Length() / 1000;
            else
                m_currentTime = 0;
        }
        else
            m_currentTime = rs = oe->elapsedSeconds();

        QString time_string = getTimeString(rs, m_maxTime);

        updateProgressBar();

        if (curMeta)
        {
            if (LCD *lcd = LCD::Get())
            {
                float percent_heard = m_maxTime <= 0 ?
                    0.0:((float)rs / (float)curMeta->Length()) * 1000.0;

                QString lcd_time_string = time_string;

                // if the string is longer than the LCD width, remove all spaces
                if (time_string.length() > (int)lcd->getLCDWidth())
                    lcd_time_string.remove(' ');

                lcd->setMusicProgress(lcd_time_string, percent_heard);
            }
        }

        QString info_string;

        //  Hack around for cd bitrates
        if (oe->bitrate() < 2000)
        {
            info_string.sprintf(QString("%d "+tr("kbps")+ "   %.1f "+ tr("kHz")+ "   %s "+ tr("ch")).toUtf8().data(),
                                oe->bitrate(), float(oe->frequency()) / 1000.0,
                                oe->channels() > 1 ? "2" : "1");
        }
        else
        {
            info_string.sprintf(QString("%.1f "+ tr("kHz")+ "   %s "+ tr("ch")).toUtf8().data(),
                                float(oe->frequency()) / 1000.0,
                                oe->channels() > 1 ? "2" : "1");
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
    else if (event->type() == OutputEvent::Stopped)
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
        DialogCompletionEvent *dce = static_cast<DialogCompletionEvent*>(event);

        // make sure the user didn't ESCAPE out of the menu
        if (dce->GetResult() < 0)
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        if (resultid == "viewmenu")
        {
            if (dce->GetResult() >= 0)
            {
                MusicView view = (MusicView)dce->GetData().toInt();
                switchView(view);
            }
        }
        else if (resultid == "actionmenu")
        {
            if (resulttext == tr("Search List..."))
            {
                searchButtonList();
            }
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
                    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
                    if (mdata)
                        gPlayer->removeTrack(mdata->ID());
                }
            }
            else if (resulttext == tr("Remove All Tracks"))
            {
                gPlayer->getPlaylist()->removeAllTracks();
                gPlayer->activePlaylistChanged(-1, true);
            }
            else if (resulttext == tr("Save To New Playlist"))
            {
                QString message = tr("Enter new playlist name");

                MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

                MythTextInputDialog *inputdialog = new MythTextInputDialog(popupStack, message);

                if (inputdialog->Create())
                {
                    inputdialog->SetReturnEvent(this, "addplaylist");
                    popupStack->AddScreen(inputdialog);
                }
                else
                    delete inputdialog;
            }
            else if (resulttext == tr("Save To Existing Playlist"))
            {
                QString message = tr("Select the playlist to save to");
                QStringList playlists = gMusicData->all_playlists->getPlaylistNames();

                MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

                MythUISearchDialog *searchdialog = new MythUISearchDialog(popupStack, message, playlists);

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
                doUpdatePlaylist(false);
            }
            else if (resulttext == tr("Add Tracks"))
            {
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                doUpdatePlaylist(false);
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
            gMusicData->all_playlists->copyNewPlaylist(resulttext);
            Playlist *playlist = gMusicData->all_playlists->getPlaylist(resulttext);
            gPlayer->playlistChanged(playlist->getID());
        }
        else if (resultid == "updateplaylist")
        {
            Playlist *playlist = gMusicData->all_playlists->getPlaylist(resulttext);
            QString songList = gPlayer->getPlaylist()->toRawSonglist();
            playlist->removeAllTracks();
            playlist->fillSongsFromSonglist(songList);
            playlist->changed();
            gPlayer->playlistChanged(playlist->getID());
        }
    }
    else if (event->type() == MusicPlayerEvent::TrackChangeEvent)
    {
        MusicPlayerEvent *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackNo = mpe->TrackID;

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

        gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                         m_currentTrack, &m_playlistPlayedTime);
        if (m_playlistProgress)
        {
            m_playlistProgress->SetTotal(m_playlistMaxTime);
            m_playlistProgress->SetUsed(0);
        }

        updatePlaylistStats();
        updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == MusicPlayerEvent::VolumeChangeEvent)
    {
        updateVolume();
    }
    else if (event->type() == MusicPlayerEvent::TrackRemovedEvent)
    {
        MusicPlayerEvent *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackID = mpe->TrackID;

        if (m_currentPlaylist)
        {
            // find and remove the list item for the removed track
            for (int x = 0; x < m_currentPlaylist->GetCount(); x++)
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(x);
                MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
                if (mdata && mdata->ID() == (MusicMetadata::IdType) trackID)
                {
                    // work around a bug in MythUIButtonlist not updating properly after removing the last item
                    if (m_currentPlaylist->GetCount() == 1)
                        m_currentPlaylist->Reset();
                    else
                        m_currentPlaylist->RemoveItem(item);
                    break;
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

        gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                          m_currentTrack, &m_playlistPlayedTime);
        updatePlaylistStats();
        updateTrackInfo(gPlayer->getCurrentMetadata());

        if (m_noTracksText)
            m_noTracksText->SetVisible((gPlayer->getPlaylist()->getSongs().count() == 0));
    }
    else if (event->type() == MusicPlayerEvent::TrackAddedEvent)
    {
        MusicPlayerEvent *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        int trackID = mpe->TrackID;

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
                MusicMetadata *mdata = gMusicData->all_music->getMetadata(trackID);

                if (mdata)
                {
                    InfoMap metadataMap;
                    mdata->toMap(metadataMap);

                    MythUIButtonListItem *item =
                            new MythUIButtonListItem(m_currentPlaylist, "", qVariantFromValue(mdata));

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

                if (m_noTracksText)
                    m_noTracksText->SetVisible((gPlayer->getPlaylist()->getSongs().count() == 0));
            }
        }

        gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                          m_currentTrack, &m_playlistPlayedTime);

        updatePlaylistStats();
        updateTrackInfo(gPlayer->getCurrentMetadata());
    }
    else if (event->type() == MusicPlayerEvent::AllTracksRemovedEvent)
    {
        updateUIPlaylist();
        updatePlaylistStats();
        updateTrackInfo(NULL);
    }
    else if (event->type() == MusicPlayerEvent::MetadataChangedEvent ||
             event->type() == MusicPlayerEvent::TrackStatsChangedEvent)
    {
        MusicPlayerEvent *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        uint trackID = mpe->TrackID;

        if (m_currentPlaylist)
        {
            for (int x = 0; x < m_currentPlaylist->GetCount(); x++)
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(x);
                MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());

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
                MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());

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
    else if (event->type() == MusicPlayerEvent::AlbumArtChangedEvent)
    {
        MusicPlayerEvent *mpe = dynamic_cast<MusicPlayerEvent *>(event);

        if (!mpe)
            return;

        uint trackID = mpe->TrackID;

        if (m_currentPlaylist)
        {
            for (int x = 0; x < m_currentPlaylist->GetCount(); x++)
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(x);
                MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
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

    EditMetadataDialog *editDialog = new EditMetadataDialog(mainStack, mdata);

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
        m_maxTime = 0;
    else
        m_maxTime = mdata->Length() / 1000;

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
            m_coverartImage->Reset();
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

    TrackInfoDialog *dlg = new TrackInfoDialog(popupStack, mdata, "trackinfopopup");

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
        gPlayer->setCurrentTrackPos(m_currentPlaylist->GetCurrentPos());

    if (m_cycleVisualizer)
        cycleVisualizer();
}

void MusicCommon::playlistItemVisible(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MusicMetadata *mdata = qVariantValue<MusicMetadata*> (item->GetData());
    if (mdata)
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

        if (item->GetText() == " ")
        {
            InfoMap metadataMap;
            mdata->toMap(metadataMap);
            item->SetText("");
            item->SetTextFromMap(metadataMap);
            item->DisplayState(QString("%1").arg(mdata->Rating()), "ratingstate");
        }
    }
}

void MusicCommon::updateUIPlaylist(void)
{
    if (m_noTracksText)
        m_noTracksText->SetVisible((gPlayer->getPlaylist()->getSongs().count() == 0));

    if (!m_currentPlaylist)
        return;

    m_currentPlaylist->Reset();

    m_currentTrack = -1;

    Playlist *playlist = gPlayer->getPlaylist();

    QList<MusicMetadata*> songlist = playlist->getSongs();
    QList<MusicMetadata*>::iterator it = songlist.begin();

    for (; it != songlist.end(); ++it)
    {
        MusicMetadata *mdata = (*it);
        if (mdata)
        {
            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_currentPlaylist, " ", qVariantFromValue(mdata));

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
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_playedTracksList, "", qVariantFromValue(mdata));

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
    int trackCount = gPlayer->getPlaylist()->getSongs().size();

    InfoMap map;
    if (gPlayer->isPlaying() && trackCount > 0)
    {
        QString playlistcurrent = QLocale::system().toString(m_currentTrack + 1);
        QString playlisttotal = QLocale::system().toString(trackCount);

        map["playlistposition"] = tr("%1 of %2").arg(playlistcurrent)
                                                .arg(playlisttotal);
        map["playlistcurrent"] = playlistcurrent;
        map["playlistcount"] = playlisttotal;
        map["playlisttime"] = getTimeString(m_playlistPlayedTime + m_currentTime, m_playlistMaxTime);
        map["playlistplayedtime"] = getTimeString(m_playlistPlayedTime + m_currentTime, 0);
        map["playlisttotaltime"] = getTimeString(m_playlistMaxTime, 0);
        QString playlistName = gPlayer->getPlaylist()->getName();
        if (playlistName == "default_playlist_storage")
            playlistName = tr("Default Playlist");
        else if (playlistName ==  "stream_playlist")
            playlistName = tr("Stream Playlist");
        map["playlistname"] = playlistName;
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
    }

    SetTextFromMap(map);

    if (m_playlistProgress)
        m_playlistProgress->SetUsed(m_playlistPlayedTime + m_currentTime);
}

QString MusicCommon::getTimeString(int exTime, int maxTime)
{
    QString time_string;

    int eh = exTime / 3600;
    int em = (exTime / 60) % 60;
    int es = exTime % 60;

    int maxh = maxTime / 3600;
    int maxm = (maxTime / 60) % 60;
    int maxs = maxTime % 60;

    if (maxTime <= 0)
    {
        if (eh > 0)
            time_string.sprintf("%d:%02d:%02d", eh, em, es);
        else
            time_string.sprintf("%02d:%02d", em, es);
    }
    else
    {
        if (maxh > 0)
            time_string.sprintf("%d:%02d:%02d / %02d:%02d:%02d", eh, em,
                    es, maxh, maxm, maxs);
        else
            time_string.sprintf("%02d:%02d / %02d:%02d", em, es, maxm,
                    maxs);
    }

    return time_string;
}

void MusicCommon::searchButtonList(void)
{
    MythUIButtonList *buttonList = dynamic_cast<MythUIButtonList *>(GetFocusWidget());
    if (buttonList)
        buttonList->ShowSearchDialog();

    MythUIButtonTree *buttonTree = dynamic_cast<MythUIButtonTree *>(GetFocusWidget());
    if (buttonTree)
        buttonTree->ShowSearchDialog();
}

void MusicCommon::ShowMenu(void)
{
    MythMenu *mainMenu = createMainMenu();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(mainMenu, popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete mainMenu;
}

MythMenu* MusicCommon::createMainMenu(void)
{
    QString label = tr("Actions");

    MythMenu *menu = new MythMenu(label, this, "actionmenu");

    if (GetFocusWidget() && (GetFocusWidget()->inherits("MythUIButtonList") ||
                             GetFocusWidget()->inherits("MythUIButtonTree")))
        menu->AddItem(tr("Search List..."));

    menu->AddItem(tr("Switch View"),      NULL, createViewMenu());

    if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_TRACKS)
    {
        menu->AddItem(tr("Playlist Options"), NULL, createPlaylistMenu());
        menu->AddItem(tr("Set Shuffle Mode"), NULL, createShuffleMenu());
        menu->AddItem(tr("Set Repeat Mode"),  NULL, createRepeatMenu());
    }

    menu->AddItem(tr("Player Options"),   NULL, createPlayerMenu());

    if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_TRACKS)
        menu->AddItem(tr("Quick Playlists"),  NULL, createQuickPlaylistsMenu());

    if (m_visualizerVideo)
        menu->AddItem(tr("Change Visualizer"), NULL, createVisualizerMenu());

    return menu;
}

MythMenu* MusicCommon::createViewMenu(void)
{
    QString label = tr("Switch View");

    MythMenu *menu = new MythMenu(label, this, "viewmenu");

    if (m_currentView != MV_PLAYLIST)
        menu->AddItem(tr("Current Playlist"), qVariantFromValue((int)MV_PLAYLIST));
    if (m_currentView != MV_PLAYLISTEDITORTREE)
        menu->AddItem(tr("Playlist Editor - Tree"), qVariantFromValue((int)MV_PLAYLISTEDITORTREE));
    if (m_currentView != MV_PLAYLISTEDITORGALLERY)
        menu->AddItem(tr("Playlist Editor - Gallery"), qVariantFromValue((int)MV_PLAYLISTEDITORGALLERY));
    if (m_currentView != MV_SEARCH)
        menu->AddItem(tr("Search for Music"), qVariantFromValue((int)MV_SEARCH));
    if (m_currentView != MV_RADIO)
        menu->AddItem(tr("Play Radio Stream"), qVariantFromValue((int)MV_RADIO));
    if (m_currentView != MV_VISUALIZER)
        menu->AddItem(tr("Fullscreen Visualizer"), qVariantFromValue((int)MV_VISUALIZER));

    return menu;
}

MythMenu* MusicCommon::createPlaylistMenu(void)
{
    QString label = tr("Playlist Options");

    MythMenu *menu = new MythMenu(label, this, "playlistmenu");

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

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "exitmenu");

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

    MythMenu *menu = new MythMenu(label, this, "playermenu");

    menu->AddItem(tr("Change Volume"));
    menu->AddItem(tr("Mute"));
    menu->AddItem(tr("Previous Track"));
    menu->AddItem(tr("Next Track"));

    if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_TRACKS)
    {
        menu->AddItem(tr("Jump Back"));
        menu->AddItem(tr("Jump Forward"));
    }

    menu->AddItem(tr("Play"));
    menu->AddItem(tr("Stop"));

    if (gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_TRACKS)
        menu->AddItem(tr("Pause"));

    return menu;
}

MythMenu* MusicCommon::createRepeatMenu(void)
{
    QString label = tr("Set Repeat Mode");

    MythMenu *menu = new MythMenu(label, this, "repeatmenu");

    menu->AddItem(tr("None"),  qVariantFromValue((int)MusicPlayer::REPEAT_OFF));
    menu->AddItem(tr("Track"), qVariantFromValue((int)MusicPlayer::REPEAT_TRACK));
    menu->AddItem(tr("All"),   qVariantFromValue((int)MusicPlayer::REPEAT_ALL));

    menu->SetSelectedByData(static_cast<int>(gPlayer->getRepeatMode()));

    return menu;
}

MythMenu* MusicCommon::createShuffleMenu(void)
{
    QString label = tr("Set Shuffle Mode");

    MythMenu *menu = new MythMenu(label, this, "shufflemenu");

    menu->AddItem(tr("None"),   qVariantFromValue((int)MusicPlayer::SHUFFLE_OFF));
    menu->AddItem(tr("Random"), qVariantFromValue((int)MusicPlayer::SHUFFLE_RANDOM));
    menu->AddItem(tr("Smart"),  qVariantFromValue((int)MusicPlayer::SHUFFLE_INTELLIGENT));
    menu->AddItem(tr("Album"),  qVariantFromValue((int)MusicPlayer::SHUFFLE_ALBUM));
    menu->AddItem(tr("Artist"), qVariantFromValue((int)MusicPlayer::SHUFFLE_ARTIST));

    menu->SetSelectedByData(static_cast<int>(gPlayer->getShuffleMode()));

    return menu;
}

MythMenu* MusicCommon::createQuickPlaylistsMenu(void)
{
    QString label = tr("Quick Playlists");

    MythMenu *menu = new MythMenu(label, this, "quickplaylistmenu");

    menu->AddItem(tr("All Tracks"));

    if (gMusicData->all_music->getCDTrackCount() > 0)
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

    MythMenu *menu = new MythMenu(label, this, "visualizermenu");

    for (uint x = 0; x < static_cast<uint>(m_visualModes.count()); x++)
        menu->AddItem(m_visualModes.at(x), qVariantFromValue(x));

    menu->SetSelectedByData(m_currentVisual);

    return menu;
}

MythMenu* MusicCommon::createPlaylistOptionsMenu(void)
{
    QString label = tr("Add to Playlist Options");

    MythMenu *menu = new MythMenu(label, this, "playlistoptionsmenu");

    menu->AddItem(tr("Replace Tracks"));
    menu->AddItem(tr("Add Tracks"));

    return menu;
}

void MusicCommon::allTracks()
{
   m_whereClause = "ORDER BY music_artists.artist_name, album_name, track";
   showPlaylistOptionsMenu();
}

void MusicCommon::fromCD(void)
{
    m_whereClause = "";
    m_songList.clear();

    // get the list of cd tracks
    for (int x = 1; x <= gMusicData->all_music->getCDTrackCount(); x++)
    {
        MusicMetadata *mdata = gMusicData->all_music->getCDMetadata(x);
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
                    " ORDER BY album_name, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byAlbum(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Album().toUtf8().constData());
    m_whereClause = "WHERE album_name = " + value +
                    " ORDER BY track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byGenre(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Genre().toUtf8().constData());
    m_whereClause = "WHERE genre = " + value +
                    " ORDER BY music_artists.artist_name, album_name, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byYear(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Year());
    m_whereClause = "WHERE music_songs.year = " + value +
                    " ORDER BY music_artists.artist_name, album_name, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byTitle(void)
{
    MusicMetadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Title().toUtf8().constData());
    m_whereClause = "WHERE music_songs.name = " + value +
                    " ORDER BY music_artists.artist_name, album_name, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::showPlaylistOptionsMenu(bool addMainMenu)
{
    m_playlistOptions.playPLOption = PL_CURRENT;

    // Don't bother showing the dialog if the current playlist is empty
    if (gPlayer->getPlaylist()->getSongs().count() == 0)
    {
        m_playlistOptions.insertPLOption = PL_REPLACE;
        doUpdatePlaylist(true);
        return;
    }

    MythMenu *menu = createPlaylistOptionsMenu();

    if (addMainMenu)
        menu->AddItem(tr("More Options"), NULL, createMainMenu());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack, "playlistoptionsmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menu;
}

void MusicCommon::doUpdatePlaylist(bool startPlayback)
{
    int curTrackID, trackCount;
    int curPos = gPlayer->getCurrentTrackPos();

    trackCount = gPlayer->getPlaylist()->getSongs().count();

    // store id of current track
    if (gPlayer->getCurrentMetadata())
        curTrackID = gPlayer->getCurrentMetadata()->ID();
    else
        curTrackID = -1;

    if (!m_whereClause.isEmpty())
    {
        // update playlist from quick playlist
        gMusicData->all_playlists->getActive()->fillSonglistFromQuery(
                    m_whereClause, true,
                    m_playlistOptions.insertPLOption, curTrackID);
        m_whereClause.clear();
    }
    else if (!m_songList.isEmpty())
    {
        // update playlist from song list (from the playlist editor)
        gMusicData->all_playlists->getActive()->fillSonglistFromList(
                    m_songList, true,
                    m_playlistOptions.insertPLOption, curTrackID);

        m_songList.clear();
    }

    m_currentTrack = gPlayer->getCurrentTrackPos();

    updateUIPlaylist();

    if (startPlayback || gPlayer->isPlaying())
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
                        if (!gPlayer->setCurrentTrackPos(trackCount))
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
    else
    {
        switch (m_playlistOptions.playPLOption)
        {
            case PL_CURRENT:
                break;

            case PL_FIRST:
                m_currentTrack = 0;
                break;

            case PL_FIRSTNEW:
            {
                switch (m_playlistOptions.insertPLOption)
                {
                    case PL_REPLACE:
                        m_currentTrack = 0;
                        break;

                    case PL_INSERTATEND:
                        m_currentTrack = 0;
                        break;

                    case PL_INSERTAFTERCURRENT:
                        // this wont work if the track are shuffled
                        m_currentTrack = m_currentTrack + 1;
                        break;

                    default:
                        m_currentTrack = 0;
                }

                break;
            }
        }

        gPlayer->changeCurrentTrack(m_currentTrack);
    }

    gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                      m_currentTrack, &m_playlistPlayedTime);
    updatePlaylistStats();
    updateTrackInfo(gPlayer->getCurrentMetadata());
}

bool MusicCommon::restorePosition(int trackID)
{
    // try to move to the current track
    bool foundTrack = false;

    if (trackID != -1)
    {
        for (int x = 0; x < gPlayer->getPlaylist()->getSongs().size(); x++)
        {
            MusicMetadata *mdata = gPlayer->getPlaylist()->getSongs().at(x);
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
#define MUSICVOLUMEPOPUPTIME 4 * 1000

MythMusicVolumeDialog::MythMusicVolumeDialog(MythScreenStack *parent, const char *name)
         : MythScreenType(parent, name, false),
    m_displayTimer(NULL),  m_messageText(NULL),
    m_volText(NULL),       m_muteState(NULL),
    m_volProgress(NULL)
{
}

MythMusicVolumeDialog::~MythMusicVolumeDialog(void)
{
    if (m_displayTimer)
    {
        m_displayTimer->stop();
        delete m_displayTimer;
        m_displayTimer = NULL;
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
    connect(m_displayTimer, SIGNAL(timeout()), this, SLOT(Close()));
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
        QString action = actions[i];
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
TrackInfoDialog::TrackInfoDialog(MythScreenStack *parent, MusicMetadata *metadata, const char *name)
         : MythScreenType(parent, name, false)
{
    m_metadata = metadata;
}

TrackInfoDialog::~TrackInfoDialog(void)
{
}

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

    return true;
}

bool TrackInfoDialog::keyPressEvent(QKeyEvent *event)
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
