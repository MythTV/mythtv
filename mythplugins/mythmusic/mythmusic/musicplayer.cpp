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

// mythmusic
#include "musicplayer.h"
#include "decoder.h"
#include "decoderhandler.h"
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

////////////////////////////////////////////////////////////////

QEvent::Type MusicPlayerEvent::TrackChangeEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::VolumeChangeEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::TrackAddedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::TrackRemovedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::AllTracksRemovedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::MetadataChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::TrackStatsChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::AlbumArtChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::CDChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::PlaylistChangedEvent = (QEvent::Type) QEvent::registerEventType();
QEvent::Type MusicPlayerEvent::PlayedTracksChangedEvent = (QEvent::Type) QEvent::registerEventType();

MusicPlayer::MusicPlayer(QObject *parent, const QString &dev)
    :QObject(parent)
{
    setObjectName("MusicPlayer");

    m_CDdevice = dev;
    m_output = NULL;
    m_decoderHandler = NULL;
    m_cdWatcher = NULL;
    m_currentPlaylist = NULL;
    m_currentTrack = -1;

    m_currentTime = 0;
    m_lastTrackStart = 0;

    m_bufferAvailable = 0;
    m_bufferSize = 0;

    m_currentMetadata = NULL;
    m_oneshotMetadata = NULL;

    m_isAutoplay = false;
    m_isPlaying = false;
    m_playMode = PLAYMODE_TRACKS;
    m_canShowPlayer = true;
    m_wasPlaying = true;
    m_updatedLastplay = false;
    m_allowRestorePos = true;

    m_playSpeed = 1.0;

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
}

MusicPlayer::~MusicPlayer()
{
    if (m_cdWatcher)
    {
        m_cdWatcher->stop();
        m_cdWatcher->wait();
        delete m_cdWatcher;
    }

    if (!hasClient())
        savePosition();

    gCoreContext->removeListener(this);

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

void MusicPlayer::loadSettings(void )
{
    QString resumestring = gCoreContext->GetSetting("ResumeMode", "off");
    if (resumestring.toLower() == "off")
        m_resumeMode = RESUME_OFF;
    else if (resumestring.toLower() == "track")
        m_resumeMode = RESUME_TRACK;
    else
        m_resumeMode = RESUME_EXACT;

    m_lastplayDelay = gCoreContext->GetNumSetting("MusicLastPlayDelay", LASTPLAY_DELAY);

    m_autoShowPlayer = (gCoreContext->GetNumSetting("MusicAutoShowPlayer", 1) > 0);

    //  Do we check the CD?
    bool checkCD = gCoreContext->GetNumSetting("AutoLookupCD");
    if (checkCD)
    {
        m_cdWatcher = new CDWatcherThread(m_CDdevice);
        // don't start the cd watcher here
        // since the playlists haven't been loaded yet
    }
}

// this stops playing the playlist and plays the file pointed to by mdata
void MusicPlayer::playFile(const Metadata &mdata)
{
    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = NULL;
    }

    m_oneshotMetadata = new Metadata();
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
    Metadata *meta = getCurrentMetadata();
    if (!meta)
        return;

    stopDecoder();


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

    if (!m_currentPlaylist)
        return;

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = NULL;
    }
    else
        currentTrack++;

    if (currentTrack >= m_currentPlaylist->getSongs().size())
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

    if (m_currentMetadata)
        play();
    else
        stop();
}

