/*
    mtd.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Methods for the core mtd object

*/

// POSIX headers
#include <unistd.h>

// C headers
#include <cstdlib>

// Qt headers
#include <QWaitCondition>
#include <QApplication>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QRegExp>
#include <QTimer>
#include <QDir>

// MythTV headers
#include <util.h>
#include <mythcontext.h>
#include <compat.h>
#include <util.h>

// MythTranscodeDaemon headers
#include "mtd.h"
#include "logging.h"

enum RIP_QUALITIES { QUALITY_ISO = -1, QUALITY_PERFECT, QUALITY_TRANSCODE };

/** \class DiscCheckingThread
 *  \brief Periodically checks if there is a disc in the DVD drive.
 *
 *  This is just a little object that asks the DVDProbe object to
 *  keep checking the drive to see if we have a disc, if the disc
 *  has changed, etc.
 */
DiscCheckingThread::DiscCheckingThread(
    DVDProbe *probe,
    QMutex   *drive_access_mutex,
    QMutex   *mutex_for_titles) :
    dvd_probe(probe), have_disc(false),
    dvd_drive_access(drive_access_mutex),
    titles_mutex(mutex_for_titles),
    cancelLock(new QMutex()),
    cancelWaitCond(new QWaitCondition()),
    cancel_me(false)
{
}

DiscCheckingThread::~DiscCheckingThread()
{
    delete cancelLock;
    delete cancelWaitCond;
}

void DiscCheckingThread::Cancel(void)
{
    QMutexLocker locker(cancelLock);
    cancel_me = true;
    cancelWaitCond->wakeAll();
}

/// \brief Checks if the MythTranscodeDaemon class has asked us to stop.
bool DiscCheckingThread::IsCancelled(void) const
{
    QMutexLocker locker(cancelLock);
    return cancel_me;
}

void DiscCheckingThread::run(void)
{
    while (!IsCancelled())
    {
        bool gota = dvd_drive_access->tryLock();
        bool gotb = titles_mutex->tryLock();

        if (gota && gotb)
            have_disc = dvd_probe->Probe();

        if (gota)
            dvd_drive_access->unlock();
        if (gotb)
            titles_mutex->unlock();

        QMutexLocker locker(cancelLock);
        if (!cancel_me)
            cancelWaitCond->wait(cancelLock, 14000);
    }
}

/*
---------------------------------------------------------------------
*/

#define LOC      QString("MTD: ")
#define LOC_WARN QString("MTD, Warning: ")
#define LOC_ERR  QString("MTD, Error: ")

MythTranscodeDaemon::MythTranscodeDaemon(int port, bool log_stdout) :
    QObject(),
    listening_port(port),
    log_to_stdout(log_stdout),
    mtd_log(NULL),
    server_socket(new QTcpServer(this)),
    dvd_drive_access(new QMutex()),
    titles_mutex(new QMutex()),
    have_disc(false),
    thread_cleaning_timer(new QTimer(this)),
    disc_checking_timer(new QTimer(this)),
    dvd_device(gCoreContext->GetSetting("DVDDeviceLocation")),
    dvd_probe(new DVDProbe(dvd_device)),
    disc_checking_thread(NULL),
    nice_level(gCoreContext->GetNumSetting("MTDNiceLevel", 20)),
    concurrent_transcodings_mutex(new QMutex()),
    concurrent_transcodings(0),
    max_concurrent_transcodings(
        gCoreContext->GetNumSetting("MTDConcurrentTranscodings", 1))
{
    myth_nice(nice_level);

    connect(server_socket, SIGNAL(newConnection()),
            this,          SLOT(  newConnection()));
    connect(thread_cleaning_timer, SIGNAL(timeout()),
            this,                  SLOT(  cleanThreads()));
    connect(disc_checking_timer,   SIGNAL(timeout()),
            this,                  SLOT(checkDisc()));

    disc_checking_thread = new DiscCheckingThread(
        dvd_probe, dvd_drive_access, titles_mutex);
    disc_checking_thread->start();
}

