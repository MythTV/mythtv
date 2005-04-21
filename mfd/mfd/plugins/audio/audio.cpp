/*
	audio.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	

*/


#include "../../../config.h"
#include <qapplication.h>
#include <qfileinfo.h>

#include "mfd_events.h"
#include "../../servicelister.h"

#include "audio.h"
#include "daapinput.h"
#include "constants.h"
#include "audiolistener.h"
#include "rtspout.h"

AudioPlugin::AudioPlugin(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 2343, "audio")
{
    input = NULL;
    output = NULL;
    decoder = NULL;
    output_buffer_size = 256;
    elapsed_time = 0;
    is_playing = false;
    is_paused = false;
    state_of_play_mutex = new QMutex(true);    
    metadata_server = parent->getMetadataServer();
    stopPlaylistMode();
    audio_listener = new AudioListener(this);
#ifdef MFD_RTSP_SUPPORT
    //
    //  Create a NULL audio_output device that we will use forever. Since
    //  it's NULL, it's not holding up a soundcard. It's just an object that
    //  thr RTSP streamer can use to get PCM bits to send out.
    //

    output = AudioOutput::OpenAudio("NULL", 16, 2, 44100, AUDIOOUTPUT_MUSIC, true );
    output->bufferOutputData(true);
    output->setBufferSize(output_buffer_size * 1024);
    output->SetBlocking(false);
    output->addListener(audio_listener);
    
    //
    //  Create and start the RtspOut thread
    //

    rtsp_out = new RtspOut(owner, -1, output);
    rtsp_out->start();
#endif
    maop_instances.setAutoDelete(true);
    speaker_release_timer.start();
    waiting_for_speaker_release = false;
    
}

void AudioPlugin::swallowOutputUpdate(int type, int numb_seconds, int channels, int bitrate, int frequency)
{
    //
    //  Get the latest data about the state of play if this is an info event
    //

    if (type == OutputEvent::Info)
    {

        playlist_mode_mutex.lock();
            play_data_mutex.lock();
                elapsed_time = numb_seconds;
                current_channels = channels;
                current_bitrate = bitrate;
                current_frequency = frequency;
                if (playlist_mode)
                {
                    current_playlist = current_playlist_id;
                    current_playlist_item = current_playlist_item_index;
                }
                else
                {
                    current_playlist = -1;
                    current_playlist_item = -1;
                }

                //
                //  Tell the client(s) what's going on
                //

                if (is_playing)
                {
                    sendMessage(QString("playing %1 %2 %3 %4 %5 %6 %7 %8")
                                        .arg(current_playlist)
                                        .arg(current_playlist_item)
                                        .arg(current_collection)
                                        .arg(current_metadata)
                                        .arg(elapsed_time)
                                        .arg(current_channels)
                                        .arg(current_bitrate)
                                        .arg(current_frequency));
                }

            play_data_mutex.unlock();
        playlist_mode_mutex.unlock();
    }
}

void AudioPlugin::run()
{
    char my_hostname[2049];
    QString local_hostname = "unknown";

    if (gethostname(my_hostname, 2048) < 0)
    {
        warning("could not call gethostname()");
    }
    else
    {
        local_hostname = my_hostname;
    }

    //
    //  Have our macp service (Myth Audio Control Protocol) broadcast on
    //  the local lan
    //

    Service *macp_service = new Service(
                                        QString("Myth Audio Control on %1").arg(local_hostname),
                                        QString("macp"),
                                        local_hostname,
                                        SLT_HOST,
                                        (uint) port_number
                                       );
 
    ServiceEvent *se = new ServiceEvent( true, true, *macp_service);
    QApplication::postEvent(parent, se);
    delete macp_service;

    //
    //  Init our sockets
    //
    
    if ( !initServerSocket())
    {
        fatal("audio could not initialize its core server socket");
        return;
    }
    
    while(keep_going)
    {
        //
        //  Update the status of our sockets and wait for something to happen
        //
        
        updateSockets();
        waitForSomethingToHappen();
        checkInternalMessages();
        checkMetadataChanges();
        checkServiceChanges();
        checkSpeakers();
    }

    moap_mutex.lock();
        turnOffSpeakers();
    moap_mutex.unlock();
    
    stopAudio();
}

