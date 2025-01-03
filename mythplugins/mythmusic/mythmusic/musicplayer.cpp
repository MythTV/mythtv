// ANSI C includes
#include <cstdlib>

// qt
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QList>
#include <QWidget>

// mythtv
#include <libmyth/audio/audiooutput.h>
#include <libmyth/mythcontext.h>
#include <libmythbase/mthreadpool.h>
#include <libmythbase/mythdb.h>
#include <libmythmetadata/metaio.h>
#include <libmythmetadata/musicutils.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuihelper.h>

// mythmusic
#include "config.h"
#include "constants.h"
#include "decoder.h"
#include "decoderhandler.h"
#include "mainvisual.h"
#include "miniplayer.h"
#include "musicdata.h"
#include "musicplayer.h"
#include "playlistcontainer.h"

#ifdef HAVE_CDIO
#include "cddecoder.h"
#endif

MusicPlayer  *gPlayer = nullptr;
QString gCDdevice = "";

////////////////////////////////////////////////////////////////

const QEvent::Type MusicPlayerEvent::kTrackChangeEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kVolumeChangeEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kTrackAddedEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kTrackRemovedEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kTrackUnavailableEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kAllTracksRemovedEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kMetadataChangedEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kTrackStatsChangedEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kAlbumArtChangedEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kCDChangedEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kPlaylistChangedEvent = (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MusicPlayerEvent::kPlayedTracksChangedEvent = (QEvent::Type) QEvent::registerEventType();

MusicPlayer::MusicPlayer(QObject *parent)
    :QObject(parent)
{
    setObjectName("MusicPlayer");

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
    gCoreContext->RegisterForPlayback(this, &MusicPlayer::StopPlayback);
    connect(gCoreContext, &MythCoreContext::TVPlaybackStopped, this, &MusicPlayer::StartPlayback);
    connect(gCoreContext, &MythCoreContext::TVPlaybackAborted, this, &MusicPlayer::StartPlayback);
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
        m_decoderHandler = nullptr;
    }

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = nullptr;
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
                          (m_autoShowPlayer ? "1" : "0"));
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
    if (m_playMode == PLAYMODE_TRACKSEDITOR)
        return m_resumeModeEditor;
    return m_resumeModePlayback;
}

void MusicPlayer::loadSettings(void)
{
    m_resumeModePlayback = (ResumeMode) gCoreContext->GetNumSetting("ResumeModePlayback", MusicPlayer::RESUME_EXACT);
    m_resumeModeEditor = (ResumeMode) gCoreContext->GetNumSetting("ResumeModeEditor", MusicPlayer::RESUME_OFF);
    m_resumeModeRadio = (ResumeMode) gCoreContext->GetNumSetting("ResumeModeRadio", MusicPlayer::RESUME_TRACK);

    m_lastplayDelay = gCoreContext->GetDurSetting<std::chrono::seconds>("MusicLastPlayDelay", LASTPLAY_DELAY);
    m_autoShowPlayer = (gCoreContext->GetNumSetting("MusicAutoShowPlayer", 1) > 0);

}

void MusicPlayer::setPlayNow(bool PlayNow)
{
    gCoreContext->SaveBoolSetting("MusicPreferPlayNow", PlayNow);
}
bool MusicPlayer::getPlayNow(void)
{
    return gCoreContext->GetBoolSetting("MusicPreferPlayNow", false);
}

