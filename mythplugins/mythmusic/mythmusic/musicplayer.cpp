// ANSI C includes
#include <cstdlib>

// qt
#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QList>
#include <QDir>

// mythtv
#include <mythcontext.h>
#include <audiooutput.h>
#include <mythdb.h>
#include <mythdialogbox.h>
#include <mythmainwindow.h>
#include <musicutils.h>

// mythmusic
#include "musicdata.h"
#include "musicplayer.h"
#include "decoder.h"
#include "decoderhandler.h"
#include "metaio.h"
#ifdef HAVE_CDIO
#include "cddecoder.h"
#endif
#include "constants.h"
#include "mainvisual.h"
#include "miniplayer.h"
#include "playlistcontainer.h"

// how long to wait before updating the lastplay and playcount fields
#define LASTPLAY_DELAY 15

MusicPlayer  *gPlayer = NULL;
QString gCDdevice = "";

////////////////////////////////////////////////////////////////

QEvent::Type MusicPlayerEvent::TrackChangeEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::VolumeChangeEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::TrackAddedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::TrackRemovedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::TrackUnavailableEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::AllTracksRemovedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::MetadataChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::TrackStatsChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::AlbumArtChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::CDChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::PlaylistChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::PlayedTracksChangedEvent = (QEvent::Type) QEvent::registerEventType();

MusicPlayer::MusicPlayer(QObject *parent)
    :QObject(parent)
{
    setObjectName("MusicPlayer");

    m_output = NULL;
    m_decoderHandler = NULL;
    m_currentTrack = -1;

    m_currentTime = 0;
    m_lastTrackStart = 0;

    m_bufferAvailable = 0;
    m_bufferSize = 0;

    m_oneshotMetadata = NULL;

    m_isAutoplay = false;
    m_isPlaying = false;
    m_playMode = PLAYMODE_TRACKSPLAYLIST;
    m_canShowPlayer = true;
    m_wasPlaying = false;
    m_updatedLastplay = false;
    m_allowRestorePos = true;

    m_playSpeed = 1.0;

    m_showScannerNotifications = true;

    m_errorCount = 0;

    QString playmode = gCoreContext->GetSetting("PlayMode", "none");
    if (playmode.toLower() == "random")
        setShuffleMode(SHUFFLE_RANDOM);
    else if (playmode.toLower() == "intelligent")
        setShuffleMode(SHUFFLE_INTELLIGENT);
    else if (playmode.toLower() == "album")
        setShuffleMode(SHUFFLE_ALBUM);
    else if (playmode.toLower() == "artist")
        setShuffleMode(SHUFFLE_ARTIST);
    else
        setShuffleMode(SHUFFLE_OFF);

    QString repeatmode = gCoreContext->GetSetting("RepeatMode", "all");
    if (repeatmode.toLower() == "track")
        setRepeatMode(REPEAT_TRACK);
    else if (repeatmode.toLower() == "all")
        setRepeatMode(REPEAT_ALL);
    else
        setRepeatMode(REPEAT_OFF);

    loadSettings();

    gCoreContext->addListener(this);
    gCoreContext->RegisterForPlayback(this, SLOT(StopPlayback()));
    connect(gCoreContext, SIGNAL(TVPlaybackStopped()), this, SLOT(StartPlayback()));
    connect(gCoreContext, SIGNAL(TVPlaybackAborted()), this, SLOT(StartPlayback()));
}

MusicPlayer::~MusicPlayer()
{
    if (!hasClient())
        savePosition();

    gCoreContext->removeListener(this);
    gCoreContext->UnregisterForPlayback(this);

    QMap<QString, int>::Iterator i;
    for (i = m_notificationMap.begin(); i != m_notificationMap.end(); i++)
        GetNotificationCenter()->UnRegister(this, (*i));

    m_notificationMap.clear();

    stop(true);

    if (m_decoderHandler)
    {
        m_decoderHandler->removeListener(this);
        m_decoderHandler->deleteLater();
        m_decoderHandler = NULL;
    }

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = NULL;
    }

    while (!m_playedList.empty())
    {
        delete m_playedList.back();
        m_playedList.pop_back();
    }

    if (m_shuffleMode == SHUFFLE_INTELLIGENT)
        gCoreContext->SaveSetting("PlayMode", "intelligent");
    else if (m_shuffleMode == SHUFFLE_RANDOM)
        gCoreContext->SaveSetting("PlayMode", "random");
    else if (m_shuffleMode == SHUFFLE_ALBUM)
        gCoreContext->SaveSetting("PlayMode", "album");
    else if (m_shuffleMode == SHUFFLE_ARTIST)
        gCoreContext->SaveSetting("PlayMode", "artist");
    else
        gCoreContext->SaveSetting("PlayMode", "none");

    if (m_repeatMode == REPEAT_TRACK)
        gCoreContext->SaveSetting("RepeatMode", "track");
    else if (m_repeatMode == REPEAT_ALL)
        gCoreContext->SaveSetting("RepeatMode", "all");
    else
        gCoreContext->SaveSetting("RepeatMode", "none");

    gCoreContext->SaveSetting("MusicAutoShowPlayer",
                          (m_autoShowPlayer ? "1" : "1"));
}

void MusicPlayer::addListener(QObject *listener)
{
    if (listener && m_output)
        m_output->addListener(listener);

    if (listener && getDecoder())
        getDecoder()->addListener(listener);

    if (listener && m_decoderHandler)
        m_decoderHandler->addListener(listener);

    MythObservable::addListener(listener);

    m_isAutoplay = !hasListeners();
}

void MusicPlayer::removeListener(QObject *listener)
{
    if (listener && m_output)
        m_output->removeListener(listener);

    if (listener && getDecoder())
        getDecoder()->removeListener(listener);

    if (listener && m_decoderHandler)
        m_decoderHandler->removeListener(listener);

    MythObservable::removeListener(listener);

    m_isAutoplay = !hasListeners();
}

