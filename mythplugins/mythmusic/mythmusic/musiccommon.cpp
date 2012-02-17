// ANSI C includes
#include <cstdlib>

// C++ includes
#include <iostream>
using namespace std;

// Qt includes
#include <QApplication>

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

// MythMusic includes
#include "metadata.h"
#include "constants.h"
#include "streaminput.h"
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
#include "visualizerview.h"
#include "searchview.h"

MusicCommon::MusicCommon(MythScreenStack *parent, const QString &name)
            : MythScreenType(parent, name)
{
    m_mainvisual = NULL;
    m_visualModeTimer = NULL;
    m_moveTrackMode = false;
    m_movingTrack = false;
    m_currentTime = 0;
    m_maxTime = 0;

#if 0
    cd_reader_thread = NULL;
    cd_watcher = NULL;
    scan_for_cd = gContext->GetNumSetting("AutoPlayCD", 0);
    m_CDdevice = dev;
#endif

    m_cycleVisualizer = gCoreContext->GetNumSetting("VisualCycleOnSongChange", 0);

    if (LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
        lcd->setFunctionLEDs(FUNC_MUSIC, true);
    }
}

MusicCommon::~MusicCommon(void)
{
    gPlayer->removeListener(this);

    if (m_visualModeTimer)
    {
        delete m_visualModeTimer;
        m_visualModeTimer = NULL;
    }

    if (m_mainvisual)
    {
        stopVisualizer();
        delete m_mainvisual;
        m_mainvisual = NULL;
    }

#if 0
    if (cd_reader_thread)
    {
        cd_watcher->stop();
        cd_reader_thread->wait();
        delete cd_reader_thread;
    }

    if (class LCD *lcd = LCD::Get())
        lcd->switchToTime();

    gMusicData->all_music->save();
    gPlayer->refreshMetadata();
#endif

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
    UIUtilW::Assign(this, m_playlistProgressText, "playlistposition", &err);
    UIUtilW::Assign(this, m_playlistLengthText,   "playlisttime", &err);

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
        gPlayer->restorePosition();

    m_currentTrack = gPlayer->getCurrentTrackPos();

    Metadata *curMeta = gPlayer->getCurrentMetadata();
    if (curMeta)
        updateTrackInfo(curMeta);

    updateProgressBar();

    if (m_currentPlaylist)
    {
        connect(m_currentPlaylist, SIGNAL(itemClicked(MythUIButtonListItem*)),
                this, SLOT(playlistItemClicked(MythUIButtonListItem*)));
        connect(m_currentPlaylist, SIGNAL(itemVisible(MythUIButtonListItem*)),
                this, SLOT(playlistItemVisible(MythUIButtonListItem*)));

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

        QString visual_delay = gCoreContext->GetSetting("VisualModeDelay");
        bool delayOK;
        m_visualModeDelay = visual_delay.toInt(&delayOK);
        if (!delayOK)
            m_visualModeDelay = 0;
        if (m_visualModeDelay > 0)
        {
            m_visualModeTimer = new QTimer(this);
            m_visualModeTimer->start(m_visualModeDelay * 1000);
            connect(m_visualModeTimer, SIGNAL(timeout()), this, SLOT(visEnable()));
        }

        m_mainvisual->setVisual(m_visualModes[m_currentVisual]);

        if (m_visualText)
            m_visualText->SetText(m_visualModes[m_currentVisual]);

        if (gPlayer->isPlaying())
            startVisualizer();
    }

    m_controlVolume = gCoreContext->GetNumSetting("MythControlsVolume", 0);
    updateVolume();

    if (m_movingTracksState)
        m_movingTracksState->DisplayState("off");

    if (m_trackState)
    {
        if (!gPlayer->isPlaying()) // TODO: Add Player status check
        {
            if (gPlayer->getOutput() && gPlayer->getOutput()->IsPaused())
                m_trackState->DisplayState("paused");
            else
                m_trackState->DisplayState("stopped");
        }
        else
            m_trackState->DisplayState("playing");
    }

    if (gPlayer->isPlaying())
    {
        if (m_stopButton)
            m_stopButton->SetLocked(false);
        if (m_playButton)
            m_playButton->SetLocked(true);
        if (m_pauseButton)
            m_pauseButton->SetLocked(false);
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
}

void MusicCommon::updateShuffleMode(void)
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

    switch (view)
    {
#if 0
        case MV_LYRICS:
        {
            LyricsView *view = new LyricsView(mainStack);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }
#endif
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
            bool restorePos = (m_currentView ==  MV_PLAYLISTEDITORGALLERY);
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
            bool restorePos = (m_currentView ==  MV_PLAYLISTEDITORTREE);
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

        default:
            return;
    }

    Close();
}