// this stops playing the playlist and plays the file pointed to by mdata
void MusicPlayer::playFile(const MusicMetadata &mdata)
{
    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = nullptr;
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

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = nullptr;
    }

    m_isPlaying = false;

    if (stopAll && getDecoder())
    {
        getDecoder()->removeListener(this);

        // remove any listeners from the decoder
        {
            QMutexLocker locker(m_lock);
            // NOLINTNEXTLINE(modernize-loop-convert)
            for (auto it = m_listeners.begin(); it != m_listeners.end() ; ++it)
                getDecoder()->removeListener(*it);
        }
    }

    if (stopAll && m_output)
    {
        m_output->removeListener(this);
        delete m_output;
        m_output = nullptr;
    }

    // because we don't actually stop the audio output we have to fake a Stopped
    // event so any listeners can act on it
    OutputEvent oe(OutputEvent::kStopped);
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
    Decoder *decoder = getDecoder();
    if (decoder)
    {
        decoder->lock();
        decoder->cond()->wakeAll();
        decoder->unlock();
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
            ShowOkPopup(tr("Got too many track unavailable errors. Maybe the host with the music on is off-line?"));
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
    QString adevice;
    QString pdevice;

    adevice = gCoreContext->GetSetting("MusicAudioDevice", "default");
    if (adevice == "default" || adevice.isEmpty())
        adevice = gCoreContext->GetSetting("AudioOutputDevice");
    else
        adevice = gCoreContext->GetSetting("MusicAudioDevice");

    pdevice = gCoreContext->GetBoolSetting("PassThruDeviceOverride", false) ?
              gCoreContext->GetSetting("PassThruOutputDevice") : "auto";

    m_output = AudioOutput::OpenAudio(
                   adevice, pdevice, FORMAT_S16, 2, AV_CODEC_ID_NONE, 44100,
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
        m_output = nullptr;

        return false;
    }

    m_output->setBufferSize(256 * 1024);

    m_output->addListener(this);

    // add any visuals to the audio output
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_visualisers.begin(); it != m_visualisers.end() ; ++it)
        m_output->addVisual((MythTV::Visual*)(*it));

    // add any listeners to the audio output
    QMutexLocker locker(m_lock);
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_listeners.begin(); it != m_listeners.end() ; ++it)
        m_output->addListener(*it);

    return true;
}

void MusicPlayer::next(void)
{
    int currentTrack = m_currentTrack;

    Playlist *playlist = getCurrentPlaylist();
    if (nullptr == playlist)
        return;

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = nullptr;
    }
    else
    {
        currentTrack++;
    }

    if (currentTrack >= playlist->getTrackCount())
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

    Playlist *playlist = getCurrentPlaylist();
    if (nullptr == playlist)
        return;

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = nullptr;
    }
    else
    {
        currentTrack--;
    }

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
    Playlist *playlist = getCurrentPlaylist();
    if (nullptr == playlist)
        return;

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = nullptr;
        stop(true);
        return;
    }

    if (m_repeatMode == REPEAT_TRACK)
    {
        play();
        return;
    }
    if (!m_decoderHandler->next())
        next();

    // if we don't already have a gui attached show the miniplayer if configured to do so
    if (m_isAutoplay && m_canShowPlayer && m_autoShowPlayer && m_isPlaying)
    {
        MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

        auto *miniplayer = new MiniPlayer(popupStack);

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

        m_wasPlaying = false;
    }
}

void MusicPlayer::StopPlayback(void)
{
    if (m_output)
    {
        m_wasPlaying = m_isPlaying;

        savePosition();
        stop(true);
    }
}

