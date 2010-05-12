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
    m_CDdevice = dev;
    m_decoder = NULL;
    m_input = NULL;
    m_output = NULL;

    m_playlistTree = NULL;
    m_currentNode = NULL;
    m_currentMetadata = NULL;

    m_isAutoplay = false;
    m_isPlaying = false;
    m_canShowPlayer = true;
    m_wasPlaying = true;
    m_updatedLastplay = false;

    m_playSpeed = 1.0;

    QString playmode = gContext->GetSetting("PlayMode", "none");
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

    QString repeatmode = gContext->GetSetting("RepeatMode", "all");
    if (repeatmode.toLower() == "track")
        setRepeatMode(REPEAT_TRACK);
    else if (repeatmode.toLower() == "all")
        setRepeatMode(REPEAT_ALL);
    else
        setRepeatMode(REPEAT_OFF);

    QString resumestring = gContext->GetSetting("ResumeMode", "off");
    if (resumestring.toLower() == "off")
        m_resumeMode = RESUME_OFF;
    else if (resumestring.toLower() == "track")
        m_resumeMode = RESUME_TRACK;
    else
        m_resumeMode = RESUME_EXACT;

    m_lastplayDelay = gContext->GetNumSetting("MusicLastPlayDelay",
                                              LASTPLAY_DELAY);

    m_autoShowPlayer = (gContext->GetNumSetting("MusicAutoShowPlayer", 1) > 0);

    gContext->addListener(this);
}

MusicPlayer::~MusicPlayer()
{
    if (!hasClient())
        savePosition();

    gContext->removeListener(this);

    stop(true);

    if (m_playlistTree)
        delete m_playlistTree;

    if (m_currentMetadata)
    {
        delete m_currentMetadata;
        m_currentMetadata = NULL;
    }

    if (m_shuffleMode == SHUFFLE_INTELLIGENT)
        gContext->SaveSetting("PlayMode", "intelligent");
    else if (m_shuffleMode == SHUFFLE_RANDOM)
        gContext->SaveSetting("PlayMode", "random");
    else if (m_shuffleMode == SHUFFLE_ALBUM)
        gContext->SaveSetting("PlayMode", "album");
    else if (m_shuffleMode == SHUFFLE_ARTIST)
        gContext->SaveSetting("PlayMode", "artist");
    else
        gContext->SaveSetting("PlayMode", "none");

    if (m_repeatMode == REPEAT_TRACK)
        gContext->SaveSetting("RepeatMode", "track");
    else if (m_repeatMode == REPEAT_ALL)
        gContext->SaveSetting("RepeatMode", "all");
    else
        gContext->SaveSetting("RepeatMode", "none");

    gContext->SaveSetting("MusicAutoShowPlayer",
                          (m_autoShowPlayer ? "1" : "0"));
}

void MusicPlayer::addListener(QObject *listener)
{
    if (listener && m_output)
        m_output->addListener(listener);

    if (listener && m_decoder)
        m_decoder->addListener(listener);

    MythObservable::addListener(listener);

    m_isAutoplay = !hasListeners();
}

void MusicPlayer::removeListener(QObject *listener)
{
    if (listener && m_output)
        m_output->removeListener(listener);

    if (listener && m_decoder)
        m_decoder->removeListener(listener);

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
    playFile(meta.Filename());
    m_currentMetadata = new Metadata(meta);
    m_currentNode = NULL;
}

void MusicPlayer::playFile(const QString &filename)
{
    m_currentFile = filename;
    play();
}

