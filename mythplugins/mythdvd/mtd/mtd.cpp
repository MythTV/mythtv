/*
	mtd.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the core mtd object

*/

#include <unistd.h>
#include <qstringlist.h>
#include <qregexp.h>
#include <qdir.h>
#include <qtimer.h>

#include <mythtv/util.h>
#include <mythtv/mythcontext.h>

#include "mtd.h"
#include "logging.h"

enum RIP_QUALITIES { QUALITY_ISO = -1, QUALITY_PERFECT, QUALITY_TRANSCODE };

DiscCheckingThread::DiscCheckingThread(MTD *owner,
                                       DVDProbe *probe, 
                                       QMutex *drive_access_mutex,
                                       QMutex *mutex_for_titles)
{
    //
    //  This is just a little object that asks the
    //  DVDProbe object to keep checking the drive to
    //  see if we have a disc, if the disc has changed,
    //  etc.
    //
    
    have_disc = false;
    dvd_probe = probe;
    dvd_drive_access = drive_access_mutex;
    titles_mutex = mutex_for_titles;
    parent = owner;
}

bool DiscCheckingThread::keepGoing()
{
    //
    //  When the mtd wants to finish (quit), it
    //  tells this checking thread to stop 
    //  running
    //  

    return parent->threadsShouldContinue();
} 

void DiscCheckingThread::run()
{
    //
    //  Unless the mtd wants to quit,
    //  keep checking the DVD.
    //

    while(keepGoing())
    {
        while(!dvd_drive_access->tryLock())
        {
            sleep(3);
        }
        while(!titles_mutex->tryLock())
        {
            sleep(3);
        }
        if(dvd_probe->probe())
        {
            //
            //  Yeah! We can read the DVD
            //
        
            have_disc = true;
        }
        else
        {
            have_disc = false;
        }
        titles_mutex->unlock();
        dvd_drive_access->unlock();
        sleep(1);
    }
    return;
}


/*
---------------------------------------------------------------------
*/




MTD::MTD(int port, bool log_stdout)
    :QObject()
{
    keep_running = true;
    have_disc = false;
    
    int nice_priority = gContext->GetNumSetting("MTDNiceLevel", 20);
    nice(nice_priority);
    nice_level = nice_priority;

    //
    //  Create the socket to listen to for connections
    //
    
    server_socket = new MTDServerSocket(port);
    connect(server_socket, SIGNAL(newConnect(QSocket *)),
            this, SLOT(newConnection(QSocket *)));
    connect(server_socket, SIGNAL(endConnect(QSocket *)),
            this, SLOT(endConnection(QSocket *)));
    
    //
    //  Create the logging object
    //        
    
    mtd_log = new MTDLogger(log_stdout);
    mtd_log->addStartup();
    connect(this, SIGNAL(writeToLog(const QString &)), mtd_log, SLOT(addEntry(const QString &)));
    
    //
    //  Announce in the log the port we're listening on
    //
    
    emit writeToLog(QString("mtd is listening on port %1").arg(port));

    //
    //  Setup the job threads list
    //    

    job_threads.setAutoDelete( TRUE );
    
    //
    //  Create a Mutex for locking access to the DVD drive.
    //

    dvd_drive_access = new QMutex();

    //
    //  Create a mutex for access to the titles (PtrList)
    //

    titles_mutex = new QMutex();

    //
    //  Create a mutex for protecting the variable for
    //  the number of concurrent jobs and set start up
    //  values.
    //
    
    concurrent_transcodings_mutex = new QMutex();
    concurrent_transcodings = 0;
    max_concurrent_transcodings = gContext->GetNumSetting("MTDConcurrentTranscodings", 1);

    //
    //  Create a timer to occassionally 
    //  clean out dead threads
    //
    
    thread_cleaning_timer = new QTimer();
    connect(thread_cleaning_timer, SIGNAL(timeout()), this, SLOT(cleanThreads()));
    thread_cleaning_timer->start(2000);

    //
    //  Create (but do not open) the DVD probing object
    //  and then ask a thread to check it for us. Make a
    //  timer to query whether the thread is done or not
    //
        
    QString dvd_device = gContext->GetSetting("DVDDeviceLocation");
    if(dvd_device.length() < 1)
    {
        cerr << "dvdripbox.o: Can't get a value for DVD device location. Did you run setup?" << endl;
        exit(0);
    }
    dvd_probe = new DVDProbe(dvd_device);
    disc_checking_thread = new DiscCheckingThread(this, dvd_probe, dvd_drive_access, titles_mutex);
    disc_checking_thread->start();
    disc_checking_timer = new QTimer();
    disc_checking_timer->start(1000);
    connect(disc_checking_timer, SIGNAL(timeout()), this, SLOT(checkDisc()));

}