void MusicPlayer::customEvent(QEvent *event)
{
    // handle decoderHandler events
    if (event->type() == DecoderHandlerEvent::kReady)
    {
        m_errorCount = 0;
        decoderHandlerReady();
    }
    else if (event->type() == DecoderHandlerEvent::kMeta)
    {
        auto *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;

        auto *mdata = new MusicMetadata(*dhe->getMetadata());

        m_lastTrackStart += m_currentTime;

        if (getCurrentMetadata())
        {
            mdata->setID(getCurrentMetadata()->ID());
            mdata->setHostname(gCoreContext->GetMasterHostName());
            mdata->setTrack(m_playedList.count() + 1);
            mdata->setBroadcaster(getCurrentMetadata()->Broadcaster());
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

            auto *miniplayer = new MiniPlayer(popupStack);

            if (miniplayer->Create())
                popupStack->AddScreen(miniplayer);
            else
                delete miniplayer;
        }

        // tell any listeners we've added a new track to the played list
        MusicPlayerEvent me(MusicPlayerEvent::kPlayedTracksChangedEvent, mdata->ID());
        dispatch(me);
    }
    // handle MythEvent events
    else if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent*>(event);
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
                    if (list.size() > 3)
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
                    MythEvent me2(message);
                    gCoreContext->dispatch(me2);
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
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("MusicPlayer: got invalid MUSIC_COMMAND "
                                    "PLAY_FILE - %1").arg(me->Message()));
                    }
                }
                else if (list[2] == "PLAY_URL")
                {
                    if (list.size() == 4)
                    {
                        const QString& filename = list[3];
                        MusicMetadata mdata;
                        mdata.setFilename(filename);
                        playFile(mdata);
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("MusicPlayer: got invalid MUSIC_COMMAND "
                                    "PLAY_URL - %1").arg(me->Message()));
                    }
                }
                else if (list[2] == "PLAY_TRACK")
                {
                    if (list.size() == 4)
                    {
                        int trackID = list[3].toInt();
                        MusicMetadata *mdata = gMusicData->m_all_music->getMetadata(trackID);
                        if (mdata)
                            playFile(*mdata);
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                             QString("MusicPlayer: got invalid MUSIC_COMMAND "
                                     "PLAY_TRACK - %1").arg(me->Message()));
                    }
                }
                else if (list[2] == "GET_METADATA")
                {
                    QString mdataStr;
                    MusicMetadata *mdata = getCurrentMetadata();
                    if (mdata)
                        mdataStr = QString("%1 by %2 from %3").arg(mdata->Title(), mdata->Artist(), mdata->Album());
                    else
                        mdataStr = "Unknown Track2";

                    QString message = QString("MUSIC_CONTROL ANSWER %1 %2")
                            .arg(gCoreContext->GetHostName(), mdataStr);
                    MythEvent me2(message);
                    gCoreContext->dispatch(me2);
                }
                else if (list[2] == "GET_STATUS")
                {
                    QString statusStr = "STOPPED";

                    if (gPlayer->isPlaying())
                        statusStr = "PLAYING";
                    else if (gPlayer->isPaused())
                        statusStr = "PAUSED";

                    QString message = QString("MUSIC_CONTROL ANSWER %1 %2")
                            .arg(gCoreContext->GetHostName(), statusStr);
                    MythEvent me2(message);
                    gCoreContext->dispatch(me2);
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("MusicPlayer: got unknown/invalid MUSIC_COMMAND "
                            "- %1").arg(me->Message()));
            }
        }
        else if (me->Message().startsWith("MUSIC_SETTINGS_CHANGED"))
        {
            loadSettings();
        }
        else if (me->Message().startsWith("MUSIC_METADATA_CHANGED"))
        {
            if (gMusicData->m_initialized)
            {
                QStringList list = me->Message().simplified().split(' ');
                if (list.size() == 2)
                {
                    int songID = list[1].toInt();
                    MusicMetadata *mdata =  gMusicData->m_all_music->getMetadata(songID);

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
                const QString& host = list[1];
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
                const QString& host = list[1];
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
                const QString& host = list[1];
                const QString& error = list[2];
                int id = getNotificationID(host);

                if (error == "Already_Running")
                {
                    sendNotification(id, tr(""),
                                     tr("Music File Scanner"),
                                     tr("Can't run the music file scanner because it is already running on %1").arg(host));
                }
                else if (error == "Stalled")
                {
                    sendNotification(id, tr(""),
                                     tr("Music File Scanner"),
                                     tr("The music file scanner has been running for more than 60 minutes on %1.\nResetting and trying again")
                                         .arg(host));
                }
            }
        }
    }

    if (event->type() == OutputEvent::kError)
    {
        auto *aoe = dynamic_cast<OutputEvent *>(event);

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
    else if (event->type() == DecoderEvent::kError)
    {
        auto *dxe = dynamic_cast<DecoderEvent *>(event);

        if (!dxe)
            return;

        LOG(VB_GENERAL, LOG_ERR, QString("Decoder Error: %2").arg(*dxe->errorMessage()));

        MythErrorNotification n(tr("Decoder Error"), tr("MythMusic"), *dxe->errorMessage());
        GetNotificationCenter()->Queue(n);

        m_errorCount++;

        if (m_playMode != PLAYMODE_RADIO && m_errorCount <= 5)
            nextAuto();
        else
        {
            m_errorCount = 0;
            stop(true);
        }
    }
    else if (event->type() == DecoderHandlerEvent::kError)
    {
        auto *dhe = dynamic_cast<DecoderHandlerEvent*>(event);

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
    else if (event->type() == OutputEvent::kInfo)
    {
        auto *oe = dynamic_cast<OutputEvent*>(event);

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
            if ((getCurrentMetadata() &&
                 m_currentTime > duration_cast<std::chrono::seconds>(getCurrentMetadata()->Length()) / 2) ||
                 m_currentTime >= m_lastplayDelay)
            {
                updateLastplay();
            }
        }

        // update the current tracks time in the last played list
        if (m_playMode == PLAYMODE_RADIO)
        {
            if (!m_playedList.isEmpty() && m_currentTime > 0s)
            {
                m_playedList.last()->setLength(m_currentTime);
                // this will update any track lengths displayed on screen
                gPlayer->sendMetadataChangedEvent(m_playedList.last()->ID());
            }
        }
    }
    else if (event->type() == DecoderEvent::kFinished)
    {
        if (m_oneshotMetadata)
        {
            delete m_oneshotMetadata;
            m_oneshotMetadata = nullptr;
            stop(true);
        }
        else
        {
            auto metadataSecs = duration_cast<std::chrono::seconds>(getCurrentMetadata()->Length());
            if (m_playMode != PLAYMODE_RADIO && getCurrentMetadata() &&
                m_currentTime != metadataSecs)
            {
                LOG(VB_GENERAL, LOG_NOTICE, QString("MusicPlayer: Updating track length was %1s, should be %2s")
                    .arg(metadataSecs.count()).arg(m_currentTime.count()));

                getCurrentMetadata()->setLength(m_currentTime);
                getCurrentMetadata()->dumpToDatabase();

                // this will update any track lengths displayed on screen
                gPlayer->sendMetadataChangedEvent(getCurrentMetadata()->ID());

                // this will force the playlist stats to update
                MusicPlayerEvent me(MusicPlayerEvent::kTrackChangeEvent, m_currentTrack);
                dispatch(me);
            }

            nextAuto();
        }
    }
    else if (event->type() == DecoderEvent::kStopped)
    {
    }
    else if (event->type() == DecoderHandlerEvent::kBufferStatus)
    {
        auto *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;

        dhe->getBufferStatus(&m_bufferAvailable, &m_bufferSize);
    }

    QObject::customEvent(event);
}

