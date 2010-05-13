#include <QApplication>
#include <QProcess>
#include <QRegExp>
#include <QTimer>

#include <mythcontext.h>
#include <mythmediamonitor.h>
#include <mythdirs.h>

#include <mythscreenstack.h>
#include <mythuibutton.h>
#include <mythuitext.h>
#include <mythuiprogressbar.h>

#include "titledialog.h"
#include "videoutils.h"
#include "dvdripbox.h"

MTDJob::MTDJob() : job_number(-1), overall_progress(0.0), subjob_progress(0.0),
    cancelled(false)
{
}

MTDJob::MTDJob(QString lname) : job_number(-1), job_name(lname),
    overall_progress(0.0), subjob_progress(0.0), cancelled(false)
{
}

void MTDJob::SetName(const QString &a_name)
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

namespace
{
    class LaunchMTD : public QObject
    {
        Q_OBJECT

      public:
        static LaunchMTD *Create()
        {
            return new LaunchMTD;
        }

      private:
        LaunchMTD()
        {
            QStringList args;
            args << "-d";

            QProcess::startDetached(QString("%1/bin/mtd")
                    .arg(GetInstallPrefix()), args);
            QTimer::singleShot(2000, this, SLOT(OnLaunchWaitDone()));
        }

        ~LaunchMTD() {}

      signals:
        void SigLaunchAttemptComplete();

      private slots:
        void OnLaunchWaitDone()
        {
            emit SigLaunchAttemptComplete();
            deleteLater();
        }
    };
}

DVDRipBox::DVDRipBox(MythScreenStack *lparent, QString lname, QString device) :
    MythScreenType(lparent, lname),
    m_mtdPort(gCoreContext->GetNumSetting("MTDPort", 2442)),
    m_clientSocket(this),
    m_triedMTDLaunch(false), m_connected(false), m_firstRun(true),
    m_haveDisc(false),
    m_firstDiscFound(false), m_blockMediaRequests(false), m_jobCount(0),
    m_currentJob(-1), m_ignoreCancels(false), m_device(device), m_dvdInfo(0),
    m_warningText(0), m_overallText(0), m_jobText(0),
    m_numjobsText(0), m_overallProgress(0), m_jobProgress(0),
    m_ripscreenButton(0), m_cancelButton(0), m_nextjobButton(0),
    m_prevjobButton(0)
{
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
    connect(&m_statusTimer, SIGNAL(timeout()), SLOT(pollStatus()));

    connect(&m_clientSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(OnConnectionError(QAbstractSocket::SocketError)));
    connect(&m_clientSocket, SIGNAL(connected()), SLOT(connectionMade()));
    connect(&m_clientSocket, SIGNAL(readyRead()), SLOT(readFromServer()));
    connect(&m_clientSocket, SIGNAL(disconnected()),
            SLOT(OnMTDConnectionDisconnected()));
}

DVDRipBox::~DVDRipBox()
{
    while (!m_jobs.isEmpty())
        delete m_jobs.takeFirst();

    m_jobs.clear();
}

