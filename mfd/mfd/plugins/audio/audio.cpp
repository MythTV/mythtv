/*
	audio.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	

*/

#include <qapplication.h>
#include <qfileinfo.h>

#include "mfd_events.h"

#include "audio.h"
#include "audiooutput.h"
#include "daapinput.h"

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
    audio_device = "/dev/dsp";  //  <--- ewww ... change that soon
    state_of_play_mutex = new QMutex(true);    
    metadata_server = parent->getMetadataServer();
}

void AudioPlugin::swallowOutputUpdate(int numb_seconds, int channels, int bitrate, int frequency)
{
    //
    //  Get the latest data about the state of play
    //

    play_data_mutex.lock();
        elapsed_time = numb_seconds;
        current_channels = channels;
        current_bitrate = bitrate;
        current_frequency = frequency;
    play_data_mutex.unlock();

    //
    //  Tell the clients
    //
    
    sendMessage(QString("playing %1 %2 %3 %4")
                        .arg(elapsed_time)
                        .arg(current_channels)
                        .arg(current_bitrate)
                        .arg(current_frequency));

}

void AudioPlugin::run()
{
    char my_hostname[2049];
    QString local_hostname = "unknown";

    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("could not call gethostname()");
       
    }
    else
    {
        local_hostname = my_hostname;
    }

    ServiceEvent *se = new ServiceEvent(QString("services add macp %1 MythMusic on %2")
                                       .arg(port_number)
                                       .arg(local_hostname));
    QApplication::postEvent(parent, se);



    //
    //  Init our sockets
    //
    
    if( !initServerSocket())
    {
        fatal("audio could not initialize its core server socket");
        return;
    }
    
    while(keep_going)
    {
        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        waitForSomethingToHappen();
    }

    stopAudio();
}