void MusicPlayer::getBufferStatus(int *bufferAvailable, int *bufferSize) const
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
            Playlist *playlist = getCurrentPlaylist();
            if ((bookmark < 0) ||
                (playlist && bookmark >= playlist->getTrackCount()))
                bookmark = 0;

            m_currentTrack = bookmark;
        }
        else
        {
            m_currentTrack = 0;
        }

        setShuffleMode(SHUFFLE_OFF);
    }
    else
    {
        if (getResumeMode() > MusicPlayer::RESUME_OFF)
        {
            int bookmark = gCoreContext->GetNumSetting("MusicBookmark", 0);
            Playlist *playlist = getCurrentPlaylist();
            if ((bookmark < 0) ||
                (playlist && bookmark >= getCurrentPlaylist()->getTrackCount()))
                bookmark = 0;

            m_currentTrack = bookmark;
        }
        else
        {
            m_currentTrack = 0;
        }
    }
}

void MusicPlayer::loadStreamPlaylist(void)
{
    MusicMetadata::IdType id = getCurrentMetadata() ? getCurrentMetadata()->ID() : -1;

    // create the radio playlist
    gMusicData->m_all_playlists->getStreamPlaylist()->disableSaves();
    gMusicData->m_all_playlists->getStreamPlaylist()->removeAllTracks();
    StreamList *list = gMusicData->m_all_streams->getStreams();

    for (int x = 0; x < list->count(); x++)
    {
        MusicMetadata *mdata = list->at(x);
        gMusicData->m_all_playlists->getStreamPlaylist()->addTrack(mdata->ID(), false);

        if (mdata->ID() == id)
            m_currentTrack = x;
    }

    gMusicData->m_all_playlists->getStreamPlaylist()->enableSaves();
}