void MusicPlayer::addVisual(MainVisual *visual)
{
    if (visual && !m_visualisers.contains(visual))
    {
        if (m_output)
        {
            m_output->addListener(visual);
            m_output->addVisual(visual);
        }

        m_visualisers.insert(visual);
    }
}

void MusicPlayer::removeVisual(MainVisual *visual)
{
    if (visual)
    {
        if (m_output)
        {
            m_output->removeListener(visual);
            m_output->removeVisual(visual);
        }

        m_visualisers.remove(visual);
    }
}

MusicPlayer::ResumeMode MusicPlayer::getResumeMode(void)
{
    if (m_playMode == PLAYMODE_RADIO)
        return m_resumeModeRadio;
    else if (m_playMode == PLAYMODE_TRACKSEDITOR)
        return m_resumeModeEditor;
    else
        return m_resumeModePlayback;
}

void MusicPlayer::loadSettings(void)
{
    m_resumeModePlayback = (ResumeMode) gCoreContext->GetNumSetting("ResumeModePlayback", (ResumeMode) MusicPlayer::RESUME_EXACT);
    m_resumeModeEditor = (ResumeMode) gCoreContext->GetNumSetting("ResumeModeEditor", (ResumeMode) MusicPlayer::RESUME_OFF);
    m_resumeModeRadio = (ResumeMode) gCoreContext->GetNumSetting("ResumeModeRadio", (ResumeMode) MusicPlayer::RESUME_TRACK);

    m_lastplayDelay = gCoreContext->GetNumSetting("MusicLastPlayDelay", LASTPLAY_DELAY);
    m_autoShowPlayer = (gCoreContext->GetNumSetting("MusicAutoShowPlayer", 1) > 0);
}

// this stops playing the playlist and plays the file pointed to by mdata
void MusicPlayer::playFile(const MusicMetadata &mdata)
{
    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = NULL;
    }

    m_oneshotMetadata = new MusicMetadata();
    *m_oneshotMetadata = mdata;

    play();
}

void MusicPlayer::stop(bool stopAll)
{
    stopDecoder();

    if (m_output)
    {
        if (m_output->IsPaused())
            pause();
        m_output->Reset();
    }

    m_isPlaying = false;

    if (stopAll && getDecoder())
    {
        getDecoder()->removeListener(this);

        // remove any listeners from the decoder
        {
            QMutexLocker locker(m_lock);
            QSet<QObject*>::const_iterator it = m_listeners.begin();
            for (; it != m_listeners.end() ; ++it)
            {
                getDecoder()->removeListener(*it);
            }
        }
    }

    if (stopAll && m_output)
    {
        m_output->removeListener(this);
        delete m_output;
        m_output = NULL;
    }

    // because we don't actually stop the audio output we have to fake a Stopped
    // event so any listeners can act on it
    OutputEvent oe(OutputEvent::Stopped);
    dispatch(oe);

    gCoreContext->emitTVPlaybackStopped();

    GetMythMainWindow()->PauseIdleTimer(false);
}

void MusicPlayer::pause(void)
{
    if (m_output)
    {
        m_isPlaying = !m_isPlaying;
        m_output->Pause(!m_isPlaying);
    }
    // wake up threads
    if (getDecoder())
    {
        getDecoder()->lock();
        getDecoder()->cond()->wakeAll();
        getDecoder()->unlock();
    }

    GetMythMainWindow()->PauseIdleTimer(false);
}

void MusicPlayer::play(void)
{
    stopDecoder();

    MusicMetadata *meta = getCurrentMetadata();
    if (!meta)
        return;

    if (meta->Filename() == METADATA_INVALID_FILENAME)
    {
        // put an upper limit on the number of consecutive track unavailable errors
        if (m_errorCount >= 1000)
        {
            ShowOkPopup(tr("Got to many track unavailable errors. Maybe the host with the music on is off-line?"));
            stop(true);
            m_errorCount = 0;
            return;
        }

        if (m_errorCount < 5)
        {
            MythErrorNotification n(tr("Track Unavailable"), tr("MythMusic"), QString("Cannot find file '%1'").arg(meta->Filename(false)));
            GetNotificationCenter()->Queue(n);
        }

        m_errorCount++;

        sendTrackUnavailableEvent(meta->ID());
        next();
        return;
    }

    // Notify others that we are about to play
    gCoreContext->WantingPlayback(this);

    if (!m_output)
    {
        if (!openOutputDevice())
            return;
    }

    if (!getDecoderHandler())
        setupDecoderHandler();

    getDecoderHandler()->start(meta);

    GetMythMainWindow()->PauseIdleTimer(true);
}

void MusicPlayer::stopDecoder(void)
{
    if (getDecoderHandler())
        getDecoderHandler()->stop();
}

bool MusicPlayer::openOutputDevice(void)
{
    QString adevice, pdevice;

    if (gCoreContext->GetSetting("MusicAudioDevice") == "default")
        adevice = gCoreContext->GetSetting("AudioOutputDevice");
    else
        adevice = gCoreContext->GetSetting("MusicAudioDevice");

    pdevice = gCoreContext->GetNumSetting("PassThruDeviceOverride", false) ?
              gCoreContext->GetSetting("PassThruOutputDevice") : "auto";

    m_output = AudioOutput::OpenAudio(
                   adevice, pdevice, FORMAT_S16, 2, 0, 44100,
                   AUDIOOUTPUT_MUSIC, true, false,
                   gCoreContext->GetNumSetting("MusicDefaultUpmix", 0) + 1);

    if (!m_output)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MusicPlayer: Cannot open audio output device: %1").arg(adevice));

        return false;
    }

    if (!m_output->GetError().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MusicPlayer: Cannot open audio output device: %1").arg(adevice));
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error was: %1").arg(m_output->GetError()));

        delete m_output;
        m_output = NULL;

        return false;
    }

    m_output->setBufferSize(256 * 1024);

    m_output->addListener(this);

    // add any visuals to the audio output
    QSet<QObject*>::const_iterator it = m_visualisers.begin();

    for (; it != m_visualisers.end() ; ++it)
    {
        m_output->addVisual((MythTV::Visual*)(*it));
    }

    // add any listeners to the audio output
    QMutexLocker locker(m_lock);
    it = m_listeners.begin();
    for (; it != m_listeners.end() ; ++it)
    {
        m_output->addListener(*it);
    }

    return true;
}