#if 0
bool MusicCommon::onMediaEvent(MythMediaDevice*)
{
    return scan_for_cd;
}
#endif

bool MusicCommon::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    resetVisualiserTimer();

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
            if (m_ffButton)
                m_ffButton->Push();
            else
                seekforward();
        }
        else if (action == "RWND")
        {
            if (m_rewButton)
                m_rewButton->Push();
            else
                seekback();
        }
        else if (action == "PAUSE")
        {
            // if we are playing or are paused PAUSE will toggle the pause state
            if (gPlayer->isPlaying() || (gPlayer->getOutput() && gPlayer->getOutput()->IsPaused()))
            {
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
                    Metadata *mdata = qVariantValue<Metadata*> (m_currentPlaylist->GetItemCurrent()->GetData());
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
                Metadata *mdata = qVariantValue<Metadata*> (item->GetData());
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
    if (gPlayer->getOutput())
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
    if (m_trackProgress)
    {
        int percentplayed = 1;
        if (m_maxTime)
            percentplayed = (int)(((double)m_currentTime / (double)m_maxTime)*100);
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
#if 0
    if (speed_status)
    {
        if (volume_status && (volume_status->getOrder() != -1))
        {
            volume_status->SetOrder(-1);
            volume_status->refresh();
        }

        if (show)
        {
            QString speed_text;
            float playSpeed = gPlayer->getSpeed();
            speed_text.sprintf("x%4.2f",playSpeed);
            speed_status->SetText(speed_text);
            speed_status->SetOrder(0);
            speed_status->refresh();
            volume_display_timer->setSingleShot(true);
            volume_display_timer->start(2000);
        }
    }

    if (LCD *lcd = LCD::Get())
    {
        QString speed_text;
        float playSpeed = gPlayer->getSpeed();
        speed_text.sprintf("x%4.2f", playSpeed);
        speed_text = tr("Speed: ") + speed_text;
        QList<LCDTextItem> textItems;
        textItems.append(LCDTextItem(lcd->getLCDHeight() / 2, ALIGN_CENTERED,
                                     speed_text, "Generic", false));
        lcd->switchToGeneric(textItems);
    }
#endif
}


void MusicCommon::resetVisualiserTimer()
{
    //FIXME do we still need the timer?
    if (m_visualModeDelay > 0 && m_visualModeTimer)
        m_visualModeTimer->start(m_visualModeDelay * 1000);
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
        resetVisualiserTimer();
        m_mainvisual->setVisual("Blank");
        m_mainvisual->setVisual(m_visualModes[m_currentVisual]);
    }
    else if (m_visualModes.count() == 1 && m_visualModes[m_currentVisual] == "AlbumArt")
    {
        // If only the AlbumArt visualization is selected, then go ahead and
        // restart the visualization.  This will give AlbumArt the opportunity
        // to change images if there are multiple images available.
        resetVisualiserTimer();
        m_mainvisual->setVisual("Blank");
        m_mainvisual->setVisual(m_visualModes[m_currentVisual]);
    }

    if (m_visualText)
        m_visualText->SetText(m_visualModes[m_currentVisual]);
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

void MusicCommon::setTrackOnLCD(Metadata *mdata)
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
    // Rationale here is that if you can't get visual feedback on ratings
    // adjustments, you probably should not be changing them
    // TODO: should check if the rating is visible in the playlist buttontlist
    if (!m_ratingState)
        return;

    Metadata *curMeta = gPlayer->getCurrentMetadata();
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
        Metadata *curMeta = gPlayer->getCurrentMetadata();
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

        int rs;
        m_currentTime = rs = oe->elapsedSeconds();

        QString time_string = getTimeString(rs, m_maxTime);

        updateProgressBar();

        Metadata *curMeta = gPlayer->getCurrentMetadata();
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

        if (curMeta )
        {
            if (m_timeText)
                m_timeText->SetText(time_string);
            if (m_infoText)
                m_infoText->SetText(info_string);
        }

        // TODO only need to update the playlist times here
        updatePlaylistStats();
    }
    else if (event->type() == OutputEvent::Error)
    {
        OutputEvent *aoe = dynamic_cast<OutputEvent *>(event);

        if (!aoe)
            return;

        statusString = tr("Output error.");

        LOG(VB_GENERAL, LOG_ERR, QString("%1 %2").arg(statusString)
            .arg(*aoe->errorMessage()));
        ShowOkPopup(QString("MythMusic has encountered the following error:\n%1")
                    .arg(*aoe->errorMessage()));
        stopAll();
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
    else if (event->type() == DecoderEvent::Error)
    {
        stopAll();

        statusString = tr("Decoder error.");

        DecoderEvent *dxe = dynamic_cast<DecoderEvent *>(event);

        if (!dxe)
            return;

        LOG(VB_GENERAL, LOG_ERR, QString("%1 %2").arg(statusString)
            .arg(*dxe->errorMessage()));

        ShowOkPopup(QString("MythMusic has encountered the following error:\n%1")
                    .arg(*dxe->errorMessage()));
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = dynamic_cast<DialogCompletionEvent*>(event);

        if (!dce)
            return;

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
                    Metadata *mdata = qVariantValue<Metadata*> (item->GetData());
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
            if (resulttext != tr("Cancel"))
            {
                int mode = dce->GetData().toInt();
                gPlayer->setRepeatMode((MusicPlayer::RepeatMode) mode);
                updateRepeatMode();
            }
        }
        else if (resultid == "shufflemenu")
        {
            if (resulttext != tr("Cancel"))
            {
                int mode = dce->GetData().toInt();
                gPlayer->setShuffleMode((MusicPlayer::ShuffleMode) mode);
                updateShuffleMode();

                //TODO maybe this should be done using an event from the player?
                // store id of current track
                int curTrackID = -1;
                if (gPlayer->getCurrentMetadata())
                    curTrackID = gPlayer->getCurrentMetadata()->ID();

                updateUIPlaylist();

                if (!restorePosition(curTrackID))
                    playFirstTrack();

                // need this to update the next track info
                Metadata *curMeta = gPlayer->getCurrentMetadata();
                if (curMeta)
                    updateTrackInfo(curMeta);
            }
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
            else if (resulttext ==  tr("Tracks by current Artist"))
                byArtist();
            else if (resulttext == tr("Tracks from current Genre"))
                byGenre();
            else if (resulttext == tr("Tracks from current Album"))
                byAlbum();
            else if (resulttext == tr("Track from current Year"))
                byYear();
            else if (resulttext == tr("Tracks with same Title"))
                byTitle();
        }
        else if (resultid == "playlistoptionsmenu")
        {
            if (resulttext == tr("Replace"))
            {
                m_playlistOptions.insertPLOption = PL_REPLACE;
                m_playlistOptions.removeDups = false;
                doUpdatePlaylist();
            }
            else if (resulttext ==  tr("Insert after current track"))
            {
                m_playlistOptions.insertPLOption = PL_INSERTAFTERCURRENT;
                doUpdatePlaylist();
            }
            else if (resulttext == tr("Append to end"))
            {
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                doUpdatePlaylist();
            }
            else if (resulttext == tr("Add"))
            {
                m_playlistOptions.insertPLOption = PL_INSERTATEND;
                doUpdatePlaylist();
            }
        }
        else if (resultid == "visualizermenu")
        {
            if (dce->GetResult() >= 0)
            {
                m_currentVisual = dce->GetData().toInt();

                //Change to the new visualizer
                resetVisualiserTimer();
                m_mainvisual->setVisual(m_visualModes[m_currentVisual]);

                if (m_visualText)
                    m_visualText->SetText(m_visualModes[m_currentVisual]);
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
                Metadata *mdata = qVariantValue<Metadata*> (item->GetData());
                if (mdata && mdata->ID() == (Metadata::IdType) trackID)
                {
                    m_currentPlaylist->RemoveItem(item);
                    break;
                }
            }
        }

        m_currentTrack = gPlayer->getCurrentTrackPos();

        // if we have just removed the playing track from the playlist
        // move to the next trackCount
        if (gPlayer->getCurrentMetadata())
        {
            if (gPlayer->getCurrentMetadata()->ID() == (Metadata::IdType) trackID)
                gPlayer->next();
        }

        gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                          m_currentTrack, &m_playlistPlayedTime);
        updatePlaylistStats();
        updateTrackInfo(gPlayer->getCurrentMetadata());
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
                Metadata *mdata = gMusicData->all_music->getMetadata(trackID);

                if (mdata)
                {
                    MetadataMap metadataMap;
                    mdata->toMap(metadataMap);

                    MythUIButtonListItem *item =
                            new MythUIButtonListItem(m_currentPlaylist, "", qVariantFromValue(mdata));

                    item->SetTextFromMap(metadataMap);

                    if (gPlayer->isPlaying() && mdata->ID() == gPlayer->getCurrentMetadata()->ID())
                    {
                        item->SetFontState("running");
                        item->DisplayState("playing", "playstate");
                    }
                    else
                    {
                        item->SetFontState("normal");
                        item->DisplayState("default", "playstate");
                    }
                }
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
                Metadata *mdata = qVariantValue<Metadata*> (item->GetData());
                if (mdata && mdata->ID() == trackID)
                {
                    MetadataMap metadataMap;
                    mdata->toMap(metadataMap);
                    item->SetTextFromMap(metadataMap);

                    item->DisplayState(QString("%1").arg(mdata->Rating()), "ratingstate");
                }
            }
        }

        if (trackID == gPlayer->getCurrentMetadata()->ID())
            updateTrackInfo(gPlayer->getCurrentMetadata());

        // this will ensure the next track info gets updated
        if (trackID == gPlayer->getNextMetadata()->ID())
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
                Metadata *mdata = qVariantValue<Metadata*> (item->GetData());
                if (mdata && mdata->ID() == trackID)
                {
                    // reload the albumart image if one has already been loaded for this track
                    if (!item->GetImage().isEmpty())
                    {
                        QString artFile = mdata->getAlbumArtFile();
                        if (artFile.isEmpty())
                            item->SetImage("mm_nothumb.png");
                        else
                            item->SetImage(mdata->getAlbumArtFile());
                    }
                }
            }
        }

        if (trackID == gPlayer->getCurrentMetadata()->ID())
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
        QHash<QString, QString> map;
        gPlayer->toMap(map);
        m_volumeText->SetTextFromMap(map);
    }

    if (m_muteState)
    {
        bool muted = gPlayer->isMuted();
        m_muteState->DisplayState(muted ? "on" : "off");
    }
}