void MusicPlayer::moveTrackUpDown(bool moveUp, int whichTrack)
{
    Playlist *playlist = getCurrentPlaylist();
    if (nullptr == playlist)
        return;

    if (moveUp && whichTrack <= 0)
        return;

    if (!moveUp && whichTrack >=  getCurrentPlaylist()->getTrackCount() - 1)
        return;

    MusicMetadata *currTrack = playlist->getSongAt(m_currentTrack);

    playlist->moveTrackUpDown(moveUp, whichTrack);

    m_currentTrack = playlist->getTrackPosition(currTrack->ID());
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
        gCoreContext->SaveDurSetting("MusicBookmarkPosition", m_currentTime);
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

    Playlist *playlist = gPlayer->getCurrentPlaylist();
    if (playlist)
    {
        for (int x = 0; x < playlist->getTrackCount(); x++)
        {
            if (playlist->getSongAt(x) &&
                playlist->getSongAt(x)->ID() == id)
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
            seek(gCoreContext->GetDurSetting<std::chrono::seconds>("MusicBookmarkPosition", 0s));
    }
}

void MusicPlayer::seek(std::chrono::seconds pos)
{
    if (m_output)
    {
        Decoder *decoder = getDecoder();
        if (decoder && decoder->isRunning())
            decoder->seek(pos.count());

        m_output->SetTimecode(pos);
    }
}

void MusicPlayer::showMiniPlayer(void) const
{
    if (m_canShowPlayer)
    {
        MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

        auto *miniplayer = new MiniPlayer(popupStack);

        if (miniplayer->Create())
            popupStack->AddScreen(miniplayer);
        else
            delete miniplayer;
    }
}

/// change the current track to the given track
void MusicPlayer::changeCurrentTrack(int trackNo)
{
    Playlist *playlist = getCurrentPlaylist();
    if (nullptr == playlist)
        return;

    // check to see if we need to save the current tracks volatile  metadata (playcount, last played etc)
    updateVolatileMetadata();

    m_currentTrack = trackNo;

    // sanity check the current track
    if (m_currentTrack < 0 || m_currentTrack >= playlist->getTrackCount())
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

    Playlist *playlist = getCurrentPlaylist();
    if (!playlist || !playlist->getSongAt(m_currentTrack))
        return nullptr;

    return playlist->getSongAt(m_currentTrack);
}

/// get the metadata for the next track in the playlist
MusicMetadata *MusicPlayer::getNextMetadata(void)
{
    if (m_playMode == PLAYMODE_RADIO)
        return nullptr;

    if (m_oneshotMetadata)
        return getCurrentMetadata();

    Playlist *playlist = gPlayer->getCurrentPlaylist();
    if (!playlist || !playlist->getSongAt(m_currentTrack))
        return nullptr;

    if (m_repeatMode == REPEAT_TRACK)
        return getCurrentMetadata();

    // if we are not playing the last track then just return the next track
    if (m_currentTrack < playlist->getTrackCount() - 1)
        return playlist->getSongAt(m_currentTrack + 1);

    // if we are playing the last track then we need to take the
    // repeat mode into account
    if (m_repeatMode == REPEAT_ALL)
        return playlist->getSongAt(0);
    return nullptr;
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

    Playlist *playlist = getCurrentPlaylist();
    if (nullptr == playlist)
        return;

    playlist->shuffleTracks(mode);

    if (curTrackID != -1)
    {
        for (int x = 0; x < playlist->getTrackCount(); x++)
        {
            MusicMetadata *mdata = playlist->getSongAt(x);
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
                strList << QString("MUSIC_TAG_UPDATE_VOLATILE")
                        << getCurrentMetadata()->Hostname()
                        << QString::number(getCurrentMetadata()->ID())
                        << QString::number(getCurrentMetadata()->Rating())
                        << QString::number(getCurrentMetadata()->Playcount())
                        << getCurrentMetadata()->LastPlay().toString(Qt::ISODate);
                auto *thread = new SendStringListThread(strList);
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
    m_playSpeed += 0.05F;
    setSpeed(m_playSpeed);
}

void MusicPlayer::decSpeed()
{
    m_playSpeed -= 0.05F;
    setSpeed(m_playSpeed);
}

void MusicPlayer::sendVolumeChangedEvent(void)
{
    MusicPlayerEvent me(MusicPlayerEvent::kVolumeChangeEvent, getVolume(), isMuted());
    dispatch(me);
}

void MusicPlayer::sendMetadataChangedEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::kMetadataChangedEvent, trackID);
    dispatch(me);
}

void MusicPlayer::sendTrackStatsChangedEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::kTrackStatsChangedEvent, trackID);
    dispatch(me);
}