MythTranscodeDaemon::~MythTranscodeDaemon()
{
    disconnect();

    if (mtd_log)
    {
        mtd_log->addShutdown();
        mtd_log->deleteLater();
        mtd_log = NULL;
    }

    // server_socket is auto deleted by Qt
    // thread_cleaning_timer is auto deleted by Qt
    thread_cleaning_timer->disconnect();
    // disc_checking_timer is auto deleted by Qt
    disc_checking_timer->disconnect();

    if (disc_checking_thread)
    {
        disc_checking_thread->Cancel();
        disc_checking_thread->wait();
        disc_checking_thread->deleteLater();
        disc_checking_thread = NULL;
    }

    if (dvd_drive_access)
    {
        delete dvd_drive_access;
        dvd_drive_access = NULL;
    }

    if (titles_mutex)
    {
        delete titles_mutex;
        titles_mutex = NULL;
    }

    if (concurrent_transcodings_mutex)
    {
        delete concurrent_transcodings_mutex;
        concurrent_transcodings_mutex = NULL;
    }
}

bool MythTranscodeDaemon::Init(void)
{
    // Start listening on the server socket
    if (!server_socket->listen(QHostAddress::Any, listening_port))
    {
         VERBOSE(VB_IMPORTANT, LOC_ERR +
                 QString("Can't bind to server port %1").arg(listening_port) +
                 "\n\t\t\tThere is probably copy of mtd already running."
                 "\n\t\t\tYou can verify this by running 'ps ax | grep mtd'.");
         return false;
    }

    //  Create the logging object
    mtd_log = new MTDLogger(log_to_stdout);
    if (!mtd_log->Init())
        return false;
    mtd_log->addStartup();
    connect(this,    SIGNAL(writeToLog(const QString&)),
            mtd_log, SLOT(  addEntry(  const QString&)));

    // Announce in the log the port we're listening on
    emit writeToLog(QString("mtd is listening on port %1")
                    .arg(listening_port));

    // Start timers
    thread_cleaning_timer->start(2000);
    disc_checking_timer->start(1000);

    return true;
}

void MythTranscodeDaemon::checkDisc()
{
    // forgetDVD() can delete dvd_probe
    if (!dvd_probe)
        return;

    bool had_disc = have_disc;
    QString old_name = dvd_probe->GetName();
    if (disc_checking_thread->IsDiscPresent())
    {
        have_disc = true;
        if (had_disc)
        {
            if (old_name != dvd_probe->GetName())
            {
                //
                //  DVD changed
                //
                emit writeToLog(QString("DVD changed to: %1")
                                .arg(dvd_probe->GetName()));
            }
        }
        else
        {
            //
            //  DVD inserted
            //
            emit writeToLog(QString("DVD inserted: %1")
                            .arg(dvd_probe->GetName()));

            DVDTitleList list_of_titles = dvd_probe->GetTitles();
            for (uint i = 0; i < (uint)list_of_titles.size(); i++)
            {
                emit writeToLog(
                    QString("            : Title %1 is of "
                            "type %2 (dvdinput table)")
                    .arg(i+1)
                    .arg(list_of_titles[i].GetInputID()));
            }
        }
    }
    else
    {
        have_disc = false;
        if (had_disc)
        {
            //
            //  DVD removed
            //
            emit writeToLog("DVD removed");
        }
    }
}

void MythTranscodeDaemon::cleanThreads()
{
    //
    //  Should probably have the job threads
    //  post an event when they are done, but
    //  this works.
    //

    JobThreadList::iterator it = job_threads.begin();
    for (; it != job_threads.end(); ++it)
    {
        if (!(*it)->isFinished())
            continue;

        QString problem = (*it)->GetLastProblem();
        QString job_command = (*it)->GetJobString();
        if (problem.length() > 0)
        {
            emit writeToLog(QString("job failed: %1").arg(job_command));
            //emit writeToLog(QString("    reason: %1").arg(problem));
        }
        else
        {
            emit writeToLog(QString("job finished successfully: %1")
                            .arg(job_command));
        }

        //
        //  If this is a transcoding thread, adjust the
        //  count.
        //

        if ((*it)->transcodeSlotUsed())
        {
            QMutexLocker locker(concurrent_transcodings_mutex);
            concurrent_transcodings--;
            if (concurrent_transcodings < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "One of the transcode jobs died very early.\n\t\t\t"
                        "There may or may not be anything informative in "
                        "the log.");
                concurrent_transcodings = 0;
            }
        }

        (*it)->deleteLater();
        job_threads.erase(it);
    }

}