void MusicPlayer::next(void)
{
    int currentTrack = m_currentTrack;

    if (!getCurrentPlaylist())
        return;

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = NULL;
    }
    else
        currentTrack++;

    if (currentTrack >= getCurrentPlaylist()->getTrackCount())
    {
        if (m_repeatMode == REPEAT_ALL)
        {
            // start playing again from first track
            currentTrack = 0;
        }
        else
        {
            stop();
            return;
        }
    }

    changeCurrentTrack(currentTrack);

    if (getCurrentMetadata())
        play();
    else
        stop();
}

void MusicPlayer::previous(void)
{
    int currentTrack = m_currentTrack;

    if (!getCurrentPlaylist())
        return;

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = NULL;
    }
    else
        currentTrack--;

    if (currentTrack >= 0)
    {
        changeCurrentTrack(currentTrack);

        if (getCurrentMetadata())
            play();
        else
            return;//stop();
    }
    else
    {
        // FIXME take repeat mode into account
        return; //stop();
    }
}

void MusicPlayer::nextAuto(void)
{
    if (!getCurrentPlaylist())
        return;

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = NULL;
        stop(true);
        return;
    }

    if (m_repeatMode == REPEAT_TRACK)
    {
        play();
        return;
    }
    else
    {
        if (!m_decoderHandler->next())
            next();
    }

    // if we don't already have a gui attached show the miniplayer if configured to do so
    if (m_isAutoplay && m_canShowPlayer && m_autoShowPlayer)
    {
        MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

        MiniPlayer *miniplayer = new MiniPlayer(popupStack);

        if (miniplayer->Create())
            popupStack->AddScreen(miniplayer);
        else
            delete miniplayer;
    }
}

void MusicPlayer::StartPlayback(void)
{
    if (!gCoreContext->InWantingPlayback() && m_wasPlaying)
    {
        play();
        seek(gCoreContext->GetNumSetting("MusicBookmarkPosition", 0));
        gCoreContext->SaveSetting("MusicBookmark", "");
        gCoreContext->SaveSetting("MusicBookmarkPosition", 0);

        m_wasPlaying = false;
    }
}

void MusicPlayer::StopPlayback(void)
{
    if (m_isPlaying)
    {
        m_wasPlaying = m_isPlaying;

        savePosition();
        stop(true);
    }
}

