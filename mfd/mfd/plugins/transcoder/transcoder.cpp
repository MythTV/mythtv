/*
    transcoder.cpp

    (c) 2005 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    

*/

#include <qfileinfo.h>
#include <qdir.h>

#include "transcoder.h"
#include "settings.h"
#include "httpoutresponse.h"
#include "audiocdjob.h"
#include "clienttracker.h"
#include "../../../mtcplib/markupcodes.h"
#include "../../../mtcplib/mtcpoutput.h"

Transcoder *transcoder = NULL;


Transcoder::Transcoder(MFD *owner, int identity)
      :MFDHttpPlugin(owner, identity, mfdContext->getNumSetting("mfd_transcoder_port"), "transcoder", 2)
{
    job_id = 0;    
    status_generation = 1;
    top_level_directory = NULL;
    music_destination_directory = NULL;
    jobs.setAutoDelete(true);
    hanging_status_clients.setAutoDelete(true);
}


bool Transcoder::initialize()
{
    //
    //  Find some scratch space to work on
    //

    QString startdir = mfdContext->GetSetting("MfdScratchDirectory");
    if(startdir.length() < 1)
    {
        log("no MfdScratchDirectory set, transcoder will attempt to use /tmp", 3);
        startdir = "/tmp";
    }

    startdir = QDir::cleanDirPath(startdir);
    if(!startdir.endsWith("/"));
    {
        startdir += "/";
    }

    //
    //  See if the scratch/tmp space is writable
    //
    
    QFileInfo starting_directory(startdir);
    if (!starting_directory.exists())
    {
        warning(QString("giving up, scratch space directory "
                        "does not exist: \"%1\"")
                        .arg(startdir));
        return false;
    }

    if (!starting_directory.isDir())
    {
        warning(QString("giving up, scratch space starting "
                        "point is not a directory: \"%1\"")
                        .arg(startdir));
        return false;
    }

    if (!starting_directory.isWritable())
    {
        warning(QString("giving up, scratch space starting "
                        "directory is not writable "
                        "(permissions?): \"%1\"")
                        .arg(startdir));
        return false;
    }
    
    //
    //  See if the mfd_transcoder is lying around from a previous crash
    //
    
    QFileInfo preexisting(QString("%1%2").arg(startdir).arg("mfd_transcoder"));    
    
    if(preexisting.exists())
    {
        warning("found a pre-existing mfd_transcoder dir in scratch "
                "space ... did your mfd crash?");
    } 
    else
    {
    
        //
        //  Make the top level directory (mfd_transcoder/) that all the jobs
        //  will write below
        //    

        QDir a_dir;

        if (! (a_dir.mkdir(QString("%1%2").arg(startdir).arg("mfd_transcoder/"))))
        {
            warning(QString("giving up, could not make \"mfd_transcoder\" "
                            "directory in \"%1\"")
                            .arg(startdir));
            return false;
        }
    }
    
    //
    //  Assing the top level directory and make sure it is writeable
    //

    top_level_directory = new QDir(QString("%1%2").arg(startdir).arg("mfd_transcoder/"));
    QFileInfo writechecker(top_level_directory->path());

    if(!writechecker.isDir())
    {
        warning(QString("top level exists (\"%1\"), but "
                        "it is not a directory")
                        .arg(top_level_directory->path()));
        return false;
    }
    
    if(!writechecker.isWritable())
    {
        warning(QString("top level exists (\"%1\"), but "
                        "it is not a writeable directory")
                        .arg(top_level_directory->path()));
        return false;
    }

    //
    //  OK, scratch space is all copasetic. Now, do we have somewhere
    //  writable for music files to end up?
    //
    
    QString musicdir = mfdContext->GetSetting("MusicLocation");
    if(musicdir.length() < 1)
    {
        warning("nowhere to store imported music files "
                "(MusicLocation setting is an empty string)");
        return false;
    }

    musicdir = QDir::cleanDirPath(musicdir);
    if(!musicdir.endsWith("/"));
    {
        musicdir += "/";
    }

    music_destination_directory = new QDir(musicdir);
    QFileInfo musicchecker(music_destination_directory->path());

    if(!musicchecker.isDir())
    {
        warning(QString("music destination exists (\"%1\"), but "
                        "it is not a directory")
                        .arg(music_destination_directory->path()));
        return false;
    }
    
    if(!musicchecker.isWritable())
    {
        warning(QString("music destination exists (\"%1\"), but "
                        "it is not a writeable directory")
                        .arg(music_destination_directory->path()));
        return false;
    }

    
    
    
    return true;
}

