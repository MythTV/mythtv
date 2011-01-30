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
//#include <mythuivideo.h>
#include <audiooutput.h>
#include <compat.h>

// MythMusic includes
#include "metadata.h"
#include "constants.h"
#include "streaminput.h"
#include "decoder.h"
#include "databasebox.h"
#include "mainvisual.h"
#include "smartplaylist.h"
#include "playlistcontainer.h"
#include "search.h"
#include "editmetadata.h"

#ifndef USING_MINGW
#include "cddecoder.h"
#endif // USING_MINGW

#include "musiccommon.h"
//#include "playlistview.h"
//#include "lyricsview.h"
//#include "playlisteditorview.h"
//#include "visualizerview.h"
//#include "searchview.h"

#if 0
enum MusicViews
{
    MV_PLAYLIST,
    MV_LYRICS,
    MV_PLAYLISTEDITOR,
    MV_VISUALIZER,
    MV_SEARCH,
    MV_ARTISTINFO,
    MV_ALBUMINFO,
    MV_TRACKINFO,
    MV_RADIO
};

Q_DECLARE_METATYPE(MusicViews);
#endif

MusicCommon::MusicCommon(MythScreenStack *parent, const QString &name)
            : MythScreenType(parent, name)
{
    m_mainvisual = NULL;
    m_visualModeTimer = NULL;
    m_moveTrackMode = false;
    m_movingTrack = false;

    m_cycleVisualizer = gCoreContext->GetNumSetting("VisualCycleOnSongChange");

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

    UIUtilW::Assign(this, m_shuffleState,      "shufflestate", &err);
    UIUtilW::Assign(this, m_repeatState,       "repeatstate", &err);
    UIUtilW::Assign(this, m_movingTracksState, "movingtracksstate", &err);

    UIUtilW::Assign(this, m_ratingState,       "ratingstate", &err);

    UIUtilW::Assign(this, m_trackProgress,     "progress", &err);
    UIUtilW::Assign(this, m_trackProgressText, "trackprogress", &err);
    UIUtilW::Assign(this, m_trackSpeedText, "trackspeed", &err);
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

    UIUtilW::Assign(this, m_currentPlaylist, "currentplaylist", &err);

    m_visualizerVideo = NULL;
    //    UIUtilW::Assign(this, m_visualizerVideo, "visualizer", &err);

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

    if ((this->objectName() != "music_miniplayer") // HACK
        && !gPlayer->isPlaying())
    {
        gPlayer->loadPlaylist();
        gPlayer->restorePosition(gCoreContext->GetNumSetting("MusicBookmark"));
    }

    m_currentTrack = gPlayer->getCurrentTrackPos();

    Metadata *curMeta = gPlayer->getCurrentMetadata();
    if (curMeta)
        updateTrackInfo(curMeta);

    if (m_currentPlaylist)
    {
        connect(m_currentPlaylist, SIGNAL(itemClicked(MythUIButtonListItem*)),
                this, SLOT(playlistItemClicked(MythUIButtonListItem*)));
        connect(m_currentPlaylist, SIGNAL(itemSelected(MythUIButtonListItem*)),
                this, SLOT(playlistItemSelected(MythUIButtonListItem*)));

        updateUIPlaylist();
    }

#if 0
    if (m_visualizerVideo)
    {
        // Warm up the visualizer
        m_mainvisual = new MainVisual(m_visualizerVideo);

        gPlayer->addVisual(m_mainvisual);

        m_fullscreenBlank = false;

        m_visualModes = MainVisual::visualizers;

        m_randomVisualizer = gCoreContext->GetNumSetting("VisualRandomize");

        m_currentVisual = MainVisual::currentVisualizer;

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

        m_mainvisual->setDecoder(gPlayer->getDecoder());
        m_mainvisual->setOutput(gPlayer->getOutput());

        m_mainvisual->setVisual(m_visualModes[m_currentVisual]);

        if (gPlayer->isPlaying())
            startVisualizer();
    }
#endif

    m_controlVolume = gCoreContext->GetNumSetting("MythControlsVolume");
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

    gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                      m_currentTrack, &m_playlistPlayedTime);
    updatePlaylistStats();

    return err;
}


