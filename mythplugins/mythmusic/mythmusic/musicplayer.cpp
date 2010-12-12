// ANSI C includes
#include <cstdlib>

// qt
#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QList>

// mythtv
#include <mythcontext.h>
#include <audiooutput.h>
#include <mythdb.h>

// mythmusic
#include "musicplayer.h"
#include "decoder.h"
#include "decoderhandler.h"
#include "cddecoder.h"
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

MusicPlayer::MusicPlayer(QObject *parent, const QString &dev)
    :QObject(parent)
{
    setObjectName("MusicPlayer");

    m_CDdevice = dev;
    m_output = NULL;
    m_decoderHandler = NULL;

    m_playlistTree = NULL;
    m_currentNode = NULL;
    m_currentMetadata = NULL;

    m_isAutoplay = false;
    m_isPlaying = false;
    m_canShowPlayer = true;
    m_wasPlaying = true;
    m_updatedLastplay = false;

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

    QString resumestring = gCoreContext->GetSetting("ResumeMode", "off");
    if (resumestring.toLower() == "off")
        m_resumeMode = RESUME_OFF;
    else if (resumestring.toLower() == "track")
        m_resumeMode = RESUME_TRACK;
    else
        m_resumeMode = RESUME_EXACT;

    m_lastplayDelay = gCoreContext->GetNumSetting("MusicLastPlayDelay",
                                              LASTPLAY_DELAY);

    m_autoShowPlayer = (gCoreContext->GetNumSetting("MusicAutoShowPlayer", 1) > 0);

    gCoreContext->addListener(this);
}