void MythTranscodeDaemon::newConnection(void)
{
    QTcpSocket *socket = server_socket->nextPendingConnection();
    while (socket)
    {
        connect(socket, SIGNAL(readyRead()),
                this,   SLOT(  readSocket()));
        connect(socket, SIGNAL(disconnected()),
                this,   SLOT(  endConnection()));
        mtd_log->socketOpened();

        socket = server_socket->nextPendingConnection();
    }
}

void MythTranscodeDaemon::endConnection(void)
{
    QTcpSocket *socket = dynamic_cast<QTcpSocket*>(sender());
    if (socket)
    {
        socket->close();
        socket->deleteLater();
        mtd_log->socketClosed();
    }
}

void MythTranscodeDaemon::readSocket(void)
{
    QTcpSocket *socket = dynamic_cast<QTcpSocket*>(sender());
    if (socket && socket->canReadLine())
    {
        QString incoming_data = socket->readLine();
        incoming_data = incoming_data.replace( QRegExp("\n"), "" );
        incoming_data = incoming_data.replace( QRegExp("\r"), "" );
        incoming_data.simplified();
        QStringList tokens = incoming_data.split(" ");
        parseTokens(tokens, socket);
    }
}

void MythTranscodeDaemon::parseTokens(const QStringList &tokens,
                                      QTcpSocket *socket)
{
    //
    //  parse commands coming in from the socket
    //

    if (tokens[0] == "halt" ||
       tokens[0] == "quit" ||
       tokens[0] == "shutdown")
    {
        ShutDownThreads();
        // The rest will be handled when the class is deleted..
        QApplication::exit(0);
    }
    else if (tokens[0] == "hello")
    {
        sayHi(socket);
    }
    else if (tokens[0] == "status")
    {
        sendStatusReport(socket);
    }
    else if (tokens[0] == "media")
    {
        sendMediaReport(socket);
    }
    else if (tokens[0] == "job")
    {
        startJob(tokens);
    }
    else if (tokens[0] == "abort")
    {
        startAbort(tokens);
    }
    else if (tokens[0] == "use")
    {
        useDrive(tokens);
    }
    else if (tokens[0] == "no")
    {
        noDrive(tokens);
    }
    else
    {
        QString did_not_parse = tokens.join(" ");
        emit writeToLog(QString("failed to parse this: %1").arg(did_not_parse));
    }
}

void MythTranscodeDaemon::ShutDownThreads(void)
{
    // Tell job threads to exit soon...
    JobThreadList::iterator it = job_threads.begin();
    for (; it != job_threads.end(); ++it)
        (*it)->Cancel();

    // Tell disc checking thread to exit soon...
    if (disc_checking_thread)
        disc_checking_thread->Cancel();

    // Wait for job threads to actually exit, then delete
    while (!job_threads.empty())
    {
        job_threads.back()->wait();
        job_threads.back()->deleteLater();
        job_threads.pop_back();
    }

    // Wait for disc checking thread to actually exit, then delete
    if (disc_checking_thread)
    {
        disc_checking_thread->wait();
        disc_checking_thread->deleteLater();
        disc_checking_thread = NULL;
    }
}

void MythTranscodeDaemon::sendMessage(QTcpSocket *where, const QString &what)
{
    QString message = what;
    message.append("\n");
    QByteArray buf = message.toUtf8();
    where->write(buf);
}

void MythTranscodeDaemon::sayHi(QTcpSocket *socket)
{
    sendMessage(socket, "greetings");
}

void MythTranscodeDaemon::sendStatusReport(QTcpSocket *socket)
{
    //
    //  Tell the client how many jobs are
    //  in progress, and then details of
    //  each job.
    //

    sendMessage(socket, QString("status dvd summary %1")
                .arg(job_threads.size()));

    for (uint i = 0; i < (uint)job_threads.size() ; i++)
    {
        QString a_status_message = QString("status dvd job %1 overall %2 %3")
                                   .arg(i)
                                   .arg(job_threads[i]->GetProgress())
                                   .arg(job_threads[i]->GetJobName());
        sendMessage(socket, a_status_message);
        a_status_message = QString("status dvd job %1 subjob %2 %3")
                                   .arg(i)
                                   .arg(job_threads[i]->GetSubProgress())
                                   .arg(job_threads[i]->GetSubName());
        sendMessage(socket, a_status_message);
    }

    sendMessage(socket, "status dvd complete");
}

