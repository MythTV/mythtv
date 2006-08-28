/*
	dvdripbox.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    implementation of the main interface
*/
#include <qapplication.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
using namespace std;

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>

#include "dvdripbox.h"
#include "titledialog.h"

MTDJob::MTDJob()
       :QObject(NULL)
{
    init();
}

MTDJob::MTDJob(const QString &a_name)
{
    init();
    job_name = a_name;
}

void MTDJob::init()
{
    job_number = -1;
    job_name = "";
    current_activity = "";
    overall_progress = 0.0;
    subjob_progress = 0.0;
    cancelled = false;
}

void MTDJob::setName(const QString &a_name)
{
    if(a_name != job_name && cancelled)
    {
        cancelled = false;
        emit toggledCancelled();
    }
    job_name = a_name;
}

void MTDJob::setActivity(const QString &an_act)
{
    if(!cancelled)
    {
        current_activity = an_act;
    }
}

void MTDJob::setSubjob(double a_number)
{
    if(!cancelled)
    {
        subjob_progress = a_number;
    }
}

/*
---------------------------------------------------------------------
*/


DVDRipBox::DVDRipBox(MythMainWindow *parent, QString window_name,
                     QString theme_filename, const char *name)

           : MythThemedDialog(parent, window_name, theme_filename, name)
{
    //
    //  A DVDRipBox is a single dialog that does a bunch of things
    //  depending on the MythThemedDialog "context"
    //
    //  These contexts are:
    //
    //          0 - started up, don't know if we can (or can't talk to the mtd)
    //          1 - can't connect, error messages
    //          2 - can connect, but nothing going on
    //          3 - browse through transcoding jobs currently running 
    //          4 - browse through a DVD and (possibly) launch a transcoding job
    //              (^^ this is "temporarily" launching a new dialog)
    //

    //
    //  Set some startup defaults
    //
    
    client_socket = NULL;
    tried_mtd = false;
    connected = false;
    jobs.clear();
    jobs.setAutoDelete(true);
    numb_jobs = 0;
    current_job = -1;
    first_time_through = true;
    have_disc = false;
    first_disc_found = false;
    block_media_requests = false;
    ignore_cancels = false;
                    
    //
    //  Set up the timer which kicks off polling
    //  the mtd for status information
    //
    
    status_timer = new QTimer(this);
    connect(status_timer, SIGNAL(timeout()), this, SLOT(pollStatus()));
        
    //
    //  Wire up the theme 
    //  (connect operational widgets to whatever the theme ui.xml
    //  file says is supposed to be on this dialog)
    //
    
    wireUpTheme();

    //
    //  Set our initial context and try to connect
    //

    setContext(0);
    createSocket();
    connectToMtd(false);

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
    dvd_info = NULL;
    disc_checking_timer = new QTimer();
    disc_checking_timer->start(600);
    connect(disc_checking_timer, SIGNAL(timeout()), this, SLOT(checkDisc()));
}

void DVDRipBox::checkDisc()
{
    if(!connected)
    {
        return;
    }
    
    if(block_media_requests)
    {
        return;
    }

    if(have_disc)
    {
        if(ripscreen_button)
        {
            if(ripscreen_button->GetContext() != -1)
            {
                ripscreen_button->SetContext(-1);
                ripscreen_button->refresh();
            }    
        }
        if(!first_disc_found)
        {
            first_disc_found = true;

            //
            //  If we got the first disc, the user
            //  probably has what they want.
            //

            disc_checking_timer->changeInterval(4000);
        }
        
    }
    else
    {
        if(ripscreen_button)
        {
            if(ripscreen_button->GetContext() != -2)
            {
                ripscreen_button->SetContext(-2);
                ripscreen_button->refresh();
            }
        }
    }

    //
    //  Ask the mtd to send us info about 
    //  current media (DVD) information
    //
    sendToServer("media");
}