void MusicPlayer::customEvent(QEvent *event)
{
    // handle decoderHandler events
    if (event->type() == DecoderHandlerEvent::Ready)
    {
        m_errorCount = 0;
        decoderHandlerReady();
    }
    else if (event->type() == DecoderHandlerEvent::Meta)
    {
        DecoderHandlerEvent *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;

        MusicMetadata *mdata = new MusicMetadata(*dhe->getMetadata());

        m_lastTrackStart += m_currentTime;

        if (getCurrentMetadata())
        {
            mdata->setID(getCurrentMetadata()->ID());
            mdata->setTrack(m_playedList.count() + 1);
            mdata->setStation(getCurrentMetadata()->Station());
            mdata->setChannel(getCurrentMetadata()->Channel());

            getCurrentMetadata()->setTitle(mdata->Title());
            getCurrentMetadata()->setArtist(mdata->Artist());
            getCurrentMetadata()->setAlbum(mdata->Album());
            getCurrentMetadata()->setTrack(mdata->Track());
        }

        m_playedList.append(mdata);

        if (m_isAutoplay && m_canShowPlayer && m_autoShowPlayer)
        {
            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

            MiniPlayer *miniplayer = new MiniPlayer(popupStack);

            if (miniplayer->Create())
                popupStack->AddScreen(miniplayer);
            else
                delete miniplayer;
        }

        // tell any listeners we've added a new track to the played list
        MusicPlayerEvent me(MusicPlayerEvent::PlayedTracksChangedEvent, mdata->ID());
        dispatch(me);
    }
    // handle MythEvent events
    else if (event->type() == MythEvent::MythEventMessage)
    {
        MythEvent *me = dynamic_cast<MythEvent*>(event);

        if (!me)
            return;

        if (me->Message().left(13) == "MUSIC_COMMAND")
        {
            QStringList list = me->Message().simplified().split(' ');

            if (list.size() >= 3 && list[1] == gCoreContext->GetHostName())
            {
                if (list[2] == "PLAY")
                    play();
                else if (list[2] == "STOP")
                    stop();
                else if (list[2] == "PAUSE")
                    pause();
                else if (list[2] == "SET_VOLUME")
                {
                    if (list.size() >= 3)
                    {
                        int volume = list[3].toInt();
                        if (volume >= 0 && volume <= 100)
                            setVolume(volume);
                    }
                }
                else if (list[2] == "GET_VOLUME")
                {
                    QString message = QString("MUSIC_CONTROL ANSWER %1 %2")
                            .arg(gCoreContext->GetHostName()).arg(getVolume());
                    MythEvent me(message);
                    gCoreContext->dispatch(me);
                }
                else if (list[2] == "PLAY_FILE")
                {
                    int start = me->Message().indexOf("'");
                    int end = me->Message().lastIndexOf("'");

                    if (start != -1 && end != -1 && start != end)
                    {
                        QString filename = me->Message().mid(start + 1, end - start - 1);
                        MusicMetadata mdata;
                        mdata.setFilename(filename);
                        playFile(mdata);
                    }
                    else
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("MusicPlayer: got invalid MUSIC_COMMAND "
                                    "PLAY_FILE - %1").arg(me->Message()));
                }
                else if (list[2] == "PLAY_URL")
                {
                    if (list.size() == 4)
                    {
                        QString filename = list[3];
                        MusicMetadata mdata;
                        mdata.setFilename(filename);
                        playFile(mdata);
                    }
                    else
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("MusicPlayer: got invalid MUSIC_COMMAND "
                                    "PLAY_URL - %1").arg(me->Message()));
                }
                else if (list[2] == "PLAY_TRACK")
                {
                    if (list.size() == 4)
                    {
                        int trackID = list[3].toInt();
                        MusicMetadata *mdata = gMusicData->all_music->getMetadata(trackID);
                        if (mdata)
                            playFile(*mdata);
                    }
                    else
                        LOG(VB_GENERAL, LOG_ERR,
                             QString("MusicPlayer: got invalid MUSIC_COMMAND "
                                     "PLAY_TRACK - %1").arg(me->Message()));
                }
                else if (list[2] == "GET_METADATA")
                {
                    QString mdataStr;
                    MusicMetadata *mdata = getCurrentMetadata();
                    if (mdata)
                        mdataStr = QString("%1 by %2 from %3").arg(mdata->Title()).arg(mdata->Artist()).arg(mdata->Album());
                    else
                        mdataStr = "Unknown Track2";

                    QString message = QString("MUSIC_CONTROL ANSWER %1 %2")
                            .arg(gCoreContext->GetHostName()).arg(mdataStr);
                    MythEvent me(message);
                    gCoreContext->dispatch(me);
                }
            }
            else
                LOG(VB_GENERAL, LOG_ERR,
                    QString("MusicPlayer: got unknown/invalid MUSIC_COMMAND "
                            "- %1").arg(me->Message()));
        }
        else if (me->Message().startsWith("MUSIC_SETTINGS_CHANGED"))
        {
            loadSettings();
        }
        else if (me->Message().startsWith("MUSIC_METADATA_CHANGED"))
        {
            if (gMusicData->initialized)
            {
                QStringList list = me->Message().simplified().split(' ');
                if (list.size() == 2)
                {
                    int songID = list[1].toInt();
                    MusicMetadata *mdata =  gMusicData->all_music->getMetadata(songID);

                    if (mdata)
                    {
                        mdata->reloadMetadata();

                        // tell any listeners the metadata has changed for this track
                        sendMetadataChangedEvent(songID);
                    }
                }
            }
        }
        else if (me->Message().startsWith("MUSIC_SCANNER_STARTED"))
        {
            QStringList list = me->Message().simplified().split(' ');
            if (list.size() == 2)
            {
                QString host = list[1];
                int id = getNotificationID(host);
                sendNotification(id,
                                 tr("A music file scan has started on %1").arg(host),
                                 tr("Music File Scanner"),
                                 tr("This may take a while I'll give a shout when finished"));
            }
        }
        else if (me->Message().startsWith("MUSIC_SCANNER_FINISHED"))
        {
            QStringList list = me->Message().simplified().split(' ');
            if (list.size() == 6)
            {
                QString host = list[1];
                int id = getNotificationID(host);
                int totalTracks = list[2].toInt();
                int newTracks = list[3].toInt();
                int totalCoverart = list[4].toInt();
                int newCoverart = list[5].toInt();

                QString summary = QString("Total Tracks: %2, new tracks: %3,\nTotal Coverart: %4, New CoverArt %5")
                                          .arg(totalTracks).arg(newTracks).arg(totalCoverart).arg(newCoverart);
                sendNotification(id,
                                 tr("A music file scan has finished on %1").arg(host),
                                 tr("Music File Scanner"), summary);

                gMusicData->reloadMusic();
            }
        }
        else if (me->Message().startsWith("MUSIC_SCANNER_ERROR"))
        {
            QStringList list = me->Message().simplified().split(' ');
            if (list.size() == 3)
            {
                QString host = list[1];
                QString error = list[2];
                int id = getNotificationID(host);

                if (error == "Already_Running")
                    sendNotification(id, tr(""),
                                     tr("Music File Scanner"),
                                     tr("Can't run the music file scanner because it is already running on %1").arg(host));
                else if (error == "Stalled")
                    sendNotification(id, tr(""),
                                     tr("Music File Scanner"),
                                     tr("The music file scanner has been running for more than 60 minutes on %1.\nResetting and trying again")
                                         .arg(host));
            }
        }
    }

    if (event->type() == OutputEvent::Error)
    {
        OutputEvent *aoe = dynamic_cast<OutputEvent *>(event);

        if (!aoe)
            return;

        LOG(VB_GENERAL, LOG_ERR, QString("Audio Output Error: %1").arg(*aoe->errorMessage()));

        MythErrorNotification n(tr("Audio Output Error"), tr("MythMusic"), *aoe->errorMessage());
        GetNotificationCenter()->Queue(n);

        m_errorCount++;

        if (m_errorCount <= 5)
            nextAuto();
        else
        {
            m_errorCount = 0;
            stop(true);
        }
    }
    else if (event->type() == DecoderEvent::Error)
    {
        DecoderEvent *dxe = dynamic_cast<DecoderEvent *>(event);

        if (!dxe)
            return;

        LOG(VB_GENERAL, LOG_ERR, QString("Decoder Error: %2").arg(*dxe->errorMessage()));

        MythErrorNotification n(tr("Decoder Error"), tr("MythMusic"), *dxe->errorMessage());
        GetNotificationCenter()->Queue(n);

        m_errorCount++;

        if (m_errorCount <= 5)
            nextAuto();
        else
        {
            m_errorCount = 0;
            stop(true);
        }
    }
    else if (event->type() == DecoderHandlerEvent::Error)
    {
        DecoderHandlerEvent *dhe = dynamic_cast<DecoderHandlerEvent*>(event);

        if (!dhe)
            return;

        LOG(VB_GENERAL, LOG_ERR, QString("Decoder Handler Error - %1").arg(*dhe->getMessage()));

        MythErrorNotification n(tr("Decoder Handler Error"), tr("MythMusic"), *dhe->getMessage());
        GetNotificationCenter()->Queue(n);

        m_errorCount++;

        if (m_errorCount <= 5)
            nextAuto();
        else
        {
            m_errorCount = 0;
            stop(true);
        }
    }
    else if (event->type() == OutputEvent::Info)
    {
        OutputEvent *oe = dynamic_cast<OutputEvent*>(event);

        if (!oe)
            return;

        if (m_playMode != PLAYMODE_RADIO)
            m_currentTime = oe->elapsedSeconds();
        else
            m_currentTime = oe->elapsedSeconds() - m_lastTrackStart;

        if (m_playMode != PLAYMODE_RADIO && !m_updatedLastplay)
        {
            // we update the lastplay and playcount after playing
            // for m_lastplayDelay seconds or half the total track time
            if ((getCurrentMetadata() &&  m_currentTime >
                 (getCurrentMetadata()->Length() / 1000) / 2) ||
                 m_currentTime >= m_lastplayDelay)
            {
                updateLastplay();
            }
        }

        // update the current tracks time in the last played list
        if (m_playMode == PLAYMODE_RADIO)
        {
            if (!m_playedList.isEmpty())
            {
                m_playedList.last()->setLength(m_currentTime * 1000);
                // this will update any track lengths displayed on screen
                gPlayer->sendMetadataChangedEvent(m_playedList.last()->ID());
            }
        }
    }
    else if (event->type() == DecoderEvent::Finished)
    {
        if (m_oneshotMetadata)
        {
            delete m_oneshotMetadata;
            m_oneshotMetadata = NULL;
            stop(true);
        }
        else
        {
            if (m_playMode != PLAYMODE_RADIO && getCurrentMetadata() &&
                m_currentTime != getCurrentMetadata()->Length() / 1000)
            {
                LOG(VB_GENERAL, LOG_NOTICE, QString("MusicPlayer: Updating track length was %1s, should be %2s")
                    .arg(getCurrentMetadata()->Length() / 1000).arg(m_currentTime));

                getCurrentMetadata()->setLength(m_currentTime * 1000);
                getCurrentMetadata()->dumpToDatabase();

                // this will update any track lengths displayed on screen
                gPlayer->sendMetadataChangedEvent(getCurrentMetadata()->ID());

                // this will force the playlist stats to update
                MusicPlayerEvent me(MusicPlayerEvent::TrackChangeEvent, m_currentTrack);
                dispatch(me);
            }

            nextAuto();
        }
    }
    else if (event->type() == DecoderEvent::Stopped)
    {
    }
    else if (event->type() == DecoderHandlerEvent::BufferStatus)
    {
        DecoderHandlerEvent *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;

        dhe->getBufferStatus(&m_bufferAvailable, &m_bufferSize);
    }

    QObject::customEvent(event);
}