void MythTranscodeDaemon::sendMediaReport(QTcpSocket *socket)
{
    //
    //  Tell the client what's on a disc
    //

    if (have_disc)
    {
        //
        //  Send number of titles and disc's name
        //

        QMutexLocker locker(titles_mutex);

        DVDTitleList dvd_titles = dvd_probe->GetTitles();
        sendMessage(socket, QString("media dvd summary %1 %2")
                    .arg(dvd_titles.size()).arg(dvd_probe->GetName()));

        //
        //  For each title, send:
        //      track/title number, numb chapters, numb angles,
        //      hours, minutes, seconds, and type of disc
        //      (from the dvdinput table in the database)
        //

        for (uint i = 0; i < (uint)dvd_titles.size(); i++)
        {
            sendMessage(socket, QString("media dvd title %1 %2 %3 %4 %5 %6 %7")
                        .arg(dvd_titles[i].GetTrack())
                        .arg(dvd_titles[i].GetChapters())
                        .arg(dvd_titles[i].GetAngles())
                        .arg(dvd_titles[i].GetHours())
                        .arg(dvd_titles[i].GetMinutes())
                        .arg(dvd_titles[i].GetSeconds())
                        .arg(dvd_titles[i].GetInputID())
                       );
            //
            //  For each track, send all the audio info
            //
            DVDAudioList audio_tracks = dvd_titles[i].GetAudioTracks();
            for (uint j = 0; j < (uint)audio_tracks.size(); j++)
            {
                //
                //  Send title/track #, audio track #, descriptive string
                //

                sendMessage(socket, QString("media dvd title-audio %1 %2 %3 %4")
                            .arg(dvd_titles[i].GetTrack())
                            .arg(j+1)
                            .arg(audio_tracks[j].GetChannels())
                            .arg(audio_tracks[j].GetAudioString())
                           );
            }

            //
            //  For each subtitle, send the id number, the lang code, and the name
            //
            DVDSubTitleList subtitles = dvd_titles[i].GetSubTitles();
            for (uint j = 0; j < (uint)subtitles.size(); j++)
            {
                sendMessage(socket,
                            QString("media dvd title-subtitle %1 %2 %3 %4")
                            .arg(dvd_titles[i].GetTrack())
                            .arg(subtitles[j].GetID())
                            .arg(subtitles[j].GetLanguage())
                            .arg(subtitles[j].GetName())
                            );
            }



        }
    }
    else
    {
        sendMessage(socket, "media dvd summary 0 No Disc");
    }
    sendMessage(socket, "media dvd complete");
}

void MythTranscodeDaemon::startAbort(const QStringList &tokens)
{
    QString flat = tokens.join(" ");

    //
    //  Sanity check
    //

    if (tokens.size() < 4)
    {
        emit writeToLog(QString("bad abort request: %1").arg(flat));
        return;
    }

    if (tokens[1] != "dvd" ||
       tokens[2] != "job")
    {
        emit writeToLog(QString("I don't know how to handle this abort request: %1").arg(flat));
        return;
    }

    bool ok;
    int job_to_kill = tokens[3].toInt(&ok);
    if (!ok || job_to_kill < 0)
    {
        emit writeToLog(QString("Could not make out a job number in this abort request: %1").arg(flat));
        return;
    }

    if (job_to_kill >= (int) job_threads.size())
    {
        emit writeToLog(QString("Was asked to kill a job that does not exist: %1").arg(flat));
        return;
    }

    // OK, we can probably kill this job
    job_threads[job_to_kill]->Cancel(true);
}

void MythTranscodeDaemon::startJob(const QStringList &tokens)
{
    //
    //  Check that the tokens make sense
    //

    if (tokens.size() < 2)
    {
        QString flat = tokens.join(" ");
        emit writeToLog(QString("bad job request: %1").arg(flat));
        return;
    }

    if (tokens[1] == "dvd")
    {
        startDVD(tokens);
    }
    else
    {
        emit writeToLog(QString("I don't know how to process jobs of type %1").arg(tokens[1]));
    }

}

