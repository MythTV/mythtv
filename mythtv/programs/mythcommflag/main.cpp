#include <qapplication.h>
#include <qstring.h>
#include <qregexp.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qdir.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <sys/time.h>

#include "libmyth/exitcodes.h"
#include "libmyth/mythcontext.h"
#include "libmythtv/NuppelVideoPlayer.h"
#include "libmythtv/programinfo.h"
#include "libmythtv/remoteutil.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/remoteencoder.h"
#include "libmyth/mythdbcon.h"

#include "CommDetectorBase.h"
#include "CommDetectorFactory.h"
#include "SlotRelayer.h"
#include "CustomEventRelayer.h"

using namespace std;

bool quiet = false;
bool force = false;

bool showPercentage = true;
bool fullSpeed = true;
bool rebuildSeekTable = false;
bool beNice = true;
bool inJobQueue = false;
bool stillRecording = false;
bool copyToCutlist = false;
bool watchingRecording = false;
CommDetectorBase* commDetector = NULL;
RemoteEncoder* recorder = NULL;
ProgramInfo* program_info = NULL;
int commDetectMethod = -1;
bool dontSubmitCommbreakListToDB =  false;
QString outputfilename;
bool onlyDumpDBCommercialBreakList = false;

int jobID = -1;
int lastCmd = -1;

void BuildVideoMarkup(QString& filename)
{
    program_info = new ProgramInfo;
    program_info->recstartts = QDateTime::currentDateTime().addSecs( -180 * 60);
    program_info->recendts = QDateTime::currentDateTime().addSecs(-1);
    program_info->isVideo = true;
    program_info->pathname = filename;

    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "Unable to open DB connection for commercial flagging.");
        delete tmprbuf;
        delete program_info;
        return;
    }

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer(program_info);
    nvp->SetRingBuffer(tmprbuf);

    nvp->RebuildSeekTable(!quiet);

    cerr << "Rebuilt\n";

    delete nvp;
    delete tmprbuf;
    delete program_info;
}

int QueueCommFlagJob(QString chanid, QString starttime)
{
    ProgramInfo *pginfo =
        ProgramInfo::GetProgramFromRecorded(chanid, starttime);

    if (!pginfo)
    {
        if (!quiet)
            cerr << "Unable to find program info for chanid " << chanid
                 << " @ " << starttime << endl;
        return COMMFLAG_EXIT_NO_PROGRAM_DATA;
    }

    bool result = JobQueue::QueueJob(JOB_COMMFLAG, pginfo->chanid,
                                     pginfo->recstartts);

    if (result)
    {
        if (!quiet)
            cerr << "Job Queued for chanid " << chanid << " @ "
                 << starttime << endl;
        return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
    }
    else
    {
        if (!quiet)
            cerr << "Error queueing job for chanid " << chanid
                 << " @ " << starttime << endl;
        return COMMFLAG_EXIT_DB_ERROR;
    }

    return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
}

int CopySkipListToCutList(QString chanid, QString starttime)
{
    QMap<long long, int> cutlist;
    QMap<long long, int>::Iterator it;

    ProgramInfo *pginfo =
        ProgramInfo::GetProgramFromRecorded(chanid, starttime);

    if (!pginfo)
    {
        VERBOSE(VB_IMPORTANT,
                QString("No program data exists for channel %1 at %2")
                .arg(chanid.ascii()).arg(starttime.ascii()));
        return COMMFLAG_BUGGY_EXIT_NO_CHAN_DATA;
    }

    pginfo->GetCommBreakList(cutlist);
    for (it = cutlist.begin(); it != cutlist.end(); ++it)
        if (it.data() == MARK_COMM_START)
            cutlist[it.key()] = MARK_CUT_START;
        else
            cutlist[it.key()] = MARK_CUT_END;
    pginfo->SetCutList(cutlist);

    return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
}

void streamOutCommercialBreakList(ostream& output,
                                  QMap<long long, int>& commercialBreakList)
{
    if (commercialBreakList.empty())
        output << "No breaks" << endl;
    else
    {
        for (QMap<long long, int>::iterator it = commercialBreakList.begin();
             it!=commercialBreakList.end(); it++)
        {
            output << "framenum: " << it.key() << "\tmarktype: " << it.data()
                   << endl;
        }
    }
    output << "----------------------------" << endl;
}

