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
bool watchingRecording = false;
CommDetectorBase* commDetector = NULL;
RemoteEncoder* recorder = NULL;
ProgramInfo* program_info = NULL;
int commDetectMethod = -1;
int recorderNum = -1;
bool dontSubmitCommbreakListToDB =  false;
QString outputfilename;
bool onlyDumpDBCommercialBreakList = false;

int jobID = -1;
int lastCmd = -1;

int BuildVideoMarkup(QString& filename)
{
    program_info = new ProgramInfo;
    program_info->recstartts = QDateTime::currentDateTime().addSecs( -180 * 60);
    program_info->recendts = QDateTime::currentDateTime().addSecs(-1);
    program_info->isVideo = true;
    program_info->pathname = filename;

    RingBuffer *tmprbuf = new RingBuffer(filename, false);
    if (!tmprbuf)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to create RingBuffer for %1").arg(filename));
        delete program_info;
        return COMMFLAG_EXIT_NO_RINGBUFFER;
    }

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "Unable to open DB connection for commercial flagging.");
        delete tmprbuf;
        delete program_info;
        return COMMFLAG_EXIT_DB_ERROR;
    }

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer("seektable rebuilder",
                                                   program_info);
    nvp->SetRingBuffer(tmprbuf);

    nvp->RebuildSeekTable(!quiet);

    cerr << "Rebuilt\n";

    delete nvp;
    delete tmprbuf;
    delete program_info;

    return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
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

int ClearCutList(QString chanid, QString starttime)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded "
                  "SET cutlist = NULL "
                  "WHERE chanid = :CHANID "
                      "AND starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("clear cutlist",
                             query);

    VERBOSE(VB_IMPORTANT, "Cutlist cleared");

    return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
}

int SetCutList(QString chanid, QString starttime, QString newCutList)
{
    newCutList.replace(QRegExp(","), "\n");
    newCutList.replace(QRegExp("-"), " - ");

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded "
                  "SET cutlist = :CUTLIST "
                  "WHERE chanid = :CHANID "
                      "AND starttime = :STARTTIME ;");
    query.bindValue(":CUTLIST", newCutList);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("clear cutlist",
                             query);

    newCutList.replace(QRegExp("\n"), ",");
    newCutList.replace(QRegExp(" - "), "-");

    VERBOSE(VB_IMPORTANT, QString("Cutlist set to: %1").arg(newCutList));

    return COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;
}