void Transcoder::run()
{
    //
    //  Set up scratch space and destination locations (for transcoded content)
    //
    
    if(!initialize())
    {
        fatal("had fatal problems setting up, exiting");
        exit();
    }

    //
    //  Register this (mtcp) service
    //

    Service *mtcp_service = new Service(
                                        QString("MythTranscoder on %1").arg(hostname),
                                        QString("mtcp"),
                                        hostname,
                                        SLT_HOST,
                                        (uint) port_number
                                       );
 
    ServiceEvent *se = new ServiceEvent( true, true, *mtcp_service);
    QApplication::postEvent(parent, se);
    delete mtcp_service;

    metadata_server = parent->getMetadataServer();

    if(!initServerSocket())
    {
        warning(QString("could not init server socker on port %1")
                .arg(port_number));
    }

    while(keep_going)
    {

        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        waitForSomethingToHappen();
        checkInternalMessages();
        checkMetadataChanges();
        checkJobs();
        updateStatusClients();
    }
    
    //
    //  Tell any jobs that are still in existence that they have to stop,
    //  then kill them all off
    //
    
    jobs_mutex.lock();
        TranscoderJob *a_job;
        for(a_job = jobs.first(); a_job; a_job = jobs.next())
        {
            a_job->stop();
            a_job->wait();
        }
        jobs.clear();
    jobs_mutex.unlock();
}

void Transcoder::handleIncoming(HttpInRequest *httpin_request, int client_id)
{

    //
    //  Get the branches of the get, figure what it's looking for
    //
    
    QString get_request = httpin_request->getUrl();
    QString top_level = get_request.section('/',1,1);

    if (top_level.lower() == "status")
    {
        handleStatusRequest(httpin_request, client_id);
    }
    else if(top_level.lower() == "startjob")
    {
        handleStartJobRequest(httpin_request);
    }
    else if(top_level.lower() == "canceljob")
    {
        handleCancelJobRequest(httpin_request);
    }
    else
    {
        HttpOutResponse *response = httpin_request->getResponse();
        response->setError(404);
    }
}

void Transcoder::handleStatusRequest(HttpInRequest *httpin_request, int client_id)
{
    status_generation_mutex.lock();
        int current_generation = status_generation;
    status_generation_mutex.unlock();

    //
    //  See what generation the client thinks it is for overall status. If
    //  they're current, hold them in a "hanging status" request.
    //
    
    QString client_generation_string = httpin_request->getHeader("Client-Generation");
    if(client_generation_string.length() < 1)
    {
        warning("client asked for /status, but did not include a Client-Generation header");
        HttpOutResponse *response = httpin_request->getResponse();
        response->setError(404);
        return;
    }

    bool ok;
    
    int client_generation = client_generation_string.toInt(&ok);
    if(!ok)
    {
        warning(QString("could not parse \"%1\" into an integer from Client-Generation header")
                        .arg(client_generation_string));
        HttpOutResponse *response = httpin_request->getResponse();
        response->setError(404);
        return;
    }
    
    if(client_generation > current_generation)
    {
        //
        //  WTF? Client is ahead of us?
        //
        warning("client has higher status generation than server (?)");
        HttpOutResponse *response = httpin_request->getResponse();
        response->setError(404);
        return;
    }
    
    if(client_generation < current_generation)
    {
        //
        //  Need to tell client about new status of jobs
        //
        
        MtcpOutput status_response;
        int generation_value = buildStatusResponse(&status_response);
        sendResponse(httpin_request->getResponse(), status_response, generation_value);
    }
    else
    {
        //
        //  They're up to speed, put 'em in hanging status 
        //
        
        hanging_status_mutex.lock();

            bool found_it = false;
            ClientTracker *ct;
            for ( ct = hanging_status_clients.first(); ct; ct = hanging_status_clients.next() )            
            {
                if(ct->getClientId() == client_id)
                {
                    ct->setGeneration(current_generation);
                    found_it = true;
                }
            }
            if(!found_it)
            {
                ClientTracker *new_ct = new ClientTracker(client_id, current_generation);
                hanging_status_clients.append(new_ct);
            }
        hanging_status_mutex.unlock();
        httpin_request->sendResponse(false);
    }    
}