void MTD::checkDisc()
{
    bool had_disc = have_disc;
    QString old_name = dvd_probe->getName();
    if(disc_checking_thread->haveDisc())
    {
        have_disc = true;
        if(had_disc)
        {
            if(!old_name == dvd_probe->getName())
            {
                //
                //  DVD changed
                //
                emit writeToLog(QString("DVD changed to: %1").arg(dvd_probe->getName()));
            }
        }
        else
        {
            //
            //  DVD inserted
            //
            emit writeToLog(QString("DVD inserted: %1").arg(dvd_probe->getName()));
            QPtrList<DVDTitle> *list_of_titles = dvd_probe->getTitles();
            
            //
            //  Do not use an iterator, cause 
            //  we're not mutex locking title access
            //
            
            if(list_of_titles)
            {
                for(uint i = 0; i < list_of_titles->count(); i++)
                {
                    emit writeToLog(QString("            : Title %1 is of type %2 (dvdinput table)")
                                            .arg(i+1)
                                            .arg(list_of_titles->at(i)->getInputID()));
                }
            }
        }
    }
    else
    {
        have_disc = false;
        if(had_disc)
        {
            //
            //  DVD removed
            //
            emit writeToLog("DVD removed");
        }
    }
}

void MTD::cleanThreads()
{
    //
    //  Should probably have the job threads 
    //  post an event when they are done, but
    //  this works. 
    //

    JobThread *iterator;
    for(iterator = job_threads.first(); iterator; iterator = job_threads.next() )
    {
        if(iterator->finished())
        {
            QString problem = iterator->getProblem();
            QString job_command = iterator->getJobString();
            if(problem.length() > 0)
            {   
                emit writeToLog(QString("job failed: %1").arg(job_command)); 
                //emit writeToLog(QString("    reason: %1").arg(problem));
            }
            else
            {
                emit writeToLog(QString("job finished succesfully: %1").arg(job_command)); 
            }
            
            //
            //  If this is a transcoding thread, adjust the
            //  count.
            //
            
            if(iterator->transcodeSlotUsed())
            {
                concurrent_transcodings_mutex->lock();
                concurrent_transcodings--;
                if(concurrent_transcodings < 0)
                {
                    cerr << "mtd.o: Your number of transcode jobs ended up negative. That should be impossible." << endl;
                    concurrent_transcodings = 0;
                }
                concurrent_transcodings_mutex->unlock();
            }
            
            
            job_threads.remove(iterator);
        }
    }
    
}

void MTD::newConnection(QSocket *socket)
{
    connect(socket, SIGNAL(readyRead()),
            this, SLOT(readSocket()));
            
    mtd_log->socketOpened();
}

void MTD::endConnection(QSocket *socket)
{
    socket->close();
    mtd_log->socketClosed();
}

void MTD::readSocket()
{

    QSocket *socket = (QSocket *)sender();
    if(socket->canReadLine())
    {
        QString incoming_data = socket->readLine();
        incoming_data = incoming_data.replace( QRegExp("\n"), "" );
        incoming_data = incoming_data.replace( QRegExp("\r"), "" );
        incoming_data.simplifyWhiteSpace();
        QStringList tokens = QStringList::split(" ", incoming_data);
        parseTokens(tokens, socket);
    }
}


void MTD::parseTokens(const QStringList &tokens, QSocket *socket)
{
    //
    //  parse commands coming in from the socket
    //
    
    if(tokens[0] == "halt" ||
       tokens[0] == "quit" ||
       tokens[0] == "shutdown")
    {
        shutDown();
    }
    else if(tokens[0] == "hello")
    {
        sayHi(socket);
    }
    else if(tokens[0] == "status")
    {
        sendStatusReport(socket);
    }
    else if(tokens[0] == "media")
    {
        sendMediaReport(socket);
    }
    else if(tokens[0] == "job")
    {
        startJob(tokens);
    }
    else if(tokens[0] == "abort")
    {
        startAbort(tokens);
    }
    else
    {
        QString did_not_parse = tokens.join(" ");
        emit writeToLog(QString("failed to parse this: %1").arg(did_not_parse));
    }
}