void MusicPlayer::stop(bool stopAll)
{
    stopDecoder();

    if (m_output)
    {
        if (m_output->IsPaused())
        {
            pause();
        }
        m_output->Reset();
    }

    if (m_input)
        delete m_input;
    m_input = NULL;

    m_isPlaying = false;

    if (stopAll && m_decoder)
    {
        m_decoder->removeListener(this);
        delete m_decoder;
        m_decoder = NULL;
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
    if (m_decoder)
    {
        m_decoder->lock();
        m_decoder->cond()->wakeAll();
        m_decoder->unlock();
    }
}

void MusicPlayer::play(void)
{
    stopDecoder();

    if (!m_output)
        openOutputDevice();

    if (m_input)
        delete m_input;

    m_input = new QFile(m_currentFile);

    if (m_decoder && !m_decoder->factory()->supports(m_currentFile))
    {
        m_decoder->removeListener(this);
        delete m_decoder;
        m_decoder = NULL;
    }

    if (!m_decoder)
    {
        m_decoder = Decoder::create(m_currentFile, m_input, m_output, true);
        if (!m_decoder)
        {
            VERBOSE(VB_IMPORTANT,
                    "MusicPlayer: Failed to create decoder for playback");
            return;
        }

        if (m_currentFile.contains("cda") == 1)
            dynamic_cast<CdDecoder*>(m_decoder)->setDevice(m_CDdevice);

        m_decoder->setBlockSize(2 * 1024);

        m_decoder->addListener(this);
        // add any listeners to the decoder
        {
            QMutexLocker locker(m_lock);
            QSet<QObject*>::const_iterator it = m_listeners.begin();
            for (; it != m_listeners.end() ; ++it)
            {
                m_decoder->addListener(*it);
            }
        }
    }
    else
    {
        m_decoder->setInput(m_input);
        m_decoder->setFilename(m_currentFile);
        m_decoder->setOutput(m_output);
    }

    if (m_decoder->initialize())
    {
        if (m_output)
            m_output->Reset();

        m_decoder->start();

        m_isPlaying = true;

        if (m_currentNode)
        {
            if (m_currentNode->getInt() > 0)
            {
                m_currentMetadata = Metadata::getMetadataFromID(
                    m_currentNode->getInt());
                m_updatedLastplay = false;
            }
            else
            {
                // CD track
                CdDecoder *cddecoder = dynamic_cast<CdDecoder*>(m_decoder);
                if (cddecoder)
                    m_currentMetadata = cddecoder->getMetadata(
                        -m_currentNode->getInt());
            }
        }

        MusicPlayerEvent me(MusicPlayerEvent::TrackChangeEvent, m_currentNode->getInt());
        dispatch(me);
    }
}

void MusicPlayer::stopDecoder(void)
{
    if (m_decoder && m_decoder->isRunning())
    {
        m_decoder->lock();
        m_decoder->stop();
        m_decoder->unlock();
    }

    if (m_decoder)
    {
        m_decoder->lock();
        m_decoder->cond()->wakeAll();
        m_decoder->unlock();
    }

    if (m_decoder)
        m_decoder->wait();

    if (m_currentMetadata)
    {
        if (m_currentMetadata->hasChanged())
            m_currentMetadata->persist();
        delete m_currentMetadata;
    }
    m_currentMetadata = NULL;
}

void MusicPlayer::openOutputDevice(void)
{
    QString adevice, pdevice;

    if (gContext->GetSetting("MusicAudioDevice") == "default")
        adevice = gContext->GetSetting("AudioOutputDevice");
    else
        adevice = gContext->GetSetting("MusicAudioDevice");

    pdevice = gContext->GetSetting("PassThruOutputDevice");

    // TODO: Error checking that device is opened correctly!
    m_output = AudioOutput::OpenAudio(adevice, pdevice, 16, 2, 0, 44100,
                                      AUDIOOUTPUT_MUSIC, true, false,
                                      gContext->GetNumSetting("MusicDefaultUpmix", 0) + 1);
    m_output->setBufferSize(256 * 1024);
    m_output->SetBlocking(false);

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

    GenericTree *node
        = m_currentNode->nextSibling(1, ((int) m_shuffleMode) + 1);
    if (node)
    {
        m_currentNode = node;
    }
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

    QString filename = getFilenameFromID(node->getInt());
    if (!filename.isEmpty())
        playFile(filename);
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
        QString filename = getFilenameFromID(node->getInt());
        if (!filename.isEmpty())
            playFile(filename);
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
        next();

    if (m_canShowPlayer && m_autoShowPlayer)
    {
        MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

        MiniPlayer *miniplayer = new MiniPlayer(popupStack, this);

        if (miniplayer->Create())
            popupStack->AddScreen(miniplayer);
        else
            delete miniplayer;
    }
}

void MusicPlayer::customEvent(QEvent *event)
{
    if (m_isAutoplay)
    {
        if (event->type() == OutputEvent::Error)
        {
            OutputEvent *aoe = (OutputEvent *) event;

            VERBOSE(VB_IMPORTANT, QString("Output Error - %1")
                    .arg(*aoe->errorMessage()));
            MythPopupBox::showOkPopup(
                gContext->GetMainWindow(),
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
                gContext->GetMainWindow(), "Decoder Error",
                QString("MythMusic has encountered the following error:\n%1")
                .arg(*dxe->errorMessage()));
        }
        else if (event->type() == MythEvent::MythEventMessage)
        {
            MythEvent *me = (MythEvent*) event;
            if (me->Message().left(14) == "PLAYBACK_START")
            {
                m_wasPlaying = m_isPlaying;
                QString hostname = me->Message().mid(15);

                if (hostname == gContext->GetHostName())
                {
                    if (m_isPlaying)
                        savePosition();
                    stop(true);
                }
            }

            if (me->Message().left(12) == "PLAYBACK_END")
            {
                if (m_wasPlaying)
                {
                    QString hostname = me->Message().mid(13);
                    if (hostname == gContext->GetHostName())
                    {
                        play();
                        seek(gContext->GetNumSetting(
                                 "MusicBookmarkPosition", 0));
                        gContext->SaveSetting("MusicBookmark", "");
                        gContext->SaveSetting("MusicBookmarkPosition", 0);
                    }

                    m_wasPlaying = false;
                }
            }
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
        CdDecoder *cddecoder = dynamic_cast<CdDecoder*>(m_decoder);
        if (cddecoder)
        {
            Metadata *meta = cddecoder->getMetadata(-id);
            if (meta)
                filename = meta->Filename();
        }
    }
    return filename;
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

void MusicPlayer::savePosition(void)
{
    if (m_resumeMode != RESUME_OFF)
    {
        gContext->SaveSetting("MusicBookmark", getRouteToCurrent());
        if (m_resumeMode == RESUME_EXACT)
            gContext->SaveSetting("MusicBookmarkPosition", m_currentTime);
    }
}

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
            m_currentFile = getFilenameFromID(m_currentNode->getInt());
            if (!m_currentFile.isEmpty())
                play();
        }
    }
}

