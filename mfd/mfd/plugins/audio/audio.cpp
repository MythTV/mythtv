/*
	audio.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	

*/

#include <qapplication.h>
#include <qfileinfo.h>

#include "../../events.h"

#include "audio.h"
#include "audiooutput.h"

AudioPlugin::AudioPlugin(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 2343)
{

    input = NULL;
    output = NULL;
    decoder = NULL;
    fd_watcher = NULL;
    output_buffer_size = 256;
    elapsed_time = 0;
    is_playing = false;
    is_paused = false;
    audio_device = "/dev/dsp";  //  <--- ewww ... change that soon
    
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
    //  Put an internal request on the list of things to do
    //
    
    addToDo("playupdate", -1);
    
    //
    //  wake up the running thread so it can send this new data to connected
    //  clients
    //    
    
    wakeUp();
}

void AudioPlugin::run()
{
    char my_hostname[2049];
    QString local_hostname = "unknown";

    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("audio plugin could not call gethostname()");
       
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
    
    int nfds = 0;
	fd_set readfds;
    int fd_watching_pipe[2];
    if(pipe(fd_watching_pipe) < 0)
    {
        warning("audio plugin could not create a pipe to its FD watching thread");
    }

    fd_watcher = new MFDFileDescriptorWatchingPlugin(this, &file_descriptors_mutex, &nfds, &readfds);
    file_descriptors_mutex.lock();
    fd_watcher->start();


    while(keep_going)
    {
        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        
        
        QStringList pending_tokens;
        int         pending_socket = 0;

        //
        //  Pull off the first pending command request
        //

        things_to_do_mutex.lock();
            if(things_to_do.count() > 0)
            {
                pending_tokens = things_to_do.getFirst()->getTokens();
                pending_socket = things_to_do.getFirst()->getSocketIdentifier();
                things_to_do.removeFirst();
            }
        things_to_do_mutex.unlock();
            
        if(pending_tokens.count() > 0)
        {
            doSomething(pending_tokens, pending_socket);
        }
        else
        {
    		nfds = 0;
	    	FD_ZERO(&readfds);

            //
            //  Add out core server socket to the set of file descriptors to
            //  listen to
            //
            
            FD_SET(core_server_socket->socket(), &readfds);
            if(nfds <= core_server_socket->socket())
            {
                nfds = core_server_socket->socket() + 1;
            }

            //
            //  Next, add all the client sockets
            //
            
            QPtrListIterator<MFDServiceClientSocket> iterator(client_sockets);
            MFDServiceClientSocket *a_client;
            while ( (a_client = iterator.current()) != 0 )
            {
                ++iterator;
                FD_SET(a_client->socket(), &readfds);
                if(nfds <= a_client->socket())
                {
                    nfds = a_client->socket() + 1;
                }
            }
            

            //
            //  Finally, add the control pipe
            //
            
    		FD_SET(fd_watching_pipe[0], &readfds);
            if(nfds <= fd_watching_pipe[0])
            {
                nfds = fd_watching_pipe[0] + 1;
            }
            
            
       		fd_watcher->setTimeout(10, 0);  //  wait ten seconds ... could be really high
            file_descriptors_mutex.unlock();

            main_wait_condition.wait();

            write(fd_watching_pipe[1], "X", 1);
            file_descriptors_mutex.lock();
            char back[2];
            read(fd_watching_pipe[0], back, 2);

        }
    }

    //
    //  Stop the sub thread
    //
    
    file_descriptors_mutex.unlock();
    fd_watcher->stop();
    fd_watcher->wait();
    delete fd_watcher;
    fd_watcher = NULL;

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
            if(tokens.count() < 2)
            {
                ok = false;
            }
            else
            {
                QUrl play_url(tokens[1]);
                if(!playAudio(play_url))
                {
                    ok = false;
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
        else if(tokens[0] == "playupdate")
        {
            sendMessage(QString("playing %1 %2 %3 %4")
                        .arg(elapsed_time)
                        .arg(current_channels)
                        .arg(current_bitrate)
                        .arg(current_frequency));
        }
        else
        {
            ok = false;
        }
    }

    if(!ok)
    {
        warning(QString("audio did not understand these tokens: %1").arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
    }
}

bool AudioPlugin::playAudio(QUrl url)
{

    //
    //  Before we do anything else, check to see if we can access this new
    //  url. If we can't, we don't want to stop anything that might already
    //  be playing.
    //
    
    if(!url.isValid())
    {
        warning("audio was passed an invalid url");
        return false;
    }
    if ( url.isLocalFile())
    {
        QString   path = url.path();
        QFileInfo fi( path );
        if ( ! fi.exists() && fi.extension() != "cda") 
        {
            warning(QString("audio was passed url to a file that does not exist: %1")
                    .arg(path));
            return false;
        }
    }
    
    //
    //  If we are currently playing, we need to stop
    //
    
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
            warning("audio could not initialize an output object");
            delete output;
            output = NULL;
            return false;
        }
        start_output = true;
    }
    
    if(url.protocol() == "file")
    {
        QString file_path = url.path();
        
        //
        //  Correct Qt *always* wanting a "/" in a url path.
        //

        QFileInfo fi( file_path );
        if( fi.extension() == "cda")
        {
            file_path = file_path.section('/', -1, -1);
        }        



        input = new QFile(file_path);

        if(decoder && !decoder->factory()->supports(file_path))
        {
            decoder = 0;
        }
        
        if(!decoder)
        {
        
            decoder = Decoder::create(file_path, input, output);
            if(!decoder)
            {
                warning(QString("audio passed unsupported file in unsupported format: %1")
                        .arg(file_path));
                delete output;
                output = NULL;
                delete input;
                input = NULL;
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
            return true;
        }
        else
        {
            delete output;
            output = NULL;
            delete input;
            input = NULL;
            return false;
        }
    }
    
    return false;
}

void AudioPlugin::stopAudio()
{
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
    
    delete input;
    input = NULL;
        
    is_playing = false;
    is_paused = false;
}

void AudioPlugin::pauseAudio(bool true_or_false)
{
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
}

void AudioPlugin::seekAudio(int seek_amount)
{
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


}

