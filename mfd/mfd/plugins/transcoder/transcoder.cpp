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

Transcoder *transcoder = NULL;


Transcoder::Transcoder(MFD *owner, int identity)
      :MFDHttpPlugin(owner, identity, mfdContext->getNumSetting("mfd_transcoder_port"), "transcoder", 2)
{

    status_generation = 1;
    top_level_directory = NULL;
    music_destination_directory = NULL;
    jobs.setAutoDelete(true);
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

void Transcoder::handleIncoming(HttpInRequest *httpin_request, int)
{

    //
    //  Get the branches of the get, figure what it's looking for
    //
    
    QString get_request = httpin_request->getUrl();
    QString top_level = get_request.section('/',1,1);

    if (top_level == "status")
    {
        handleStatusRequest(httpin_request);
    }
    else if(top_level == "startjob")
    {
        handleStartJobRequest(httpin_request);
    }
    else
    {
        HttpOutResponse *response = httpin_request->getResponse();
        response->setError(404);
    }
}

void Transcoder::handleStatusRequest(HttpInRequest *httpin_request)
{
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
    
    if(client_generation > status_generation)
    {
        //
        //  WTF? Client is ahead of us?
        //
        warning("client has higher status generation than server (?)");
        HttpOutResponse *response = httpin_request->getResponse();
        response->setError(404);
        return;
    }
    
    if(client_generation < status_generation)
    {
        //
        //  Need to tell client about new status of jobs
        //
    }
    else
    {
        //
        //  They're up to speed, put 'em in hanging status 
        //
    }    
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
        if(ok)
        {
            //
            //  launch an audio cd ripping job
            //
            
            
            jobs_mutex.lock();
                TranscoderJob *new_job = new AudioCdJob(    
                                                        this, 
                                                        bumpStatusGeneration(),
                                                        top_level_directory->path(),
                                                        music_destination_directory->path(),
                                                        metadata_server,
                                                        container_id,
                                                        MFD_TRANSCODER_AUDIO_CODEC_OGG,
                                                        MFD_TRANSCODER_AUDIO_QUALITY_HIGH
                                                       );
                log(QString("started new audio cd job (total now %1)").arg(jobs.count() + 1), 3);
                new_job->start();
                jobs.append(new_job);
            jobs_mutex.unlock();
            updateStatusClients();
            
        }
        else
        {
            warning(QString("could not parse container id from \"%1\" to "
                            "start audio cd rip").arg(container_id_string));
        }
    }
    else
    {
        warning("unrecognized Job-Type in Transcoder::handleStartJobRequest()");
    }

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
        updateStatusClients();
    }
}

void Transcoder::updateStatusClients()
{
    //
    //  Tell every connected client with an unanswered /status request that
    //  things have changed
    //
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
}