void MusicPlayer::getBufferStatus(int *bufferAvailable, int *bufferSize)
{
    *bufferAvailable = m_bufferAvailable;
    *bufferSize = m_bufferSize;
}

void MusicPlayer::setPlayMode(PlayMode mode)
{
    if (m_playMode == mode)
        return;

    savePosition();

    m_playMode = mode;

    loadPlaylist();
}

void MusicPlayer::loadPlaylist(void)
{
    if (m_playMode == PLAYMODE_RADIO)
    {
        if (getResumeMode() > MusicPlayer::RESUME_OFF)
        {
            int bookmark = gCoreContext->GetNumSetting("MusicRadioBookmark", 0);
            if (bookmark < 0 || bookmark >= getCurrentPlaylist()->getTrackCount())
                bookmark = 0;

            m_currentTrack = bookmark;
        }
        else
            m_currentTrack = 0;

        setShuffleMode(SHUFFLE_OFF);
    }
    else
    {
        if (getResumeMode() > MusicPlayer::RESUME_OFF)
        {
            int bookmark = gCoreContext->GetNumSetting("MusicBookmark", 0);
            if (bookmark < 0 || bookmark >= getCurrentPlaylist()->getTrackCount())
                bookmark = 0;

            m_currentTrack = bookmark;
        }
        else
            m_currentTrack = 0;
    }
}

void MusicPlayer::loadStreamPlaylist(void)
{
    // create the radio playlist
    gMusicData->all_playlists->getStreamPlaylist()->disableSaves();
    gMusicData->all_playlists->getStreamPlaylist()->removeAllTracks();
    StreamList *list = gMusicData->all_streams->getStreams();

    for (int x = 0; x < list->count(); x++)
    {
        MusicMetadata *mdata = list->at(x);
        gMusicData->all_playlists->getStreamPlaylist()->addTrack(mdata->ID(), false);
    }

    gMusicData->all_playlists->getStreamPlaylist()->enableSaves();
}

void MusicPlayer::moveTrackUpDown(bool moveUp, int whichTrack)
{
    if (!getCurrentPlaylist())
        return;

    if (moveUp && whichTrack <= 0)
        return;

    if (!moveUp && whichTrack >=  getCurrentPlaylist()->getTrackCount() - 1)
        return;

    MusicMetadata *currTrack = getCurrentPlaylist()->getSongAt(m_currentTrack);

    getCurrentPlaylist()->moveTrackUpDown(moveUp, whichTrack);

    m_currentTrack = getCurrentPlaylist()->getTrackPosition(currTrack->ID());
}