MusicPlayer::~MusicPlayer()
{
    if (!hasClient())
        savePosition();

    gCoreContext->removeListener(this);

    stop(true);

    if (m_playlistTree)
        delete m_playlistTree;

    if (m_decoderHandler)
    {
        m_decoderHandler->removeListener(this);
        m_decoderHandler->deleteLater();
        m_decoderHandler = NULL;
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
    if (visual)
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

void MusicPlayer::playFile(const Metadata &meta)
{
    m_currentMetadata = new Metadata(meta);
    play();
    m_currentNode = NULL;
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
}

void MusicPlayer::play(void)
{
    Metadata *meta = getCurrentMetadata();
    if (!meta)
        return;

    stopDecoder();


    if (!m_output)
        openOutputDevice();

    if (!getDecoderHandler())
        setupDecoderHandler();

    getDecoderHandler()->start(meta);
}

void MusicPlayer::stopDecoder(void)
{
    if (m_currentMetadata)
    {
        if (m_currentMetadata->hasChanged())
        {
            m_currentMetadata->persist();
            if (getDecoder())
                getDecoder()->commitVolatileMetadata(m_currentMetadata);
        }
    }

    if (getDecoderHandler())
        getDecoderHandler()->stop();

    m_currentMetadata = NULL;
}

void MusicPlayer::openOutputDevice(void)
{
    QString adevice, pdevice;

    if (gCoreContext->GetSetting("MusicAudioDevice") == "default")
        adevice = gCoreContext->GetSetting("AudioOutputDevice");
    else
        adevice = gCoreContext->GetSetting("MusicAudioDevice");

    pdevice = gCoreContext->GetNumSetting("AdvancedAudioSettings",
                                          false) &&
                gCoreContext->GetNumSetting("PassThruDeviceOverride",
                                            false) ?
              gCoreContext->GetSetting("PassThruOutputDevice") : QString::null;

    // TODO: Error checking that device is opened correctly!
    m_output = AudioOutput::OpenAudio(
                   adevice, pdevice, FORMAT_S16, 2, 0, 44100,
                   AUDIOOUTPUT_MUSIC, true, false,
                   gCoreContext->GetNumSetting("MusicDefaultUpmix", 0) + 1);

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
}

void MusicPlayer::next(void)
{
    if (!m_currentNode)
        return;

    GenericTree *node =
        m_currentNode->nextSibling(1, ((int) m_shuffleMode) + 1);

    if (node)
        m_currentNode = node;
    else
    {
        if (m_repeatMode == REPEAT_ALL)
        {
            // start playing again from first track
            GenericTree *parent = m_currentNode->getParent();
            if (parent)
            {
                node = parent->getChildAt(0, ((int) m_shuffleMode) + 1);
                if (node)
                    m_currentNode = node;
                else
                    return; // stop()
            }
            else
                return; // stop()
        }
        else
            return; // stop()
    }

    m_currentMetadata = gMusicData->all_music->getMetadata(node->getInt());
    if (m_currentMetadata)
        play();
    else
        stop();
}

void MusicPlayer::previous(void)
{
    if (!m_currentNode)
        return;

    GenericTree *node
        = m_currentNode->prevSibling(1, ((int) m_shuffleMode) + 1);
    if (node)
    {
        m_currentNode = node;
        m_currentMetadata = gMusicData->all_music->getMetadata(node->getInt());
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
    if (!m_isAutoplay)
        return;

    if (!m_currentNode)
        return;

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

    if (m_canShowPlayer && m_autoShowPlayer)
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
    if (event->type() == DecoderHandlerEvent::Ready)
    {
        decoderHandlerReady();
    }
    else if (event->type() == DecoderEvent::Decoding)
    {
        if (getCurrentMetadata())
            m_displayMetadata = *getCurrentMetadata();
    }
    else if (event->type() == DecoderHandlerEvent::Info)
    {
        DecoderHandlerEvent *dxe = (DecoderHandlerEvent*)event;
        if (getCurrentMetadata())
            m_displayMetadata = *getCurrentMetadata();
        m_displayMetadata.setArtist("");
        m_displayMetadata.setTitle(*dxe->getMessage());
    }
    else if (event->type() == DecoderHandlerEvent::Meta)
    {
        DecoderHandlerEvent *dxe = (DecoderHandlerEvent*)event;
        m_displayMetadata = *dxe->getMetadata();
    }
    else if (event->type() == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent*) event;
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
            QStringList list = me->Message().split(' ');

            if (list.size() >= 2)
            {
                if (list[1] == "PLAY")
                    play();
                else if (list[1] == "STOP")
                    stop();
                else if (list[1] == "PAUSE")
                    pause();
                else if (list[1] == "SET_VOLUME")
                {
                    if (list.size() >= 3)
                    {
                        int volume = list[2].toInt();
                        if (volume >= 0 && volume <= 100) 
                            setVolume(volume);
                    }
                }
                else if (list[1] == "GET_VOLUME")
                {
                    QString message = QString("MUSIC_CONTROL ANSWER %1").arg(getVolume());
                    MythEvent me(message);
                    gCoreContext->dispatch(me);
                }
                else if (list[1] == "PLAY_FILE")
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
                        VERBOSE(VB_IMPORTANT, QString("MusicPlayer: got invalid MUSIC_COMMAND PLAY_FILE - %1").arg(me->Message()));
                }
                else if (list[1] == "PLAY_URL")
                {
                    if (list.size() == 3)
                    {
                        QString filename = list[2];
                        Metadata mdata;
                        mdata.setFilename(filename);
                        playFile(mdata);
                    }
                    else
                        VERBOSE(VB_IMPORTANT, QString("MusicPlayer: got invalid MUSIC_COMMAND PLAY_URL - %1").arg(me->Message()));
                }
                else if (list[1] == "PLAY_TRACK")
                {
                    if (list.size() == 3)
                    {
                        int trackID = list[2].toInt();
                        Metadata *mdata = gMusicData->all_music->getMetadata(trackID);
                        if (mdata)
                            playFile(*mdata);
                    }
                    else
                        VERBOSE(VB_IMPORTANT, QString("MusicPlayer: got invalid MUSIC_COMMAND PLAY_TRACK - %1").arg(me->Message()));
                }
                else if (list[1] == "GET_METADATA")
                {
                    QString mdataStr;
                    Metadata *mdata = getCurrentMetadata();
                    if (mdata)
                        mdataStr = QString("%1 by %2 from %3").arg(mdata->Title()).arg(mdata->Artist()).arg(mdata->Album());
                    else
                        mdataStr = "Unknown Track";

                    QString message = QString("MUSIC_CONTROL ANSWER %1").arg(mdataStr);
                    MythEvent me(message);
                    gCoreContext->dispatch(me);
                }
            }
            else
                VERBOSE(VB_IMPORTANT, QString("MusicPlayer: got unknown/invalid MUSIC_COMMAND - %1").arg(me->Message()));
        }
    }

    if (m_isAutoplay)
    {
        if (event->type() == OutputEvent::Error)
        {
            OutputEvent *aoe = (OutputEvent *) event;

            VERBOSE(VB_IMPORTANT, QString("Output Error - %1")
                    .arg(*aoe->errorMessage()));
            MythPopupBox::showOkPopup(
                GetMythMainWindow(),
                "Output Error:",
                QString("MythMusic has encountered"
                        " the following error:\n%1")
                .arg(*aoe->errorMessage()));
            stop(true);
        }
        else if (event->type() == DecoderEvent::Finished)
        {
            nextAuto();
        }
        else if (event->type() == DecoderEvent::Error)
        {
            stop(true);

            QApplication::sendPostedEvents();

            DecoderEvent *dxe = (DecoderEvent *) event;

            VERBOSE(VB_IMPORTANT, QString("Decoder Error - %1")
                    .arg(*dxe->errorMessage()));
            MythPopupBox::showOkPopup(
                GetMythMainWindow(), "Decoder Error",
                QString("MythMusic has encountered the following error:\n%1")
                .arg(*dxe->errorMessage()));
        }
    }

    if (event->type() == OutputEvent::Info)
    {
        OutputEvent *oe = (OutputEvent *) event;
        m_currentTime = oe->elapsedSeconds();

        if (!m_updatedLastplay)
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
    }

    QObject::customEvent(event);
}