void MythTranscodeDaemon::startDVD(const QStringList &tokens)
{
    QString flat = tokens.join(" ");
    bool ok;

    if (tokens.size() < 8)
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
    if (dvd_title < 1 || dvd_title > 99 || !ok)
    {
        emit writeToLog(QString("bad title number in job request: %1")
                        .arg(flat));
        return;
    }

    int audio_track = tokens[3].toInt(&ok);
    if (audio_track < 0 || audio_track > 10 || !ok)
    {
        // 10 ?? I just made that up as a boundary ??
        emit writeToLog(QString("bad audio track in job request: %1")
                        .arg(flat));
        return;
    }

    int quality = tokens[4].toInt(&ok);
    if (quality < QUALITY_ISO || !ok)
    {
        emit writeToLog(QString("bad quality value in job request: %1")
                        .arg(flat));
        return;
    }

    bool ac3_flag = false;
    int flag_value = tokens[5].toUInt(&ok);
    if (!ok)
    {
        emit writeToLog(QString("bad ac3 flag in job request: %1").arg(flat));
        return;
    }
    if (flag_value)
    {
        ac3_flag = true;
    }

    int subtitle_track = tokens[6].toInt(&ok);
    if (!ok)
    {
        emit writeToLog(QString("bad subtitle reference "
                                "in job request: %1").arg(flat));
        return;
    }

    //
    //  Parse the dir and file as one string
    //  and then break it up
    //

    QString dir_and_file = "";

    for (QStringList::size_type i = 7; i < tokens.size(); i++)
    {
        dir_and_file += tokens[i];
        if (i != tokens.size() - 1)
        {
            dir_and_file += " ";
        }
    }

    QDir dest_dir(dir_and_file.section("/", 0, -2));
    if (!dest_dir.exists())
    {
        emit writeToLog(QString("bad destination directory "
                                "in job request: '%1'").arg(flat));
        return;
    }

    QString file_name = dir_and_file.section("/", -1, -1);

    if (dvd_device.length() < 1)
    {
        emit writeToLog("Can not launch DVD job until a dvd device is defined");
        return;
    }

    //
    //  OK, we are ready to launch this job
    //

    if (quality == QUALITY_ISO)
    {
        QFile final_file(dest_dir.filePath(file_name));

        if (!checkFinalFile(&final_file, ".iso"))
        {
            emit writeToLog(
                QString("Final file name '%1' is not useable. "
                        "File exists? Other Pending job?")
                .arg(final_file.fileName()));
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
                                                  final_file.fileName(),
                                                  file_name,
                                                  flat,
                                                  nice_level);
        job_threads.push_back(new_job);
        new_job->start();
    }
    else if (quality == QUALITY_PERFECT)
    {
        QFile final_file(dest_dir.filePath(file_name));

        if (!checkFinalFile(&final_file, ".mpg"))
        {
            emit writeToLog(
                QString("Final file name '%1' is not useable. "
                        "File exists? Other Pending job?")
                .arg(final_file.fileName()));
            return;
        }

        emit writeToLog(QString("launching job: %1").arg(flat));

        //
        //  perfect quality, straight copy via libdvdread
        //

        JobThread *new_job = new DVDPerfectThread(
            this, dvd_drive_access, dvd_device, dvd_title,
            final_file.fileName(), file_name,
            flat, nice_level);

        job_threads.push_back(new_job);
        new_job->start();
    }
    else if (quality >= QUALITY_TRANSCODE)
    {
        QFile final_file(dest_dir.filePath(file_name));

        if (!checkFinalFile(&final_file, ".avi"))
        {
            emit writeToLog(
                QString("Final file name '%1' is not useable. "
                        "File exists? Other Pending job?")
                .arg(final_file.fileName()));
            return;
        }

        titles_mutex->lock();
        DVDTitle which_title = dvd_probe->GetTitle(dvd_title);
        titles_mutex->unlock();

        if (!which_title.IsValid())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Title number not valid?");
            return;
        }

        uint numb_seconds = which_title.GetPlayLength();

        emit writeToLog(QString("launching job: %1").arg(flat));

        //
        //  transcoding job
        //

        JobThread *new_job = new DVDTranscodeThread(
            this, dvd_drive_access, dvd_device, dvd_title,
            final_file.fileName(), file_name,
            flat, nice_level, quality, ac3_flag, audio_track,
            numb_seconds, subtitle_track);

        job_threads.push_back(new_job);
        new_job->start();

    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Invalid transcode quality parameter, ignoring job");
    }
}

/**
 * Remember the supplied device path
 */