bool DVDRipBox::Create()
{
    if (!LoadWindowFromXML("dvd-ui.xml", "dvd_rip", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_warningText, "warning", &err);
    UIUtilE::Assign(this, m_overallText, "overall_text", &err);
    UIUtilE::Assign(this, m_jobText, "job_text", &err);
    UIUtilE::Assign(this, m_numjobsText, "numbjobs", &err);

    UIUtilE::Assign(this, m_overallProgress, "overall_progress", &err);
    UIUtilE::Assign(this, m_jobProgress, "job_progress", &err);

    UIUtilE::Assign(this, m_ripscreenButton, "ripscreen", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    UIUtilE::Assign(this, m_nextjobButton, "next", &err);
    UIUtilE::Assign(this, m_prevjobButton, "prev", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'dvd_rip'");
        return false;
    }

    connect(m_ripscreenButton, SIGNAL(Clicked()), SLOT(goRipScreen()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(cancelJob()));
    connect(m_nextjobButton, SIGNAL(Clicked()), SLOT(nextJob()));
    connect(m_prevjobButton, SIGNAL(Clicked()), SLOT(prevJob()));

    m_ripscreenButton->SetText(tr("New Rip"));
    m_cancelButton->SetText(tr("Cancel Job"));
    m_overallProgress->SetTotal(1000);
    m_jobProgress->SetTotal(1000);

    m_cancelButton->SetVisible(false);
    m_ripscreenButton->SetVisible(false);
    m_nextjobButton->SetVisible(false);
    m_prevjobButton->SetVisible(false);
    m_overallProgress->SetVisible(false);
    m_jobProgress->SetVisible(false);
    m_overallText->SetVisible(false);
    m_jobText->SetVisible(false);

    BuildFocusList();

    Init();

    return true;
}

void DVDRipBox::Init()
{
    ConnectToMTD();

    //  Create (but do not open) the DVD probing object
    //  and then ask a thread to check it for us. Make a
    //  timer to query whether the thread is done or not

    connect(&m_discCheckingTimer, SIGNAL(timeout()), SLOT(checkDisc()));
    m_discCheckingTimer.start(600);
}

void DVDRipBox::checkDisc()
{
    if (!m_connected)
        return;

    if(m_blockMediaRequests)
        return;

    if(m_haveDisc)
    {
        m_ripscreenButton->SetVisible(true);

        if(!m_firstDiscFound)
        {
            m_firstDiscFound = true;

            m_discCheckingTimer.setInterval(4000);
        }

    }
    else
        m_ripscreenButton->SetVisible(false);

    sendToServer("media");
}

void DVDRipBox::OnMTDConnectionDisconnected()
{
    m_connected = false;

    stopStatusPolling();
    m_context = RIPSTATE_UNKNOWN;
    m_haveDisc = false;

    m_ripscreenButton->SetCanTakeFocus(false);
    m_cancelButton->SetCanTakeFocus(false);

    QString warning = tr("Your connection to the Myth "
                         "Transcoding Daemon has gone "
                         "away. This is not a good thing.");
    m_warningText->SetText(warning);
}

void DVDRipBox::ConnectToMTD(void)
{
    if (!m_connected)
        m_clientSocket.connectToHost("localhost", m_mtdPort);
}

void DVDRipBox::OnConnectionError(QAbstractSocket::SocketError error_id)
{
    m_context = RIPSTATE_NOCONNECT;

    switch (error_id)
    {
        case QAbstractSocket::ConnectionRefusedError:
        {
            if (!m_triedMTDLaunch)
            {
                m_triedMTDLaunch = true;
                LaunchMTD *tmp = LaunchMTD::Create();
                connect(tmp, SIGNAL(SigLaunchAttemptComplete()),
                        SLOT(OnMTDLaunchAttemptComplete()));

                m_warningText->SetText(tr("Attempting to launch mtd..."));
            }
            else
            {
                m_warningText->SetText(tr("Cannot connect to your Myth "
                                "Transcoding Daemon."));
            }

            break;
        }
        case QAbstractSocket::HostNotFoundError:
        {
            m_warningText->SetText(tr("Attempting to connect to your mtd said "
                            "host not found. Unable to recover."));
            break;
        }
        default:
        {
            m_warningText->SetText(tr("Unknown connection error."));
            break;
        }
    }
}

void DVDRipBox::connectionMade()
{
    m_context = RIPSTATE_NOJOBS;
    m_connected = true;
    sendToServer("hello");
    sendToServer("use dvd " + m_device);
}

void DVDRipBox::readFromServer()
{
    while (m_clientSocket.canReadLine())
    {
        QString line_from_server =
                QString::fromUtf8(m_clientSocket.readLine());
        line_from_server = line_from_server.replace( QRegExp("\n"), "" );
        line_from_server = line_from_server.replace( QRegExp("\r"), "" );
        line_from_server.simplified();

        QStringList tokens = line_from_server.split(" ", QString::SkipEmptyParts);
        if(tokens.count() > 0)
        {
            parseTokens(tokens);
        }
    }
}

void DVDRipBox::sendToServer(const QString &some_text)
{
    if (m_connected)
    {
        QTextStream os(&m_clientSocket);
        os << some_text << "\n";
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("dvdripbox.o: was asked to send the following text "
                        "while not m_connected: \"%1\"").arg(some_text));
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
    m_statusTimer.start(1000);
}

void DVDRipBox::stopStatusPolling()
{
    m_statusTimer.stop();
}

void DVDRipBox::pollStatus()
{
    //
    //  Ask the server to send us more data
    //

    sendToServer("status");

}

void DVDRipBox::nextJob()
{
    if(m_currentJob + 1 < (int) m_jobCount)
    {
        m_currentJob++;
    }
    showCurrentJob();
}

void DVDRipBox::prevJob()
{
    if(m_currentJob > 0)
    {
        m_currentJob--;
    }
    showCurrentJob();
}

void DVDRipBox::goToJob(int which_job)
{
    which_job--;
    if(which_job > -1 &&
       which_job < (int) m_jobCount)
    {
        m_currentJob = which_job;
        showCurrentJob();
    }
}

void DVDRipBox::showCurrentJob()
{
    if(m_currentJob < 0)
        return;

    bool buildfocus = false;

    if(m_currentJob > 0 && !m_prevjobButton->IsVisible())
    {
        m_prevjobButton->SetVisible(true);
        buildfocus = true;
    }
    else if ((m_jobCount == 1 || m_currentJob == 0)
                              && m_nextjobButton->IsVisible())
    {
        m_nextjobButton->SetVisible(false);
        buildfocus = true;
    }

    if (m_jobCount > 0 && m_currentJob+1 < (int)m_jobCount
                       && !m_nextjobButton->IsVisible())
    {
        m_nextjobButton->SetVisible(true);
        buildfocus = true;
    }
    else if ((m_jobCount == 1 || m_currentJob+1 == (int)m_jobCount)
                              && m_nextjobButton->IsVisible())
    {
        m_nextjobButton->SetVisible(false);
        buildfocus = true;
    }

    if (buildfocus)
        BuildFocusList();

    MTDJob *a_job = m_jobs.at((uint)m_currentJob);

    if(a_job)
    {
        m_overallText->SetVisible(true);
        m_jobText->SetVisible(true);
        m_overallProgress->SetVisible(true);
        m_jobProgress->SetVisible(true);

        m_overallText->SetText(a_job->getName());
        m_jobText->SetText(a_job->getActivity());

        int an_int = (int) (a_job->getOverall() * 1000);
        m_overallProgress->SetUsed(an_int);

        an_int = (int) (a_job->getSubjob() * 1000);
        m_jobProgress->SetUsed(an_int);

        m_numjobsText->SetText(tr("Job %1 of %2").arg(m_currentJob + 1)
                .arg(m_jobCount));
    }
}

void DVDRipBox::handleStatus(QStringList tokens)
{
    //
    //  Initial sanity checking
    //

    if(tokens.count() < 3)
    {
        VERBOSE(VB_IMPORTANT, "dvdripbox.o: I got an mtd status update with a bad number of tokens");
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
        VERBOSE(VB_IMPORTANT,
                QString("dvdripbox.o: I got an mtd update I couldn't "
                        "understand: %1").arg(tokens.join(" ")));
        return;
    }

    //
    //  if we got called, and we are still in context 0,
    //  and there are no jobs, switch to context 2. If
    //  there are jobs, switch to context 1 (as long as
    //  we're not already in context 2).
    //

    if(m_context < RIPSTATE_HAVEJOBS)
    {
        if ((tokens[2] == "summary" &&
           tokens[3].toInt() > 0) ||
           (tokens[2] == "job"))
        {
            m_cancelButton->SetCanTakeFocus(true);
            m_context = RIPSTATE_HAVEJOBS;
            m_warningText->SetText("");
            m_cancelButton->SetVisible(true);
        }
        else
        {
            m_cancelButton->SetVisible(false);
            m_context = RIPSTATE_NOJOBS;
            if(m_haveDisc)
            {
                if(m_firstRun)
                {
                    m_firstRun = false;
                    goRipScreen();
                }
                else
                    m_warningText->SetText(tr("No jobs and nothing else to do. You could rip a DVD."));
            }
            else
                m_warningText->SetText(tr("No Jobs. Checking and/or waiting for DVD."));
        }
    }
    else if(m_context == RIPSTATE_HAVEJOBS)
    {
        if(tokens[2] == "summary" && tokens[3].toInt() == 0)
        {
            m_nextjobButton->SetVisible(false);
            m_prevjobButton->SetVisible(false);
            m_cancelButton->SetVisible(false);

            BuildFocusList();

            m_overallProgress->SetVisible(false);
            m_jobProgress->SetVisible(false);
            m_overallText->SetVisible(false);
            m_jobText->SetVisible(false);

            m_context = RIPSTATE_NOJOBS;
            if(m_haveDisc)
                m_warningText->SetText(tr("No jobs and nothing else to do. You could rip a DVD."));
            else
                m_warningText->SetText(tr("No Jobs. Checking and/or waiting for DVD."));

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

        if(tokens[3].toUInt() != m_jobCount)
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
        VERBOSE(VB_IMPORTANT, "dvdripbox.o: got wrong number of tokens on a DVD job.");
        return; // exit(0) ?
    }
    else if(tokens[2] == "job" && tokens[4] == "overall")
    {
        QString title = "";
        for(QStringList::size_type i = 6; i < tokens.count(); i++)
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
        for(QStringList::size_type i = 6; i < tokens.count(); i++)
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
        VERBOSE(VB_IMPORTANT, "dvdripbox.o: Getting stuff I don't understand from the mtd");
    }

}

void DVDRipBox::handleMedia(QStringList tokens)
{
    //
    //  Initial sanity checking
    //

    if(tokens.count() < 3)
    {
        VERBOSE(VB_IMPORTANT, "dvdripbox.o: I got an mtd media update with a bad number of tokens");
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
        m_blockMediaRequests = false;
        if(m_dvdInfo)
        {
            if(m_dvdInfo->getTitles()->count() > 0)
            {
                m_haveDisc = true;
                m_ripscreenButton->SetCanTakeFocus(true);
            }
            else
            {
                m_haveDisc = false;
                m_ripscreenButton->SetCanTakeFocus(false);
            }
        }
        return;
    }
    else if(tokens[2] == "summary")
    {
        m_blockMediaRequests = true;
        if(m_dvdInfo)
        {
            delete m_dvdInfo;
            m_dvdInfo = NULL;
        }
        if(tokens[3].toUInt() > 0)
        {
            QString disc_name = "";
            for(QStringList::size_type i = 4; i < tokens.count(); i++)
            {
                disc_name += tokens[i];
                if(i < tokens.count() - 1)
                {
                    disc_name += " ";
                }
            }
            m_dvdInfo = new DVDInfo(disc_name);
        }
        else
        {
            m_haveDisc = false;
            m_ripscreenButton->SetCanTakeFocus(false);
        }
        return;
    }
    else if(tokens[2] == "title")
    {
        if(tokens.count() != 10)
        {
            VERBOSE(VB_IMPORTANT, "dvdripbox.o: Got wrong number of tokens in media title report.");
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
            m_dvdInfo->addTitle(new_title);
        }
        return;
    }
    else if(tokens[2] == "title-audio")
    {
        DVDTitleInfo *which_title = m_dvdInfo->getTitle(tokens[3].toUInt());
        if(!which_title)
        {
            VERBOSE(VB_IMPORTANT, "dvdripbox.o: Asked to add an audio track for a title that doesn't exist");
            return;
        }

        QString audio_string = "";
        for(QStringList::size_type i = 6; i < tokens.count(); i++)
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
        DVDTitleInfo *which_title = m_dvdInfo->getTitle(tokens[3].toUInt());
        if(!which_title)
        {
            VERBOSE(VB_IMPORTANT, "dvdripbox.o: Asked to add a subtitle for a title that doesn't exist");
            return;
        }

        QString name_string = "";
        for(QStringList::size_type i = 6; i < tokens.count(); i++)
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
    if(job_number + 1 > (int) m_jobs.count())
    {
        VERBOSE(VB_IMPORTANT, QString(
                "dvdripbox.o: mtd job summary didn't tell us the right number of jobs\n"
                "             (int) m_jobs.count() is %1\n"
                "             requested job_number was %2")
                .arg((int) m_jobs.count())
                .arg(job_number));
    }
    else
    {
        MTDJob *which_one = m_jobs.at(job_number);
        which_one->SetName(title);
        which_one->setOverall(status);
        which_one->setNumber(job_number);
    }
}

void DVDRipBox::setSubJobStatus(int job_number, double status, QString subjob_string)
{
    if(job_number + 1 > (int) m_jobs.count())
    {
        VERBOSE(VB_IMPORTANT, "dvdripbox.o: mtd job summary didn't tell us the right number of m_jobs. The Bastard!");
    }
    else
    {
        MTDJob *which_one = m_jobs.at(job_number);
        which_one->setActivity(subjob_string);
        which_one->setSubjob(status);
    }
}

void DVDRipBox::adjustJobs(uint new_number)
{
    if(new_number > m_jobCount)
    {
        for(uint i = 0; i < (new_number - m_jobCount); i++)
        {
            MTDJob *new_one = new MTDJob("I am a job");
            connect(new_one, SIGNAL(toggledCancelled()), SLOT(toggleCancel()));
            m_jobs.append(new_one);
        }
        if(m_currentJob < 0)
            m_currentJob = 0;
    }
    else if (new_number < m_jobCount)
    {
        int new_last = m_jobCount - new_number;
        if (new_last > 0)
        {
            // TODO: fix m_jobs, it doesn't free members correctly
            m_jobs.erase(m_jobs.begin() + new_last, m_jobs.end());
        }

        if(m_currentJob >= (int) m_jobs.count())
            m_currentJob = m_jobs.count() - 1;
    }
    m_jobCount = new_number;
    if(m_jobCount == 0)
    {
        if(m_ignoreCancels)
        {
            toggleCancel();
        }
    }
}

void DVDRipBox::goRipScreen()
{
    m_warningText->SetText("");
    stopStatusPolling();
    m_blockMediaRequests = true;

    MythScreenStack *screenStack = GetScreenStack();

    TitleDialog *title_dialog = new TitleDialog(screenStack, "title dialog",
                                                &m_clientSocket,
                                                m_dvdInfo->getName(),
                                                m_dvdInfo->getTitles());

    if (title_dialog->Create())
        screenStack->AddScreen(title_dialog);

    connect(title_dialog, SIGNAL(Exiting()), SLOT(ExitingRipScreen()));
}

void DVDRipBox::ExitingRipScreen()
{
    m_blockMediaRequests = false;
    pollStatus();
    showCurrentJob();
    m_warningText->SetText("");
    startStatusPolling();
}

void DVDRipBox::OnMTDLaunchAttemptComplete()
{
    ConnectToMTD();
}

void DVDRipBox::cancelJob()
{
    if( m_currentJob > -1 &&
        m_currentJob < (int) m_jobs.count() &&
        !m_ignoreCancels)
    {
        MTDJob *a_job = m_jobs.at(m_currentJob);
        if(a_job->getNumber() >= 0)
        {
            m_ignoreCancels = true;
            stopStatusPolling();
            sendToServer(QString("abort dvd job %1").arg(a_job->getNumber()));
            qApp->processEvents();
            a_job->setSubjob(0.0);
            a_job->setActivity(tr("Cancelling ..."));
            a_job->setCancelled(true);
            showCurrentJob();
            startStatusPolling();
        }
    }
}

void DVDRipBox::toggleCancel()
{
    m_ignoreCancels = false;
}

#include "dvdripbox.moc"