QString MusicPlayer::getFilenameFromID(int id)
{
    QString filename;

    if (id > 0)
    {
        QString aquery = "SELECT CONCAT_WS('/', "
            "music_directories.path, music_songs.filename) AS filename "
            "FROM music_songs "
            "LEFT JOIN music_directories"
            " ON music_songs.directory_id=music_directories.directory_id "
            "WHERE music_songs.song_id = :ID";

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(aquery);
        query.bindValue(":ID", id);
        if (!query.exec() || query.size() < 1)
            MythDB::DBError("get filename", query);

        if (query.isActive() && query.size() > 0)
        {
            query.first();
            filename = query.value(0).toString();
            if (!filename.contains("://"))
                filename = Metadata::GetStartdir() + filename;
        }
    }
    else
    {
        // cd track
        CdDecoder *cddecoder = dynamic_cast<CdDecoder*>(getDecoder());
        if (cddecoder)
        {
            Metadata *meta = cddecoder->getMetadata(-id);
            if (meta)
                filename = meta->Filename();
        }
    }
    return filename;
}

void MusicPlayer::loadPlaylist(void)
{
    // wait for loading to complete
    while (!gMusicData->all_playlists->doneLoading() || !gMusicData->all_music->doneLoading())
    {
        usleep(500);
    }

    m_currentPlaylist  = gMusicData->all_playlists->getActive();
    setCurrentTrackPos(0);
}

void MusicPlayer::setCurrentNode(GenericTree *node)
{
    m_currentNode = node;
    refreshMetadata();
}

GenericTree *MusicPlayer::constructPlaylist(void)
{
    QString position;

    if (m_playlistTree)
    {
        position = getRouteToCurrent();
        delete m_playlistTree;
    }

    m_playlistTree = new GenericTree(tr("playlist root"), 0);
    m_playlistTree->setAttribute(0, 0);
    m_playlistTree->setAttribute(1, 0);
    m_playlistTree->setAttribute(2, 0);
    m_playlistTree->setAttribute(3, 0);
    m_playlistTree->setAttribute(4, 0);

    GenericTree *active_playlist_node =
            gMusicData->all_playlists->writeTree(m_playlistTree);

    if (!position.isEmpty()) //|| m_currentNode == NULL)
        restorePosition(position);

    m_currentPlaylist  = gMusicData->all_playlists->getActive();

    return active_playlist_node;
}

QString MusicPlayer::getRouteToCurrent(void)
{
    QStringList route;

    if (m_currentNode)
    {
        GenericTree *climber = m_currentNode;

        route.push_front(QString::number(climber->getInt()));
        while((climber = climber->getParent()))
        {
            route.push_front(QString::number(climber->getInt()));
        }
    }
    return route.join(",");
}

void MusicPlayer::setCurrentTrackPos(int pos)
{
    if (pos < 0 || pos >= m_currentPlaylist->getSongs().size())
        return;

    m_currentTrack = pos;

    m_currentMetadata = gMusicData->all_music->getMetadata(m_currentPlaylist->getSongAt(m_currentTrack)->getValue());
    play();
}

void MusicPlayer::savePosition(void)
{
    if (m_resumeMode != RESUME_OFF)
    {
        gCoreContext->SaveSetting("MusicBookmark", getRouteToCurrent());
        if (m_resumeMode == RESUME_EXACT)
            gCoreContext->SaveSetting("MusicBookmarkPosition", m_currentTime);
    }
}