void commDetectorBreathe()
{
    //this is connected to the commdetectors breathe signal so we get a chance
    //while its busy to see if the user already told us to stop.
    qApp->processEvents();

    if (jobID != -1)
    {
        int curCmd = JobQueue::GetJobCmd(jobID);
        if (curCmd==lastCmd)
            return;

        switch (curCmd)
        {
                case JOB_STOP:
                {
                    commDetector->stop();
                    break;
                }
                case JOB_PAUSE:
                {
                    JobQueue::ChangeJobStatus(jobID, JOB_PAUSED,
                                              QObject::tr("Paused"));
                    commDetector->pause();
                    break;
                }
                case JOB_RESUME:
                {
                    JobQueue::ChangeJobStatus(jobID, JOB_PAUSED,
                                              QObject::tr("Running"));
                    commDetector->resume();
                    break;
                }
        }
    }
}

void commDetectorStatusUpdate(const QString& status)
{
    if (jobID != -1)
        JobQueue::ChangeJobStatus(jobID, JOB_RUNNING,  status);
}

void commDetectorGotNewCommercialBreakList()
{
    QMap<long long, int> newCommercialMap;
    commDetector->getCommercialBreakList(newCommercialMap);

    QMap<long long, int>::Iterator it = newCommercialMap.begin();
    QString message = "COMMFLAG_UPDATE ";
    message += program_info->chanid + " " +
               program_info->recstartts.toString(Qt::ISODate);

    for (it = newCommercialMap.begin();
            it != newCommercialMap.end(); ++it)
    {
        if (it != newCommercialMap.begin())
            message += ",";
        else
            message += " ";
        message += QString("%1:%2").arg(it.key())
                   .arg(it.data());
    }

    VERBOSE(VB_COMMFLAG,
            QString("mythcommflag sending update: %1").arg(message));

    RemoteSendMessage(message);
}

void incomingCustomEvent(QCustomEvent* e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        message = message.simplifyWhiteSpace();
        QStringList tokens = QStringList::split(" ", message);

        VERBOSE(VB_COMMFLAG,
                QString("mythcommflag: Received Event: '%1'")
                .arg(message));

        if ((watchingRecording) &&
            (tokens[0] == "DONE_RECORDING"))
        {
            int cardnum = tokens[1].toInt();
            int filelen = tokens[2].toInt();

            message = QString("mythcommflag: Received a "
                              "DONE_RECORDING event for card %1.  ")
                              .arg(cardnum);

            if (cardnum == recorder->GetRecorderNumber())
            {
                commDetector->recordingFinished(filelen);
                watchingRecording = false;
                message += "Informed CommDetector that recording has finished.";
                VERBOSE(VB_COMMFLAG, message);
            }
        }

        if (tokens[0] == "COMMFLAG_REQUEST")
        {
            QString chanid = tokens[1];
            QString starttime = tokens[2];
            QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);

            message = QString("mythcommflag: Received a "
                              "COMMFLAG_REQUEST event for chanid %1 @ %2.  ")
                              .arg(chanid).arg(starttime);

            if ((program_info->chanid == chanid) &&
                (program_info->startts == startts))
            {
                commDetector->requestCommBreakMapUpdate();
                message += "Requested CommDetector to generate new break list.";
                VERBOSE(VB_COMMFLAG, message);
            }
        }
    }
}