void MusicPlayer::sendAlbumArtChangedEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::kAlbumArtChangedEvent, trackID);
    dispatch(me);
}

void MusicPlayer::sendTrackUnavailableEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::kTrackUnavailableEvent, trackID);
    dispatch(me);
}

void MusicPlayer::sendCDChangedEvent(void)
{
    MusicPlayerEvent me(MusicPlayerEvent::kCDChangedEvent, -1);
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

void MusicPlayer::toMap(InfoMap &map) const
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
            MusicPlayerEvent me(MusicPlayerEvent::kAllTracksRemovedEvent, 0);
            dispatch(me);
        }
        else
        {
            MusicPlayerEvent me(MusicPlayerEvent::kTrackAddedEvent, trackID);
            dispatch(me);
        }
    }
    else
    {
        if (deleted)
        {
            MusicPlayerEvent me(MusicPlayerEvent::kTrackRemovedEvent, trackID);
            dispatch(me);
        }
        else
        {
            MusicPlayerEvent me(MusicPlayerEvent::kTrackAddedEvent, trackID);
            dispatch(me);
        }
    }

    // if we don't have any tracks to play stop here
    Playlist *playlist = getCurrentPlaylist();
    if (!playlist || playlist->getTrackCount() == 0)
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
        for (int x = 0; x < playlist->getTrackCount(); x++)
        {
            if (playlist->getSongAt(x)->ID() == getDecoderHandler()->getMetadata().ID())
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
    MusicPlayerEvent me(MusicPlayerEvent::kPlaylistChangedEvent, playlistID);
    dispatch(me);
}

void MusicPlayer::setupDecoderHandler(void)
{
    m_decoderHandler = new DecoderHandler();
    m_decoderHandler->addListener(this);

    // add any listeners to the decoderHandler
    {
        QMutexLocker locker(m_lock);
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = m_listeners.begin(); it != m_listeners.end() ; ++it)
            m_decoderHandler->addListener(*it);
    }
}