void MTD::shutDown()
{
    //
    //  Setting this flag to false
    //  will make all the threads 
    //  shutdown 
    //

    keep_running = false;

    //
    //  Stop checking the DVD
    //
    
    disc_checking_thread->wait();

    //
    //  Get rid of job threads
    //
    
    JobThread *iterator;
    for(iterator = job_threads.first(); iterator; iterator = job_threads.next() )
    {
        iterator->wait();
    }
    
    for(uint i = 0; i < job_threads.count(); i++)
    {
        job_threads.remove(i);
    }


    //
    //  Clean things up and go away.
    //
    
    mtd_log->addShutdown();
    delete mtd_log;
    
    delete server_socket;
    exit(0);
}

void MTD::sendMessage(QSocket *where, const QString &what)
{
    QString message = what;
    message.append("\n");
    // cout << "Sending : " << message.local8Bit();
    where->writeBlock(message.utf8(), message.utf8().length());
}

void MTD::sayHi(QSocket *socket)
{
    sendMessage(socket, "greetings");
}

void MTD::sendStatusReport(QSocket *socket)
{
    //
    //  Tell the client how many jobs are 
    //  in progress, and then details of
    //  each job. 
    //

    

    sendMessage(socket, QString("status dvd summary %1").arg(job_threads.count()));

    for(uint i = 0; i < job_threads.count() ; i++)
    {
        QString a_status_message = QString("status dvd job %1 overall %2 %3")
                                   .arg(i)
                                   .arg(job_threads.at(i)->getProgress())
                                   .arg(job_threads.at(i)->getJobName());
        sendMessage(socket, a_status_message);
        a_status_message = QString("status dvd job %1 subjob %2 %3")
                                   .arg(i)
                                   .arg(job_threads.at(i)->getSubProgress())
                                   .arg(job_threads.at(i)->getSubName());
        sendMessage(socket, a_status_message);
    }
    
    sendMessage(socket, "status dvd complete");
}

void MTD::sendMediaReport(QSocket *socket)
{
    //
    //  Tell the client what's on a disc
    //

    if(have_disc)
    {
        //
        //  Send number of titles and disc's name
        //

        while(!titles_mutex->tryLock())
        {
            sleep(1);
        }
        QPtrList<DVDTitle>   *dvd_titles = dvd_probe->getTitles();
        sendMessage(socket, QString("media dvd summary %1 %2").arg(dvd_titles->count()).arg(dvd_probe->getName()));

        //
        //  For each title, send:
        //      track/title number, numb chapters, numb angles, 
        //      hours, minutes, seconds, and type of disc
        //      (from the dvdinput table in the database)
        //
        
        for(uint i = 0; i < dvd_titles->count(); ++i)
        {
            sendMessage(socket, QString("media dvd title %1 %2 %3 %4 %5 %6 %7")
                        .arg(dvd_titles->at(i)->getTrack())
                        .arg(dvd_titles->at(i)->getChapters())
                        .arg(dvd_titles->at(i)->getAngles())
                        .arg(dvd_titles->at(i)->getHours())
                        .arg(dvd_titles->at(i)->getMinutes())
                        .arg(dvd_titles->at(i)->getSeconds())
                        .arg(dvd_titles->at(i)->getInputID())
                       );
            //
            //  For each track, send all the audio info
            //
            QPtrList<DVDAudio> *audio_tracks = dvd_titles->at(i)->getAudioTracks();
            for(uint j = 0; j < audio_tracks->count(); j++)
            {
                //
                //  Send title/track #, audio track #, descriptive string
                //
                
                sendMessage(socket, QString("media dvd title-audio %1 %2 %3 %4")
                            .arg(dvd_titles->at(i)->getTrack())
                            .arg(j+1)
                            .arg(audio_tracks->at(j)->getChannels())
                            .arg(audio_tracks->at(j)->getAudioString())
                           );
            }
            
            //
            //  For each subtitle, send the id number, the lang code, and the name
            //
            QPtrList<DVDSubTitle> *subtitles = dvd_titles->at(i)->getSubTitles();
            for(uint j = 0; j < subtitles->count(); j++)
            {
                sendMessage(socket, QString("media dvd title-subtitle %1 %2 %3 %4")
                            .arg(dvd_titles->at(i)->getTrack())
                            .arg(subtitles->at(j)->getID())
                            .arg(subtitles->at(j)->getLanguage())
                            .arg(subtitles->at(j)->getName())
                           );
            }
            
            
            
        }
        titles_mutex->unlock();
    }
    else
    {
        sendMessage(socket, "media dvd summary 0 No Disc");
    }    
    sendMessage(socket, "media dvd complete");
}