bool MusicPlayer::setCurrentTrackPos(int pos)
{
    changeCurrentTrack(pos);

    if (!getCurrentMetadata())
    {
        stop();
        return false;
    }

    play();

    return true;
}

void MusicPlayer::savePosition(void)
{
    // can't save a bookmark if we don't know what we are playing
    if (!getCurrentMetadata())
        return;

    if (m_playMode == PLAYMODE_RADIO)
    {
        gCoreContext->SaveSetting("MusicRadioBookmark", getCurrentMetadata()->ID());
    }
    else
    {
        gCoreContext->SaveSetting("MusicBookmark", getCurrentMetadata()->ID());
        gCoreContext->SaveSetting("MusicBookmarkPosition", m_currentTime);
    }
}

void MusicPlayer::restorePosition(void)
{
    // if we are switching views we don't wont to restore the position
    if (!m_allowRestorePos)
        return;

    m_currentTrack = 0;
    uint id = -1;

    if (gPlayer->getResumeMode() > MusicPlayer::RESUME_FIRST)
    {
        if (m_playMode == PLAYMODE_RADIO)
            id = gCoreContext->GetNumSetting("MusicRadioBookmark", 0);
        else
            id = gCoreContext->GetNumSetting("MusicBookmark", 0);
    }

    if (getCurrentPlaylist())
    {
        for (int x = 0; x < getCurrentPlaylist()->getTrackCount(); x++)
        {
            if (getCurrentPlaylist()->getSongAt(x)->ID() == id)
            {
                m_currentTrack = x;
                break;
            }
        }
    }

    if (getCurrentMetadata())
    {
        if (gPlayer->getResumeMode() > MusicPlayer::RESUME_OFF)
            play();

        if (gPlayer->getResumeMode() == MusicPlayer::RESUME_EXACT && m_playMode != PLAYMODE_RADIO)
            seek(gCoreContext->GetNumSetting("MusicBookmarkPosition", 0));
    }
}

void MusicPlayer::seek(int pos)
{
    if (m_output)
    {
        if (getDecoder() && getDecoder()->isRunning())
            getDecoder()->seek(pos);

        m_output->SetTimecode(pos*1000);
    }
}

void MusicPlayer::showMiniPlayer(void)
{
    if (m_canShowPlayer)
    {
        MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

        MiniPlayer *miniplayer = new MiniPlayer(popupStack);

        if (miniplayer->Create())
            popupStack->AddScreen(miniplayer);
        else
            delete miniplayer;
    }
}

/// change the current track to the given track
void MusicPlayer::changeCurrentTrack(int trackNo)
{
    if (!getCurrentPlaylist())
        return;

    // check to see if we need to save the current tracks volatile  metadata (playcount, last played etc)
    updateVolatileMetadata();

    m_currentTrack = trackNo;

    // sanity check the current track
    if (m_currentTrack < 0 || m_currentTrack >= getCurrentPlaylist()->getTrackCount())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MusicPlayer: asked to set the current track to an invalid track no. %1")
            .arg(trackNo));
        m_currentTrack = -1;
        return;
    }
}

/// get the metadata for the current track in the playlist
MusicMetadata *MusicPlayer::getCurrentMetadata(void)
{
    if (m_oneshotMetadata)
        return m_oneshotMetadata;

    if (!getCurrentPlaylist() || !getCurrentPlaylist()->getSongAt(m_currentTrack))
        return NULL;

    return getCurrentPlaylist()->getSongAt(m_currentTrack);
}

/// get the metadata for the next track in the playlist
MusicMetadata *MusicPlayer::getNextMetadata(void)
{
    if (m_playMode == PLAYMODE_RADIO)
        return NULL;

    if (m_oneshotMetadata)
        return getCurrentMetadata();

    if (!getCurrentPlaylist() || !getCurrentPlaylist()->getSongAt(m_currentTrack))
        return NULL;

    if (m_repeatMode == REPEAT_TRACK)
        return getCurrentMetadata();

    // if we are not playing the last track then just return the next track
    if (m_currentTrack < getCurrentPlaylist()->getTrackCount() - 1)
        return getCurrentPlaylist()->getSongAt(m_currentTrack + 1);
    else
    {
        // if we are playing the last track then we need to take the
        // repeat mode into account
        if (m_repeatMode == REPEAT_ALL)
            return getCurrentPlaylist()->getSongAt(0);
        else
            return NULL;
    }

    return NULL;
}

MusicPlayer::RepeatMode MusicPlayer::toggleRepeatMode(void)
{
    switch (m_repeatMode)
    {
        case REPEAT_OFF:
            m_repeatMode = REPEAT_TRACK;
            break;
        case REPEAT_TRACK:
            m_repeatMode = REPEAT_ALL;
            break;
        case REPEAT_ALL:
            m_repeatMode = REPEAT_OFF;
           break;
        default:
            m_repeatMode = REPEAT_OFF;
            break;
    }

    return m_repeatMode;
}

MusicPlayer::ShuffleMode MusicPlayer::toggleShuffleMode(void)
{
    switch (m_shuffleMode)
    {
        case SHUFFLE_OFF:
            m_shuffleMode = SHUFFLE_RANDOM;
            break;
        case SHUFFLE_RANDOM:
            m_shuffleMode = SHUFFLE_INTELLIGENT;
            break;
        case SHUFFLE_INTELLIGENT:
            m_shuffleMode = SHUFFLE_ALBUM;
           break;
        case SHUFFLE_ALBUM:
            m_shuffleMode = SHUFFLE_ARTIST;
           break;
        case SHUFFLE_ARTIST:
            m_shuffleMode = SHUFFLE_OFF;
           break;
        default:
            m_shuffleMode = SHUFFLE_OFF;
            break;
    }

    setShuffleMode(m_shuffleMode);

    return m_shuffleMode;
}

