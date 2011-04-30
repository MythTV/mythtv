// POSIX headers
#include <unistd.h>
#include <sys/time.h> // for gettimeofday

// ANSI C headers
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cmath>

// C++ headers
#include <string>
#include <iostream>
#include <fstream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QString>
#include <QRegExp>
#include <QDir>
#include <QEvent>

// MythTV headers
#include "util.h"
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythcommflagplayer.h"
#include "programinfo.h"
#include "remoteutil.h"
#include "remotefile.h"
#include "tvremoteutil.h"
#include "jobqueue.h"
#include "remoteencoder.h"
#include "ringbuffer.h"
#include "mythcommandlineparser.h"
#include "mythtranslation.h"
#include "mythlogging.h"

// Commercial Flagging headers
#include "CommDetectorBase.h"
#include "CommDetectorFactory.h"
#include "SlotRelayer.h"
#include "CustomEventRelayer.h"

#define LOC      QString("MythCommFlag: ")
#define LOC_WARN QString("MythCommFlag, Warning: ")
#define LOC_ERR  QString("MythCommFlag, Error: ")

namespace
{
    void cleanup()
    {
        delete gContext;
        gContext = NULL;

    }

    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

int  quiet = 0;
bool force = false;

bool showPercentage = true;
bool fullSpeed = true;
bool rebuildSeekTable = false;
bool beNice = true;
bool inJobQueue = false;
bool watchingRecording = false;
CommDetectorBase* commDetector = NULL;
RemoteEncoder* recorder = NULL;
ProgramInfo *global_program_info = NULL;
enum SkipTypes commDetectMethod = COMM_DETECT_UNINIT;
int recorderNum = -1;
bool dontSubmitCommbreakListToDB =  false;
bool onlyDumpDBCommercialBreakList = false;

int jobID = -1;
int lastCmd = -1;

static QMap<QString,SkipTypes> *init_skip_types();
QMap<QString,SkipTypes> *skipTypes = init_skip_types();

static QMap<QString,SkipTypes> *init_skip_types(void)
{
    QMap<QString,SkipTypes> *tmp = new QMap<QString,SkipTypes>;
    (*tmp)["commfree"]    = COMM_DETECT_COMMFREE;
    (*tmp)["uninit"]      = COMM_DETECT_UNINIT;
    (*tmp)["off"]         = COMM_DETECT_OFF;
    (*tmp)["blank"]       = COMM_DETECT_BLANKS;
    (*tmp)["blanks"]      = COMM_DETECT_BLANKS;
    (*tmp)["scene"]       = COMM_DETECT_SCENE;
    (*tmp)["blankscene"]  = COMM_DETECT_BLANK_SCENE;
    (*tmp)["blank_scene"] = COMM_DETECT_BLANK_SCENE;
    (*tmp)["logo"]        = COMM_DETECT_LOGO;
    (*tmp)["all"]         = COMM_DETECT_ALL;
    (*tmp)["d2"]          = COMM_DETECT_2;
    (*tmp)["d2_logo"]     = COMM_DETECT_2_LOGO;
    (*tmp)["d2_blank"]    = COMM_DETECT_2_BLANK;
    (*tmp)["d2_scene"]    = COMM_DETECT_2_SCENE;
    (*tmp)["d2_all"]      = COMM_DETECT_2_ALL;
    return tmp;
}

typedef enum
{
    kOutputMethodEssentials = 1,
    kOutputMethodFull,
} OutputMethod;
OutputMethod outputMethod = kOutputMethodEssentials;

static QMap<QString,OutputMethod> *init_output_types();
QMap<QString,OutputMethod> *outputTypes = init_output_types();

static QMap<QString,OutputMethod> *init_output_types(void)
{
    QMap<QString,OutputMethod> *tmp = new QMap<QString,OutputMethod>;
    (*tmp)["essentials"] = kOutputMethodEssentials;
    (*tmp)["full"]       = kOutputMethodFull;
    return tmp;
}

static QString get_filename(ProgramInfo *program_info)
{
    QString filename = program_info->GetPathname();
    if (!QFile::exists(filename))
        filename = program_info->GetPlaybackURL(true);
    return filename;
}

static int BuildVideoMarkup(ProgramInfo *program_info, bool useDB)
{
    QString filename;

    if (program_info->IsMythStream())
        filename = program_info->GetPathname();
    else
        filename = get_filename(program_info);

    RingBuffer *tmprbuf = RingBuffer::Create(filename, false);
    if (!tmprbuf)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to create RingBuffer for %1").arg(filename));
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    if (useDB && !MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "Unable to open DB connection for commercial flagging.");
        delete tmprbuf;
        return GENERIC_EXIT_DB_ERROR;
    }