void MusicPlayer::seek(int pos)
{
    if (m_output)
    {
        m_output->Reset();
        m_output->SetTimecode(pos*1000);

        if (m_decoder && m_decoder->isRunning())
        {
            m_decoder->lock();
            m_decoder->seek(pos);
            m_decoder->unlock();
        }
    }
}

void MusicPlayer::showMiniPlayer(void)
{
    if (m_canShowPlayer)
    {
        MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

        MiniPlayer *miniplayer = new MiniPlayer(popupStack, this);

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

    m_currentMetadata = Metadata::getMetadataFromID(m_currentNode->getInt());

    return m_currentMetadata;
}

void MusicPlayer::refreshMetadata(void)
{
    if (m_currentMetadata)
    {
        delete m_currentMetadata;
        m_currentMetadata = NULL;
    }

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
    MusicPlayerEvent me(MusicPlayerEvent::VolumeChangeEvent, GetVolume(), IsMuted());
    dispatch(me);
}

void MusicPlayer::sendMetadataChangedEvent(int trackID)
{
    MusicPlayerEvent me(MusicPlayerEvent::MetadataChangedEvent, trackID);
    dispatch(me);
}

uint MusicPlayer::GetVolume(void) const
{
    if (m_output)
        return m_output->GetCurrentVolume();
    return 0;
}

MuteState MusicPlayer::GetMuteState(void) const
{
    if (m_output)
        return m_output->GetMuteState();
    return kMuteAll;
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