void MusicCommon::editTrackInfo(Metadata *mdata)
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

void MusicCommon::updateTrackInfo(Metadata *mdata)
{
    if (!mdata)
    {
        MetadataMap metadataMap;
        Metadata metadata;
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

    m_maxTime = mdata->Length() / 1000;

    // get map for current track
    MetadataMap metadataMap;
    mdata->toMap(metadataMap);

    // add the map from the next track
    Metadata *nextMetadata = gPlayer->getNextMetadata();
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

void MusicCommon::showTrackInfo(Metadata *mdata)
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

    if (item->GetImage().isEmpty())
    {
        Metadata *mdata = qVariantValue<Metadata*> (item->GetData());
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

void MusicCommon::updateUIPlaylist(void)
{
    if (m_noTracksText)
        m_noTracksText->SetVisible((gPlayer->getPlaylist()->getSongs().count() == 0));

    if (!m_currentPlaylist)
        return;

    m_currentPlaylist->Reset();

    Playlist *playlist = gPlayer->getPlaylist();

    QList<Metadata*> songlist = playlist->getSongs();
    QList<Metadata*>::iterator it = songlist.begin();
    for (; it != songlist.end(); ++it)
    {
        Metadata *mdata = (*it);
        if (mdata)
        {
            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_currentPlaylist, "", qVariantFromValue(mdata));

            MetadataMap metadataMap;
            mdata->toMap(metadataMap);
            item->SetTextFromMap(metadataMap);

            if (gPlayer->isPlaying() && mdata->ID() == gPlayer->getCurrentMetadata()->ID())
            {
                item->SetFontState("running");
                item->DisplayState("playing", "playstate");
            }
            else
            {
                item->SetFontState("normal");
                item->DisplayState("default", "playstate");
            }

            item->DisplayState(QString("%1").arg(mdata->Rating()), "ratingstate");

            if (mdata->ID() == gPlayer->getCurrentMetadata()->ID())
                m_currentPlaylist->SetItemCurrent(item);
        }
    }
}

void MusicCommon::updateUIPlayedList(void)
{
    if (!m_playedTracksList)
        return;

    m_playedTracksList->Reset();

    QList<Metadata> playedList = gPlayer->getPlayedTracksList();

    for (int x = playedList.count(); x > 0; x--)
    {
        Metadata *mdata = &playedList[x-1];
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_playedTracksList, "");

        MetadataMap metadataMap;
        mdata->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);

        if (x == playedList.count() && gPlayer->isPlaying())
        {
            item->SetFontState("playing");
            item->DisplayState("playing", "playstate");
        }
        else
        {
            item->SetFontState("normal");
            item->DisplayState("default", "playstate");
        }
    }
}