#if 0
void MusicPlayer::savePosition(void)
{
    if (m_resumeMode != RESUME_OFF)
    {
        gContext->SaveSetting("MusicBookmark", m_currentTrack);
        if (m_resumeMode == RESUME_EXACT)
        gContext->SaveSetting("MusicBookmarkPosition", m_currentTime);
    }
}
#endif

void MusicPlayer::restorePosition(const QString &position)
{
    QList<int> branches_to_current_node;

    if (!position.isEmpty())
    {
        QStringList list = position.split(",", QString::SkipEmptyParts);

        for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
            branches_to_current_node.append((*it).toInt());

        //try to restore the position
        m_currentNode = m_playlistTree->findNode(branches_to_current_node);
        if (m_currentNode)
            return;
    }

    // failed to find the position so go to the first track in the playlist
    branches_to_current_node.clear();
    branches_to_current_node.append(0);
    branches_to_current_node.append(1);
    branches_to_current_node.append(0);
    m_currentNode = m_playlistTree->findNode(branches_to_current_node);
    if (m_currentNode)
    {
        m_currentNode = m_currentNode->getChildAt(0, -1);
        if (m_currentNode)
        {
            m_currentMetadata = gMusicData->all_music->getMetadata(m_currentNode->getInt());
            play();
        }
    }
}

void MusicPlayer::restorePosition(int position)
{
    if (position < 0 || position >= m_currentPlaylist->getSongs().size())
    {
        // cannot find the track so move to first track
        m_currentTrack = 0;
    }
    else
    {
        m_currentTrack = position;
    }

    Track *track = m_currentPlaylist->getSongAt(m_currentTrack);

    if (track)
        m_currentMetadata = gMusicData->all_music->getMetadata(track->getValue());

    play();
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

Metadata *MusicPlayer::getCurrentMetadata(void)
{
    if (m_currentMetadata)
        return m_currentMetadata;

    if (!m_currentNode)
        return NULL;

    m_currentMetadata = gMusicData->all_music->getMetadata(m_currentNode->getInt());

    return m_currentMetadata;
}

void MusicPlayer::refreshMetadata(void)
{
    if (m_currentMetadata)
        m_currentMetadata = NULL;

    getCurrentMetadata();
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

    return m_shuffleMode;
}

void MusicPlayer::updateLastplay()
{
    // FIXME this is ugly having to keep two metadata objects in sync
    if (m_currentNode && m_currentNode->getInt() > 0)
    {
        if (m_currentMetadata)
        {
            m_currentMetadata->incPlayCount();
            m_currentMetadata->setLastPlay();
            sendMetadataChangedEvent(m_currentMetadata->ID());
        }
        // if all_music is still in scope we need to keep that in sync
        if (gMusicData->all_music)
        {
            Metadata *mdata
                = gMusicData->all_music->getMetadata(m_currentNode->getInt());
            if (mdata)
            {
                mdata->incPlayCount();
                mdata->setLastPlay();
            }
        }
    }

    m_updatedLastplay = true;
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
    return kMuteAll;
}

void MusicPlayer::toMap(QHash<QString, QString> &map)
{
    map["volumemute"] = QString("%1%").arg(getVolume()) +
                        (isMuted() ? " (" + tr("Muted") + ")" : "");
    map["volume"] = QString("%1").arg(getVolume());
    map["volumepercent"] = QString("%1%").arg(getVolume());
    map["mute"] = isMuted() ? tr("Muted") : "";
}

void MusicPlayer::playlistChanged(int trackID, bool deleted)
{
    if (trackID == -1)
    {
        // all tracks were removed
        stop();
        MusicPlayerEvent me(MusicPlayerEvent::AllTracksRemovedEvent, 0);
        dispatch(me);
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

void MusicPlayer::setupDecoderHandler()
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
    VERBOSE(VB_PLAYBACK, QString ("decoder handler is ready, decoding %1").
            arg(getDecoder()->getFilename()));

    if (getDecoder()->getFilename().contains("cda") == 1)
        dynamic_cast<CdDecoder*>(getDecoder())->setDevice(m_CDdevice);

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
        VERBOSE(VB_PLAYBACK, QString("Cannot initialise decoder for %1")
                .arg(getDecoder()->getFilename()));
        return;
    }

    // tell any listeners we've started playing a new track
    MusicPlayerEvent me(MusicPlayerEvent::TrackChangeEvent, m_currentNode ? m_currentNode->getInt() : -1);
    dispatch(me);
}