int DoFlagCommercials(bool showPercentage, bool fullSpeed, bool inJobQueue,
                      NuppelVideoPlayer* nvp, int commDetectMethod)
{
    CommDetectorFactory factory;
    commDetector = factory.makeCommDetector(commDetectMethod, showPercentage,
                                            fullSpeed, nvp,
                                            program_info->recstartts,
                                            program_info->recendts);

    jobID = -1;

    if (inJobQueue)
    {
        jobID = JobQueue::GetJobID(JOB_COMMFLAG, program_info->chanid,
                                   program_info->startts);

        if (jobID != -1)
            VERBOSE(VB_COMMFLAG,
                QString("mythcommflag: Processing JobID %1").arg(jobID));
        else
            VERBOSE(VB_COMMFLAG, "mythcommflag: Unable to determine jobID");
    }

    program_info->SetCommFlagged(COMM_FLAG_PROCESSING);

    CustomEventRelayer cer(incomingCustomEvent);
    SlotRelayer a(commDetectorBreathe);
    SlotRelayer b(commDetectorStatusUpdate);
    SlotRelayer c(commDetectorGotNewCommercialBreakList);
    QObject::connect(commDetector, SIGNAL(breathe()), &a, SLOT(relay()));
    QObject::connect(commDetector, SIGNAL(statusUpdate(const QString&)), &b,
                     SLOT(relay(const QString&)));
    QObject::connect(commDetector, SIGNAL(gotNewCommercialBreakList()), &c,
                     SLOT(relay()));

    VERBOSE(VB_COMMFLAG, "mythcommflag sending COMMFLAG_START notification");
    QString message = "COMMFLAG_START ";
    message += program_info->chanid + " " +
               program_info->recstartts.toString(Qt::ISODate);
    RemoteSendMessage(message);


    int result = commDetector->go();
    int comms_found = 0;

    if (result >= 0)
    {
        QMap<long long, int> commBreakList;
        commDetector->getCommercialBreakList(commBreakList);
        comms_found = commBreakList.size() / 2;

        if (!dontSubmitCommbreakListToDB)
        {
            program_info->SetMarkupFlag(MARK_UPDATED_CUT, true);
            program_info->SetCommBreakList(commBreakList);
            program_info->SetCommFlagged(COMM_FLAG_DONE);
        }
        if (outputfilename.length())
        {
            fstream outputstream(outputfilename, ios::app | ios::out );
            outputstream << "commercialBreakListFor: " << program_info->title
                         << " on " << program_info->chanid << " @ "
                         << program_info->startts.toString(Qt::ISODate) << endl;
            streamOutCommercialBreakList(outputstream, commBreakList);
        }
    }
    else
    {
        if (!dontSubmitCommbreakListToDB)
            program_info->SetCommFlagged(COMM_FLAG_NOT_FLAGGED);
    }

    delete commDetector;

    return comms_found;
}