void MusicPlayer::setShuffleMode(ShuffleMode mode)
{
    int curTrackID = -1;
    if (getCurrentMetadata())
        curTrackID = getCurrentMetadata()->ID();

    // only save the mode if we are playing tracks
    if (m_playMode != PLAYMODE_RADIO)
        m_shuffleMode = mode;

    if (!getCurrentPlaylist())
        return;

    getCurrentPlaylist()->shuffleTracks(mode);

    if (curTrackID != -1)
    {
        for (int x = 0; x < getCurrentPlaylist()->getTrackCount(); x++)
        {
            MusicMetadata *mdata = getCurrentPlaylist()->getSongAt(x);
            if (mdata && mdata->ID() == (MusicMetadata::IdType) curTrackID)
            {
                m_currentTrack = x;
                break;
            }
        }
    }
}

void MusicPlayer::updateLastplay()
{
    if (m_playMode != PLAYMODE_RADIO && getCurrentMetadata())
    {
        getCurrentMetadata()->incPlayCount();
        getCurrentMetadata()->setLastPlay();
    }

    m_updatedLastplay = true;
}

void MusicPlayer::updateVolatileMetadata(void)
{
    if (m_playMode != PLAYMODE_RADIO && getCurrentMetadata() && getDecoder())
    {
        if (getCurrentMetadata()->hasChanged())
        {
            getCurrentMetadata()->persist();

            // only write the last played, playcount & rating to the tag if it's enabled by the user
            if (GetMythDB()->GetNumSetting("AllowTagWriting", 0) == 1)
            {
                QStringList strList;
                strList << QString("MUSIC_TAG_UPDATE_VOLATILE %1 %2 %3 %4 %5")
                                   .arg(getCurrentMetadata()->Hostname())
                                   .arg(getCurrentMetadata()->ID())
                                   .arg(getCurrentMetadata()->Rating())
                                   .arg(getCurrentMetadata()->Playcount())
                                   .arg(getCurrentMetadata()->LastPlay().toString(Qt::ISODate));
                SendStringListThread *thread = new SendStringListThread(strList);
                MThreadPool::globalInstance()->start(thread, "UpdateVolatile");
            }

            sendTrackStatsChangedEvent(getCurrentMetadata()->ID());
        }
    }
}

void MusicPlayer::setSpeed(float newspeed)
{
    if (m_output)
    {
        m_playSpeed = newspeed;
        m_output->SetStretchFactor(m_playSpeed);
    }
}

void MusicPlayer::incSpeed()
{
    m_playSpeed += 0.05;
    setSpeed(m_playSpeed);
}

void MusicPlayer::decSpeed()
{
    m_playSpeed -= 0.05;
    setSpeed(m_playSpeed);
}

void MusicPlayer::sendVolumeChangedEvent(void)
{
    MusicPlayerEvent me(MusicPlayerEvent::VolumeChangeEvent, getVolume(), isMuted());
    dispatch(me);
}

void MusicPlayer::sendMetadataChangedEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::MetadataChangedEvent, trackID);
    dispatch(me);
}

void MusicPlayer::sendTrackStatsChangedEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::TrackStatsChangedEvent, trackID);
    dispatch(me);
}

void MusicPlayer::sendAlbumArtChangedEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::AlbumArtChangedEvent, trackID);
    dispatch(me);
}

void MusicPlayer::sendTrackUnavailableEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::TrackUnavailableEvent, trackID);
    dispatch(me);
}

void MusicPlayer::sendCDChangedEvent(void)
{
    MusicPlayerEvent me(MusicPlayerEvent::CDChangedEvent, -1);
    dispatch(me);
}

void MusicPlayer::incVolume()
{
    if (getOutput())
    {
        getOutput()->AdjustCurrentVolume(2);
        sendVolumeChangedEvent();
    }
}

void MusicPlayer::decVolume()
{
    if (getOutput())
    {
        getOutput()->AdjustCurrentVolume(-2);
        sendVolumeChangedEvent();
    }
}

void MusicPlayer::setVolume(int volume)
{
    if (getOutput())
    {
        getOutput()->SetCurrentVolume(volume);
        sendVolumeChangedEvent();
    }
}

uint MusicPlayer::getVolume(void) const
{
    if (m_output)
        return m_output->GetCurrentVolume();
    return 0;
}

void MusicPlayer::toggleMute(void)
{
    if (m_output)
    {
        m_output->ToggleMute();
        sendVolumeChangedEvent();
    }
}

MuteState MusicPlayer::getMuteState(void) const
{
    if (m_output)
        return m_output->GetMuteState();
    return kMuteOff;
}

void MusicPlayer::toMap(InfoMap &map)
{
    map["volumemute"] = isMuted() ? tr("%1% (Muted)", "Zero Audio Volume").arg(getVolume()) :
                                    QString("%1%").arg(getVolume());
    map["volume"] = QString("%1").arg(getVolume());
    map["volumepercent"] = QString("%1%").arg(getVolume());
    map["mute"] = isMuted() ? tr("Muted") : "";
}