int Transcoder::buildStatusResponse(MtcpOutput *status_response)
{
    //
    //  Lock the status_generation_mutex so that the status generation value
    //  is valid throughout the building of this status response
    //

    status_generation_mutex.lock();
        int return_value = status_generation;

    status_response->addServerInfoGroup();
        status_response->addServiceName(QString("mtcp server on %1").arg(hostname));
        status_response->addProtocolVersion();

        status_response->addJobInfoGroup();

        jobs_mutex.lock();
            status_response->addJobCount(jobs.count());
            TranscoderJob *a_job;
            for(a_job = jobs.first(); a_job; a_job = jobs.next())
            {
                status_response->addJobGroup();
                    status_response->addJobId(a_job->getId());
                    status_response->addJobMajorDescription(a_job->getMajorStatusDescription());
                    status_response->addJobMajorProgress(a_job->getMajorProgress());
                    status_response->addJobMinorDescription(a_job->getMinorStatusDescription());
                    status_response->addJobMinorProgress(a_job->getMinorProgress());
                status_response->endGroup();
            }
        jobs_mutex.unlock();
        status_response->endGroup();        
    status_response->endGroup();

    //
    //  Unlock the status generation value, and return the generation that
    //  was valid when this status response was constructed
    //
    
    status_generation_mutex.unlock();
    
    return return_value;

}

void Transcoder::handleStartJobRequest(HttpInRequest *httpin_request)
{
    //
    //  Client wants to launch some kind of transcoding job
    //
    
    QString job_type = httpin_request->getHeader("Job-Type");

    if(job_type == "AudioCD")
    {
        //
        //  All we know how to do thus far
        //

        QString container_id_string = httpin_request->getHeader("Container");
        bool ok;
        int container_id = container_id_string.toInt(&ok);
        if(!ok)
        {
            warning(QString("could not parse container id from \"%1\" to "
                            "start audio cd rip").arg(container_id_string));
            return;
        }
        QString playlist_id_string = httpin_request->getHeader("Playlist");
        int playlist_id = playlist_id_string.toInt(&ok);
        
        if(ok)
        {
            //
            //  launch an audio cd ripping job
            //
            
            //
            //  Get the quality and encoder settings
            //

            MfdTranscoderAudioQuality quality = MFD_TRANSCODER_AUDIO_QUALITY_LOW;
            int quality_setting = mfdContext->getNumSetting("DefaultRipQuality", 23);
            
            if(quality_setting > 3)
            {
                warning("no DefaultRipQuality defined in settings, will use low");
                quality_setting = 0;
            }
            else if(quality_setting < 0)
            {
                warning("DefaultRipQuality defined as less than 0, will use low");
                quality_setting = 0;
            }
            
            if (quality_setting == 1)
            {
                quality = MFD_TRANSCODER_AUDIO_QUALITY_MED;
            }
            else if (quality_setting == 2)
            {
                quality = MFD_TRANSCODER_AUDIO_QUALITY_HIGH;
            }
            else if (quality_setting == 3)
            {
                quality = MFD_TRANSCODER_AUDIO_QUALITY_PERFECT;
            }
            
            MfdTranscoderAudioCodec codec = MFD_TRANSCODER_AUDIO_CODEC_OGG;

            QString codec_setting = mfdContext->getSetting("EncoderType");

            if (codec_setting == "flac")
            {
                codec = MFD_TRANSCODER_AUDIO_CODEC_FLAC;
            }
            else if (codec_setting == "mp3")
            {
                codec = MFD_TRANSCODER_AUDIO_CODEC_MP3;
            }
            
            jobs_mutex.lock();
                TranscoderJob *new_job = new AudioCdJob(    
                                                        this, 
                                                        bumpJobId(),
                                                        top_level_directory->path(),
                                                        music_destination_directory->path(),
                                                        metadata_server,
                                                        container_id,
                                                        playlist_id,
                                                        codec,
                                                        quality
                                                       );
                log(QString("started new audio cd job (total now %1)").arg(jobs.count() + 1), 3);
                new_job->start();
                jobs.append(new_job);
            jobs_mutex.unlock();
            httpin_request->sendResponse(false);
        }
        else
        {
            warning(QString("could not parse playlist id from \"%1\" to "
                            "start audio cd rip").arg(playlist_id_string));
            httpin_request->getResponse()->setError(404);
        }
    }
    else
    {
        warning("unrecognized Job-Type in Transcoder::handleStartJobRequest()");
        httpin_request->getResponse()->setError(404);
    }

}