int FlagCommercials(QString chanid, QString starttime)
{
    int breaksFound = 0;
    if (commDetectMethod==-1)
        commDetectMethod = gContext->GetNumSetting("CommercialSkipMethod",
                                                   COMM_DETECT_BLANKS);
    QMap<long long, int> blanks;
    recorder = NULL;
    program_info = ProgramInfo::GetProgramFromRecorded(chanid, starttime);

    if (!program_info)
    {
        VERBOSE(VB_IMPORTANT,
                QString("No program data exists for channel %1 at %2")
                .arg(chanid.ascii()).arg(starttime.ascii()));
        return COMMFLAG_EXIT_NO_PROGRAM_DATA;
    }

    if (onlyDumpDBCommercialBreakList)
    {
        QMap<long long, int> commBreakList;
        program_info->GetCommBreakList(commBreakList);
        if (outputfilename.length())
        {
            fstream output(outputfilename, ios::app | ios::out );
            output << "commercialBreakListFor: " << program_info->title
                   << " on " << program_info->chanid << " @ "
                   << program_info->startts.toString(Qt::ISODate) << endl;
            streamOutCommercialBreakList(output, commBreakList);
        }
        else
        {
            cout << "commercialBreakListFor: " << program_info->title
                 << " on " << program_info->chanid << " @ "
                 << program_info->startts.toString(Qt::ISODate) << endl;
            streamOutCommercialBreakList(cout, commBreakList);
        }
        delete program_info;
        return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
    }

    QString filename = program_info->GetRecordFilename("...");

    if (!quiet)
    {
        cerr << chanid.leftJustify(6, ' ', true) << "  "
             << starttime.leftJustify(14, ' ', true) << "  "
             << program_info->title.leftJustify(41, ' ', true) << "  ";
        cerr.flush();

        if ((!force) && (program_info->IsCommProcessing()))
        {
            cerr << "IN USE\n";
            cerr << "                        "
                    "(the program is already being flagged elsewhere)\n";
            return COMMFLAG_EXIT_IN_USE;
        }
    }
    else
    {
        if ((!force) && (program_info->IsCommProcessing()))
            return COMMFLAG_EXIT_IN_USE;
    }

    filename = program_info->GetPlaybackURL();

    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "Unable to open commflag DB connection");
        delete tmprbuf;
        delete program_info;
        return COMMFLAG_EXIT_DB_ERROR;
    }

    NuppelVideoPlayer* nvp = new NuppelVideoPlayer(program_info);
    nvp->SetRingBuffer(tmprbuf);

    if (rebuildSeekTable)
    {
        nvp->RebuildSeekTable();

        if (!quiet)
            cerr << "Rebuilt\n";

        delete nvp;
        delete tmprbuf;
        delete program_info;

        return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
    }

    if ((stillRecording) &&
        (program_info->recendts > QDateTime::currentDateTime()))
    {
        nvp->SetWatchingRecording(true);
        gContext->ConnectToMasterServer();

        recorder = RemoteGetExistingRecorder(program_info);
        if (recorder && (recorder->GetRecorderNumber() != -1))
            nvp->SetRecorder(recorder);
        else
            cerr << "Unable to find active recorder for this recording, "
                 << "flagging info may be negatively affected." << endl;
    }

    breaksFound = DoFlagCommercials(showPercentage, fullSpeed, inJobQueue,
                                    nvp, commDetectMethod);

    if (!quiet)
        cerr << breaksFound << "\n";

    delete nvp;
    delete tmprbuf;
    delete program_info;

    return breaksFound;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);
    int argpos = 1;
    bool isVideo = false;

    QString filename;

    QString chanid;
    QString starttime;
    QString allStart = "19700101000000";
    QString allEnd   = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
    time_t time_now;
    bool allRecorded = false;
    QString verboseString = QString("");
    bool queueJobInstead = false;

    QFileInfo finfo(a.argv()[0]);

    QString binname = finfo.baseName();

    print_verbose_messages = VB_IMPORTANT;
    verboseString = "important";

    while (argpos < a.argc())
    {
        if (!strcmp(a.argv()[argpos],"-c") ||
            !strcmp(a.argv()[argpos],"--chanid"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                VERBOSE(VB_IMPORTANT,
                        "Missing or invalid parameters for --chanid option");
                return COMMFLAG_EXIT_INVALID_CHANID;
            }
            
            chanid += a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos],"-s") ||
                 !strcmp(a.argv()[argpos],"--starttime"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                VERBOSE(VB_IMPORTANT,
                        "Missing or invalid parameters for --starttime option");
                return COMMFLAG_EXIT_INVALID_STARTTIME;
            }
            
            starttime += a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos],"-f") ||
                 !strcmp(a.argv()[argpos],"--file"))
        {
            QDir fullfile(a.argv()[++argpos]);

            chanid = fullfile.dirName();
            chanid.replace(QRegExp("_[^_]*$"), "");
            chanid.replace(QRegExp("_[^_]*$"), "");

            starttime = fullfile.dirName();
            starttime.replace(QRegExp("_[^_]*$"), "");
            starttime.replace(QRegExp("^[^_]*_"), "");
        }
        else if (!strcmp(a.argv()[argpos],"--video"))
        {
            filename = (a.argv()[++argpos]);
            VERBOSE(VB_GENERAL, filename);
            isVideo = true;
            rebuildSeekTable = true;
            beNice = false;
        }
        else if (!strcmp(a.argv()[argpos],"--method"))
        {
            QString method = (a.argv()[++argpos]);
            bool ok;
            commDetectMethod = method.toInt(&ok);
            if (!ok)
                commDetectMethod = -1;
        }
        else if (!strcmp(a.argv()[argpos], "--gencutlist"))
        {
            copyToCutlist = true;
        }
        else if (!strcmp(a.argv()[argpos], "-j"))
        {
            inJobQueue = true;
        }
        else if (!strcmp(a.argv()[argpos], "-l"))
        {
            stillRecording = true;
        }
        else if (!strcmp(a.argv()[argpos], "--all"))
        {
            allRecorded = true;
        }
        else if (!strcmp(a.argv()[argpos], "--allstart"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                cerr << "Missing or invalid parameter for --allstart\n";
                return COMMFLAG_EXIT_INVALID_STARTTIME;
            }
            
            allStart = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos], "--allend"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                cerr << "Missing or invalid parameter for --allend\n";
                return COMMFLAG_EXIT_INVALID_STARTTIME;
            }
            
            allEnd = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos], "--quiet"))
        {
            quiet = true;
            showPercentage = false;
        }
        else if (!strcmp(a.argv()[argpos], "--queue"))
        {
            queueJobInstead = true;
        }
        else if (!strcmp(a.argv()[argpos], "--sleep"))
        {
            fullSpeed = false;
        }
        else if (!strcmp(a.argv()[argpos], "--rebuild"))
        {
            rebuildSeekTable = true;
            beNice = false;
        }
        else if (!strcmp(a.argv()[argpos], "--force"))
        {
            force = true;
        }
        else if (!strcmp(a.argv()[argpos], "--hogcpu"))
        {
            beNice = false;
        }
        else if (!strcmp(a.argv()[argpos], "--dontwritetodb"))
        {
            dontSubmitCommbreakListToDB = true;
        }
        else if (!strcmp(a.argv()[argpos], "--onlydumpdb"))
        {
            onlyDumpDBCommercialBreakList = true;
        }
        else if (!strcmp(a.argv()[argpos], "--outputfile"))
        {
            if (a.argc() > argpos)
            {
                outputfilename = a.argv()[++argpos];
                //clear file.
                fstream output(outputfilename, ios::out);
            }
            else
            {
                cerr << "Missing argument to --outputfile option\n";
                return -1;
            }
        }

        else if (!strcmp(a.argv()[argpos],"-V"))
        {
            if (a.argc() > argpos)
            {
                QString temp = a.argv()[++argpos];
                print_verbose_messages = temp.toInt();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Missing argument to -V option");
                return COMMFLAG_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-v") ||
                 !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc() > argpos)
            {
                QString temp = a.argv()[argpos+1];
                if (temp.startsWith("-"))
                {
                    VERBOSE(VB_IMPORTANT, "Invalid or missing "
                            "argument to -v/--verbose option");
                    return COMMFLAG_EXIT_INVALID_CMDLINE;
                }
                else
                {
                    QStringList verboseOpts;
                    verboseOpts = QStringList::split(',', a.argv()[argpos+1]);
                    ++argpos;
                    print_verbose_messages = VB_NONE;
                    for (QStringList::Iterator it = verboseOpts.begin();
                         it != verboseOpts.end(); ++it )
                    {
                        if (!strcmp(*it,"none"))
                        {
                            print_verbose_messages = VB_NONE;
                            verboseString = "";
                        }
                        else if (!strcmp(*it,"all"))
                        {
                            print_verbose_messages = VB_ALL;
                            verboseString = "all";
                        }
                        else if (!strcmp(*it,"quiet"))
                        {
                            print_verbose_messages = VB_IMPORTANT;
                            verboseString = "important";
                        }
                        else if (!strcmp(*it,"record"))
                        {
                            print_verbose_messages |= VB_RECORD;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"playback"))
                        {
                            print_verbose_messages |= VB_PLAYBACK;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"channel"))
                        {
                            print_verbose_messages |= VB_CHANNEL;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"osd"))
                        {
                            print_verbose_messages |= VB_OSD;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"file"))
                        {
                            print_verbose_messages |= VB_FILE;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"schedule"))
                        {
                            print_verbose_messages |= VB_SCHEDULE;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"network"))
                        {
                            print_verbose_messages |= VB_NETWORK;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"jobqueue"))
                        {
                            print_verbose_messages |= VB_JOBQUEUE;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"commflag"))
                        {
                            print_verbose_messages |= VB_COMMFLAG;
                            verboseString += " " + *it;
                        }
                        else
                        {
                            VERBOSE(VB_IMPORTANT,
                                    QString("Unknown argument for -v/"
                                            "--verbose: %1").arg(*it));
                        }
                    }
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        "Missing argument to -v/--verbose option");
                return COMMFLAG_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help"))
        {
            VERBOSE(VB_IMPORTANT,
                    "Valid Options are:\n"
                    "-c OR --chanid chanid        Flag recording with given channel ID\n"
                    "-s OR --starttime starttime  Flag recording with given starttime\n"
                    "-f OR --file filename        Flag recording with specific filename\n"
                    "--video filename             Rebuild the seektable for a video (non-recording) file\n"
                    "--sleep                      Give up some CPU time after processing\n"
                    "--rebuild                    Do not flag commercials, just rebuild seektable\n"
                    "--gencutlist                 Copy the commercial skip list to the cutlist\n"
                    "                             each frame.\n"
                    "-v OR --verbose debug-level  Prints more information\n"
                    "                             Accepts any combination (separated by comma)\n" 
                    "                             of all,none,quiet,record,playback,\n"
                    "                             channel,osd,file,schedule,network,commflag\n"
                    "--queue                      Insert flagging job into the JobQueue rather than\n"
                    "                             running flagging in the foreground\n"
                    "--quiet                      Turn OFF display (also causes the program to\n"
                    "                             sleep a little every frame so it doesn't hog CPU)\n"
                    "                             takes precedence over --blanks if given first)\n"
                    "--blanks                     Show list of blank frames if already in database\n"
                    "                             (takes precedence over --quiet if given first)\n"
                    "--all                        Re-run commercial flagging for all recorded\n"
                    "                             programs using current detection method.\n"
                    "--allstart YYYYMMDDHHMMSS    when using --all, only flag programs starting\n"
                    "                             after the 'allstart' date (default = Jan 1, 1970).\n"
                    "--allend YYYYMMDDHHMMSS      when using --all, only flag programs ending\n"
                    "                             before the 'allend' date (default is now).\n"
                    "--force                      Force flagging of a video even if mythcommflag\n"
                    "                             thinks it is already in use by another instance.\n"
                    "--hogcpu                     Do not nice the flagging process.\n"
                    "                             WARNING: This will consume all free CPU time.\n"
                    "-h OR --help                 This text\n\n"
                    "Note: both --chanid and --starttime must be used together\n"
                    "      if either is used.\n\n"
                    "If no command line arguments are specified, all\n"
                    "unflagged videos will be flagged.\n\n");
            return COMMFLAG_EXIT_INVALID_CMDLINE;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("Illegal option: '%1' (use --help)")
                    .arg(a.argv()[argpos]));
            return COMMFLAG_EXIT_INVALID_CMDLINE;
        }

        ++argpos;
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if(!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return COMMFLAG_EXIT_NO_MYTHCONTEXT;
    }

    if ((chanid.isEmpty() && !starttime.isEmpty()) ||
        (!chanid.isEmpty() && starttime.isEmpty()))
    {
        VERBOSE(VB_IMPORTANT, "You must specify both the Channel ID "
                "and the Start Time.");
        return COMMFLAG_EXIT_INVALID_CMDLINE;
    }

    if (queueJobInstead)
        return QueueCommFlagJob(chanid, starttime);

    if (copyToCutlist)
        return CopySkipListToCutList(chanid, starttime);

    if (inJobQueue)
    {
        int jobQueueCPU = gContext->GetNumSetting("JobQueueCPU", 0);

        if (beNice || jobQueueCPU < 2)
            nice(17);

        if (jobQueueCPU)
            fullSpeed = true;
        else
            fullSpeed = false;

        quiet = true;
        isVideo = false;
        showPercentage = false;

        int breaksFound = FlagCommercials(chanid, starttime);

        delete gContext;

        return breaksFound; // exit(breaksFound);
    }

    // be nice to other programs since FlagCommercials() can consume 100% CPU
    if (beNice)
        nice(17);

    time_now = time(NULL);
    if (!quiet)
    {
        VERBOSE(VB_ALL, QString("%1 version: %2 www.mythtv.org")
                                .arg(binname).arg(MYTH_BINARY_VERSION));

        VERBOSE(VB_ALL, QString("Enabled verbose msgs :%1").arg(verboseString));

        cerr << "\nMythTV Commercial Flagger, started at "
             << ctime(&time_now);

        if (!isVideo)
        {
            if (!rebuildSeekTable)
            {
                cerr << "Flagging commercial breaks for:\n";
                if (a.argc() == 1)
                    cerr << "ALL Un-flagged programs\n";
            }
            else
            {
                cerr << "Rebuilding SeekTable(s) for:\n";
            }

            cerr << "ChanID  Start Time      "
                    "Title                                      ";
            if (rebuildSeekTable)
                cerr << "Status\n";
            else
                cerr << "Breaks\n";

            cerr << "------  --------------  "
                    "-----------------------------------------  ------\n";
        }
        else
        {
            cerr << "Building seek table for: " << filename << "\n";
        }
    }

    if (isVideo)
    {
        BuildVideoMarkup(filename);
    }
    else if (!chanid.isEmpty() && !starttime.isEmpty())
    {
        FlagCommercials(chanid, starttime);
    }
    else
    {
        MSqlQuery recordedquery(MSqlQuery::InitCon());
        QString querystr = QString("SELECT chanid, starttime "
                                   "FROM recorded "
                                   "WHERE starttime >= %1 and endtime <= %2 "
                                   "ORDER BY starttime;")
                                   .arg(allStart).arg(allEnd);
        recordedquery.exec(querystr);

        if ((recordedquery.isActive()) && (recordedquery.numRowsAffected() > 0))
        {
            while (recordedquery.next())
            {
                QDateTime startts =
                    QDateTime::fromString(recordedquery.value(1).toString(),
                                          Qt::ISODate);
                starttime = startts.toString("yyyyMMddhhmm");
                starttime += "00";

                chanid = recordedquery.value(0).toString();

                if ( allRecorded )
                {
                    FlagCommercials(chanid, starttime);
                }
                else
                {
                    // check to see if this show is already marked
                    MSqlQuery markupquery(MSqlQuery::InitCon());
                    QString markupstr;
                   
                    markupstr = QString("SELECT commflagged, count(rm.type) "
                                        "FROM recorded r "
                                        "LEFT JOIN recordedmarkup rm ON "
                                            "( r.chanid = rm.chanid AND "
                                              "r.starttime = rm.starttime AND "
                                              "type in (%1,%2)) "
                                        "WHERE r.chanid = %3 AND "
                                            "r.starttime = %4 "
                                        "GROUP BY COMMFLAGGED;")
                                        .arg(MARK_COMM_START)
                                        .arg(MARK_COMM_END)
                                        .arg(chanid)
                                        .arg(starttime);
                    markupquery.exec(markupstr);
                    if ((markupquery.isActive()) &&
                        (markupquery.numRowsAffected() > 0))
                    {
                        if (markupquery.next())
                        {
                            int flagStatus = markupquery.value(0).toInt();
                            int marksFound = markupquery.value(1).toInt();

                            QString flagStatusStr = "UNKNOWN";
                            switch (flagStatus) {
                                case COMM_FLAG_NOT_FLAGGED:
                                        flagStatusStr = "Not Flagged";
                                        break;
                                case COMM_FLAG_DONE:
                                        flagStatusStr =
                                            QString("Flagged with %1 breaks")
                                                    .arg(marksFound / 2);
                                        break;
                                case COMM_FLAG_PROCESSING:
                                        flagStatusStr = "Flagging";
                                        break;
                                case COMM_FLAG_COMMFREE:
                                        flagStatusStr = "Commercial Free";
                                        break;
                            }

                            VERBOSE(VB_COMMFLAG,
                                    QString("Status for chanid %1 @ %2 is '%3'")
                                            .arg(chanid)
                                            .arg(starttime)
                                            .arg(flagStatusStr));

                            if ((flagStatus == COMM_FLAG_NOT_FLAGGED) &&
                                (marksFound == 0))
                                FlagCommercials(chanid, starttime);
                        }
                    }
                }
            }
        }
        else
        {
            MythContext::DBError("Querying recorded programs", recordedquery);
            return COMMFLAG_EXIT_DB_ERROR;
        }
    }

    delete gContext;

    time_now = time(NULL);

    cerr << "\nFinished commercial break flagging at "
         << ctime(&time_now) << "\n";

    return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