void MTD::startAbort(const QStringList &tokens)
{
    QString flat = tokens.join(" ");

    //
    //  Sanity check
    //
    
    if(tokens.count() < 4)
    {
        emit writeToLog(QString("bad abort request: %1").arg(flat));
        return;
    }

    if(tokens[1] != "dvd" ||
       tokens[2] != "job")
    {
        emit writeToLog(QString("I don't know how to handle this abort request: %1").arg(flat));
        return;
    }

    bool ok;
    int job_to_kill = tokens[3].toInt(&ok);
    if(!ok || job_to_kill < 0)
    {
        emit writeToLog(QString("Could not make out a job number in this abort request: %1").arg(flat));
        return;
    }    
    
    if(job_to_kill >= (int) job_threads.count())
    {
        emit writeToLog(QString("Was asked to kill a job that does not exist: %1").arg(flat));
        return;
    }

    //
    //  OK, we can probably kill this job
    //

    job_threads.at(job_to_kill)->setSubProgress(0.0, 0);
    job_threads.at(job_to_kill)->setSubName("Cancelling ...", 0);
    job_threads.at(job_to_kill)->cancelMe(true);
}

void MTD::startJob(const QStringList &tokens)
{
    //
    //  Check that the tokens make sense
    //
    
    if(tokens.count() < 2)
    {
        QString flat = tokens.join(" ");
        emit writeToLog(QString("bad job request: %1").arg(flat));
        return;
    }

    if(tokens[1] == "dvd")
    {
        startDVD(tokens);
    }
    else
    {
        emit writeToLog(QString("I don't know how to process jobs of type %1").arg(tokens[1]));
    }
    
}