void Transcoder::handleCancelJobRequest(HttpInRequest *httpin_request)
{
    //
    //  Client wants to cancel an existing job
    //
    
    QString job_string = httpin_request->getHeader("Job");


    bool ok;
    int job_id = job_string.toInt(&ok);
    if(!ok)
    {
        warning(QString("could not parse job id from \"%1\" to "
                        "cancel job").arg(job_string));
        return;
    }

    //
    //  Find job and kill it
    //

    bool found_job = false;
    jobs_mutex.lock();
        TranscoderJob *a_job;
        for(a_job = jobs.first(); a_job; a_job = jobs.next())
        {
            if(a_job->getId() == job_id)
            {
                a_job->stop();
                a_job->wait();
                jobs.remove(a_job);
                found_job = true;
                bumpStatusGeneration();
                break;
            }
        }
    jobs_mutex.unlock();

    if(!found_job)
    {
        warning(QString("could not find a job with an id of %1 "
                        "to satisy a cancel job request")
                        .arg(job_id));
    }

    httpin_request->sendResponse(false);
}

void Transcoder::checkJobs()
{
    bool something_changed = false;
    jobs_mutex.lock();
        
        //
        //  If any jobs have stopped (done, error, whatever), then we need
        //  to delete them and up the generation counter
        //

        TranscoderJob *a_job;
        for(a_job = jobs.first(); a_job; a_job = jobs.next())
        {
            if(a_job->finished())
            {
                something_changed = true;
                log(QString("job %1 exited, removing from list (total now %2)")
                            .arg(a_job->getId())
                            .arg(jobs.count() - 1), 2);
                jobs.remove(a_job);
            }
        }
        
    jobs_mutex.unlock();
    
    if(something_changed)
    {
        bumpStatusGeneration();
    }
}

void Transcoder::updateStatusClients()
{
    //
    //  Tell every connected client with an unanswered /status request that
    //  things have changed
    //
    
    HttpOutResponse *hanging_response = new HttpOutResponse(this, NULL);
    MtcpOutput status_response;
    int generation_value = buildStatusResponse(&status_response);
    sendResponse(hanging_response, status_response, generation_value);

    status_generation_mutex.lock();
        int current_generation = status_generation;
    status_generation_mutex.unlock();


    hanging_status_mutex.lock();

        QPtrListIterator<ClientTracker> iter( hanging_status_clients );
        ClientTracker *ct = NULL;
        while( (ct = iter.current()) != NULL)
        {
            if(ct->getGeneration() < current_generation)
            {
                MFDHttpPlugin::sendResponse(ct->getClientId(), hanging_response);
                hanging_status_clients.remove(ct);
            }
            else
            {
                ++iter;
            }            
        }

    hanging_status_mutex.unlock();
    
    delete hanging_response;
}

int Transcoder::bumpStatusGeneration()
{
    int return_value = 0;

    status_generation_mutex.lock();
        status_generation++;
        return_value = status_generation;
    status_generation_mutex.unlock();

    return return_value;
}

int Transcoder::bumpJobId()
{
    int return_value = 0;
    job_id_mutex.lock();
        job_id++;
        return_value = job_id;
    job_id_mutex.unlock();
    return return_value;
}

void Transcoder::sendResponse(HttpOutResponse *httpout, MtcpOutput &response, int generation_value)
{

    //
    //  Make sure we don't have any open content code groups in what we're sending
    //
    
    if(response.openGroups())
    {
        warning("asked to send mtcp response, but there are open groups");
        httpout->setError(500);
        return;
    }

    //
    //  Tell them our status generation 
    //
    
    httpout->addHeader(QString("Server-Generation: %1").arg(generation_value));

    //
    //  Set some header stuff 
    //
    
    httpout->addHeader(
                        QString("MTCP-Server: MythTV/%1.%1 (Probably Linux)")
                            .arg(MTCP_PROTOCOL_VERSION_MAJOR)
                            .arg(MTCP_PROTOCOL_VERSION_MINOR)
                      );
                        
    //
    //  Set the payload
    //
    
    httpout->setPayload(response.getContents());
}



Transcoder::~Transcoder()
{
    if(top_level_directory)
    {
        //
        //  Try and delete it
        //
        
        if(!top_level_directory->rmdir(top_level_directory->path()))
        {
            warning(QString("during shutdown, could not remove top level "
                            "directory of %1 (you probably want to clean "
                            "it out by hand)")
                            .arg(top_level_directory->path()));
        }
        
        delete top_level_directory;
        top_level_directory = NULL;
    }

    if(music_destination_directory)
    {
        delete music_destination_directory;
        music_destination_directory = NULL;
    }
    
    hanging_status_clients.clear();
}