void MusicPlayer::activePlaylistChanged(int trackID, bool deleted)
{
    if (trackID == -1)
    {
        if (deleted)
        {
            MusicPlayerEvent me(MusicPlayerEvent::AllTracksRemovedEvent, 0);
            dispatch(me);
        }
        else
        {
            MusicPlayerEvent me(MusicPlayerEvent::TrackAddedEvent, trackID);
            dispatch(me);
        }
    }
    else
    {
        if (deleted)
        {
            MusicPlayerEvent me(MusicPlayerEvent::TrackRemovedEvent, trackID);
            dispatch(me);
        }
        else
        {
            MusicPlayerEvent me(MusicPlayerEvent::TrackAddedEvent, trackID);
            dispatch(me);
        }
    }

    // if we don't have any tracks to play stop here
    if (!getCurrentPlaylist() || getCurrentPlaylist()->getTrackCount() == 0)
    {
        m_currentTrack = -1;
        if (isPlaying())
            stop(true);
        return;
    }

    int trackPos = -1;

    // make sure the current playing track is still valid
    if (isPlaying() && getDecoderHandler())
    {
        for (int x = 0; x < getCurrentPlaylist()->getTrackCount(); x++)
        {
            if (getCurrentPlaylist()->getSongAt(x)->ID() == getDecoderHandler()->getMetadata().ID())
            {
                trackPos = x;
                break;
            }
        }
    }

    if (trackPos != m_currentTrack)
        m_currentTrack = trackPos;

    if (!getCurrentMetadata())
    {
        m_currentTrack = -1;
        stop(true);
    }
}

void MusicPlayer::playlistChanged(int playlistID)
{
    MusicPlayerEvent me(MusicPlayerEvent::PlaylistChangedEvent, playlistID);
    dispatch(me);
}

void MusicPlayer::setupDecoderHandler(void)
{
    m_decoderHandler = new DecoderHandler();
    m_decoderHandler->addListener(this);

    // add any listeners to the decoderHandler
    {
        QMutexLocker locker(m_lock);
        QSet<QObject*>::const_iterator it = m_listeners.begin();
        for (; it != m_listeners.end() ; ++it)
        {
            m_decoderHandler->addListener(*it);
        }
    }
}

void MusicPlayer::decoderHandlerReady(void)
{
    if (!getDecoder())
        return;

    LOG(VB_PLAYBACK, LOG_INFO, QString ("decoder handler is ready, decoding %1")
            .arg(getDecoder()->getFilename()));

#ifdef HAVE_CDIO
    CdDecoder *cddecoder = dynamic_cast<CdDecoder*>(getDecoder());
    if (cddecoder)
        cddecoder->setDevice(gCDdevice);
#endif

    // Decoder thread can't be running while being initialized
    if (getDecoder()->isRunning())
    {
        getDecoder()->stop();
        getDecoder()->wait();
    }

    getDecoder()->setOutput(m_output);
    //getDecoder()-> setBlockSize(2 * 1024);
    getDecoder()->addListener(this);

    // add any listeners to the decoder
    {
        QMutexLocker locker(m_lock);
        QSet<QObject*>::const_iterator it = m_listeners.begin();
        for (; it != m_listeners.end() ; ++it)
        {
            getDecoder()->addListener(*it);
        }
    }

    m_currentTime = 0;
    m_lastTrackStart = 0;

    QSet<QObject*>::const_iterator it = m_visualisers.begin();
    for (; it != m_visualisers.end() ; ++it)
    {
        //m_output->addVisual((MythTV::Visual*)(*it));
        //(*it)->setDecoder(getDecoder());
        //m_visual->setOutput(m_output);
    }

    if (getDecoder()->initialize())
    {
        if (m_output)
             m_output->Reset();

        getDecoder()->start();

        if (!m_oneshotMetadata && getResumeMode() == RESUME_EXACT &&
            gCoreContext->GetNumSetting("MusicBookmarkPosition", 0) > 0)
        {
            seek(gCoreContext->GetNumSetting("MusicBookmarkPosition", 0));
            gCoreContext->SaveSetting("MusicBookmarkPosition", 0);
        }

        m_isPlaying = true;
        m_updatedLastplay = false;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_ERR, QString("Cannot initialise decoder for %1")
                .arg(getDecoder()->getFilename()));
        return;
    }

    // tell any listeners we've started playing a new track
    MusicPlayerEvent me(MusicPlayerEvent::TrackChangeEvent, m_currentTrack);
    dispatch(me);
}

void MusicPlayer::removeTrack(int trackID)
{
    MusicMetadata *mdata = gMusicData->all_music->getMetadata(trackID);
    if (mdata)
    {
        int trackPos = getCurrentPlaylist()->getTrackPosition(mdata->ID());
        if (m_currentTrack > 0 && m_currentTrack >= trackPos)
            m_currentTrack--;

        getCurrentPlaylist()->removeTrack(trackID);
    }
}

void MusicPlayer::addTrack(int trackID, bool updateUI)
{
    getCurrentPlaylist()->addTrack(trackID, updateUI);
}

Playlist* MusicPlayer::getCurrentPlaylist ( void )
{
    if (!gMusicData || !gMusicData->all_playlists)
        return NULL;

    if (m_playMode == PLAYMODE_RADIO)
    {
        return gMusicData->all_playlists->getStreamPlaylist();
    }
    else
    {
        return gMusicData->all_playlists->getActive();
    }

    return NULL;
}

StreamList  *MusicPlayer::getStreamList(void) 
{
    return gMusicData->all_streams->getStreams();
}

int MusicPlayer::getNotificationID (const QString& hostname)
{
    if (m_notificationMap.find(hostname) == m_notificationMap.end())
        m_notificationMap.insert(hostname, GetNotificationCenter()->Register(this));

    return m_notificationMap[hostname];
}

void MusicPlayer::sendNotification(int notificationID, const QString &title, const QString &author, const QString &desc)
{
    QString image = "musicscanner.png";
    GetMythUI()->FindThemeFile(image);

    DMAP map;
    map["asar"] = title;
    map["minm"] = author;
    map["asal"] = desc;

    MythImageNotification *n = new MythImageNotification(MythNotification::Info, image, map);

    n->SetId(notificationID);
    n->SetParent(this);
    n->SetDuration(5);
    n->SetFullScreen(false);
    GetNotificationCenter()->Queue(*n);
    delete n;
}