void MusicCommon::updatePlaylistStats(void)
{
    int trackCount = gPlayer->getPlaylist()->getSongs().size();

    QHash<QString, QString> map;
    if (gPlayer->isPlaying() && trackCount > 0)
    {
        map["playlistposition"] = QString("%1 of %2").arg(m_currentTrack + 1).arg(trackCount);
        map["playlistcurrent"] = QString("%1").arg(m_currentTrack + 1);
        map["playlistcount"] = QString("%1").arg(trackCount);
        map["playlisttime"] = getTimeString(m_playlistPlayedTime + m_currentTime, m_playlistMaxTime);
        map["playlistplayedtime"] = getTimeString(m_playlistPlayedTime + m_currentTime, 0);
        map["playlisttotaltime"] = getTimeString(m_playlistMaxTime, 0);
    }
    else
    {
        map["playlistposition"] = "";
        map["playlistcurrent"] = "";
        map["playlistcount"] = "";
        map["playlisttime"] = "";
        map["playlistplayedtime"] = "";
        map["playlisttotaltime"] = "";
    }

    if (m_playlistProgressText)
        m_playlistProgressText->SetTextFromMap(map);

    if (m_playlistLengthText)
        m_playlistLengthText->SetTextFromMap(map);

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
    menu->AddItem(tr("Playlist Options"), NULL, createPlaylistMenu());
    menu->AddItem(tr("Set Shuffle Mode"), NULL, createShuffleMenu());
    menu->AddItem(tr("Set Repeat Mode"),  NULL, createRepeatMenu());
    menu->AddItem(tr("Player Options"),   NULL, createPlayerMenu());
    menu->AddItem(tr("Quick Playlists"),  NULL, createQuickPlaylistsMenu());

    if (m_visualizerVideo)
        menu->AddItem(tr("Change Visualizer"), NULL, createVisualizerMenu());

    menu->AddItem(tr("Cancel"));

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
    if (m_currentView != MV_VISUALIZER)
        menu->AddItem(tr("Fullscreen Visualizer"), qVariantFromValue((int)MV_VISUALIZER));
#if 0
    menu->AddItem(tr("Lyrics"), qVariantFromValue((int)MV_LYRICS));
    menu->AddItem(tr("Artist Information"), qVariantFromValue((int)MV_ARTISTINFO));
#endif
    menu->AddItem(tr("Cancel"));

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

    menu->AddItem(tr("Cancel"));

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
    menu->AddItem(tr("Jump Back"));
    menu->AddItem(tr("Jump Forward"));
    menu->AddItem(tr("Play"));
    menu->AddItem(tr("Stop"));
    menu->AddItem(tr("Pause"));

    menu->AddItem(tr("Cancel"));

    return menu;
}