void MTD::startDVD(const QStringList &tokens)
{
    QString flat = tokens.join(" ");
    bool ok;

    if(tokens.count() < 8)
    {
        emit writeToLog(QString("bad dvd job request: %1").arg(flat));
        return;
    }
    
    //
    //  Parse the tokens into "real" variables,
    //  checking for silly values as we go. If
    //  everything checks out, fire it up.
    //
    
    int dvd_title = tokens[2].toInt(&ok);
    if(dvd_title < 1 || dvd_title > 99 || !ok)
    {
        emit writeToLog(QString("bad title number in job request: %1").arg(flat));
        return;
    }
    
    int audio_track = tokens[3].toInt(&ok);
    if(audio_track < 0 || audio_track > 10 || !ok) // 10 ?? I just made that up as a boundary ??
    {
        emit writeToLog(QString("bad audio track in job request: %1").arg(flat));
        return;
    }
    
    int quality = tokens[4].toInt(&ok);
    if(quality < QUALITY_ISO || !ok)
    {
        emit writeToLog(QString("bad quality value in job request: %1").arg(flat));
        return;
    }

    bool ac3_flag = false;
    int flag_value = tokens[5].toUInt(&ok);
    if(!ok)
    {
        emit writeToLog(QString("bad ac3 flag in job request: %1").arg(flat));
        return;
    }
    if(flag_value)
    {
        ac3_flag = true;
    }

    int subtitle_track = tokens[6].toInt(&ok);
    if(!ok)
    {
        emit writeToLog(QString("bad subtitle reference in job request: %1").arg(flat));
        return;
    }

    //
    //  Parse the dir and file as one string
    //  and then break it up
    //

    QString dir_and_file = "";

    for(uint i=7; i < tokens.count(); i++)
    {
        dir_and_file += tokens[i];
        if(i != tokens.count() - 1)
        {
            dir_and_file += " ";
        }        
    }

    QDir dest_dir(dir_and_file.section("/", 0, -2));
    if(!dest_dir.exists())
    {
        emit writeToLog(QString("bad destination directory in job request: %1").arg(flat));
        return;
    }

    QString file_name = dir_and_file.section("/", -1, -1);


    QString dvd_device = gContext->GetSetting("DVDDeviceLocation");
    if(dvd_device.length() < 1)
    {
        emit writeToLog("crapity crap crap - all set to launch a dvd job and you don't have a dvd device defined");
        return;
    }

    //
    //  OK, we are ready to launch this job
    //

    if(quality == QUALITY_ISO)
    {
        QFile final_file(dest_dir.filePath(file_name));

        if(!checkFinalFile(&final_file, ".iso"))
        {
            emit writeToLog("Final file name is not useable. File exists? Other Pending job?");
            return;
        }
    
        emit writeToLog(QString("launching job: %1").arg(flat));

        //
        //  Full disc copy to an iso image.
        //
 
        JobThread *new_job = new DVDISOCopyThread(this, 
                                                  dvd_drive_access, 
                                                  dvd_device, 
                                                  dvd_title, 
                                                  final_file.name(), 
                                                  file_name,
                                                  flat,
                                                  nice_level);
        job_threads.append(new_job);
        new_job->start();
    }
    else if(quality == QUALITY_PERFECT)
    {
        QFile final_file(dest_dir.filePath(file_name));

        if(!checkFinalFile(&final_file, ".mpg"))
        {
            emit writeToLog("Final file name is not useable. File exists? Other Pending job?");
            return;
        }
    
        emit writeToLog(QString("launching job: %1").arg(flat));

        //
        //  perfect quality, straight copy via libdvdread
        //
 
        JobThread *new_job = new DVDPerfectThread(this, 
                                                  dvd_drive_access, 
                                                  dvd_device, 
                                                  dvd_title, 
                                                  final_file.name(), 
                                                  file_name,
                                                  flat,
                                                  nice_level);
        job_threads.append(new_job);
        new_job->start();
    }
    else if (quality >= QUALITY_TRANSCODE)
    {
        QFile final_file(dest_dir.filePath(file_name));

        if(!checkFinalFile(&final_file, ".avi"))
        {
            emit writeToLog("Final file name is not useable. File exists? Other Pending job?");
            return;
        }
    
        while(!titles_mutex->tryLock())
        {
            sleep(1);
        }

        DVDTitle *which_title = dvd_probe->getTitle(dvd_title);
        titles_mutex->unlock();

        if(!which_title)
        {
            cerr << "mtd.o: title number not valid? " << endl;
            return;
        }
        
        uint numb_seconds = which_title->getPlayLength();

        emit writeToLog(QString("launching job: %1").arg(flat));

        //
        //  transcoding job
        //
 
        JobThread *new_job = new DVDTranscodeThread(this, 
                                                    dvd_drive_access, 
                                                    dvd_device, 
                                                    dvd_title, 
                                                    final_file.name(), 
                                                    file_name,
                                                    flat,
                                                    nice_level,
                                                    quality,
                                                    ac3_flag,
                                                    audio_track,
                                                    numb_seconds,
                                                    subtitle_track);
        job_threads.append(new_job);
        new_job->start();
       
    }
    else
    {
        cerr << "mtd.o: Hmmmmm. Got sent a job with a negative quality parameter. That's just plain weird." << endl;
    }
}

bool MTD::checkFinalFile(QFile *final_file, const QString &extension)
{
    //
    //  Check if file exists
    //
    
    QString final_with_extension_string = final_file->name() + extension;
    QFile tester(final_with_extension_string);
    
    if(tester.exists())
    {
        return false;
    }
    
    //
    //  Check already active jobs
    //
    JobThread *iterator;
    for(iterator = job_threads.first(); iterator; iterator = job_threads.next() )
    {
        if(iterator->getFinalFileName() == final_file->name())
        {
            return false;
        }
    }
    
    return true;
}

bool MTD::isItOkToStartTranscoding()
{
    concurrent_transcodings_mutex->lock();
    if(concurrent_transcodings < max_concurrent_transcodings)
    {
        concurrent_transcodings++;
        concurrent_transcodings_mutex->unlock();
        return true;
    }
    concurrent_transcodings_mutex->unlock();
    return false;
}

void MTD::customEvent(QCustomEvent *ce)
{
    if(ce->type() == 65432)
    {
        LoggingEvent *le = (LoggingEvent*)ce;
        emit writeToLog(le->getString());
    }
    else if(ce->type() == 65431)
    {
        ErrorEvent *ee = (ErrorEvent*)ce;
        QString error_string = "Error: " + ee->getString();
        emit writeToLog(error_string);
    }
    else
    {
        cerr << "mtd.o: receiving evenets I don't understand" << endl;
    }
}