int GetCutList(QString chanid, QString starttime)
{
    QString result;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cutlist FROM recorded "
                  "WHERE chanid = :CHANID "
                      "AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);

    if (query.exec() && query.isActive() && query.size() > 0 && query.next())
        result = query.value(0).toString();
    else
        MythContext::DBError("get cutlist",
                             query);

    result.replace(QRegExp("\n"), ",");
    result.replace(QRegExp(" - "), "-");

    cout << QString("Cutlist: %1\n").arg(result);

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
        if (curCmd == lastCmd)
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

            if (recorderNum != -1 && cardnum == recorderNum)
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
                (program_info->recstartts == startts))
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
                                            program_info->startts,
                                            program_info->endts,
                                            program_info->recstartts,
                                            program_info->recendts);

    if (inJobQueue)
    {
        jobID = JobQueue::GetJobID(JOB_COMMFLAG, program_info->chanid,
                                   program_info->recstartts);

        if (jobID != -1)
            VERBOSE(VB_COMMFLAG,
                QString("mythcommflag processing JobID %1").arg(jobID));
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


    bool result = commDetector->go();
    int comms_found = 0;

    if (result)
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
                         << program_info->recstartts.toString(Qt::ISODate) << endl;
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
                   << program_info->recstartts.toString(Qt::ISODate) << endl;
            streamOutCommercialBreakList(output, commBreakList);
        }
        else
        {
            cout << "commercialBreakListFor: " << program_info->title
                 << " on " << program_info->chanid << " @ "
                 << program_info->recstartts.toString(Qt::ISODate) << endl;
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
    }

    if (!force && JobQueue::IsJobRunning(JOB_COMMFLAG, program_info))
    {
        if (!quiet)
        {
            cerr << "IN USE\n";
            cerr << "                        "
                    "(the program is already being flagged elsewhere)\n";
        }
        return COMMFLAG_EXIT_IN_USE;
    }

    filename = program_info->GetPlaybackURL();

    RingBuffer *tmprbuf = new RingBuffer(filename, false);
    if (!tmprbuf)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to create RingBuffer for %1").arg(filename));
        delete program_info;
        return COMMFLAG_EXIT_NO_RINGBUFFER;
    }

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "Unable to open commflag DB connection");
        delete tmprbuf;
        delete program_info;
        return COMMFLAG_EXIT_DB_ERROR;
    }

    NuppelVideoPlayer* nvp = new NuppelVideoPlayer("flagger", program_info);
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

    if (program_info->recendts > QDateTime::currentDateTime())
    {
        gContext->ConnectToMasterServer();

        recorder = RemoteGetExistingRecorder(program_info);
        if (recorder && (recorder->GetRecorderNumber() != -1))
        {
            recorderNum =  recorder->GetRecorderNumber();
            watchingRecording = true;
            nvp->SetRecorder(recorder);

            VERBOSE(VB_COMMFLAG, QString("mythcommflag will flag recording "
                    "currently in progress on cardid %1").arg(recorderNum));
        }
        else
        {
            recorderNum = -1;
            watchingRecording = false;

            VERBOSE(VB_IMPORTANT, "Unable to find active recorder for this "
                    "recording, realtime flagging will not be enabled.");
        }
        nvp->SetWatchingRecording(watchingRecording);
    }

    int fakeJobID = -1;
    if (!inJobQueue)
    {
        JobQueue::QueueJob(JOB_COMMFLAG, program_info->chanid,
                           program_info->recstartts, "", "",
                           gContext->GetHostName(), JOB_EXTERNAL,
                           JOB_RUNNING);
        fakeJobID = JobQueue::GetJobID(JOB_COMMFLAG, program_info->chanid,
                                       program_info->recstartts);
        jobID = fakeJobID;
        VERBOSE(VB_COMMFLAG,
            QString("Not in JobQueue, creating fake Job ID %1").arg(jobID));
    }
    else
        jobID = -1;


    breaksFound = DoFlagCommercials(showPercentage, fullSpeed, inJobQueue,
                                    nvp, commDetectMethod);

    if (fakeJobID >= 0)
    {
        jobID = -1;
        JobQueue::ChangeJobStatus(fakeJobID, JOB_FINISHED,
            QObject::tr("Finished, %1 break(s) found.").arg(breaksFound));
    }

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
    int result = COMMFLAG_EXIT_NO_ERROR_WITH_NO_BREAKS;

    QString filename;

    QString chanid;
    QString starttime;
    QString allStart = "19700101000000";
    QString allEnd   = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
    time_t time_now;
    bool allRecorded = false;
    bool queueJobInstead = false;
    bool copyToCutlist = false;
    bool clearCutlist = false;
    bool getCutlist = false;
    QString newCutList = "";

    QFileInfo finfo(a.argv()[0]);

    QString binname = finfo.baseName();

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return COMMFLAG_EXIT_NO_MYTHCONTEXT;
    }

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

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT chanid, starttime FROM recorded "
                          "WHERE basename = :BASENAME ;");
            query.bindValue(":BASENAME", fullfile.dirName());

            if (query.exec() && query.isActive() && query.size() > 0 &&
                query.next())
            {
                chanid = query.value(0).toString();
                starttime =
                    query.value(1).toDateTime().toString("yyyyMMddhhmmss");
            }
            else
            {
                cerr << "mythcommflag: ERROR: Unable to find DB info for "
                     << fullfile.dirName() << endl;
                return COMMFLAG_EXIT_NO_PROGRAM_DATA;
            }
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
            copyToCutlist = true;
        else if (!strcmp(a.argv()[argpos], "--clearcutlist"))
            clearCutlist = true;
        else if (!strcmp(a.argv()[argpos], "--getcutlist"))
            getCutlist = true;
        else if (!strcmp(a.argv()[argpos], "--setcutlist"))
            newCutList = (a.argv()[++argpos]);
        else if (!strcmp(a.argv()[argpos], "-j"))
        {
            int jobID = QString(a.argv()[++argpos]).toInt();
            int jobType = JOB_NONE;

            if ( !JobQueue::GetJobInfoFromID(jobID, jobType, chanid, starttime))
            {
                cerr << "mythcommflag: ERROR: Unable to find DB info for "
                     << "JobQueue ID# " << jobID << endl;
                return COMMFLAG_EXIT_NO_PROGRAM_DATA;
            }

            inJobQueue = true;
            force = true;
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
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return COMMFLAG_EXIT_INVALID_CMDLINE;

                ++argpos;
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
                    "--sleep                      Give up some CPU time after processing each frame\n"
                    "--rebuild                    Do not flag commercials, just rebuild seektable\n"
                    "--gencutlist                 Copy the commercial skip list to the cutlist\n"
                    "--clearcutlist               Clear the cutlist\n"
                    "--setcutlist CUTLIST         Set a new cutlist.  CUTLIST is of the form:\n"
                    "                             #-#[,#-#]...  (ie, 1-100,1520-3012,4091-5094\n"
                    "--getcutlist                 Display the current cutlist\n"
                    "-v or --verbose debug-level  Use '-v help' for level info\n"
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

    if (clearCutlist)
        return ClearCutList(chanid, starttime);

    if (getCutlist)
        return GetCutList(chanid, starttime);

    if (newCutList != "")
        return SetCutList(chanid, starttime, newCutList);

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
        VERBOSE(VB_IMPORTANT, QString("%1 version: %2 www.mythtv.org")
                                .arg(binname).arg(MYTH_BINARY_VERSION));

        VERBOSE(VB_IMPORTANT, QString("Enabled verbose msgs: %1").arg(verboseString));

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
        result = BuildVideoMarkup(filename);
    }
    else if (!chanid.isEmpty() && !starttime.isEmpty())
    {
        result = FlagCommercials(chanid, starttime);
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT chanid, starttime "
                "FROM recorded "
                "WHERE starttime >= :STARTTIME AND endtime <= :ENDTIME "
                "ORDER BY starttime;");
        query.bindValue(":STARTTIME", allStart);
        query.bindValue(":ENDTIME", allEnd);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                QDateTime m_starttime =
                    QDateTime::fromString(query.value(1).toString(),
                                          Qt::ISODate);
                starttime = m_starttime.toString("yyyyMMddhhmmss");

                chanid = query.value(0).toString();

                if ( allRecorded )
                {
                    FlagCommercials(chanid, starttime);
                }
                else
                {
                    // check to see if this show is already marked
                    MSqlQuery mark_query(MSqlQuery::InitCon());
                    mark_query.prepare("SELECT commflagged, count(rm.type) "
                                       "FROM recorded r "
                                       "LEFT JOIN recordedmarkup rm ON "
                                           "( r.chanid = rm.chanid AND "
                                             "r.starttime = rm.starttime AND "
                                             "type in (:MARK_START,:MARK_END)) "
                                        "WHERE r.chanid = :CHANID AND "
                                            "r.starttime = :STARTTIME "
                                        "GROUP BY COMMFLAGGED;");
                    mark_query.bindValue(":MARK_START", MARK_COMM_START);
                    mark_query.bindValue(":MARK_END", MARK_COMM_END);
                    mark_query.bindValue(":CHANID", chanid);
                    mark_query.bindValue(":STARTTIME", m_starttime);

                    if (mark_query.exec() && mark_query.isActive() &&
                        mark_query.size() > 0)
                    {
                        if (mark_query.next())
                        {
                            int flagStatus = mark_query.value(0).toInt();
                            int marksFound = mark_query.value(1).toInt();

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
            MythContext::DBError("Querying recorded programs", query);
            return COMMFLAG_EXIT_DB_ERROR;
        }
    }

    delete gContext;

    time_now = time(NULL);

    cerr << "\nFinished commercial break flagging at "
         << ctime(&time_now) << "\n";

    return result;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