MythMenu* MusicCommon::createRepeatMenu(void)
{
    QString label = tr("Set Repeat Mode");

    MythMenu *menu = new MythMenu(label, this, "repeatmenu");

    menu->AddItem(tr("None"),  qVariantFromValue((int)MusicPlayer::REPEAT_OFF));
    menu->AddItem(tr("Track"), qVariantFromValue((int)MusicPlayer::REPEAT_TRACK));
    menu->AddItem(tr("All"),   qVariantFromValue((int)MusicPlayer::REPEAT_ALL));

    menu->AddItem(tr("Cancel"));

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

    menu->AddItem(tr("Cancel"));

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
        menu->AddItem(tr("Tracks by current Artist"));
        menu->AddItem(tr("Tracks from current Album"));
        menu->AddItem(tr("Tracks from current Genre"));
        menu->AddItem(tr("Track from current Year"));
        menu->AddItem(tr("Tracks with same Title"));
    }

    menu->AddItem(tr("Cancel"));

    return menu;
}

MythMenu* MusicCommon::createVisualizerMenu(void)
{
    QString label = tr("Choose Visualizer");

    MythMenu *menu = new MythMenu(label, this, "visualizermenu");

    for (int x = 0; x < m_visualModes.count(); x++)
        menu->AddItem(m_visualModes.at(x), qVariantFromValue(x));

    menu->AddItem(tr("Cancel"));

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
        Metadata *mdata = gMusicData->all_music->getCDMetadata(x);
        if (mdata)
        {
            m_songList.append((mdata)->ID());
        }
    }

    showPlaylistOptionsMenu();
}