void AudioPlugin::doSomething(const QStringList &tokens, int socket_identifier)
{
    bool ok = true;

    if (tokens.count() < 1)
    {
        ok = false;
    }
    else
    {
        if (tokens[0] == "play")
        {
            if (tokens.count() < 3)
            {
                ok = false;
            }
            else
            {
                if (tokens[1] == "url")
                {
                    QUrl play_url(tokens[2]);
                    if (!playUrl(play_url))
                    {
                        ok = false;
                    }
                }
                else if (tokens[1] == "item" || tokens[1] == "metadata")
                {
                    if (tokens.count() < 4)
                    {
                        ok = false;
                    }
                    else
                    {
                        bool parsed_ok = true;
                        int collection_id = tokens[2].toInt(&parsed_ok);
                        if (!parsed_ok)
                        {
                            ok = false;
                        }
                        else
                        {
                            int metadata_id = tokens[3].toInt(&parsed_ok);
                            if (!parsed_ok)
                            {
                                ok = false;
                            }
                            else
                            {
                                if (!playMetadata(collection_id, metadata_id))
                                {
                                    ok = false;
                                }
                            }
                        }
                    }
                }
                else if (
                        tokens[1] == "container" || 
                        tokens[1] == "playlist"  || 
                        tokens[1] == "list"
                        )
                {
                    if (tokens.count() < 4)
                    {
                        ok = false;
                    }
                    else
                    {
                        bool parsed_ok = true;
                        int collection_id = tokens[2].toInt(&parsed_ok);
                        if (!parsed_ok)
                        {
                            ok = false;
                        }
                        else
                        {
                            int playlist_id = tokens[3].toInt(&parsed_ok);
                            if (!parsed_ok)
                            {
                                ok = false;
                            }
                            else
                            {
                                if (tokens.count() >= 5)
                                {
                                    int index_position = tokens[4].toInt(&parsed_ok);
                                    if (!parsed_ok)
                                    {
                                        ok = false;
                                    }
                                    else
                                    {
                                        setPlaylistMode(collection_id, playlist_id, index_position);
                                        playFromPlaylist();
                                    }
                                }
                                else
                                {
                                    setPlaylistMode(collection_id, playlist_id);
                                    playFromPlaylist();
                                }
                            }
                        }
                    }
                    
                }
                else
                {
                    ok = false;
                }
            }
        }
        else if (tokens[0] == "pause")
        {
            if (tokens.count() < 2)
            {
                ok = false;
            }
            else if (tokens[1] == "on")
            {
                pauseAudio(true);
            }
            else if (tokens[1] == "off")
            {
                pauseAudio(false);
            }
            else
            {
                ok = false;
            }
        }
        else if (tokens[0] == "seek")
        {
            if (tokens.count() < 2)
            {
                ok = false;
            }
            else
            {
                bool convert_ok;
                int seek_amount = tokens[1].toInt(&convert_ok);
                if (convert_ok)
                {
                    seekAudio(seek_amount);
                }
                else
                {
                    ok = false;
                }
            }
        }
        else if (tokens[0] == "stop")
        {
            stopAudio();
        }
        else if (tokens[0] == "next")
        {
            nextPrevAudio(true);
        }
        else if (tokens[0] == "previous" || tokens[0] == "prev")
        {
            nextPrevAudio(false);
        }
        else if (tokens[0] == "status")
        {
            //
            //  Most/many/(all?) clients will want to get the status of the
            //  current audio state when they first connect
            //

            playlist_mode_mutex.lock();
                play_data_mutex.lock();
            
                    if (is_playing)
                    {
                        sendMessage(QString("playing %1 %2 %3 %4 %5 %6 %7 %8")
                                            .arg(current_playlist)
                                            .arg(current_playlist_item)
                                            .arg(current_collection)
                                            .arg(current_metadata)
                                            .arg(elapsed_time)
                                            .arg(current_channels)
                                            .arg(current_bitrate)
                                            .arg(current_frequency),
                                            socket_identifier);
                        if (is_paused)
                        {
                            sendMessage("pause on",  socket_identifier);
                        }
                        else
                        {
                            sendMessage("pause off",  socket_identifier);
                        }
                    }
                    else
                    {
                        sendMessage("stop", socket_identifier);
                    }

                play_data_mutex.unlock();
            playlist_mode_mutex.unlock();
            

        }
        else
        {
            ok = false;
        }
    }

    if (!ok)
    {
        warning(QString("did not understand or had "
               "problems with these tokens: %1")
               .arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
    }
}


bool AudioPlugin::playUrl(QUrl url, int collection_id)
{
    log(QString("attempting to play this url: \"%1\"")
                .arg(url), 9);

    //
    //  Before we do anything else, check to see if we can access this new
    //  url. If we can't, we don't want to stop anything that might already
    //  be playing.
    //
    
    if (!url.isValid())
    {
        warning(QString("passed an invalid url: %1").arg(url.toString()));
        return false;
    }
    if ( url.isLocalFile())
    {
        QString   path = url.path();

        //
        //  for filenames with # in them
        //

        if (url.hasRef())
        {
            path += "#";
            path += url.ref();
        }
        QFileInfo fi( path );
        if ( ! fi.exists() && fi.extension() != "cda") 
        {
            warning(QString("passed url to a file that does not exist: %1")
                    .arg(path));
            return false;
        }
    }
    else if (
            url.protocol() == "daap" ||
            url.protocol() == "cd"
           )
    {
        //
        //  Some way to check that this resource still exists?
        //
    }
    
    //
    //  If we are currently playing, we need to stop
    //
    

    state_of_play_mutex->lock();
        if (is_playing || is_paused)
        {
            state_of_play_mutex->unlock();
            stopAudio();
            state_of_play_mutex->lock();
        }

        bool start_output = false;

        if (!output)
        {
#ifdef MFD_RTSP_SUPPORT
            //
            //  Output always exists
            //
#else
            QString adevice = gContext->GetSetting("AudioDevice");
            // TODO: Error checking that device is opened correctly!
            output = AudioOutput::OpenAudio(adevice, 16, 2, 44100, AUDIOOUTPUT_MUSIC, true );
            output->setBufferSize(output_buffer_size * 1024);
            output->SetBlocking(false);
            output->addListener(audio_listener);
            start_output = true;
#endif
        }
    
        //
        //  Choose the QIODevice (derived) to serve as input
        // 
    

        if (url.protocol() == "file" || url.protocol() == "cd")
        {
            QString filename = url.path();
            if (url.hasRef())
            {
                filename += "#";
                filename += url.ref();
            }
            input = new QFile(filename);
        }
        else if (url.protocol() == "daap")
        {
            //
            //  We know it's daap. Need to find out the server type of this
            //  collection though to know if we need to jump through iTunes
            //  hoops or not
            //

            if (collection_id < 1)
            {
                input = new DaapInput(this, url, DAAP_SERVER_MYTH);
            }
            else
            {
                metadata_server->lockMetadata();
                MetadataContainer *which_container = metadata_server->getMetadataContainer(collection_id);
                int daap_server_type = which_container->getServerType();
                metadata_server->unlockMetadata();

                if (daap_server_type == MST_daap_itunes47)
                {
                    input = new DaapInput(this, url, DAAP_SERVER_ITUNES47);
                }
                else if (daap_server_type == MST_daap_itunes46)
                {
                    input = new DaapInput(this, url, DAAP_SERVER_ITUNES46);
                }
                else if (daap_server_type == MST_daap_itunes45)
                {
                    input = new DaapInput(this, url, DAAP_SERVER_ITUNES45);
                }
                else if (daap_server_type == MST_daap_itunes43)
                {
                    input = new DaapInput(this, url, DAAP_SERVER_ITUNES43);
                }
                else if (daap_server_type == MST_daap_itunes42)
                {
                    input = new DaapInput(this, url, DAAP_SERVER_ITUNES42);
                }
                else if (daap_server_type == MST_daap_itunes41)
                {
                    input = new DaapInput(this, url, DAAP_SERVER_ITUNES41);
                }
                else
                {
                    input = new DaapInput(this, url, DAAP_SERVER_MYTH);
                }
            }
        }
        else
        {
            warning(QString("have no idea how to play this protocol: %1")
                    .arg(url.toString()));
            deleteOutput();
            input = NULL;
            state_of_play_mutex->unlock();
            return false;
        }

        if (decoder && !decoder->factory()->supports(url.path()))
        {
            decoder = 0;
        }
        
        if (!decoder)
        {
            QString filename = url.path();
            if (url.hasRef())
            {
                filename += "#";
                filename += url.ref();
            }
            
            decoder = Decoder::create(filename, input, output);

            if (!decoder)
            {
                warning(QString("passed unsupported url in unsupported format: %1")
                        .arg(filename));
                deleteOutput();
                delete input;
                input = NULL;
                state_of_play_mutex->unlock();
                return false;
            }
            decoder->setBlockSize(globalBlockSize);
            decoder->setParent(this);
        }
        else
        {
            decoder->setInput(input);
            decoder->setOutput(output);
        }
        
        play_data_mutex.lock();
            elapsed_time = 0;
        play_data_mutex.unlock();
        
        if (decoder->initialize())
        {
            if (output)
            {
                if (start_output)
                {
                    output->Reset();
                }
            }
            decoder->start();
            is_playing = true;
            is_paused = false;
            turnOnSpeakers();
            state_of_play_mutex->unlock();
            return true;
        }
        else
        {
            warning(QString("decoder barfed on "
                    "initialization of %1")
                    .arg(url.toString()));
            deleteOutput();
            delete input;
            input = NULL;
            state_of_play_mutex->unlock();
            return false;
        }
        
    state_of_play_mutex->unlock();
    return false;
}

bool AudioPlugin::playMetadata(int collection_id, int metadata_id)
{
    //
    //  See if this id's lead to a real metadata object before we mess with
    //  anything else
    //
    
    metadata_server->lockMetadata();
    Metadata *metadata_to_play = 
            metadata_server->getMetadataByContainerAndId(
                                                            collection_id,
                                                            metadata_id
                                                        );
    
    if (!metadata_to_play)
    {
        warning(QString("was asked to play container %1 "
                        "item %2, but it does not exist")
                        .arg(collection_id)
                        .arg(metadata_id));
        metadata_server->unlockMetadata();
        return false;
    }

    //
    //  Make sure we can cast this to Audio metadata
    //
    
    if (metadata_to_play->getType() == MDT_audio)
    {
        AudioMetadata *audio_metadata_to_play = (AudioMetadata *)metadata_to_play;

        //
        //  Note that some metadata values should change
        //
        
        audio_metadata_to_play->setLastPlayed(QDateTime::currentDateTime());
        audio_metadata_to_play->setPlayCount(audio_metadata_to_play->getPlayCount() + 1);
        audio_metadata_to_play->setChanged(true);

        QUrl url_to_play = audio_metadata_to_play->getUrl();
        metadata_server->unlockMetadata();
        bool result = playUrl(url_to_play, collection_id);
        if (result)
        {
            play_data_mutex.lock();
                current_collection = collection_id;
                current_metadata = metadata_id;
            play_data_mutex.unlock();
        }
        return result;
    }
    else
    {
        metadata_server->unlockMetadata();
        warning(QString("was asked to play container %1 "
                        "item %2, which is not audio content")
                        .arg(collection_id)
                        .arg(metadata_id));
        return false;
    }
}

void AudioPlugin::stopAudio()
{
    state_of_play_mutex->lock();

        if (decoder && decoder->running())
        {
            decoder->mutex()->lock();
                decoder->stop();
            decoder->mutex()->unlock();
        }
    
        //
        //  wake them up 
        //
    
        if(decoder)
        {
            decoder->mutex()->lock();
                decoder->cond()->wakeAll();
            decoder->mutex()->unlock();
        }

        if (output)
        {
            deleteOutput();
        }

        if (decoder)
        {
            decoder->wait();
        }

        if (input)
        {
            delete input;
            input = NULL;
        }
        
        is_playing = false;
        is_paused = false;

    state_of_play_mutex->unlock();
    maop_mutex.lock();
        waiting_for_speaker_release = true;
    maop_mutex.unlock();
    speaker_release_timer.restart();
    sendMessage("stop");
}

void AudioPlugin::pauseAudio(bool true_or_false)
{
    state_of_play_mutex->lock();
    
        if (!is_playing)
        {
            //
            //  you can't be paused if you're not playing!
            //
            
            true_or_false = false;
            
        }
    
        if (true_or_false == is_paused)
        {
            //
            //  already in the right state of pause
            //
        }
        else
        {
            is_paused = true_or_false;
            if (output)
            {
                output->mutex()->lock();
                    output->Pause(is_paused);
                output->mutex()->unlock();
            }
            if (decoder)
            {
                decoder->mutex()->lock();
                    decoder->cond()->wakeAll();
                decoder->mutex()->unlock();
            }
        }
       
        //
        //   Tell every connected client the state of the pause
        //
        if (is_paused)
        {
            sendMessage("pause on");
        }
        else
        {
            sendMessage("pause off");
        }
    state_of_play_mutex->unlock();
}

void AudioPlugin::seekAudio(int seek_amount)
{
    state_of_play_mutex->lock();

        int position = elapsed_time + seek_amount;
        if (position < 0)
        {
            position = 0;
        }

        if (output)
        {
            output->Reset();
            output->SetTimecode(position * 1000);

            if (decoder && decoder->running())
            {
                decoder->mutex()->lock();
                    decoder->seek(position);
                decoder->mutex()->unlock();
            }
        }
    state_of_play_mutex->unlock();
}

void AudioPlugin::handleInternalMessage(QString the_message)
{
    //
    //  This is where we handle internal messages (coming from the decoder)
    //
    
    if (the_message == "decoder error")
    {
        //
        //  Stop the audio and null the decoder
        //

        stopAudio();
        if(decoder)
        {
            decoder = NULL;
        }
    }
    if (the_message == "decoder stop")
    {
        //
        //  Well, ok ... stop means we really stopped (not that we just
        //  reached the end of a file) which can only mean (?) that
        //  something already called stopAudio()
        //

        // stopAudio();
    }
    if (the_message == "decoder finish")
    {
        //
        //  If we're in playlist mode, find the next thing to play.
        //  Otherwise, destruct everything.
        //

        if (playlist_mode)
        {
            playFromPlaylist(1);
        }
        else
        {
            stopAudio();
        }
    }
}

void AudioPlugin::setPlaylistMode(int container, int id, int index)
{

    playlist_mode_mutex.lock();
        playlist_mode = true;
        current_playlist_container = container;
        current_playlist_id = id;
        current_playlist_item_index = index;
        log(QString("entering playlist mode "
                    "(container=%1, playlist=%2)")
                    .arg(container)
                    .arg(id), 7);
    playlist_mode_mutex.unlock();
}

void AudioPlugin::stopPlaylistMode()
{
    playlist_mode_mutex.lock();
        playlist_mode = false;
        current_playlist_container = -1;
        current_playlist_id = -1;
        current_playlist_item_index = -1;
        log(QString("leaving playlist mode"), 7);
        current_chain_playlist.clear();
        current_chain_position.clear();
    playlist_mode_mutex.unlock();
}

void AudioPlugin::playFromPlaylist(int augment_index)
{
    uint playlist_size = 0;
    if (!playlist_mode)
    {
        warning("playFromPlaylist() called, but we're not in playlist mode");
        return;
    }
    
    QUrl url_to_play_next;

    playlist_mode_mutex.lock();

    if (augment_index)
    {
        current_playlist_item_index += augment_index;
    }

    //
    //  find the playlist in question
    //

    metadata_server->lockMetadata();

    Playlist *playlist_to_play = 
             metadata_server->getPlaylistByContainerAndId(
                 
                               current_playlist_container,
                               current_playlist_id
                                                         );        
                                                          
    if (!playlist_to_play)
    {
        warning(QString("asked to play playlist %1 in "
                        "container %2, but that "
                        "doesn't exist")
                        .arg(current_playlist_id)
                        .arg(current_playlist_container));
        metadata_server->unlockMetadata();
        playlist_mode_mutex.unlock();
        stopPlaylistMode();
        stopAudio();
        return;
    }
    
    playlist_size = playlist_to_play->getCount();
        
    //
    //  Get the list of references.
    //
        
    QValueList<int> song_list = playlist_to_play->getList();

    //
    //  At some point, do something about shuffle modes here
    //

    if (song_list.count() == 0)
    {
        //
        //  No point in trying to cycle through a playlist of 0 length
        //
        
        log("stopping playlist mode, playlist has no songs", 8);
        metadata_server->unlockMetadata();
        playlist_mode_mutex.unlock();
        stopPlaylistMode();
        stopAudio();
        return;
    }
   

    if (current_playlist_item_index >= (int) song_list.count())
    {
        //
        //  We've reached the end of the list moving forwards (down the
        //  list).  Perhaps we can pop up a level ?
        //
        
        if (current_chain_playlist.count() > 0)
        {
            current_playlist_id         = current_chain_playlist.pop();
            current_playlist_item_index = current_chain_position.pop();
            metadata_server->unlockMetadata();        
            playlist_mode_mutex.unlock();
            playFromPlaylist(1);
            return;
        }
        else
        {
            current_playlist_item_index = 0;
        }
    }
        
    if (current_playlist_item_index < 0)
    {
        //
        //  We've reached the end of the list moving backwards (up the
        //  list).  Perhaps we can pop up a level ?
        //
        
        if (current_chain_playlist.count() > 0)
        {
            current_playlist_id         = current_chain_playlist.pop();
            current_playlist_item_index = current_chain_position.pop();
            metadata_server->unlockMetadata();        
            playlist_mode_mutex.unlock();
            playFromPlaylist(-1);
            return;
        }
        else
        {
            current_playlist_item_index = (int) song_list.count() - 1;
        }
    }
        
    //
    //  Get the relevant reference
    //
        
    int song_to_play = song_list[current_playlist_item_index];
    uint collection_to_play_from = current_playlist_container;


    if (song_to_play < 0)
    {
        //
        //  We have a reference to another playlist, so we need to push down
        //
        
        Playlist *down_playlist = metadata_server->getPlaylistByContainerAndId(
                                                            current_playlist_container,
                                                            song_to_play * -1
                                                                              );        
        if (down_playlist)
        {
            current_chain_playlist.push(current_playlist_id);
            current_chain_position.push(current_playlist_item_index);
            current_playlist_id = song_to_play * -1;
            
            int where_to_jump = 1;
            current_playlist_item_index = -1;

            if (augment_index < 0)
            {
                if (down_playlist->getList().count() > 0)
                {
                    current_playlist_item_index = down_playlist->getList().count();
                    where_to_jump = -1;
                }
            }
            metadata_server->unlockMetadata();        
            playlist_mode_mutex.unlock();
            playFromPlaylist(where_to_jump);
            return;
        }
        else
        {
            warning(QString("bad reference to another playlist "
                            "on a playlist: %1")
                            .arg(song_to_play));
            metadata_server->unlockMetadata();        
            playlist_mode_mutex.unlock();
            stopPlaylistMode();
        }
    }


    metadata_server->unlockMetadata();        
    playlist_mode_mutex.unlock();

    if (!playMetadata(collection_to_play_from, song_to_play))
    {
        log(QString("in playlist mode, could not find item number "
                    "%1 in playlist %2 (of size %3)")
                    .arg(current_playlist_item_index)
                    .arg(current_playlist_container)
                    .arg(playlist_size), 8);
        stopPlaylistMode();
        stopAudio();
    }
}

void AudioPlugin::nextPrevAudio(bool forward)
{
    if (forward)
    {
        playFromPlaylist(1);
    }
    else
    {
        playFromPlaylist(-1);
    }
}

void AudioPlugin::handleMetadataChange(int which_collection, bool /*external*/)
{
    state_of_play_mutex->lock();
    playlist_mode_mutex.lock();

        //
        //  We only care if we are playing something
        //

        if (!is_playing && !is_paused)
        {
            state_of_play_mutex->unlock();
            playlist_mode_mutex.unlock();
            return;
        }

        //
        //  We need to check if the track we are currently playing has changed
        //  and/or disappered
        //

        bool stop_current_track = false;
        bool exit_current_list  = false;
        
        if(which_collection == current_collection)
        {
            metadata_server->lockMetadata();
                MetadataContainer *which_one = metadata_server->getMetadataContainer(which_collection);
                if(which_one)
                {
                    QValueList<int> item_deletions = which_one->getMetadataDeletions();
                    if (item_deletions.contains(current_metadata))
                    {
                        stop_current_track = true;
                    }
                    QValueList<int> list_deletions = which_one->getPlaylistDeletions();
                    if (list_deletions.contains(current_playlist))
                    {
                        exit_current_list = true;
                    }
                }
                else
                {
                    stop_current_track = true;
                    exit_current_list = true;
                } 
            metadata_server->unlockMetadata();
        }

    playlist_mode_mutex.unlock();
    if(exit_current_list)
    {
        stopPlaylistMode();
    }
    state_of_play_mutex->unlock();
    if(stop_current_track)
    {
        stopAudio();
    }
}

void AudioPlugin::deleteOutput()
{
#ifdef MFD_RTSP_SUPPORT
    output->Reset();
#else
    delete output;
    output = NULL;
#endif
}
                

void AudioPlugin::handleServiceChange()
{
    //
    //  Something has changed in available services. I need to query the mfd
    //  and make sure I know about all maop services (i.e. all available
    //  speakers)
    //
    
    ServiceLister *service_lister = parent->getServiceLister();
    
    service_lister->lockDiscoveredList();

        //
        //  Go through my list of maop instances and make sure they still
        //  exist in the mfd's service list
        //

        maop_mutex.lock();
            MaopInstance *an_instance;
            QPtrListIterator<MaopInstance> iter( maop_instances );

            while ( (an_instance = iter.current()) != 0 )
            {
                if( !service_lister->findDiscoveredService(an_instance->getName()) )
                {
                    log(QString("removed speakers resource called \"%1\" (total now %2)")
                        .arg(an_instance->getName()).arg(maop_instances.count() - 1), 5);
                    int fd = an_instance->getFileDescriptor();
                    if(fd > 0)
                    {
                        removeFileDescriptorToWatch(fd);
                    }
                    maop_instances.remove( an_instance );
                }
                ++iter;
            }
        maop_mutex.unlock();
    
        //
        //  Go through services on the master list, add any that I don't
        //  already know about
        //
        
        typedef     QValueList<Service> ServiceList;
        ServiceList *discovered_list = service_lister->getDiscoveredList();
        

        ServiceList::iterator it;
        for ( it  = discovered_list->begin(); 
              it != discovered_list->end(); 
              ++it )
        {
            //
            //  We just try and add any that are not on this host, as the
            //  addDaapServer() method checks for dupes
            //
                
            if ( (*it).getType() == "maop")
            {
                addMaopSpeakers((*it).getAddress(), (*it).getPort(), (*it).getName() );
            }
                
        }

    service_lister->unlockDiscoveredList();
    
}

void AudioPlugin::addMaopSpeakers(QString l_address, uint l_port, QString l_name)
{
    //
    //  If it's new, add it ... otherwise ignore it
    //


    bool already_have_it = false;

    maop_mutex.lock();
    MaopInstance *an_instance;
    for ( an_instance = maop_instances.first(); an_instance; an_instance = maop_instances.next() )
    {
        if(an_instance->isThisYou(l_name, l_address, l_port))
        {
            already_have_it = true;
            break;
        }
    }
    
    if(!already_have_it)
    {
        MaopInstance *new_maop_instance = new MaopInstance(this, l_name, l_address, l_port);
        if(new_maop_instance->allIsWell())
        {
            int fd = new_maop_instance->getFileDescriptor();
            if(fd > 0)
            {
                log(QString("adding speakers resource called \"%1\" via maop://%2:%3 (total now %4)")
                    .arg(l_name)
                    .arg(l_address)
                    .arg(l_port)
                    .arg(maop_instances.count() + 1), 5);
                maop_instances.append(new_maop_instance);
                addFileDescriptorToWatch(fd);
            }
            else
            {
                warning("maop instance said it was created ok, but returned a bad file descriptor ???");
                delete new_maop_instance;
            }
        }
        else
        {
            warning(QString("failed to add a speakers resource called \"%1\" "
                            "via maop://%2:%3")
                            .arg(l_name)
                            .arg(l_address)
                            .arg(l_port));
            delete new_maop_instance;
        }
    }
    maop_mutex.unlock();
}

void AudioPlugin::checkSpeakers()
{
#ifdef MFD_RTSP_SUPPORT
    //
    //  Give a little processing time in this main thread of the audio
    //  plugin to checking whether there's any coming in from the
    //  speakers/maop services (change of status, etc)
    //
    
    maop_mutex.lock();
        QPtrListIterator<MaopInstance> it( maop_instances );
        MaopInstance *an_instance;
        while ( (an_instance = it.current()) != 0 ) 
        {
            an_instance->checkIncoming();
            ++it;
        }        
    
    //
    //  If we're waiting to release the speakers, do so if enough time had elapsed
    //
    
    if(waiting_for_speaker_release)
    {
        if(speaker_release_timer.elapsed() > 30 * 1000) // 30 seconds
        {
            turnOffSpeakers();
            waiting_for_speaker_release = false;
        }
    }
    maop_mutex.unlock();
#endif
}

void AudioPlugin::turnOnSpeakers()
{
#ifdef MFD_RTSP_SUPPORT

    maop_mutex.lock();

    //
    //  Tell all speakers (maop instances) to listen to our rtsp stream
    //
    
    QPtrListIterator<MaopInstance> it( maop_instances );
    MaopInstance *an_instance;
    while ( (an_instance = it.current()) != 0 ) 
    {
        an_instance->sendRequest(QString("open %1").arg(rtsp_out->getUrl()));
        ++it;
    }        
    
    //
    //  If we just turned them on, we can't be waiting for a speaker release
    //
    
    waiting_for_speaker_release = false;
    
    maop_mutex.unlock();
#endif
}

void AudioPlugin::turnOffSpeakers()
{
#ifdef MFD_RTSP_SUPPORT
    //
    //  The maop mutex should be locked 
    //
    
    if(maop_mutex.tryLock())
    {
        warning("AudioPlugin::turnOffSpeakers() called without maop_mutex being locked");
        maop_mutex.unlock();
    }
    
    //
    //  Tell all speakers (maop instances) to stop listening to our stream
    //
    
    QPtrListIterator<MaopInstance> it( maop_instances );
    MaopInstance *an_instance;
    while ( (an_instance = it.current()) != 0 ) 
    {
        an_instance->sendRequest("close");
        ++it;
    }        
#endif
}

AudioPlugin::~AudioPlugin()
{
    maop_instances.clear();

#ifdef MFD_RTSP_SUPPORT
    if(rtsp_out)
    {
        rtsp_out->stop();
        rtsp_out->wakeUp();
        rtsp_out->wait();
        
        delete rtsp_out;
        rtsp_out = NULL;
    }
#endif 

    if (output)
    {
        delete output;
        output = NULL;        
    }
    
    if (input)
    {
        delete input;
        input = NULL;
    }
    
    if (state_of_play_mutex)
    {
        delete state_of_play_mutex;
        state_of_play_mutex = NULL;
    }
    
   
    if (audio_listener)
    {
        delete audio_listener;
        audio_listener = NULL;
    }
}