void MusicPlayer::decoderHandlerReady(void)
{
    Decoder *decoder = getDecoder();
    if (!decoder)
        return;

    LOG(VB_PLAYBACK, LOG_INFO, QString ("decoder handler is ready, decoding %1")
            .arg(decoder->getURL()));

#ifdef HAVE_CDIO
    auto *cddecoder = dynamic_cast<CdDecoder*>(decoder);
    if (cddecoder)
        cddecoder->setDevice(gCDdevice);
#endif

    // Decoder thread can't be running while being initialized
    if (decoder->isRunning())
    {
        decoder->stop();
        decoder->wait();
    }

    decoder->setOutput(m_output);
    //decoder-> setBlockSize(2 * 1024);
    decoder->addListener(this);

    // add any listeners to the decoder
    {
        QMutexLocker locker(m_lock);
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = m_listeners.begin(); it != m_listeners.end() ; ++it)
            decoder->addListener(*it);
    }

    m_currentTime = 0s;
    m_lastTrackStart = 0s;

    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_visualisers.begin(); it != m_visualisers.end() ; ++it)
    {
        //m_output->addVisual((MythTV::Visual*)(*it));
        //(*it)->setDecoder(decoder);
        //m_visual->setOutput(m_output);
    }

    if (decoder->initialize())
    {
        if (m_output)
             m_output->PauseUntilBuffered();

        decoder->start();

        if (!m_oneshotMetadata && getResumeMode() == RESUME_EXACT &&
            gCoreContext->GetDurSetting<std::chrono::seconds>("MusicBookmarkPosition", 0s) > 0s)
        {
            seek(gCoreContext->GetDurSetting<std::chrono::seconds>("MusicBookmarkPosition", 0s));
            gCoreContext->SaveDurSetting("MusicBookmarkPosition", 0s);
        }

        m_isPlaying = true;
        m_updatedLastplay = false;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_ERR, QString("Cannot initialise decoder for %1")
                .arg(decoder->getURL()));
        return;
    }

    // tell any listeners we've started playing a new track
    MusicPlayerEvent me(MusicPlayerEvent::kTrackChangeEvent, m_currentTrack);
    dispatch(me);
}

void MusicPlayer::removeTrack(int trackID)
{
    MusicMetadata *mdata = gMusicData->m_all_music->getMetadata(trackID);
    if (mdata)
    {
        Playlist *playlist = getCurrentPlaylist();
        if (nullptr == playlist)
            return;
        int trackPos = playlist->getTrackPosition(mdata->ID());
        if (m_currentTrack > 0 && m_currentTrack >= trackPos)
            m_currentTrack--;

        playlist->removeTrack(trackID);
    }
}

void MusicPlayer::addTrack(int trackID, bool updateUI)
{
    getCurrentPlaylist()->addTrack(trackID, updateUI);
}

Playlist* MusicPlayer::getCurrentPlaylist ( void )
{
    if (!gMusicData || !gMusicData->m_all_playlists)
        return nullptr;

    if (m_playMode == PLAYMODE_RADIO)
    {
        return gMusicData->m_all_playlists->getStreamPlaylist();
    }
    return gMusicData->m_all_playlists->getActive();
}

StreamList  *MusicPlayer::getStreamList(void) 
{
    return gMusicData->m_all_streams->getStreams();
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
    if (!GetMythUI()->FindThemeFile(image))
        LOG(VB_GENERAL, LOG_ERR, "MusicPlayer: sendNotification failed to find the 'musicscanner.png' image");

    DMAP map;
    map["asar"] = title;
    map["minm"] = author;
    map["asal"] = desc;

    auto *n = new MythImageNotification(MythNotification::kInfo, image, map);

    n->SetId(notificationID);
    n->SetParent(this);
    n->SetDuration(5s);
    n->SetFullScreen(false);
    GetNotificationCenter()->Queue(*n);
    delete n;
}