void DVDRipBox::createSocket()
{
    if(client_socket)
    {
        delete client_socket;
    }
    client_socket = new QSocket(this);
    connect(client_socket, SIGNAL(error(int)), this, SLOT(connectionError(int)));
    connect(client_socket, SIGNAL(connected()), this, SLOT(connectionMade()));
    connect(client_socket, SIGNAL(readyRead()), this, SLOT(readFromServer()));
    connect(client_socket, SIGNAL(connectionClosed()), this, SLOT(connectionClosed()));
}

void DVDRipBox::connectionClosed()
{
    if(client_socket)
    {
        delete client_socket;
        client_socket = NULL;
        connected = false;
    }

    stopStatusPolling();
    setContext(0);
    have_disc = false;
    if(ripscreen_button)
    {
        ripscreen_button->SetContext(-2);
        ripscreen_button->refresh();
    }
    if(cancel_button)
    {
        cancel_button->SetContext(-2);
        cancel_button->refresh();
    }
    QString warning = tr("Your connection to the Myth "
                         "Transcoding Daemon has gone "
                         "away. This is not a good thing.");
    warning_text->SetText(warning);
    update();
}

void DVDRipBox::connectToMtd(bool try_to_run_mtd)
{
    if(try_to_run_mtd && !tried_mtd)
    {
        //
        //  it should daemonize itself and then return
        //
        system(QString("%1/bin/mtd -d").arg(gContext->GetInstallPrefix()));

        //
        //  but we need to wait a wee bit for the
        //  socket to open up
        //
        usleep(200000);
        tried_mtd = true;
    }

    int a_port = gContext->GetNumSetting("MTDPort", 2442);
    if(a_port > 0 && a_port < 65536)
    {
        client_socket->connectToHost("localhost", a_port);
    }
    else
    {
        cerr << "dvdripbox.o: Can't get a reasonable port number" << endl;
        exit(0);
    }
}

void DVDRipBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("DVD", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;
    
        if (getContext() == 1)
        {
            if (action == "0" || action == "1" || action == "2" ||
                action == "3" || action == "4" || action == "5" ||
                action == "6" || action == "7" || action == "8" ||
                action == "9")
            {
                connectToMtd(true);
            }
            else
                handled = false;
        }    
        else if (getContext() == 2 && have_disc)
        {
            if (action == "0")
            {
                if (ripscreen_button && ripscreen_button->GetContext() == -1)
                    ripscreen_button->push();
            }
            else
                handled = false;
        }
        else if (getContext() == 3)
        {
            if (action == "RIGHT")
            {
                if (nextjob_button)
                    nextjob_button->push();
            }
            else if (action == "LEFT")
            {
                if (prevjob_button)
                    prevjob_button->push();
            }
            else if (action == "0")
            {
                if (ripscreen_button && ripscreen_button->GetContext() != -2)
                    ripscreen_button->push();
            }
            else if (action == "9")
            {
                if (cancel_button)
                    cancel_button->push();
            }
            else if (action == "1" || action == "2" || action == "3" ||
                     action == "4" || action == "5" || action == "6" ||
                     action == "7" || action == "8")
            {
                goToJob(action.toInt());
            }
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void DVDRipBox::nextJob()
{
    if(current_job + 1 < (int) numb_jobs)
    {
        current_job++;
    }
    showCurrentJob();
}


void DVDRipBox::prevJob()
{
    if(current_job > 0)
    {
        current_job--;
    }
    showCurrentJob();
}

void DVDRipBox::goToJob(int which_job)
{
    which_job--;
    if(which_job > -1 &&
       which_job < (int) numb_jobs)
    {
        current_job = which_job;
        showCurrentJob();
    }
}


void DVDRipBox::connectionError(int error_id)
{
    //
    //  Close and recycle the socket object
    //  (in case the user wants to try again)
    //

    createSocket();
    setContext(1);
    //
    //  Can't connect. User probably hasn't run mtd
    //
    if(error_id == QSocket::ErrConnectionRefused)
    {
        QString warning = tr("Cannot connect to your Myth "
                             "Transcoding Daemon. You can try "
                             "hitting any number key to start "
                             "it. If you still see this message, "
                             "then something is really wrong.");
        warning_text->SetText(warning);
    }
    else if(error_id == QSocket::ErrHostNotFound)
    {
        QString warning = tr("Attempting to connect to your "
                             "mtd said host not found. This "
                             "is unrecoverably bad. ");
        warning_text->SetText(warning);
    }
    else if(error_id == QSocket::ErrSocketRead)
    {
        QString warning = tr("Socket communication errors. "
                             "This is unrecoverably bad. ");
        warning_text->SetText(warning);
    }
    else
    {
        QString warning = tr("Something is wrong, but I don't "
                             "know what.");
        warning_text->SetText(warning);
        
    }
}

void DVDRipBox::connectionMade()
{
    //
    //  All is well and good
    //

    setContext(2);
    connected = true;
    sendToServer("hello");
}

void DVDRipBox::readFromServer()
{
    while(client_socket->canReadLine())
    {
        QString line_from_server = QString::fromUtf8(client_socket->readLine());
        line_from_server = line_from_server.replace( QRegExp("\n"), "" );
        line_from_server = line_from_server.replace( QRegExp("\r"), "" );
        line_from_server.simplifyWhiteSpace();
        // cout << "Getting \"" << line_from_server.local8Bit() << "\"" << endl ;
        QStringList tokens = QStringList::split(" ", line_from_server);
        if(tokens.count() > 0)
        {
            parseTokens(tokens);
        }
    }
}

void DVDRipBox::sendToServer(const QString &some_text)
{
    if(connected)
    {
        QTextStream os(client_socket);
        os << some_text << "\n";
    }
    else
    {
        cerr << "dvdripbox.o: was asked to send the following text while not connected: \"" 
             << some_text
             << "\"" 
             << endl;
    }
}

void DVDRipBox::parseTokens(QStringList tokens)
{
    if(tokens[0] == "greetings")
    {
        startStatusPolling();
    }
    if(tokens[0] == "status")
    {
        handleStatus(tokens);
    }
    if(tokens[0] == "media")
    {
        handleMedia(tokens);
    }
}

void DVDRipBox::startStatusPolling()
{
    status_timer->start(1000);
}

void DVDRipBox::stopStatusPolling()
{
    status_timer->stop();
}

void DVDRipBox::pollStatus()
{
    //
    //  Ask the server to send us more data
    //  

    sendToServer("status");
    
}

void DVDRipBox::showCurrentJob()
{
    if(current_job > -1)
    {
        MTDJob *a_job = jobs.at((uint)current_job);
        if(overall_text)
        {
            overall_text->SetText(a_job->getName());
        }
        if(job_text)
        {
            job_text->SetText(a_job->getActivity());
        }
        if(overall_status)
        {
            int an_int = (int) (a_job->getOverall() * 1000);
            overall_status->SetUsed(an_int);
        }
        if(job_status)
        {
            int an_int = (int) (a_job->getSubjob() * 1000);
            job_status->SetUsed(an_int);
        }
        if(numb_jobs_text)
        {
            numb_jobs_text->SetText(QString(tr("Job %1 of %2")).arg(current_job + 1).arg(numb_jobs));
        }
    }
    else
    {
        /// hmmmm
    }
}

void DVDRipBox::handleStatus(QStringList tokens)
{
    //
    //  Initial sanity checking
    //
    
    if(tokens.count() < 3)
    {
        cerr << "dvdripbox.o: I got an mtd status update with a bad number of tokens" << endl;
        return;
    }

    if(tokens[1] != "dvd")
    {
        //
        //  This is not a DVD related mtd job (someone ripping a CD?)
        //
        
        return;

    }

    if(tokens[2] == "complete")
    {
        //
        //  All jobs are updated
        //
        showCurrentJob();
        return;
    }

    if(tokens.count() < 4)
    {
        cerr << "dvdripbox.o: I got an mtd update I couldn't understand:" << tokens.join(" ") << endl;
        return;
    }

    
    
    //
    //  if we got called, and we are still in context 0,
    //  and there are no jobs, switch to context 2. If
    //  there are jobs, switch to context 1 (as long as
    //  we're not already in context 2).
    //
    
    
    if(getContext() < 3)
    {
        if((tokens[2] == "summary" &&
           tokens[3].toInt() > 0) ||
           (tokens[2] == "job"))
        {
            //ripscreen_button->SetContext(3);
            cancel_button->SetContext(3);
            setContext(3);
            update();
            if(warning_text)
            {
                warning_text->SetText("");
            }
        }
        else
        {
            setContext(2);
            update();
            if(have_disc)
            {
                if(first_time_through)
                {
                    first_time_through = false;
                    goRipScreen();
                }
                else
                {
                    if(warning_text)
                    {
                        warning_text->SetText(tr("No jobs and nothing else to do. You could hit 0 to rip a DVD."));
                    }
                }
            }
            else
            {
                if(warning_text)
                {
                    warning_text->SetText(tr("No Jobs. Checking and/or waiting for DVD."));
                }
            }
        }
    }
    else if(getContext() == 3)
    {
        if(tokens[2] == "summary" && tokens[3].toInt() == 0)
        {
            setContext(2);
            update();
            if(have_disc)
            {
                if(warning_text)
                {
                    warning_text->SetText(tr("No jobs and nothing else to do. You could hit 0 to rip a disc if you like."));
                }
            }
            else
            {
                if(warning_text)
                {
                    warning_text->SetText(tr("No Jobs. Checking and/or waiting for DVD."));
                }
            }
            
        }
    }

    //
    //  Ok, enough with initial fiddling. Actual parsing
    //  to just store the data somewhere as it comes in
    //
    
    if(tokens[2] == "summary")
    {
        //
        //  Summary statistic, which should tell us
        //  how many jobs there are
        //
        
        if(tokens[3].toUInt() != numb_jobs)
        {
            adjustJobs(tokens[3].toUInt());
        }
        return;
        
    }
    
    //
    //  So, this should be a real statistic that is
    //  actually part of a DVD job
    //

    if(tokens.count() < 6)
    {
        cerr << "dvdripbox.o: got wrong number of tokens on a DVD job." << endl;
        return; // exit(0) ?
    }
    else if(tokens[2] == "job" && tokens[4] == "overall")
    {
        QString title = "";
        for(uint i = 6; i < tokens.count(); i++)
        {
            title.append(tokens[i]);
            if(i != tokens.count() - 1)
            {
                title.append(" ");
            }
        }
        setOverallJobStatus(tokens[3].toInt(), tokens[5].toDouble(), title);
    }
    else if(tokens[2] == "job" && tokens[4] == "subjob")
    {
        QString subjob_string = "";
        for(uint i = 6; i < tokens.count(); i++)
        {
            subjob_string.append(tokens[i]);
            if(i != tokens.count() - 1)
            {
                subjob_string.append(" ");
            }
        }
        setSubJobStatus(tokens[3].toInt(), tokens[5].toDouble(), subjob_string);
    }
    else
    {
        cerr << "dvdripbox.o: Getting stuff I don't understand from the mtd" << endl ;
    }

}

void DVDRipBox::handleMedia(QStringList tokens)
{
    //
    //  Initial sanity checking
    //
    
    if(tokens.count() < 3)
    {
        cerr << "dvdripbox.o: I got an mtd media update with a bad number of tokens" << endl;
        return;
    }

    if(tokens[1] != "dvd")
    {
        //
        //  This is not a DVD related mtd job (someone ripping a CD?)
        //
        
        return;

    }
    
    
    if(tokens[2] == "complete")
    {
        block_media_requests = false;
        if(dvd_info)
        {
            if(dvd_info->getTitles()->count() > 0)
            {
                have_disc = true;
                if(ripscreen_button)
                {
                    if(ripscreen_button->GetContext() != -1)
                    {
                    }
                }
            }
            else
            {
                have_disc = false;
                if(ripscreen_button)
                {
                    if(ripscreen_button->GetContext() != -2)
                    {
                        ripscreen_button->SetContext(-1);
                        ripscreen_button->refresh();
                    }
                }
            }
        }
        return;
    }
    else if(tokens[2] == "summary")
    {
        block_media_requests = true;
        if(dvd_info)
        {
            delete dvd_info;
            dvd_info = NULL;
        }
        if(tokens[3].toUInt() > 0)
        {
            QString disc_name = "";
            for(uint i = 4; i < tokens.count(); i++)
            {
                disc_name += tokens[i];
                if(i < tokens.count() - 1)
                {
                    disc_name += " ";
                }
            }
            dvd_info = new DVDInfo(disc_name);
        }
        else
        {
            have_disc = false;
            if(ripscreen_button)
            {
                if(ripscreen_button->GetContext() != -2)
                {
                    ripscreen_button->SetContext(-2);
                    ripscreen_button->refresh();
                }
            }
        }
        return;
    }
    else if(tokens[2] == "title")
    {
        if(tokens.count() != 10)
        {
            cerr << "dvdripbox.o: Got wrong number of tokens in media title report." << endl;
            return;
        }
        else
        {
            DVDTitleInfo *new_title = new DVDTitleInfo();
            new_title->setTrack(tokens[3].toUInt());
            new_title->setChapters(tokens[4].toUInt());
            new_title->setAngles(tokens[5].toUInt());
            new_title->setTime(tokens[6].toUInt(), tokens[7].toUInt(), tokens[8].toUInt());
            new_title->setInputID(tokens[9].toUInt());
            dvd_info->addTitle(new_title);
        }
        return;
    }
    else if(tokens[2] == "title-audio")
    {
        DVDTitleInfo *which_title = dvd_info->getTitle(tokens[3].toUInt());
        if(!which_title)
        {
            cerr << "dvdripbox.o: Asked to add an audio track for a title that doesn't exist" << endl;
            return;
        }
        
        QString audio_string = "";
        for(uint i = 6; i < tokens.count(); i++)
        {
            audio_string += tokens[i];
            if(i < tokens.count() - 1)
            {
                audio_string += " ";
            }
        }
        
        DVDAudioInfo *new_audio = new DVDAudioInfo(tokens[4].toUInt() + 1, audio_string);
        new_audio->setChannels(tokens[5].toInt());
        which_title->addAudio(new_audio);
    }
    else if(tokens[2] == "title-subtitle")
    {
        DVDTitleInfo *which_title = dvd_info->getTitle(tokens[3].toUInt());
        if(!which_title)
        {
            cerr << "dvdripbox.o: Asked to add a subtitle for a title that doesn't exist" << endl;
            return;
        }
        
        QString name_string = "";
        for(uint i = 6; i < tokens.count(); i++)
        {
            name_string += tokens[i];
            if(i < tokens.count() - 1)
            {
                name_string += " ";
            }
        }
        DVDSubTitleInfo *new_subtitle = new DVDSubTitleInfo(tokens[4].toInt(), name_string);
        which_title->addSubTitle(new_subtitle);
    }
    
}



void DVDRipBox::setOverallJobStatus(int job_number, double status, QString title)
{
    if(job_number + 1 > (int) jobs.count())
    {
        cerr << "dvdripbox.o: mtd job summary didn't tell us the right number of jobs" << endl;
        cerr << "             (int) jobs.count() is " << (int) jobs.count() << endl;
        cerr << "             requested job_number was " << job_number << endl ;
    }
    else
    {
        MTDJob *which_one = jobs.at(job_number);
        which_one->setName(title);
        which_one->setOverall(status);
        which_one->setNumber(job_number);
    }    
}

void DVDRipBox::setSubJobStatus(int job_number, double status, QString subjob_string)
{
    if(job_number + 1 > (int) jobs.count())
    {
        cerr << "dvdripbox.o: mtd job summary didn't tell us the right number of jobs. The Bastard!" << endl;
    }
    else
    {
        MTDJob *which_one = jobs.at(job_number);
        which_one->setActivity(subjob_string);
        which_one->setSubjob(status);
    }    
}

void DVDRipBox::adjustJobs(uint new_number)
{
    if(new_number > numb_jobs)
    {
        for(uint i = 0; i < (new_number - numb_jobs); i++)
        {
            MTDJob *new_one = new MTDJob("I am a job");
            connect(new_one, SIGNAL(toggledCancelled()), this, SLOT(toggleCancel()));
            jobs.append(new_one);
        }
        if(current_job < 0)
        {
            current_job = 0;
        }
    }
    else if(new_number < numb_jobs)
    {
        for(uint i = 0; i < (numb_jobs - new_number); i++)
        {
            jobs.remove(jobs.getLast());
        }
        if(current_job >= (int) jobs.count())
        {
            current_job = jobs.count() - 1;
        }
    }
    numb_jobs = new_number;
    if(numb_jobs == 0)
    {
        if(ignore_cancels)
        {
            toggleCancel();
        }
    }
}

void DVDRipBox::goRipScreen()
{
    if(warning_text)
    {
        warning_text->SetText("");
    }
    stopStatusPolling();
    block_media_requests = true;
    TitleDialog title_dialog(client_socket, 
                             dvd_info->getName(), 
                             dvd_info->getTitles(), 
                             gContext->GetMainWindow(),
                             "title_dialog",
                             "dvd-", 
                             "title dialog");
    title_dialog.exec();
    block_media_requests = false;
    pollStatus();
    showCurrentJob();
    warning_text->SetText("");
    //setContext(3);
    startStatusPolling();
}

void DVDRipBox::cancelJob()
{
    if( current_job > -1 && 
        current_job < (int) jobs.count() &&
        !ignore_cancels)
    {
        if(jobs.at(current_job)->getNumber() >= 0)
        {
            ignore_cancels = true;
            stopStatusPolling();
            sendToServer(QString("abort dvd job %1").arg(jobs.at(current_job)->getNumber()));
            qApp->processEvents();
            jobs.at(current_job)->setSubjob(0.0);
            jobs.at(current_job)->setActivity(tr("Cancelling ..."));
            jobs.at(current_job)->setCancelled(true);
            showCurrentJob();
            startStatusPolling();
        }
    }
}

void DVDRipBox::toggleCancel()
{
    ignore_cancels = false;
}

void DVDRipBox::wireUpTheme()
{
    warning_text = getUITextType("warning");
    if(!warning_text)
    {
        cerr << "dvdripbox.o: Couldn't find a text type called warning in your theme" << endl;
        exit(0);
    }
    
    overall_text = getUITextType("overall_text");
    job_text = getUITextType("job_text");
    numb_jobs_text = getUITextType("numb_jobs_text");
    nodvd_text = getUITextType("nodvd_text");
    overall_status = getUIStatusBarType("overall_status");
    if(overall_status)
    {
        overall_status->SetTotal(1000);
        overall_status->SetUsed(0);
    }
    job_status = getUIStatusBarType("job_status");
    if(job_status)
    {
        job_status->SetTotal(1000);
        job_status->SetUsed(0);
    }
    
    nextjob_button = getUIPushButtonType("job_next_button");
    if(nextjob_button)
    {
        connect(nextjob_button, SIGNAL(pushed()), this, SLOT(nextJob()));
    }
    prevjob_button = getUIPushButtonType("job_prev_button");
    if(prevjob_button)
    {
        connect(prevjob_button, SIGNAL(pushed()), this, SLOT(prevJob()));
    }        

    ripscreen_button = getUITextButtonType("ripscreen_button");
    if(ripscreen_button)
    {
        ripscreen_button->setText(tr("0 New Rip"));
        connect(ripscreen_button, SIGNAL(pushed()), this, SLOT(goRipScreen()));
        ripscreen_button->SetContext(-2);
    }

    cancel_button = getUITextButtonType("cancel_button");
    if(cancel_button)
    {
        cancel_button->setText(tr("9 Cancel Job"));
        connect(cancel_button, SIGNAL(pushed()), this, SLOT(cancelJob()));
        cancel_button->SetContext(-2);
    }
}

DVDRipBox::~DVDRipBox(void)
{
    if(client_socket)
    {
        client_socket->close();
        delete client_socket;
    }
    jobs.clear();
}