void MythTranscodeDaemon::useDrive(const QStringList &tokens)
{
    if (tokens.size() != 3)
    {
        emit writeToLog("Bad usedrive request: " + tokens.join(" "));
        return;
    }

    if (tokens[1] == "dvd")
        useDVD(tokens);
    else
        emit writeToLog("I don't know how to use drives of type: " + tokens[1]);
}

/**
 * Remember the supplied DVD device path, and create a DVDProbe & thread for it
 */
void MythTranscodeDaemon::useDVD(const QStringList &tokens)
{
    if (dvd_device == tokens[2])  // Same drive? Nothing to do.
        return;

    DVDProbe *newDrive = new DVDProbe(tokens[2]);
    if (!newDrive->Probe())
    {
        emit writeToLog("Drive not available: " + tokens[2]);
        return;
    }

    forgetDVD();

    dvd_device = tokens[2];
    dvd_probe  = newDrive;

    disc_checking_thread = new DiscCheckingThread(dvd_probe,
                                                  dvd_drive_access,
                                                  titles_mutex);
    disc_checking_thread->start();

    have_disc  = true;
}

/**
 * Stop using the specified, or current, drive. Also stop any related jobs
 */
void MythTranscodeDaemon::noDrive(const QStringList &tokens)
{
    if (tokens.size() < 2 || tokens.size() > 3)
    {
        emit writeToLog("Bad no drive request: " + tokens.join(" "));
        return;
    }

    QString device;

    if (tokens[1] == "dvd")
        device = noDVD(tokens);
    else
    {
        emit writeToLog("I don't know how to forget drives of type: "
                        + tokens[1]);
        return;
    }

    JobThreadList::iterator it = job_threads.begin();
    for (; it != job_threads.end(); ++it)
    {
        if ((*it)->usesDevice(device))
        {
            (*it)->Cancel();

            emit writeToLog("Waiting for job : " + (*it)->GetJobName());
            (*it)->wait();

            (*it)->deleteLater();
            job_threads.erase(it);
        }
    }
}

/**
 * Stop using the specified, or current, dvd drive
 */
QString MythTranscodeDaemon::noDVD(const QStringList &tokens)
{
    QString device;

    if (tokens.size() == 2)
        device = dvd_device;
    else
        device = tokens[2];

    // In multi-drive setups, the user could be ripping several.
    // The drive we are forgetting about may not be the current one:
    if (device == dvd_device)
        forgetDVD();

    return device;
}

/**
 * Clear the current DVD device path, disc checking thread and DVDProbe object
 */
void MythTranscodeDaemon::forgetDVD(void)
{
    if (disc_checking_thread)
    {
        disc_checking_thread->Cancel();
        disc_checking_thread->wait();
        disc_checking_thread->deleteLater();
        disc_checking_thread = NULL;
    }

    QMutexLocker locker(dvd_drive_access);
    if (dvd_probe)
    {
        delete dvd_probe;
        dvd_probe = NULL;
    }

    dvd_device = "";     // Prevent new jobs
    have_disc  = false;  // For sendMediaReport()
}

bool MythTranscodeDaemon::checkFinalFile(
    QFile *final_file, const QString &extension)
{
    //
    //  Check if file exists
    //

    QString final_with_extension_string = final_file->fileName() + extension;
    QFile tester(final_with_extension_string);

    if (tester.exists())
    {
        return false;
    }

    //
    //  Check already active jobs
    //
    JobThreadList::iterator it = job_threads.begin();
    for (; it != job_threads.end(); ++it)
    {
        if ((*it)->GetFinalFileName() == final_file->fileName())
            return false;
    }

    return true;
}

bool MythTranscodeDaemon::IncrConcurrentTranscodeCounter(void)
{
    QMutexLocker locker(concurrent_transcodings_mutex);
    if (concurrent_transcodings < max_concurrent_transcodings)
    {
        concurrent_transcodings++;
        return true;
    }
    return false;
}

void MythTranscodeDaemon::customEvent(QEvent *ce)
{
    if (ce->type() == LoggingEvent::kEventType)
    {
        LoggingEvent *le = (LoggingEvent*)ce;
        emit writeToLog(le->getString());
    }
    else if (ce->type() == ErrorEvent::kEventType)
    {
        ErrorEvent *ee = (ErrorEvent*)ce;
        QString error_string = "Error: " + ee->getString();
        emit writeToLog(error_string);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unrecognized event");
    }
}