void AudioPlugin::doSomething(const QStringList &tokens, int socket_identifier)
{
    bool ok = true;

    if(tokens.count() < 1)
    {
        ok = false;
    }
    else
    {
        if(tokens[0] == "play")
        {
            if(tokens.count() < 3)
            {
                ok = false;
            }
            else
            {
                if(tokens[1] == "url")
                {
                    QUrl play_url(tokens[2]);
                    if(!playUrl(play_url))
                    {
                        ok = false;
                    }
                }
                else if(tokens[1] == "item" || tokens[1] == "metadata")
                {
                    if(tokens.count() < 4)
                    {
                        ok = false;
                    }
                    else
                    {
                        bool parsed_ok = true;
                        int collection_id = tokens[2].toInt(&parsed_ok);
                        if(!parsed_ok)
                        {
                            ok = false;
                        }
                        else
                        {
                            int metadata_id = tokens[3].toInt(&parsed_ok);
                            if(!parsed_ok)
                            {
                                ok = false;
                            }
                            else
                            {
                                if(!playMetadata(collection_id, metadata_id))
                                {
                                    ok = false;
                                }
                            }
                        }
                    }
                }
            }
        }
        else if(tokens[0] == "pause")
        {
            if(tokens.count() < 2)
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
        else if(tokens[0] == "seek")
        {
            if(tokens.count() < 2)
            {
                ok = false;
            }
            else
            {
                bool convert_ok;
                int seek_amount = tokens[1].toInt(&convert_ok);
                if(convert_ok)
                {
                    seekAudio(seek_amount);
                }
                else
                {
                    ok = false;
                }
            }
        }
        else if(tokens[0] == "stop")
        {
            stopAudio();
        }
        else
        {
            ok = false;
        }
    }

    if(!ok)
    {
        warning(QString("did not understand or had "
               "problems with these tokens: %1")
               .arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
    }
}


bool AudioPlugin::playUrl(QUrl url)
{

    //
    //  Before we do anything else, check to see if we can access this new
    //  url. If we can't, we don't want to stop anything that might already
    //  be playing.
    //
    
    if(!url.isValid())
    {
        warning(QString("passed an invalid url: %1").arg(url.toString()));
        return false;
    }
    if ( url.isLocalFile())
    {
        QString   path = url.path();
        QFileInfo fi( path );
        if ( ! fi.exists() && fi.extension() != "cda") 
        {
            warning(QString("passed url to a file that does not exist: %1")
                    .arg(path));
            return false;
        }
    }
    else if(url.protocol() == "daap")
    {
        //
        //  Some way to check that this still exists?
        //
    }
    
    //
    //  If we are currently playing, we need to stop
    //
    

    state_of_play_mutex->lock();
        if(is_playing || is_paused)
        {
            stopAudio();
        }

        bool start_output = false;

        if(!output)
        {
            //
            //  output may not exist because this is a first run
            //  or stopAudio() killed it off.
            //
        
            output = new MMAudioOutput(this, output_buffer_size * 1024, "/dev/dsp");
            output->setBufferSize(output_buffer_size * 1024);
            if(!output->initialize())
            {
                warning("could not initialize an output object");
                delete output;
                output = NULL;
                state_of_play_mutex->unlock();
                return false;
            }
            start_output = true;
        }
    
        //
        //  Choose the QIODevice (derived) to serve as input
        // 
    
        QString url_path = url.path();
    
        if(url.protocol() == "file")
        {
        
            //
            //  Correct Qt *always* wanting a "/" in a url path.
            //

            QFileInfo fi( url_path );
            if( fi.extension() == "cda")
            {
                url_path = url_path.section('/', -1, -1);
            }        

            input = new QFile(url_path);
        }
        else if(url.protocol() == "daap")
        {
            input = new DaapInput(url);
        }
        else
        {
            warning(QString("have no idea how to play this protocol: %1")
                    .arg(url.toString()));
            delete output;
            output = NULL;
            input = NULL;
            state_of_play_mutex->unlock();
            return false;
        }

        if(decoder && !decoder->factory()->supports(url_path))
        {
            decoder = 0;
        }
        
        if(!decoder)
        {
        
            decoder = Decoder::create(url_path, input, output);
            if(!decoder)
            {
                warning(QString("passed unsupported url in unsupported format: %1")
                        .arg(url.toString()));
                delete output;
                output = NULL;
                delete input;
                input = NULL;
                state_of_play_mutex->unlock();
                return false;
            }
            decoder->setBlockSize(globalBlockSize);
        }
        else
        {
            decoder->setInput(input);
            decoder->setOutput(output);
        }
        
        play_data_mutex.lock();
            elapsed_time = 0;
        play_data_mutex.unlock();
        
        if(decoder->initialize())
        {
            if(output)
            {
                if(start_output)
                {
                    output->start();
                }
                else
                {
                    output->resetTime();
                }
            }
            decoder->start();
            is_playing = true;
            is_paused = false;
            state_of_play_mutex->unlock();
            return true;
        }
        else
        {
            warning(QString("decoder barfed on "
                    "initialization of %1")
                    .arg(url.toString()));
            delete output;
            output = NULL;
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
    
    Metadata *metadata_to_play = 
            metadata_server->getMetadataByContainerAndId(
                                                            collection_id,
                                                            metadata_id
                                                        );
    
    if(!metadata_to_play)
    {
        warning(QString("was asked to play container %1 "
                        "item %2, but that does not exist")
                        .arg(collection_id)
                        .arg(metadata_id));
        return false;
    }

    //
    //  Make sure we can cast this to Audio metadata
    //
    
    if(metadata_to_play->getType() == MDT_audio)
    {
        AudioMetadata *audio_metadata_to_play = (AudioMetadata *)metadata_to_play;
        return playUrl(audio_metadata_to_play->getUrl());
    }
    else
    {
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
    
        if (output && output->running())
        {
            output->mutex()->lock();
                output->stop();
            output->mutex()->unlock();
        }
    
        //
        //  wake them up 
        //
    
        if (decoder)
        {
            decoder->mutex()->lock();
                decoder->cond()->wakeAll();
            decoder->mutex()->unlock();
        }

        if (output)
        {
            output->recycler()->mutex()->lock();
                output->recycler()->cond()->wakeAll();
            output->recycler()->mutex()->unlock();
        }

        if (decoder)
        {
            decoder->wait();
        }

        if (output)
        {
            output->wait();
        }

        if (output)
        {
            delete output;
            output = 0;
        }
    
        if(input)
        {
            delete input;
            input = NULL;
        }
        
        is_playing = false;
        is_paused = false;

    state_of_play_mutex->unlock();
}

void AudioPlugin::pauseAudio(bool true_or_false)
{
    state_of_play_mutex->lock();
        if(true_or_false == is_paused)
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
                    output->pause();
                output->mutex()->unlock();
            }
            if (decoder)
            {
                decoder->mutex()->lock();
                    decoder->cond()->wakeAll();
                decoder->mutex()->unlock();
            }
            if (output)
            {
                output->recycler()->mutex()->lock();
                    output->recycler()->cond()->wakeAll();
                output->recycler()->mutex()->unlock();
            }
        }
       
        //
        //   Tell every connected client the state of the pause
        //
        if(is_paused)
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
        if(position < 0)
        {
            position = 0;
        }

        if (output && output->running())
        {
            output->mutex()->lock();
            output->seek(position);
    
            if (decoder && decoder->running())
            {
                decoder->mutex()->lock();
                decoder->seek(position);
    
                /*
                if (mainvisual)
                {
                    mainvisual->mutex()->lock();
                    mainvisual->prepare();
                    mainvisual->mutex()->unlock();
                }
                */
    
                decoder->mutex()->unlock();
            }
            output->mutex()->unlock();
        }
    state_of_play_mutex->unlock();
}

AudioPlugin::~AudioPlugin()
{
    if(output)
    {
        delete output;
        output = NULL;        
    }
    
    if(input)
    {
        delete input;
        input = NULL;
    }
    
    if(state_of_play_mutex)
    {
        delete state_of_play_mutex;
        state_of_play_mutex = NULL;
    }
}