void MusicCommon::byArtist(void)
{
    Metadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Artist().toUtf8().constData());
    m_whereClause = "WHERE music_artists.artist_name = " + value +
                    " ORDER BY album_name, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byAlbum(void)
{
    Metadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Album().toUtf8().constData());
    m_whereClause = "WHERE album_name = " + value +
                    " ORDER BY track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byGenre(void)
{
    Metadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Genre().toUtf8().constData());
    m_whereClause = "WHERE genre = " + value +
                    " ORDER BY music_artists.artist_name, album_name, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byYear(void)
{
    Metadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Year());
    m_whereClause = "WHERE music_songs.year = " + value +
                    " ORDER BY music_artists.artist_name, album_name, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::byTitle(void)
{
    Metadata* mdata = gPlayer->getCurrentMetadata();
    if (!mdata)
        return;

    QString value = formattedFieldValue(mdata->Title().toUtf8().constData());
    m_whereClause = "WHERE music_songs.name = " + value +
                    " ORDER BY music_artists.artist_name, album_name, track";

    showPlaylistOptionsMenu();
}

void MusicCommon::showPlaylistOptionsMenu(void)
{
    //FIXME: hard code these for the moment - remove later?
    m_playlistOptions.playPLOption = PL_CURRENT;
    m_playlistOptions.removeDups = true;

    // Don't bother showing the dialog if the current playlist is empty
    if (gPlayer->getPlaylist()->getSongs().count() == 0)
    {
        m_playlistOptions.insertPLOption = PL_REPLACE;
        doUpdatePlaylist();
        return;
    }

    QString label = tr("Add to Playlist Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "playlistoptionsmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "playlistoptionsmenu");

    menu->AddButton(tr("Replace"));

    if (gPlayer->getShuffleMode() == MusicPlayer::SHUFFLE_OFF)
    {
        menu->AddButton(tr("Insert after current track"));
        menu->AddButton(tr("Append to end"));
    }
    else
        menu->AddButton(tr("Add"));

    menu->AddButton(tr("Cancel"));

    popupStack->AddScreen(menu);
}

void MusicCommon::doUpdatePlaylist(void)
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
                    m_whereClause, m_playlistOptions.removeDups,
                    m_playlistOptions.insertPLOption, curTrackID);
        m_whereClause.clear();
    }
    else if (!m_songList.isEmpty())
    {
        // update playlist from song list (from the playlist editor)
        gMusicData->all_playlists->getActive()->fillSonglistFromList(
                    m_songList, m_playlistOptions.removeDups,
                    m_playlistOptions.insertPLOption, curTrackID);

        m_songList.clear();
    }
    else
    {
        // update playlist from smart playlist
//        gMusicData->all_playlists->getActive()->fillSonglistFromSmartPlaylist(
//                    curSmartPlaylistCategory, curSmartPlaylistName,
//                    bRemoveDups, insertOption, curTrackID);
    }

    updateUIPlaylist();

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
            //TODO need to test these
            switch (m_playlistOptions.insertPLOption)
            {
                case PL_REPLACE:
                    playFirstTrack();
                    break;

                case PL_INSERTATEND:
                {
                    pause();
                    if (!gPlayer->setCurrentTrackPos(trackCount + 1))
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

    gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                      m_currentTrack, &m_playlistPlayedTime);
}

bool MusicCommon::restorePosition(int trackID)
{
    // try to move to the current track
    bool foundTrack = false;

    if (trackID != -1)
    {
        for (int x = 0; x < gPlayer->getPlaylist()->getSongs().size(); x++)
        {
            Metadata *mdata = gPlayer->getPlaylist()->getSongs().at(x);
            if (mdata && mdata->ID() == (Metadata::IdType) trackID)
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
#define MUSICVOLUMEPOPUPTIME 5 * 1000

MythMusicVolumeDialog::MythMusicVolumeDialog(MythScreenStack *parent, const char *name)
         : MythScreenType(parent, name, false)
{
    m_displayTimer = NULL;
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
        QHash<QString, QString> map;
        gPlayer->toMap(map);
        m_volText->SetTextFromMap(map);
    }
}

//---------------------------------------------------------
// TrackInfoDialog
//---------------------------------------------------------
TrackInfoDialog::TrackInfoDialog(MythScreenStack *parent, Metadata *metadata, const char *name)
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

    MetadataMap metadataMap;
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