    MythCommFlagPlayer *cfp = new MythCommFlagPlayer();
    PlayerContext *ctx = new PlayerContext("seektable rebuilder");
    ctx->SetSpecialDecode(kAVSpecialDecode_NoDecode);
    ctx->SetPlayingInfo(program_info);
    ctx->SetRingBuffer(tmprbuf);
    ctx->SetPlayer(cfp);
    cfp->SetPlayerInfo(NULL, NULL, true, ctx);
    cfp->RebuildSeekTable(!quiet);

    if (!quiet)
        cerr << "Rebuilt" << endl;

    delete ctx;

    return GENERIC_EXIT_OK;
}

static int QueueCommFlagJob(uint chanid, QString starttime)
{
    QDateTime recstartts = myth_dt_from_string(starttime);
    const ProgramInfo pginfo(chanid, recstartts);

    if (!pginfo.GetChanID())
    {
        if (!quiet)
        {
            QString tmp = QString(
                "Unable to find program info for chanid %1 @ %2")
                .arg(chanid).arg(starttime);
            cerr << tmp.toLocal8Bit().constData() << endl;
        }
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    bool result = JobQueue::QueueJob(
        JOB_COMMFLAG, pginfo.GetChanID(), pginfo.GetRecordingStartTime());

    if (result)
    {
        if (!quiet)
        {
            QString tmp = QString("Job Queued for chanid %1 @ %2")
                .arg(chanid).arg(starttime);
            cerr << tmp.toLocal8Bit().constData() << endl;
        }
        return GENERIC_EXIT_OK;
    }
    else
    {
        if (!quiet)
        {
            QString tmp = QString("Error queueing job for chanid %1 @ %2")
                .arg(chanid).arg(starttime);
            cerr << tmp.toLocal8Bit().constData() << endl;
        }
        return GENERIC_EXIT_DB_ERROR;
    }

    return GENERIC_EXIT_OK;
}

static int CopySkipListToCutList(QString chanid, QString starttime)
{
    frm_dir_map_t cutlist;
    frm_dir_map_t::const_iterator it;

    QDateTime recstartts = myth_dt_from_string(starttime);
    const ProgramInfo pginfo(chanid.toUInt(), recstartts);

    if (!pginfo.GetChanID())
    {
        VERBOSE(VB_IMPORTANT,
                QString("No program data exists for channel %1 at %2")
                .arg(chanid).arg(starttime));
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    pginfo.QueryCommBreakList(cutlist);
    for (it = cutlist.begin(); it != cutlist.end(); ++it)
        if (*it == MARK_COMM_START)
            cutlist[it.key()] = MARK_CUT_START;
        else
            cutlist[it.key()] = MARK_CUT_END;
    pginfo.SaveCutList(cutlist);

    return GENERIC_EXIT_OK;
}

static int ClearSkipList(QString chanid, QString starttime)
{
    QDateTime recstartts = myth_dt_from_string(starttime);
    const ProgramInfo pginfo(chanid.toUInt(), recstartts);

    if (!pginfo.GetChanID())
    {
        VERBOSE(VB_IMPORTANT,
                QString("No program data exists for channel %1 at %2")
                .arg(chanid).arg(starttime));
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    frm_dir_map_t skiplist;
    pginfo.SaveCommBreakList(skiplist);

    VERBOSE(VB_IMPORTANT, "Commercial skip list cleared");

    return GENERIC_EXIT_OK;
}

static int SetCutList(QString chanid, QString starttime, QString newCutList)
{
    frm_dir_map_t cutlist;

    newCutList.replace(QRegExp(" "), "");

    QStringList tokens = newCutList.split(",", QString::SkipEmptyParts);

    for (int i = 0; i < tokens.size(); i++)
    {
        QStringList cutpair = tokens[i].split("-", QString::SkipEmptyParts);
        cutlist[cutpair[0].toInt()] = MARK_CUT_START;
        cutlist[cutpair[1].toInt()] = MARK_CUT_END;
    }

    QDateTime recstartts = myth_dt_from_string(starttime);
    const ProgramInfo pginfo(chanid.toUInt(), recstartts);

    if (!pginfo.GetChanID())
    {
        VERBOSE(VB_IMPORTANT,
                QString("No program data exists for channel %1 at %2")
                .arg(chanid).arg(starttime));
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    pginfo.SaveCutList(cutlist);

    VERBOSE(VB_IMPORTANT, QString("Cutlist set to: %1").arg(newCutList));

    return GENERIC_EXIT_OK;
}

static int GetMarkupList(QString list, QString chanid, QString starttime)
{
    frm_dir_map_t cutlist;
    frm_dir_map_t::const_iterator it;
    QString result;

    QDateTime recstartts = myth_dt_from_string(starttime);
    const ProgramInfo pginfo(chanid.toUInt(), recstartts);

    if (!pginfo.GetChanID())
    {
        VERBOSE(VB_IMPORTANT,
                QString("No program data exists for channel %1 at %2")
                .arg(chanid).arg(starttime));
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    if (list == "cutlist")
        pginfo.QueryCutList(cutlist);
    else
        pginfo.QueryCommBreakList(cutlist);

    uint64_t lastStart = 0;
    for (it = cutlist.begin(); it != cutlist.end(); ++it)
    {
        if ((*it == MARK_COMM_START) ||
            (*it == MARK_CUT_START))
        {
            if (!result.isEmpty())
                result += ",";
            lastStart = it.key();
            result += QString("%1-").arg(lastStart);
        }
        else
        {
            if (result.isEmpty())
                result += "0-";
            result += QString("%1").arg(it.key());
        }
    }

    if (result.endsWith('-'))
    {
        uint64_t lastFrame = pginfo.QueryLastFrameInPosMap() + 60;
        if (lastFrame > lastStart)
            result += QString("%1").arg(lastFrame);
    }

    if (list == "cutlist")
        cout << QString("Cutlist: %1\n").arg(result).toLocal8Bit().constData();
    else
    {
        cout << QString("Commercial Skip List: %1\n")
            .arg(result).toLocal8Bit().constData();
    }

    return GENERIC_EXIT_OK;
}

static void streamOutCommercialBreakList(
    ostream &output, const frm_dir_map_t &commercialBreakList)
{
    if (!quiet)
        output << "----------------------------" << endl;

    if (commercialBreakList.empty())
    {
        if (!quiet)
            output << "No breaks" << endl;
    }
    else
    {
        frm_dir_map_t::const_iterator it = commercialBreakList.begin();
        for (; it != commercialBreakList.end(); it++)
        {
            output << "framenum: " << it.key() << "\tmarktype: " << *it
                   << endl;
        }
    }

    if (!quiet)
        output << "----------------------------" << endl;
}

static void print_comm_flag_output(
    const ProgramInfo          *program_info,
    const frm_dir_map_t        &commBreakList,
    uint64_t                    frame_count,
    const CommDetectorBase     *commDetect,
    const QString              &output_filename)
{
    if (output_filename.isEmpty())
        return;

    ostream *out = &cout;
    if (output_filename != "-")
    {
        QByteArray tmp = output_filename.toLocal8Bit();
        out = new fstream(tmp.constData(), ios::app | ios::out );
    }

    if (!quiet)
    {
        QString tmp = "";
        if (program_info->GetChanID())
        {
            tmp = QString("commercialBreakListFor: %1 on %2 @ %3")
                .arg(program_info->GetTitle())
                .arg(program_info->GetChanID())
                .arg(program_info->GetRecordingStartTime(ISODate));
        }
        else
        {
            tmp = QString("commercialBreakListFor: %1")
                .arg(program_info->GetPathname());
        }

        const QByteArray tmp2 = tmp.toLocal8Bit();
        *out << tmp2.constData() << endl;

        if (frame_count)
            *out << "totalframecount: " << frame_count << endl;
    }

    if (commDetect)
        commDetect->PrintFullMap(*out, &commBreakList, !quiet);
    else
        streamOutCommercialBreakList(*out, commBreakList);

    if (output_filename != "-")
        delete out;
}

static void commDetectorBreathe()
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
                    JobQueue::ChangeJobStatus(jobID, JOB_RUNNING,
                                              QObject::tr("Running"));
                    commDetector->resume();
                    break;
                }
        }
    }
}

static void commDetectorStatusUpdate(const QString& status)
{
    if (jobID != -1)
    {
        JobQueue::ChangeJobStatus(jobID, JOB_RUNNING,  status);
        JobQueue::ChangeJobComment(jobID,  status);
    }
}

static void commDetectorGotNewCommercialBreakList(void)
{
    frm_dir_map_t newCommercialMap;
    commDetector->GetCommercialBreakList(newCommercialMap);

    frm_dir_map_t::Iterator it = newCommercialMap.begin();
    QString message = "COMMFLAG_UPDATE ";
    message += global_program_info->MakeUniqueKey();

    for (it = newCommercialMap.begin();
            it != newCommercialMap.end(); ++it)
    {
        if (it != newCommercialMap.begin())
            message += ",";
        else
            message += " ";
        message += QString("%1:%2").arg(it.key())
                   .arg(*it);
    }

    VERBOSE(VB_COMMFLAG,
            QString("mythcommflag sending update: %1").arg(message));

    RemoteSendMessage(message);
}

static void incomingCustomEvent(QEvent* e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        message = message.simplified();
        QStringList tokens = message.split(" ", QString::SkipEmptyParts);

        VERBOSE(VB_COMMFLAG,
                QString("mythcommflag: Received Event: '%1'")
                .arg(message));

        if ((watchingRecording) && (tokens.size() >= 3) &&
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

        if ((tokens.size() >= 2) && (tokens[0] == "COMMFLAG_REQUEST"))
        {
            uint chanid = 0;
            QDateTime recstartts;
            ProgramInfo::ExtractKey(tokens[1], chanid, recstartts);

            message = QString("mythcommflag: Received a "
                              "COMMFLAG_REQUEST event for chanid %1 @ %2.  ")
                .arg(chanid).arg(recstartts.toString());

            if ((global_program_info->GetChanID()             == chanid) &&
                (global_program_info->GetRecordingStartTime() == recstartts))
            {
                commDetector->requestCommBreakMapUpdate();
                message += "Requested CommDetector to generate new break list.";
                VERBOSE(VB_COMMFLAG, message);
            }
        }
    }
}

static int DoFlagCommercials(
    ProgramInfo *program_info,
    bool showPercentage, bool fullSpeed, bool inJobQueue,
    MythCommFlagPlayer* cfp, enum SkipTypes commDetectMethod,
    const QString &outputfilename, bool useDB)
{
    CommDetectorFactory factory;
    commDetector = factory.makeCommDetector(
        commDetectMethod, showPercentage,
        fullSpeed, cfp,
        program_info->GetChanID(),
        program_info->GetScheduledStartTime(),
        program_info->GetScheduledEndTime(),
        program_info->GetRecordingStartTime(),
        program_info->GetRecordingEndTime(), useDB);

    if (inJobQueue && useDB)
    {
        jobID = JobQueue::GetJobID(
            JOB_COMMFLAG,
            program_info->GetChanID(), program_info->GetRecordingStartTime());

        if (jobID != -1)
            VERBOSE(VB_COMMFLAG,
                QString("mythcommflag processing JobID %1").arg(jobID));
        else
            VERBOSE(VB_COMMFLAG, "mythcommflag: Unable to determine jobID");
    }

    if (useDB)
        program_info->SaveCommFlagged(COMM_FLAG_PROCESSING);

    CustomEventRelayer *cer = new CustomEventRelayer(incomingCustomEvent);
    SlotRelayer *a = new SlotRelayer(commDetectorBreathe);
    SlotRelayer *b = new SlotRelayer(commDetectorStatusUpdate);
    SlotRelayer *c = new SlotRelayer(commDetectorGotNewCommercialBreakList);
    QObject::connect(commDetector, SIGNAL(breathe()),
                     a,            SLOT(relay()));
    QObject::connect(commDetector, SIGNAL(statusUpdate(const QString&)),
                     b,            SLOT(relay(const QString&)));
    QObject::connect(commDetector, SIGNAL(gotNewCommercialBreakList()),
                     c,            SLOT(relay()));

    if (useDB)
    {
        VERBOSE(VB_COMMFLAG, "mythcommflag sending COMMFLAG_START notification");
        QString message = "COMMFLAG_START ";
        message += program_info->MakeUniqueKey();
        RemoteSendMessage(message);
    }

    bool result = commDetector->go();
    int comms_found = 0;

    if (result)
    {
        cfp->SaveTotalDuration();

        frm_dir_map_t commBreakList;
        commDetector->GetCommercialBreakList(commBreakList);
        comms_found = commBreakList.size() / 2;

        if (!dontSubmitCommbreakListToDB)
        {
            program_info->SaveMarkupFlag(MARK_UPDATED_CUT);
            program_info->SaveCommBreakList(commBreakList);
            program_info->SaveCommFlagged(COMM_FLAG_DONE);
        }

        print_comm_flag_output(
            program_info, commBreakList, cfp->GetTotalFrameCount(),
            (outputMethod == kOutputMethodFull) ? commDetector : NULL,
            outputfilename);
    }
    else
    {
        if (!dontSubmitCommbreakListToDB && useDB)
            program_info->SaveCommFlagged(COMM_FLAG_NOT_FLAGGED);
    }

    CommDetectorBase *tmp = commDetector;
    commDetector = NULL;
    sleep(1);
    tmp->deleteLater();

    cer->deleteLater();
    c->deleteLater();
    b->deleteLater();
    a->deleteLater();

    return comms_found;
}

static int FlagCommercials(
    ProgramInfo *program_info, const QString &outputfilename, bool useDB)
{
    global_program_info = program_info;

    int breaksFound = 0;

    if (!useDB && COMM_DETECT_UNINIT == commDetectMethod)
        commDetectMethod = COMM_DETECT_ALL;

    if (commDetectMethod == COMM_DETECT_UNINIT)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT commmethod FROM channel "
                        "WHERE chanid = :CHANID;");
        query.bindValue(":CHANID", program_info->GetChanID());

        if (!query.exec())
        {
            MythDB::DBError("FlagCommercials", query);
        }
        else if (query.next())
        {
            commDetectMethod = (enum SkipTypes)query.value(0).toInt();
            if (commDetectMethod == COMM_DETECT_COMMFREE)
            {
                commDetectMethod = COMM_DETECT_UNINIT;
                VERBOSE(VB_COMMFLAG,
                        QString("Chanid %1 is marked as being Commercial Free, "
                                "we will use the default commercial detection "
                                "method").arg(program_info->GetChanID()));
            }
            else if (commDetectMethod != COMM_DETECT_UNINIT)
                VERBOSE(VB_COMMFLAG, QString("Using method: %1 from channel %2")
                        .arg(commDetectMethod).arg(program_info->GetChanID()));
        }
    }

    if (commDetectMethod == COMM_DETECT_UNINIT)
        commDetectMethod = (enum SkipTypes)gCoreContext->GetNumSetting(
                                    "CommercialSkipMethod", COMM_DETECT_ALL);
    frm_dir_map_t blanks;
    recorder = NULL;

    if (onlyDumpDBCommercialBreakList)
    {
        frm_dir_map_t commBreakList;
        program_info->QueryCommBreakList(commBreakList);

        print_comm_flag_output(program_info, commBreakList,
                               0, NULL, outputfilename);

        global_program_info = NULL;
        return GENERIC_EXIT_OK;
    }


    if (!quiet)
    {
        QString chanid =
            QString::number(program_info->GetChanID())
            .leftJustified(6, ' ', true);
        QString recstartts = program_info->GetRecordingStartTime(MythDate)
            .leftJustified(14, ' ', true);
        QString title = program_info->GetTitle()
            .leftJustified(41, ' ', true);

        QString outstr = QString("%1 %2 %3 ")
            .arg(chanid).arg(recstartts).arg(title);
        QByteArray out = outstr.toLocal8Bit();

        cerr << out.constData() << flush;
    }

    if (!force && JobQueue::IsJobRunning(JOB_COMMFLAG, *program_info))
    {
        if (!quiet)
        {
            cerr << "IN USE\n";
            cerr << "                        "
                    "(the program is already being flagged elsewhere)\n";
        }
        global_program_info = NULL;
        return GENERIC_EXIT_IN_USE;
    }

    QString filename = get_filename(program_info);
    long long size = 0;
    bool exists = false;

    if (filename.startsWith("myth://"))
    {
        RemoteFile remotefile(filename, false, false, 0);
        struct stat filestat;

        if (remotefile.Exists(filename, &filestat))
        {
            exists = true;
            size = filestat.st_size;
        }
    }
    else
    {
        QFile file(filename);
        if (file.exists())
        {
            exists = true;
            size = file.size();
        }
    }

    if (!exists)
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find file %1, aborting.")
                .arg(filename));
        global_program_info = NULL;
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    if (size == 0)
    {
        VERBOSE(VB_IMPORTANT, QString("File %1 is zero-byte, aborting.")
                .arg(filename));
        global_program_info = NULL;
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    RingBuffer *tmprbuf = RingBuffer::Create(filename, false);
    if (!tmprbuf)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to create RingBuffer for %1").arg(filename));
        global_program_info = NULL;
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    if (useDB && !MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "Unable to open commflag DB connection");
        delete tmprbuf;
        global_program_info = NULL;
        return GENERIC_EXIT_DB_ERROR;
    }

    MythCommFlagPlayer *cfp = new MythCommFlagPlayer();

    PlayerContext *ctx = new PlayerContext(kFlaggerInUseID);

    AVSpecialDecode sp = (AVSpecialDecode)
        (kAVSpecialDecode_LowRes         |
         kAVSpecialDecode_SingleThreaded |
         kAVSpecialDecode_NoLoopFilter);

    /* blank detector needs to be only sample center for this optimization. */
    if ((COMM_DETECT_BLANKS  == commDetectMethod) ||
        (COMM_DETECT_2_BLANK == commDetectMethod))
    {
        sp = (AVSpecialDecode) (sp | kAVSpecialDecode_FewBlocks);
    }

    ctx->SetSpecialDecode(sp);

    ctx->SetPlayingInfo(program_info);
    ctx->SetRingBuffer(tmprbuf);
    ctx->SetPlayer(cfp);
    cfp->SetPlayerInfo(NULL, NULL, true, ctx);

    if (rebuildSeekTable)
    {
        cfp->RebuildSeekTable(!quiet);

        if (!quiet)
            cerr << "Rebuilt\n";

        delete ctx;
        global_program_info = NULL;

        return GENERIC_EXIT_OK;
    }

    if (program_info->GetRecordingEndTime() > QDateTime::currentDateTime())
    {
        gCoreContext->ConnectToMasterServer();

        recorder = RemoteGetExistingRecorder(program_info);
        if (recorder && (recorder->GetRecorderNumber() != -1))
        {
            recorderNum =  recorder->GetRecorderNumber();
            watchingRecording = true;
            ctx->SetRecorder(recorder);

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
        cfp->SetWatchingRecording(watchingRecording);
    }

    int fakeJobID = -1;
    if (!inJobQueue && useDB)
    {
        JobQueue::QueueJob(
            JOB_COMMFLAG,
            program_info->GetChanID(), program_info->GetRecordingStartTime(),
            "", "", gCoreContext->GetHostName(), JOB_EXTERNAL, JOB_RUNNING);

        fakeJobID = JobQueue::GetJobID(
            JOB_COMMFLAG,
            program_info->GetChanID(), program_info->GetRecordingStartTime());

        jobID = fakeJobID;
        VERBOSE(VB_COMMFLAG,
            QString("Not in JobQueue, creating fake Job ID %1").arg(jobID));
    }
    else
        jobID = -1;


    breaksFound = DoFlagCommercials(
        program_info, showPercentage, fullSpeed, inJobQueue,
        cfp, commDetectMethod, outputfilename, useDB);

    if (fakeJobID >= 0)
    {
        jobID = -1;
        JobQueue::ChangeJobStatus(fakeJobID, JOB_FINISHED,
            QObject::tr("Finished, %n break(s) found.", "", breaksFound));
    }

    if (!quiet)
        cerr << breaksFound << "\n";

    delete ctx;
    global_program_info = NULL;

    return breaksFound;
}