void MusicCommon::switchView(int view)
{
    (void) view;
#if 0
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    stopVisualizer();

    gPlayer->removeListener(this);

    switch (view)
    {
        case MV_LYRICS:
        {
            LyricsView *view = new LyricsView(mainStack);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }

        case MV_PLAYLIST:
        {
            PlaylistView *view = new PlaylistView(mainStack);

            if (view->Create())
                mainStack->AddScreen(view);
            else
                delete view;

            break;
        }

        case MV_PLAYLISTEDITOR:
        {
            PlaylistEditorView *view = new PlaylistEditorView(mainStack);

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
#endif
}

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
                item->MoveUpDown(true);
                gPlayer->getPlaylist()->moveTrackUpDown(true, m_currentPlaylist->GetCurrentPos());
            }
            else if (action == "DOWN")
            {
                item->MoveUpDown(false);
                gPlayer->getPlaylist()->moveTrackUpDown(false, m_currentPlaylist->GetCurrentPos());
            }

            return true;
        }

        if (action == "ESCAPE")
        {
            QString exit_action = gCoreContext->GetSetting("MusicExitAction",
                                                           "prompt");

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
        {
            cycleVisualizer();
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
        else if (action == "REFRESH")
        {
            m_currentPlaylist->SetItemCurrent(m_currentTrack);
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
                next_visualizer = rand() % m_visualModes.count();
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
        m_infoText->SetText("");
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
    if (!m_ratingState)
        return;

    Metadata *curMeta = gPlayer->getCurrentMetadata();

    if (!curMeta)
        return;

    if (increase)
        curMeta->incRating();
    else
        curMeta->decRating();

    curMeta->persist(); // incRating() already triggers a persist on track changes - redundant?
    m_ratingState->DisplayState(QString("%1").arg(curMeta->Rating()));

    // if all_music is still in scope we need to keep that in sync
    if (gMusicData->all_music)
    {
        Metadata *mdata = gMusicData->all_music->getMetadata(curMeta->ID());
        if (mdata)
        {
            mdata->setRating(curMeta->Rating());
        }
    }
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
        }

        startVisualizer();
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
    }
    else if (event->type() == OutputEvent::Info)
    {
        OutputEvent *oe = (OutputEvent *) event;

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
            info_string.sprintf("%d "+tr("kbps")+ "   %.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
                                oe->bitrate(), float(oe->frequency()) / 1000.0,
                                oe->channels() > 1 ? "2" : "1");
        }
        else
        {
            info_string.sprintf("%.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
                                float(oe->frequency()) / 1000.0,
                                oe->channels() > 1 ? "2" : "1");
        }

        if (curMeta )
        {
            if (m_timeText)
                m_timeText->SetText(time_string);
            if (m_infoText)
                m_infoText->SetText(info_string);
            if (m_visualText)
                m_visualText->SetText(m_visualModes[m_currentVisual]);
        }

        // TODO only need to update the playlist times here
        updatePlaylistStats();
    }
    else if (event->type() == OutputEvent::Error)
    {
        statusString = tr("Output error.");

        OutputEvent *aoe = (OutputEvent *) event;

        VERBOSE(VB_IMPORTANT, QString("%1 %2").arg(statusString)
            .arg(*aoe->errorMessage()));
        //TODO change to mythui
        MythPopupBox::showOkPopup(GetMythMainWindow(),
                                    statusString,
                                    QString("MythMusic has encountered the following error:\n%1")
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

        stopVisualizer();
    }
    else if (event->type() == DecoderEvent::Finished)
    {
        statusString = tr("Finished playing stream.");
        if (gPlayer->getRepeatMode() == MusicPlayer::REPEAT_TRACK)
            gPlayer->play();
        else
            gPlayer->next();
    }
    else if (event->type() == DecoderEvent::Error)
    {
        stopAll();

        statusString = tr("Decoder error.");

        DecoderEvent *dxe = (DecoderEvent *) event;

        VERBOSE(VB_IMPORTANT, QString("%1 %2").arg(statusString)
            .arg(*dxe->errorMessage()));

        ShowOkPopup(QString("MythMusic has encountered the following error:\n%1")
                    .arg(*dxe->errorMessage()));
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        if (resultid == "viewmenu")
        {
            int view = dce->GetData().toInt();
            switchView(view);
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
                    {
                        gPlayer->getPlaylist()->removeTrack(mdata->ID(), false);
                    }
                }
            }
            else if (resulttext == tr("Remove All Tracks"))
            {
                gPlayer->getPlaylist()->removeAllTracks();
            }
            else if (resulttext == tr("Save As New Playlist"))
            {
                //TODO:
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
                m_movingTrack = false;
                if (m_movingTracksState)
                    m_movingTracksState->DisplayState((m_moveTrackMode ? "on" : "off"));
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
    }
    else if (event->type() == MusicPlayerEvent::TrackChangeEvent)
    {
        MusicPlayerEvent *mpe = (MusicPlayerEvent*)(event);
        int trackID = mpe->TrackID;

        if (m_currentPlaylist)
        {
            if (m_currentTrack >= 0 && m_currentTrack < m_currentPlaylist->GetCount())
            {
                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(m_currentTrack);
                if (item)
                {
                    item->SetFontState("normal");
                    item->DisplayState("stopped", "playstate");
                }
            }

            if (trackID >= 0 && trackID < m_currentPlaylist->GetCount())
            {
                if (m_currentTrack == m_currentPlaylist->GetCurrentPos())
                    m_currentPlaylist->SetItemCurrent(trackID);

                MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(trackID);
                if (item)
                {
                    item->SetFontState("running");
                    item->DisplayState("playing", "playstate");
                }
            }
        }

        m_currentTrack = trackID;

        gPlayer->getPlaylist()->getStats(&m_playlistTrackCount, &m_playlistMaxTime,
                                         m_currentTrack, &m_playlistPlayedTime);
        if (m_playlistProgress)
        {
            m_playlistProgress->SetTotal(m_playlistMaxTime / 1000);
            m_playlistProgress->SetUsed(0);
        }

        updatePlaylistStats();
    }
    else if (event->type() == MusicPlayerEvent::VolumeChangeEvent)
    {
        updateVolume();
    }
    else if (event->type() == MusicPlayerEvent::TrackRemovedEvent)
    {
        //FIXME should just remove the list item
        if (m_currentPlaylist)
        {
            int pos = m_currentPlaylist->GetCurrentPos();
            int topPos = m_currentPlaylist->GetTopItemPos();
            updateUIPlaylist();
            m_currentPlaylist->SetItemCurrent(pos, topPos);
        }

        updatePlaylistStats();
    }
    else if (event->type() == MusicPlayerEvent::TrackAddedEvent)
    {
        if (m_currentPlaylist)
        {
            int pos = m_currentPlaylist->GetCurrentPos();
            int topPos = m_currentPlaylist->GetTopItemPos();
            updateUIPlaylist();
            m_currentPlaylist->SetItemCurrent(pos, topPos);
        }

        updatePlaylistStats();
    }
    else if (event->type() == MusicPlayerEvent::AllTracksRemovedEvent)
    {
        updateUIPlaylist();
        updatePlaylistStats();
    }
    else if (event->type() == MusicPlayerEvent::MetadataChangedEvent)
    {
        MusicPlayerEvent *mpe = (MusicPlayerEvent*)(event);
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
    (void) mdata;
#if 0
    if (!mdata)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    EditMetadataDialog *editDialog = new EditMetadataDialog(mainStack, mdata);

    if (!editDialog->Create())
    {
        delete editDialog;
        return;
    }

    connect(editDialog, SIGNAL(metadataChanged()), this, SLOT(metadataChanged()));

    mainStack->AddScreen(editDialog);
#endif
}

void MusicCommon::metadataChanged(void)
{
}

void MusicCommon::updateTrackInfo(Metadata *mdata)
{
    if (!mdata)
        return;

    MetadataMap metadataMap;
    mdata->toMap(metadataMap);
    SetTextFromMap(metadataMap);

    m_maxTime = mdata->Length() / 1000;
    if (m_coverartImage)
    {
        //FIXME: change this to use the filename
        QImage image = gPlayer->getCurrentMetadata()->getAlbumArt();
        if (!image.isNull())
        {
            MythImage *mimage = GetMythPainter()->GetFormatImage();
            mimage->Assign(image);
            m_coverartImage->SetImage(mimage);
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

void MusicCommon::updateAlbumArtImage(Metadata *mdata)
{
    if (!m_coverartImage || !mdata)
       return;

    QSize img_size = m_coverartImage->GetArea().size();

    QImage albumArt = mdata->getAlbumArt();

    if (!albumArt.isNull())
    {
        MythImage *mimage = GetMythPainter()->GetFormatImage();
        mimage->Assign(albumArt);
        m_coverartImage->SetImage(mimage);
    }
    else
        m_coverartImage->Reset();
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

void MusicCommon::playlistItemSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    // cannot rely on GetTopItemPos() returning sane values hence this hack
    int pos = m_currentPlaylist->GetCurrentPos();
    int start = max(0, (int)(pos - m_currentPlaylist->GetVisibleCount()));
    int end = min(m_currentPlaylist->GetCount(), (int)(pos + m_currentPlaylist->GetVisibleCount()));
    for (int x = start; x < end; x++)
    {
        if (x < 0 || x >= m_currentPlaylist->GetCount())
            continue;


        MythUIButtonListItem *aitem = m_currentPlaylist->GetItemAt(x);
        if (aitem)
        {
            if (aitem->GetImage().isEmpty())
            {
                Metadata *mdata = qVariantValue<Metadata*> (aitem->GetData());
                if (mdata)
                {
                    aitem->SetImage(mdata->getAlbumArtFile());
                }
            }
        }
    }
}

void MusicCommon::updateUIPlaylist(void)
{
    if (!m_currentPlaylist)
        return;

    m_currentPlaylist->Reset();

    Playlist *playlist = gMusicData->all_playlists->getActive();

    QList<Track*> songlist = playlist->getSongs();
    QList<Track*>::iterator it = songlist.begin();
    for (; it != songlist.end(); it++)
    {
        int trackid = (*it)->getValue();
        Metadata *mdata = gMusicData->all_music->getMetadata(trackid);
        if (mdata)
        {
            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_currentPlaylist, "", qVariantFromValue(mdata));

            MetadataMap metadataMap;
            mdata->toMap(metadataMap);
            item->SetTextFromMap(metadataMap);

            item->SetFontState("normal");
            item->DisplayState("stopped", "playstate");
            // TODO rating state etc
        }
    }

    if (m_currentTrack >= 0 && m_currentTrack < m_currentPlaylist->GetCount())
    {
        m_currentPlaylist->SetItemCurrent(m_currentTrack);

        MythUIButtonListItem *item = m_currentPlaylist->GetItemAt(m_currentTrack);
        if (item)
        {
            item->SetFontState("running");
            item->DisplayState("playing", "playstate");
        }
    }
}

void MusicCommon::updatePlaylistStats(void)
{
    int trackCount = gPlayer->getPlaylist()->getSongs().size();

    QHash<QString, QString> map;
    map["playlistposition"] = QString("%1 of %2").arg(m_currentTrack + 1).arg(trackCount);
    map["playlistcurrent"] = QString("%1").arg(m_currentTrack + 1);
    map["playlistcount"] = QString("%1").arg(trackCount);
    map["playlisttime"] = getTimeString((m_playlistPlayedTime / 1000) + m_currentTime, m_playlistMaxTime / 1000);
    map["playlistplayedtime"] = getTimeString((m_playlistPlayedTime / 1000) + m_currentTime, 0);
    map["playlisttotaltime"] = getTimeString(m_playlistMaxTime / 1000, 0);

    if (m_playlistProgressText)
        m_playlistProgressText->SetTextFromMap(map);

    if (m_playlistLengthText)
        m_playlistLengthText->SetTextFromMap(map);

    if (m_playlistProgress)
        m_playlistProgress->SetUsed((m_playlistPlayedTime / 1000) + m_currentTime);
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

void MusicCommon::showViewMenu(void)
{
}

void MusicCommon::showPlaylistMenu(void)
{
}

void MusicCommon::showExitMenu(void)
{
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
    bool err = false;

    err = LoadWindowFromXML("music-ui.xml", "volume_popup", this);

    if (!err)
        return false;

    UIUtilW::Assign(this, m_volText,     "volume_text");
    UIUtilW::Assign(this, m_volProgress, "volume_progress");
    UIUtilW::Assign(this, m_muteState,   "mute_state");

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
        else if (action == "MUTE")
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
    bool err = false;

    err = LoadWindowFromXML("music-ui.xml", "trackdetail_popup", this);

    if (!err)
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