void MusicPlayer::previous(void)
{
    int currentTrack = m_currentTrack;

    if (!m_currentPlaylist)
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

        if (m_currentMetadata)
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
    if (!m_currentPlaylist)
        return;

    if (m_oneshotMetadata)
    {
        delete m_oneshotMetadata;
        m_oneshotMetadata = NULL;
        play();
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

void MusicPlayer::customEvent(QEvent *event)
{
    // handle decoderHandler events
    if (event->type() == DecoderHandlerEvent::Ready)
    {
        decoderHandlerReady();
    }
    else if (event->type() == DecoderHandlerEvent::Meta)
    {
        DecoderHandlerEvent *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
        if (!dhe)
            return;

        Metadata *mdata = new Metadata(*dhe->getMetadata());

        m_lastTrackStart += m_currentTime;

        mdata->setID(m_currentMetadata->ID());
        mdata->setTrack(m_playedList.count() + 1);

        m_playedList.append(mdata);

        m_currentMetadata = m_playedList.last();

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

        if (me->Message().left(14) == "PLAYBACK_START")
        {
            m_wasPlaying = m_isPlaying;
            QString hostname = me->Message().mid(15);

            if (hostname == gCoreContext->GetHostName())
            {
                if (m_isPlaying)
                    savePosition();
                stop(true);
            }
        }
        else if (me->Message().left(12) == "PLAYBACK_END")
        {
            if (m_wasPlaying)
            {
                QString hostname = me->Message().mid(13);
                if (hostname == gCoreContext->GetHostName())
                {
                    play();
                    seek(gCoreContext->GetNumSetting(
                                "MusicBookmarkPosition", 0));
                    gCoreContext->SaveSetting("MusicBookmark", "");
                    gCoreContext->SaveSetting("MusicBookmarkPosition", 0);
                }

                m_wasPlaying = false;
            }
        }
        else if (me->Message().left(13) == "MUSIC_COMMAND")
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
                        Metadata mdata;
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
                        Metadata mdata;
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
                        Metadata *mdata = gMusicData->all_music->getMetadata(trackID);
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
                    Metadata *mdata = getCurrentMetadata();
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
            QString startdir = gCoreContext->GetSetting("MusicLocation");
            startdir = QDir::cleanPath(startdir);
            if (!startdir.isEmpty() && !startdir.endsWith("/"))
                startdir += "/";

            gMusicData->musicDir = startdir;

            loadSettings();
        }
    }

    if (m_isAutoplay)
    {
        if (event->type() == OutputEvent::Error)
        {
            OutputEvent *aoe = dynamic_cast<OutputEvent*>(event);

            if (!aoe)
                return;

            LOG(VB_GENERAL, LOG_ERR, QString("Output Error - %1")
                    .arg(*aoe->errorMessage()));

            ShowOkPopup(QString("MythMusic has encountered the following error:\n%1")
                    .arg(*aoe->errorMessage()));
            stop(true);
        }
        else if (event->type() == DecoderEvent::Error)
        {
            stop(true);

            QApplication::sendPostedEvents();

            DecoderEvent *dxe = dynamic_cast<DecoderEvent*>(event);

            if (!dxe)
                return;

            LOG(VB_GENERAL, LOG_ERR, QString("Decoder Error - %1")
                    .arg(*dxe->errorMessage()));
            ShowOkPopup(QString("MythMusic has encountered the following error:\n%1")
                    .arg(*dxe->errorMessage()));
        }
        else if (event->type() == DecoderHandlerEvent::Error)
        {
            DecoderHandlerEvent *dhe = dynamic_cast<DecoderHandlerEvent*>(event);
            if (!dhe)
                return;

            LOG(VB_GENERAL, LOG_ERR, QString("Output Error - %1")
                    .arg(*dhe->getMessage()));

            ShowOkPopup(QString("MythMusic has encountered the following error:\n%1")
                    .arg(*dhe->getMessage()));
            stop(true);
        }
    }

    if (event->type() == OutputEvent::Info)
    {
        OutputEvent *oe = dynamic_cast<OutputEvent*>(event);

        if (!oe)
            return;

        if (m_playMode == PLAYMODE_TRACKS)
            m_currentTime = oe->elapsedSeconds();
        else
            m_currentTime = oe->elapsedSeconds() - m_lastTrackStart;

        if (m_playMode == PLAYMODE_TRACKS && !m_updatedLastplay)
        {
            // we update the lastplay and playcount after playing
            // for m_lastplayDelay seconds or half the total track time
            if ((m_currentMetadata &&  m_currentTime >
                 (m_currentMetadata->Length() / 1000) / 2) ||
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
        if (m_playMode == PLAYMODE_TRACKS && m_currentMetadata &&
            m_currentTime != m_currentMetadata->Length() / 1000)
        {
            LOG(VB_GENERAL, LOG_NOTICE, QString("MusicPlayer: Updating track length was %1s, should be %2s")
                .arg(m_currentMetadata->Length() / 1000).arg(m_currentTime));

            m_currentMetadata->setLength(m_currentTime * 1000);
            m_currentMetadata->dumpToDatabase();

            // this will update any track lengths displayed on screen
            gPlayer->sendMetadataChangedEvent(m_currentMetadata->ID());

            // this will force the playlist stats to update
            MusicPlayerEvent me(MusicPlayerEvent::TrackChangeEvent, m_currentTrack);
            dispatch(me);
        }

        nextAuto();
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
        m_currentPlaylist  = gMusicData->all_playlists->getStreamPlaylist();

        if (getResumeMode() > MusicPlayer::RESUME_OFF)
        {
            int bookmark = gCoreContext->GetNumSetting("MusicRadioBookmark", 0);
            if (bookmark < 0 || bookmark >= m_currentPlaylist->getSongs().size())
                bookmark = 0;

            m_currentTrack = bookmark;
        }
        else
            m_currentTrack = 0;

        setShuffleMode(SHUFFLE_OFF);
    }
    else
    {
        m_currentPlaylist  = gMusicData->all_playlists->getActive();

        if (getResumeMode() > MusicPlayer::RESUME_OFF)
        {
            int bookmark = gCoreContext->GetNumSetting("MusicBookmark", 0);
            if (bookmark < 0 || bookmark >= m_currentPlaylist->getSongs().size())
                bookmark = 0;

            m_currentTrack = bookmark;
        }
        else
            m_currentTrack = 0;
    }

    m_currentMetadata = NULL;

    // now we have the playlist loaded we can start the cd watcher
    if (m_cdWatcher)
        m_cdWatcher->start();
}

void MusicPlayer::moveTrackUpDown(bool moveUp, int whichTrack)
{
    if (moveUp && whichTrack <= 0)
        return;

    if (!moveUp && whichTrack >=  m_currentPlaylist->getSongs().size() - 1)
        return;

    Metadata *currTrack = m_currentPlaylist->getSongs().at(m_currentTrack);

    m_currentPlaylist->moveTrackUpDown(moveUp, whichTrack);

    m_currentTrack = m_currentPlaylist->getSongs().indexOf(currTrack);
}

bool MusicPlayer::setCurrentTrackPos(int pos)
{
    changeCurrentTrack(pos);

    if (!m_currentMetadata)
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
    if (!m_currentMetadata)
        return;

    if (m_playMode == PLAYMODE_RADIO)
    {
        gCoreContext->SaveSetting("MusicRadioBookmark", m_currentMetadata->ID());
    }
    else
    {
        gCoreContext->SaveSetting("MusicBookmark", m_currentMetadata->ID());
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

    if (gPlayer->getResumeMode() > MusicPlayer::RESUME_OFF)
    {
        if (m_playMode == PLAYMODE_RADIO)
            id = gCoreContext->GetNumSetting("MusicRadioBookmark", 0);
        else
            id = gCoreContext->GetNumSetting("MusicBookmark", 0);
    }

    for (int x = 0; x < m_currentPlaylist->getSongs().size(); x++)
    {
        if (m_currentPlaylist->getSongs().at(x)->ID() == id)
        {
            m_currentTrack = x;
            break;
        }
    }

    m_currentMetadata = m_currentPlaylist->getSongAt(m_currentTrack);

    if (m_currentMetadata)
    {
        play();

        if (gPlayer->getResumeMode() == MusicPlayer::RESUME_EXACT && m_playMode == PLAYMODE_TRACKS)
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
    if (!m_currentPlaylist)
        return;

    // check to see if we need to save the current tracks volatile  metadata (playcount, last played etc)
    updateVolatileMetadata();

    m_currentTrack = trackNo;

    // sanity check the current track
    if (m_currentTrack < 0 || m_currentTrack >= m_currentPlaylist->getSongs().size())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MusicPlayer: asked to set the current track to an invalid track no. %1")
            .arg(trackNo));
        m_currentTrack = -1;
        m_currentMetadata = NULL;
        return;
    }

    m_currentMetadata = m_currentPlaylist->getSongAt(m_currentTrack);
}

/// get the metadata for the current track in the playlist
Metadata *MusicPlayer::getCurrentMetadata(void)
{
    if (m_oneshotMetadata)
        return m_oneshotMetadata;

    if (m_currentMetadata)
        return m_currentMetadata;

    if (!m_currentPlaylist || !m_currentPlaylist->getSongAt(m_currentTrack))
        return NULL;

    m_currentMetadata = m_currentPlaylist->getSongAt(m_currentTrack);

    return m_currentMetadata;
}

/// get the metadata for the next track in the playlist
Metadata *MusicPlayer::getNextMetadata(void)
{
    if (m_playMode == PLAYMODE_RADIO)
        return NULL;

    if (m_oneshotMetadata)
        return m_currentMetadata;

    if (!m_currentPlaylist || !m_currentPlaylist->getSongAt(m_currentTrack))
        return NULL;

    if (m_repeatMode == REPEAT_TRACK)
        return getCurrentMetadata();

    // if we are not playing the last track then just return the next track
    if (m_currentTrack < m_currentPlaylist->getSongs().size() - 1)
        return m_currentPlaylist->getSongAt(m_currentTrack + 1);
    else
    {
        // if we are playing the last track then we need to take the
        // repeat mode into account
        if (m_repeatMode == REPEAT_ALL)
            return m_currentPlaylist->getSongAt(0);
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

    m_shuffleMode = mode;

    if (m_currentPlaylist)
        m_currentPlaylist->shuffleTracks(m_shuffleMode);

    if (curTrackID != -1)
    {
        for (int x = 0; x < getPlaylist()->getSongs().size(); x++)
        {
            Metadata *mdata = getPlaylist()->getSongs().at(x);
            if (mdata && mdata->ID() == (Metadata::IdType) curTrackID)
            {
                m_currentTrack = x;
                break;
            }
        }
    }
}

void MusicPlayer::updateLastplay()
{
    if (m_playMode == PLAYMODE_TRACKS && m_currentMetadata)
    {
        m_currentMetadata->incPlayCount();
        m_currentMetadata->setLastPlay();
    }

    m_updatedLastplay = true;
}

void MusicPlayer::updateVolatileMetadata(void)
{
    if (m_playMode == PLAYMODE_TRACKS && m_currentMetadata && getDecoder())
    {
        if (m_currentMetadata->hasChanged())
        {
            m_currentMetadata->persist();
            if (getDecoder())
                getDecoder()->commitVolatileMetadata(m_currentMetadata);

            sendTrackStatsChangedEvent(m_currentMetadata->ID());
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

void MusicPlayer::toMap(QHash<QString, QString> &map)
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
            // all tracks were removed
            m_currentTrack = -1;
            m_currentMetadata = NULL;
            stop(true);
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
    LOG(VB_PLAYBACK, LOG_INFO, QString ("decoder handler is ready, decoding %1")
            .arg(getDecoder()->getFilename()));

#ifdef HAVE_CDIO
    CdDecoder *cddecoder = dynamic_cast<CdDecoder*>(getDecoder());
    if (cddecoder)
        cddecoder->setDevice(m_CDdevice);
#endif

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

        if (m_resumeMode == RESUME_EXACT &&
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
    Metadata *mdata = gMusicData->all_music->getMetadata(trackID);
    if (mdata)
    {
        int trackPos = gPlayer->getPlaylist()->getSongs().indexOf(mdata);
        if (m_currentTrack > 0 && m_currentTrack >= trackPos)
            m_currentTrack--;

        getPlaylist()->removeTrack(trackID);
    }
}

void MusicPlayer::addTrack(int trackID, bool updateUI)
{
    getPlaylist()->addTrack(trackID, updateUI);
}

///////////////////////////////////////////////////////////////////////////////

CDWatcherThread::CDWatcherThread(const QString &dev)
{
    m_cdDevice = dev;
    m_cdStatusChanged = false;
    m_stopped = false;
}

void CDWatcherThread::run()
{
#ifdef HAVE_CDIO
    while (!m_stopped)
    {
        // lock all_music and cd_status_changed while running thread
        QMutexLocker locker(getLock());

        m_cdStatusChanged = false;

        CdDecoder *decoder = new CdDecoder("cda", NULL, NULL, NULL);
        decoder->setDevice(m_cdDevice);
        int numTracks = decoder->getNumCDAudioTracks();
        bool redo = false;

        if (numTracks != gMusicData->all_music->getCDTrackCount())
        {
            m_cdStatusChanged = true;
            LOG(VB_GENERAL, LOG_NOTICE, QString("CD status has changed."));
        }

        if (numTracks == 0)
        {
            // No CD, or no recognizable CD
            gMusicData->all_music->clearCDData();
            gMusicData->all_playlists->clearCDList();
        }
        else if (numTracks > 0)
        {
            // Check the last track to see if it's changed
            Metadata *checker = decoder->getLastMetadata();
            if (checker)
            {
                if (!gMusicData->all_music->checkCDTrack(checker))
                {
                    redo = true;
                    m_cdStatusChanged = true;
                    gMusicData->all_music->clearCDData();
                    gMusicData->all_playlists->clearCDList();
                }
                else
                    m_cdStatusChanged = false;
                delete checker;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "The cddecoder said it had audio tracks, "
                                         "but it won't tell me about them");
            }
        }

        int tracks = decoder->getNumTracks();
        bool setTitle = false;

        for (int actual_tracknum = 1;
            redo && actual_tracknum <= tracks; actual_tracknum++)
        {
            Metadata *track = decoder->getMetadata(actual_tracknum);
            if (track)
            {
                gMusicData->all_music->addCDTrack(*track);

                if (!setTitle)
                {

                    QString parenttitle = " ";
                    if (track->FormatArtist().length() > 0)
                    {
                        parenttitle += track->FormatArtist();
                        parenttitle += " ~ ";
                    }

                    if (track->Album().length() > 0)
                        parenttitle += track->Album();
                    else
                    {
                        parenttitle = " " + QObject::tr("Unknown");
                        LOG(VB_GENERAL, LOG_INFO, "Couldn't find your "
                        " CD. It may not be in the freedb database.\n"
                        "    More likely, however, is that you need to delete\n"
                        "    ~/.cddb and ~/.cdserverrc and restart MythMusic.");
                    }
                    gMusicData->all_music->setCDTitle(parenttitle);
                    setTitle = true;
                }
                delete track;
            }
        }

        delete decoder;

        if (m_cdStatusChanged)
            gPlayer->sendCDChangedEvent();

        usleep(1000000);
    }
#endif // HAVE_CDIO
}