static int FlagCommercials(
    uint chanid, const QString &starttime,
    const QString &outputfilename, bool useDB)
{
    QDateTime recstartts = myth_dt_from_string(starttime);
    ProgramInfo pginfo(chanid, recstartts);

    if (!pginfo.GetChanID())
    {
        VERBOSE(VB_IMPORTANT,
                QString("No program data exists for channel %1 at %2")
                .arg(chanid).arg(starttime));
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    int ret = FlagCommercials(&pginfo, outputfilename, useDB);

    return ret;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    bool isVideo = false;
    int result = GENERIC_EXIT_OK;

    QString filename;
    QString outputfilename = QString::null;

    uint chanid = 0;
    QString starttime;
    QString allStart = "19700101000000";
    QString allEnd   = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
    int jobID = -1;
    int jobType = JOB_NONE;
    QDir fullfile;
    time_t time_now;
    bool useDB = true;
    bool allRecorded = false;
    bool queueJobInstead = false;
    QString newCutList = QString::null;

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHCOMMFLAG);

    print_verbose_messages = VB_IMPORTANT;
    verboseString = "important";

    MythCommFlagCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("verbose"))
        if (parse_verbose_arg(cmdline.toString("verbose")) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
            return GENERIC_EXIT_INVALID_CMDLINE;

    if (!cmdline.toString("chanid").isEmpty())
        chanid = cmdline.toUInt("chanid");
    if (!cmdline.toString("starttime").isEmpty())
        starttime = cmdline.toString("starttime");
    if (!cmdline.toString("file").isEmpty())
    {
        filename = cmdline.toString("file");
        fullfile = cmdline.toString("file");
    }
    if (!cmdline.toString("video").isEmpty())
    {
        filename = cmdline.toString("video");
        isVideo = true;
        rebuildSeekTable = true;
        beNice = false;
    }

    if (cmdline.toBool("commmethod"))
    {
        QString method = cmdline.toString("commmethod");
        bool ok;
        commDetectMethod = (SkipTypes) method.toInt(&ok);
        if (!ok)
        {
            commDetectMethod = COMM_DETECT_OFF;
            bool off_seen = false;
            QMap<QString,SkipTypes>::const_iterator sit;
            QStringList list =
                method.split(",", QString::SkipEmptyParts);
            QStringList::const_iterator it = list.begin();
            for (; it != list.end(); ++it)
            {
                QString val = (*it).toLower();
                QByteArray aval = val.toAscii();
                off_seen |= val == "off";
                sit = skipTypes->find(val);
                if (sit == skipTypes->end())
                {
                    cerr << "Failed to decode --method option '"
                         << aval.constData() << "'" << endl;
                    return -1;
                }
                commDetectMethod = (SkipTypes)
                    ((int)commDetectMethod | (int)*sit);
            }
            if (COMM_DETECT_OFF == commDetectMethod)
                commDetectMethod = COMM_DETECT_UNINIT;
        }
    }
    if (cmdline.toBool("outputmethod"))
    {
        QString method = cmdline.toString("outputmethod");
        bool ok;
        outputMethod = (OutputMethod) method.toInt(&ok);
        if (!ok)
        {
            outputMethod = kOutputMethodEssentials;
            QString val = method.toLower();
            QByteArray aval = val.toAscii();
            QMap<QString,OutputMethod>::const_iterator it =
                outputTypes->find(val);
            if (it == outputTypes->end())
            {
                cerr << "Failed to decode --outputmethod option '"
                     << aval.constData() << "'" << endl;
                return -1;
            }
            outputMethod = (OutputMethod) *it;
        }
    }

    if (cmdline.toBool("skipdb"))
    {
        useDB = false;
        dontSubmitCommbreakListToDB = true;
        force = true;
    }
    if (parse_verbose_arg(cmdline.toString("verbose")) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
        return GENERIC_EXIT_INVALID_CMDLINE;
    if (cmdline.toBool("verboseint"))
        print_verbose_messages = cmdline.toUInt("verboseint");

    if (cmdline.toBool("quiet"))
    {
        quiet = cmdline.toUInt("quiet");
        showPercentage = false;
        if (quiet > 1)
        {
            print_verbose_messages = VB_NONE;
            parse_verbose_arg("none");
        }
    }

    int facility = cmdline.GetSyslogFacility();

    if (cmdline.toBool("queue"))
        queueJobInstead = true;
    if (cmdline.toBool("nopercent"))
        showPercentage = false;
    if (cmdline.toBool("rebuild"))
    {
        rebuildSeekTable = true;
        beNice = false;
    }
    if (cmdline.toBool("force"))
        force = true;
    if (cmdline.toBool("dontwritedb"))
        dontSubmitCommbreakListToDB = true;
    if (cmdline.toBool("dumpdb"))
        onlyDumpDBCommercialBreakList = true;
    if (cmdline.toBool("outputfile"))
    {
        outputfilename = cmdline.toString("outputfile");
        fstream output(outputfilename.toLocal8Bit().constData(), ios::out);
    }

    CleanupGuard callCleanup(cleanup);

    QString logfile = cmdline.GetLogFilePath();
    logStart(logfile, quiet, facility);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(
            false/*use gui*/, false/*prompt for backend*/,
            false/*bypass auto discovery*/, !useDB/*ignoreDB*/))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    QMap<QString, QString> settingsOverride = cmdline.GetSettingsOverride();
    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(*it));
            gCoreContext->OverrideSettingForSession(it.key(), *it);
        }
    }

    MythTranslation::load("mythfrontend");

    if ((fullfile.path() != ".") && useDB)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid, starttime FROM recorded "
                      "WHERE basename = :BASENAME ;");
        query.bindValue(":BASENAME", fullfile.dirName());

        if (query.exec() && query.next())
        {
            chanid = query.value(0).toUInt();
            starttime = query.value(1).toDateTime().toString("yyyyMMddhhmmss");
        }
        else
        {
            cerr << "mythcommflag: ERROR: Unable to find DB info for "
                 << fullfile.dirName().toLocal8Bit().constData() << endl;
            return GENERIC_EXIT_NO_RECORDING_DATA;
        }
    }

    if ((!chanid && !starttime.isEmpty()) ||
        (chanid && starttime.isEmpty()))
    {
        VERBOSE(VB_IMPORTANT, "You must specify both the Channel ID "
                "and the Start Time.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("clearskiplist"))
        return ClearSkipList(QString::number(chanid), starttime);

    if (cmdline.toBool("gencutlist"))
        return CopySkipListToCutList(QString::number(chanid), starttime);

    if (cmdline.toBool("clearcutlist"))
        return SetCutList(QString::number(chanid), starttime, "");

    if (cmdline.toBool("setcutlist"))
        return SetCutList(QString::number(chanid), starttime, cmdline.toString("setcutlist"));

    if (cmdline.toBool("getcutlist"))
        return GetMarkupList("cutlist", QString::number(chanid), starttime);

    if (cmdline.toBool("getskiplist"))
        return GetMarkupList("commflag", QString::number(chanid), starttime);

    if (cmdline.toBool("jobid"))
    {
        jobID = cmdline.toInt("jobid");
        if (!JobQueue::GetJobInfoFromID(jobID, jobType, chanid, starttime))
        {
            cerr << "mythcommflag: ERROR: Unable to find DB info for "
                 << "JobQueue ID# " << jobID << endl;
            return GENERIC_EXIT_NO_RECORDING_DATA;
        }
        inJobQueue = true;
        force = true;
        int jobQueueCPU = gCoreContext->GetNumSetting("JobQueueCPU", 0);

        if (jobQueueCPU < 2)
        {
            myth_nice(17);
            myth_ioprio((0 == jobQueueCPU) ? 8 : 7);
        }

        fullSpeed = jobQueueCPU != 0;

        quiet = true;
        isVideo = false;
        showPercentage = false;

        int breaksFound = FlagCommercials(
            chanid, starttime, outputfilename, useDB);

        return breaksFound; // exit(breaksFound);
    }

    // be nice to other programs since FlagCommercials() can consume 100% CPU
    if (beNice)
    {
        myth_nice(17);
        myth_ioprio(7);
    }

    time_now = time(NULL);
    if (!quiet)
    {
        VERBOSE(VB_IMPORTANT, QString("%1 version: %2 www.mythtv.org")
                  .arg(MYTH_APPNAME_MYTHCOMMFLAG).arg(MYTH_BINARY_VERSION));

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
            cerr << "Building seek table for: "
                 << filename.toLocal8Bit().constData() << endl;
        }
    }

    if (isVideo)
    {
        ProgramInfo pginfo(filename);
        result = BuildVideoMarkup(&pginfo, useDB);
    }
    else if (chanid && !starttime.isEmpty())
    {
        if (queueJobInstead)
            QueueCommFlagJob(chanid, starttime);
        else
            result = FlagCommercials(chanid, starttime, outputfilename, useDB);
    }
    else if (!useDB)
    {
        if (!QFile::exists(filename))
        {
            VERBOSE(VB_IMPORTANT, QString("Filename: '%1' does not exist")
                    .arg(filename));
            return -1;
        }

        ProgramInfo *pginfo = new ProgramInfo(filename);
        PMapDBReplacement *pmap = new PMapDBReplacement();
        pginfo->SetPositionMapDBReplacement(pmap);

        // RingBuffer doesn't like relative pathnames
        if (filename.left(1) != "/" && !filename.contains("://"))
        {
            pginfo->SetPathname(
                QDir::currentPath() + '/' + pginfo->GetPathname());
        }

        if ((filename.right(3).toLower() == "mpg") ||
            (filename.right(4).toLower() == "mpeg"))
        {
            result = BuildVideoMarkup(pginfo, useDB);
            if (result != GENERIC_EXIT_OK)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to build video markup");
                return result;
            }
        }

        result = FlagCommercials(pginfo, outputfilename, useDB);

        delete pginfo;
        delete pmap;
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

                chanid = query.value(0).toUInt();

                if ( allRecorded )
                {
                    if (queueJobInstead)
                        QueueCommFlagJob(chanid, starttime);
                    else
                    {
                        FlagCommercials(chanid, starttime,
                                        outputfilename, useDB);
                    }
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
                            {
                                if (queueJobInstead)
                                    QueueCommFlagJob(chanid, starttime);
                                else
                                {
                                    FlagCommercials(chanid, starttime,
                                                    outputfilename, useDB);
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            MythDB::DBError("Querying recorded programs", query);
            return GENERIC_EXIT_DB_ERROR;
        }
    }

    time_now = time(NULL);

    if (!quiet)
    {
        cerr << "\nFinished commercial break flagging at "
             << ctime(&time_now) << endl;
    }

    return result;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
