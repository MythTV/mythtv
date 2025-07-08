
#if defined ANDROID && __ANDROID_API__ < 24
// ftello and fseeko do not exist in android before api level 24
#define ftello ftell
#define fseeko fseek
#endif

// POSIX headers
#include <unistd.h>

// C++ headers
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

// Qt headers
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QString>
#include <QtGlobal>

// MythTV headers
#include "libmyth/mythcontext.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythappname.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/remoteutil.h"
#include "libmythtv/io/mythmediabuffer.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/mythcommflagplayer.h"
#include "libmythtv/remoteencoder.h"
#include "libmythtv/tvremoteutil.h"

#include "mythcommflag_commandlineparser.h"

// Commercial Flagging headers
#include "CommDetectorBase.h"
#include "CommDetectorFactory.h"
#include "CustomEventRelayer.h"
#include "SlotRelayer.h"

#define LOC      QString("MythCommFlag: ")
#define LOC_WARN QString("MythCommFlag, Warning: ")
#define LOC_ERR  QString("MythCommFlag, Error: ")

int  quiet = 0;
bool progress = true;
bool force = false;

MythCommFlagCommandLineParser cmdline;

bool watchingRecording = false;
CommDetectorBase* commDetector = nullptr;
RemoteEncoder* recorder = nullptr;
ProgramInfo *global_program_info = nullptr;
int recorderNum = -1;

int jobID = -1;
int lastCmd = -1;

static QMap<QString,SkipType> *init_skip_types();
QMap<QString,SkipType> *skipTypes = init_skip_types();

static QMap<QString,SkipType> *init_skip_types(void)
{
    auto *tmp = new QMap<QString,SkipType>;
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

enum OutputMethod : std::uint8_t
{
    kOutputMethodEssentials = 1,
    kOutputMethodFull,
};
OutputMethod outputMethod = kOutputMethodEssentials;

static QMap<QString,OutputMethod> *init_output_types();
QMap<QString,OutputMethod> *outputTypes = init_output_types();

static QMap<QString,OutputMethod> *init_output_types(void)
{
    auto *tmp = new QMap<QString,OutputMethod>;
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

static int QueueCommFlagJob(uint chanid, const QDateTime& starttime, bool rebuild)
{
    QString startstring = MythDate::toString(starttime, MythDate::kFilename);
    const ProgramInfo pginfo(chanid, starttime);

    if (!pginfo.GetChanID())
    {
        if (progress)
        {
            QString tmp = QString(
                "Unable to find program info for chanid %1 @ %2")
                .arg(chanid).arg(startstring);
            std::cerr << tmp.toLocal8Bit().constData() << std::endl;
        }
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    if (cmdline.toBool("dryrun"))
    {
        QString tmp = QString("Job have been queued for chanid %1 @ %2")
                        .arg(chanid).arg(startstring);
        std::cerr << tmp.toLocal8Bit().constData() << std::endl;
        return GENERIC_EXIT_OK;
    }

    bool result = JobQueue::QueueJob(JOB_COMMFLAG,
        pginfo.GetChanID(), pginfo.GetRecordingStartTime(), "", "", "",
        rebuild ? JOB_REBUILD : 0, JOB_QUEUED, QDateTime());

    if (result)
    {
        if (progress)
        {
            QString tmp = QString("Job Queued for chanid %1 @ %2")
                .arg(chanid).arg(startstring);
            std::cerr << tmp.toLocal8Bit().constData() << std::endl;
        }
        return GENERIC_EXIT_OK;
    }

    if (progress)
    {
        QString tmp = QString("Error queueing job for chanid %1 @ %2")
            .arg(chanid).arg(startstring);
        std::cerr << tmp.toLocal8Bit().constData() << std::endl;
    }
    return GENERIC_EXIT_DB_ERROR;
}

static void streamOutCommercialBreakList(
    std::ostream &output, const frm_dir_map_t &commercialBreakList)
{
    if (progress)
        output << "----------------------------" << std::endl;

    if (commercialBreakList.empty())
    {
        if (progress)
            output << "No breaks" << std::endl;
    }
    else
    {
        frm_dir_map_t::const_iterator it = commercialBreakList.begin();
        for (; it != commercialBreakList.end(); ++it)
        {
            output << "framenum: " << it.key() << "\tmarktype: " << *it
                   << std::endl;
        }
    }

    if (progress)
        output << "----------------------------" << std::endl;
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

    std::ostream *out = &std::cout;
    if (output_filename != "-")
    {
        QByteArray tmp = output_filename.toLocal8Bit();
        out = new std::fstream(tmp.constData(), std::ios::app | std::ios::out );
    }

    if (progress)
    {
        QString tmp = "";
        if (program_info->GetChanID())
        {
            tmp = QString("commercialBreakListFor: %1 on %2 @ %3")
                .arg(program_info->GetTitle())
                .arg(program_info->GetChanID())
                .arg(program_info->GetRecordingStartTime(MythDate::ISODate));
        }
        else
        {
            tmp = QString("commercialBreakListFor: %1")
                .arg(program_info->GetPathname());
        }

        const QByteArray tmp2 = tmp.toLocal8Bit();
        *out << tmp2.constData() << std::endl;

        if (frame_count)
            *out << "totalframecount: " << frame_count << std::endl;
    }

    if (commDetect)
        commDetect->PrintFullMap(*out, &commBreakList, progress);
    else
        streamOutCommercialBreakList(*out, commBreakList);

    if (out != &std::cout)
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
                        QCoreApplication::translate("(mythcommflag)",
                                                    "Paused", "Job status"));
                    commDetector->pause();
                    break;
                }
                case JOB_RESUME:
                {
                    JobQueue::ChangeJobStatus(jobID, JOB_RUNNING,
                        QCoreApplication::translate("(mythcommflag)",
                                                    "Running", "Job status"));
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

    QString message = "COMMFLAG_UPDATE ";
    message += global_program_info->MakeUniqueKey();

    for (auto it = newCommercialMap.begin();
            it != newCommercialMap.end(); ++it)
    {
        if (it != newCommercialMap.begin())
            message += ",";
        else
            message += " ";
        message += QString("%1:%2").arg(it.key())
                   .arg(*it);
    }

    LOG(VB_COMMFLAG, LOG_INFO,
        QString("mythcommflag sending update: %1").arg(message));

    gCoreContext->SendMessage(message);
}

static void incomingCustomEvent(QEvent* e)
{
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(e);
        if (me == nullptr)
            return;

        QString message = me->Message();

        message = message.simplified();
        QStringList tokens = message.split(" ", Qt::SkipEmptyParts);

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("mythcommflag: Received Event: '%1'") .arg(message));

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
                LOG(VB_COMMFLAG, LOG_INFO, message);
            }
        }

        if ((tokens.size() >= 2) && (tokens[0] == "COMMFLAG_REQUEST"))
        {
            uint chanid = 0;
            QDateTime recstartts;
            ProgramInfo::ExtractKey(tokens[1], chanid, recstartts);

            message = QString("mythcommflag: Received a "
                              "COMMFLAG_REQUEST event for chanid %1 @ %2.  ")
                .arg(chanid).arg(recstartts.toString(Qt::ISODate));

            if ((global_program_info->GetChanID()             == chanid) &&
                (global_program_info->GetRecordingStartTime() == recstartts))
            {
                commDetector->requestCommBreakMapUpdate();
                message += "Requested CommDetector to generate new break list.";
                LOG(VB_COMMFLAG, LOG_INFO, message);
            }
        }
    }
}

static int DoFlagCommercials(
    ProgramInfo *program_info,
    bool showPercentage, bool fullSpeed, int jobid,
    MythCommFlagPlayer* cfp, SkipType commDetectMethod,
    const QString &outputfilename, bool useDB)
{
    commDetector = CommDetectorFactory::makeCommDetector(
        commDetectMethod, showPercentage,
        fullSpeed, cfp,
        program_info->GetChanID(),
        program_info->GetScheduledStartTime(),
        program_info->GetScheduledEndTime(),
        program_info->GetRecordingStartTime(),
        program_info->GetRecordingEndTime(), useDB);

    if (jobid > 0)
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("mythcommflag processing JobID %1").arg(jobid));

    if (useDB)
        program_info->SaveCommFlagged(COMM_FLAG_PROCESSING);

    auto *cer = new CustomEventRelayer(incomingCustomEvent);
    auto *a = new SlotRelayer(commDetectorBreathe);
    auto *b = new SlotRelayer(commDetectorStatusUpdate);
    auto *c = new SlotRelayer(commDetectorGotNewCommercialBreakList);
    QObject::connect(commDetector, &CommDetectorBase::breathe,
                     a,            qOverload<>(&SlotRelayer::relay));
    QObject::connect(commDetector, &CommDetectorBase::statusUpdate,
                     b,            qOverload<const QString&>(&SlotRelayer::relay));
    QObject::connect(commDetector, &CommDetectorBase::gotNewCommercialBreakList,
                     c,            qOverload<>(&SlotRelayer::relay));

    if (useDB)
    {
        LOG(VB_COMMFLAG, LOG_INFO,
            "mythcommflag sending COMMFLAG_START notification");
        QString message = "COMMFLAG_START ";
        message += program_info->MakeUniqueKey();
        gCoreContext->SendMessage(message);
    }

    bool result = commDetector->go();
    int comms_found = 0;

    if (result)
    {
        cfp->SaveTotalDuration();

        frm_dir_map_t commBreakList;
        commDetector->GetCommercialBreakList(commBreakList);
        comms_found = commBreakList.size() / 2;

        if (useDB)
        {
            program_info->SaveMarkupFlag(MARK_UPDATED_CUT);
            program_info->SaveCommBreakList(commBreakList);
            program_info->SaveCommFlagged(COMM_FLAG_DONE);
        }

        print_comm_flag_output(
            program_info, commBreakList, cfp->GetTotalFrameCount(),
            (outputMethod == kOutputMethodFull) ? commDetector : nullptr,
            outputfilename);
    }
    else
    {
        if (useDB)
            program_info->SaveCommFlagged(COMM_FLAG_NOT_FLAGGED);
    }

    CommDetectorBase *tmp = commDetector;
    commDetector = nullptr;
    sleep(1);
    tmp->deleteLater();

    cer->deleteLater();
    c->deleteLater();
    b->deleteLater();
    a->deleteLater();

    return comms_found;
}

static qint64 GetFileSize(ProgramInfo *program_info)
{
    QString filename = get_filename(program_info);
    qint64 size = -1;

    if (filename.startsWith("myth://"))
    {
        RemoteFile remotefile(filename, false, false, 0s);
        size = remotefile.GetFileSize();
    }
    else
    {
        QFile file(filename);
        if (file.exists())
        {
            size = file.size();
        }
    }

    return size;
}

static bool DoesFileExist(ProgramInfo *program_info)
{
    QString filename = get_filename(program_info);
    qint64 size = GetFileSize(program_info);

    if (size < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find file %1, aborting.")
                .arg(filename));
        return false;
    }

    if (size == 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("File %1 is zero-byte, aborting.")
                .arg(filename));
        return false;
    }

    return true;
}

static void UpdateFileSize(ProgramInfo *program_info)
{
    qint64 size = GetFileSize(program_info);

    if (size != (qint64)program_info->GetFilesize())
        program_info->SaveFilesize(size);
}

static bool IsMarked(uint chanid, const QDateTime& starttime)
{
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
    mark_query.bindValue(":STARTTIME", starttime);

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
                flagStatusStr = QString("Flagged with %1 breaks")
                                    .arg(marksFound / 2);
                break;
              case COMM_FLAG_PROCESSING:
                flagStatusStr = "Flagging";
                break;
              case COMM_FLAG_COMMFREE:
                flagStatusStr = "Commercial Free";
                break;
            }

            LOG(VB_COMMFLAG, LOG_INFO,
                QString("Status for chanid %1 @ %2 is '%3'")
                    .arg(QString::number(chanid),
                         starttime.toString(Qt::ISODate),
                         flagStatusStr));

            if ((flagStatus == COMM_FLAG_NOT_FLAGGED) && (marksFound == 0))
                return false;
        }
    }
    return true;
}

static int FlagCommercials(ProgramInfo *program_info, int jobid,
            const QString &outputfilename, bool useDB, bool fullSpeed)
{
    global_program_info = program_info;

    int breaksFound = 0;

    // configure commercial detection method
    SkipType commDetectMethod = (SkipType)gCoreContext->GetNumSetting(
                                    "CommercialSkipMethod", COMM_DETECT_ALL);

    if (cmdline.toBool("commmethod"))
    {
        // pull commercial detection method from command line
        QString commmethod = cmdline.toString("commmethod");

        // assume definition as integer value
        bool ok = true;
        commDetectMethod = (SkipType) commmethod.toInt(&ok);
        if (!ok)
        {
            // not an integer, attempt comma separated list
            commDetectMethod = COMM_DETECT_UNINIT;

            QStringList list = commmethod.split(",", Qt::SkipEmptyParts);
            for (const auto & it : std::as_const(list))
            {
                QString val = it.toLower();
                if (val == "off")
                {
                    commDetectMethod = COMM_DETECT_OFF;
                    break;
                }

                if (!skipTypes->contains(val))
                {
                    std::cerr << "Failed to decode --method option '"
                         << val.toLatin1().constData()
                         << "'" << std::endl;
                    return GENERIC_EXIT_INVALID_CMDLINE;
                }

                if (commDetectMethod == COMM_DETECT_UNINIT) {
                    commDetectMethod = skipTypes->value(val);
                } else {
                    commDetectMethod = (SkipType) ((int)commDetectMethod
                                                  | (int)skipTypes->value(val));
                }
            }

        }
        if (commDetectMethod == COMM_DETECT_UNINIT)
            return GENERIC_EXIT_INVALID_CMDLINE;
    }
    else if (useDB)
    {
        // if not manually specified, and we have a database to access
        // pull the commflag type from the channel
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT commmethod FROM channel "
                        "WHERE chanid = :CHANID;");
        query.bindValue(":CHANID", program_info->GetChanID());

        if (!query.exec())
        {
            // if the query fails, return with an error
            commDetectMethod = COMM_DETECT_UNINIT;
            MythDB::DBError("FlagCommercials", query);
        }
        else if (query.next())
        {
            commDetectMethod = (SkipType)query.value(0).toInt();
            if (commDetectMethod == COMM_DETECT_COMMFREE)
            {
                // if the channel is commercial free, drop to the default instead
                commDetectMethod = (SkipType)gCoreContext->GetNumSetting(
                                    "CommercialSkipMethod", COMM_DETECT_ALL);
                LOG(VB_COMMFLAG, LOG_INFO,
                        QString("Chanid %1 is marked as being Commercial Free, "
                                "we will use the default commercial detection "
                                "method").arg(program_info->GetChanID()));
            }
            else if (commDetectMethod == COMM_DETECT_UNINIT)
            {
                // no value set, so use the database default
                commDetectMethod = (SkipType)gCoreContext->GetNumSetting(
                                     "CommercialSkipMethod", COMM_DETECT_ALL);
            }
            LOG(VB_COMMFLAG, LOG_INFO,
                QString("Using method: %1 from channel %2")
                    .arg(commDetectMethod).arg(program_info->GetChanID()));
        }

    }
    else if (!useDB)
    {
        // default to a cheaper method for debugging purposes
        commDetectMethod = COMM_DETECT_BLANK;
    }

    // if selection has failed, or intentionally disabled, drop out
    if (commDetectMethod == COMM_DETECT_UNINIT)
        return GENERIC_EXIT_NOT_OK;
    if (commDetectMethod == COMM_DETECT_OFF)
        return GENERIC_EXIT_OK;

    recorder = nullptr;

/*
 * is there a purpose to this not fulfilled by --getskiplist?
    if (onlyDumpDBCommercialBreakList)
    {
        frm_dir_map_t commBreakList;
        program_info->QueryCommBreakList(commBreakList);

        print_comm_flag_output(program_info, commBreakList,
                               0, nullptr, outputfilename);

        global_program_info = nullptr;
        return GENERIC_EXIT_OK;
    }
*/

    if (!DoesFileExist(program_info))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Unable to find file in defined storage paths.");
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    QString filename = get_filename(program_info);

    MythMediaBuffer *tmprbuf = MythMediaBuffer::Create(filename, false);
    if (!tmprbuf)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Unable to create RingBuffer for %1").arg(filename));
        global_program_info = nullptr;
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    if (useDB)
    {
        if (!MSqlQuery::testDBConnection())
        {
            LOG(VB_GENERAL, LOG_ERR, "Unable to open commflag DB connection");
            delete tmprbuf;
            global_program_info = nullptr;
            return GENERIC_EXIT_DB_ERROR;
        }
    }

    auto flags = static_cast<PlayerFlags>(kAudioMuted | kVideoIsNull | kNoITV);

    int flagfast = gCoreContext->GetNumSetting("CommFlagFast", 0);
    if (flagfast)
    {
        // Note: These additional flags replicate the intent of the original
        // commit that enabled lowres support - but I'm not sure why it requires
        // single threaded decoding - which surely slows everything down? Though
        // there is probably no profile to enable multi-threaded decoding anyway.
        LOG(VB_GENERAL, LOG_INFO, "Enabling experimental flagging speedup (low resolution)");
        flags = static_cast<PlayerFlags>(flags | kDecodeLowRes | kDecodeSingleThreaded | kDecodeNoLoopFilter);
    }

    // blank detector needs to be only sample center for this optimization.
    if (flagfast && ((COMM_DETECT_BLANKS  == commDetectMethod) ||
                     (COMM_DETECT_2_BLANK == commDetectMethod)))
    {
        flags = static_cast<PlayerFlags>(flags | kDecodeFewBlocks);
    }

    auto *ctx = new PlayerContext(kFlaggerInUseID);
    auto *cfp = new MythCommFlagPlayer(ctx, flags);
    ctx->SetPlayingInfo(program_info);
    ctx->SetRingBuffer(tmprbuf);
    ctx->SetPlayer(cfp);

    if (useDB)
    {
        if (program_info->GetRecordingEndTime() > MythDate::current())
        {
            gCoreContext->ConnectToMasterServer();

            recorder = RemoteGetExistingRecorder(program_info);
            if (recorder && (recorder->GetRecorderNumber() != -1))
            {
                recorderNum =  recorder->GetRecorderNumber();
                watchingRecording = true;
                ctx->SetRecorder(recorder);

                LOG(VB_COMMFLAG, LOG_INFO,
                    QString("mythcommflag will flag recording "
                            "currently in progress on cardid %1")
                        .arg(recorderNum));
            }
            else
            {
                recorderNum = -1;
                watchingRecording = false;

                LOG(VB_GENERAL, LOG_ERR,
                        "Unable to find active recorder for this "
                        "recording, realtime flagging will not be enabled.");
            }
            cfp->SetWatchingRecording(watchingRecording);
        }
    }

    // TODO: Add back insertion of job if not in jobqueue

    breaksFound = DoFlagCommercials(
        program_info, progress, fullSpeed, jobid,
        cfp, commDetectMethod, outputfilename, useDB);

    if (progress)
        std::cerr << breaksFound << "\n";

    LOG(VB_GENERAL, LOG_NOTICE, QString("Finished, %1 break(s) found.")
        .arg(breaksFound));

    delete ctx;
    global_program_info = nullptr;

    return breaksFound;
}

static int FlagCommercials( uint chanid, const QDateTime &starttime,
                            int jobid, const QString &outputfilename,
                            bool fullSpeed )
{
    QString startstring = MythDate::toString(starttime, MythDate::kFilename);
    ProgramInfo pginfo(chanid, starttime);

    if (!pginfo.GetChanID())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("No program data exists for channel %1 at %2")
                .arg(chanid).arg(startstring));
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    if (!force && JobQueue::IsJobRunning(JOB_COMMFLAG, pginfo))
    {
        if (progress)
        {
            std::cerr << "IN USE\n";
            std::cerr << "                        "
                         "(the program is already being flagged elsewhere)\n";
        }
        LOG(VB_GENERAL, LOG_ERR, "Program is already being flagged elsewhere");
        return GENERIC_EXIT_IN_USE;
    }


    if (progress)
    {
        std::cerr << "MythTV Commercial Flagger, flagging commercials for:" << std::endl;
        if (pginfo.GetSubtitle().isEmpty())
            std::cerr << "    " << pginfo.GetTitle().toLocal8Bit().constData() << std::endl;
        else
            std::cerr << "    " << pginfo.GetTitle().toLocal8Bit().constData() << " - "
                      << pginfo.GetSubtitle().toLocal8Bit().constData() << std::endl;
    }

    return FlagCommercials(&pginfo, jobid, outputfilename, true, fullSpeed);
}

static int FlagCommercials(const QString& filename, int jobid,
                            const QString &outputfilename, bool useDB,
                            bool fullSpeed)
{

    if (progress)
    {
        std::cerr << "MythTV Commercial Flagger, flagging commercials for:" << std::endl
                  << "    " << filename.toLatin1().constData() << std::endl;
    }

    ProgramInfo pginfo(filename);
    return FlagCommercials(&pginfo, jobid, outputfilename, useDB, fullSpeed);
}

static int RebuildSeekTable(ProgramInfo *pginfo, int jobid, bool writefile = false)
{
    QString filename = get_filename(pginfo);

    if (!DoesFileExist(pginfo))
    {
        // file not found on local filesystem
        // assume file is in Video storage group on local backend
        // and try again

        filename = QString("myth://Videos@%1/%2")
                            .arg(gCoreContext->GetHostName(), filename);
        pginfo->SetPathname(filename);
        if (!DoesFileExist(pginfo))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unable to find file in defined storage "
                        "paths for JobQueue ID# %1.").arg(jobid));
            return GENERIC_EXIT_PERMISSIONS_ERROR;
        }
    }

    // Update the file size since mythcommflag --rebuild is often used in user
    // scripts after transcoding or other size-changing operations
    UpdateFileSize(pginfo);

    MythMediaBuffer *tmprbuf = MythMediaBuffer::Create(filename, false);
    if (!tmprbuf)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Unable to create RingBuffer for %1").arg(filename));
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    auto *ctx = new PlayerContext(kFlaggerInUseID);
    auto *cfp = new MythCommFlagPlayer(ctx, (PlayerFlags)(kAudioMuted | kVideoIsNull |
                                                     kDecodeNoDecode | kNoITV));

    ctx->SetPlayingInfo(pginfo);
    ctx->SetRingBuffer(tmprbuf);
    ctx->SetPlayer(cfp);

    if (progress)
    {
        QString time = QDateTime::currentDateTime().toString(Qt::TextDate);
        std::cerr << "Rebuild started at " << qPrintable(time) << std::endl;
    }

    if (writefile)
        gCoreContext->RegisterFileForWrite(filename);
    cfp->RebuildSeekTable(progress);
    if (writefile)
        gCoreContext->UnregisterFileForWrite(filename);

    if (progress)
    {
        QString time = QDateTime::currentDateTime().toString(Qt::TextDate);
        std::cerr << "Rebuild completed at " << qPrintable(time) << std::endl;
    }

    delete ctx;

    return GENERIC_EXIT_OK;
}

static int RebuildSeekTable(const QString& filename, int jobid, bool writefile = false)
{
    if (progress)
    {
        std::cerr << "MythTV Commercial Flagger, building seek table for:" << std::endl
                  << "    " << filename.toLatin1().constData() << std::endl;
    }
    ProgramInfo pginfo(filename);
    return RebuildSeekTable(&pginfo, jobid, writefile);
}

static int RebuildSeekTable(uint chanid, const QDateTime& starttime, int jobid, bool writefile = false)
{
    ProgramInfo pginfo(chanid, starttime);
    if (progress)
    {
        std::cerr << "MythTV Commercial Flagger, building seek table for:" << std::endl;
        if (pginfo.GetSubtitle().isEmpty())
            std::cerr << "    " << pginfo.GetTitle().toLocal8Bit().constData() << std::endl;
        else
            std::cerr << "    " << pginfo.GetTitle().toLocal8Bit().constData() << " - "
                 << pginfo.GetSubtitle().toLocal8Bit().constData() << std::endl;
    }
    return RebuildSeekTable(&pginfo, jobid, writefile);
}

int main(int argc, char *argv[])
{
    int result = GENERIC_EXIT_OK;

//    QString allStart = "19700101000000";
//    QString allEnd   = MythDate::current().toString("yyyyMMddhhmmss");
    int jobType = JOB_NONE;

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
        MythCommFlagCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHCOMMFLAG);
    int retval = cmdline.ConfigureLogging("general",
                                          !cmdline.toBool("noprogress"));
    if (retval != GENERIC_EXIT_OK)
        return retval;

    MythContext context {MYTH_BINARY_VERSION};
    if (!context.Init( false, /*use gui*/
                         false, /*prompt for backend*/
                         false, /*bypass auto discovery*/
                         cmdline.toBool("skipdb"))) /*ignoreDB*/
    {
        LOG(VB_GENERAL, LOG_EMERG, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }
    cmdline.ApplySettingsOverride();

    MythTranslation::load("mythfrontend");

    if (cmdline.toBool("outputmethod"))
    {
        QString om = cmdline.toString("outputmethod");
        if (outputTypes->contains(om))
            outputMethod = outputTypes->value(om);
    }

    if (cmdline.toBool("chanid") && cmdline.toBool("starttime"))
    {
        // operate on a recording in the database
        uint chanid = cmdline.toUInt("chanid");
        QDateTime starttime = cmdline.toDateTime("starttime");

        // TODO: check for matching jobid
        // create temporary id to operate off of if not

        if (cmdline.toBool("queue"))
            QueueCommFlagJob(chanid, starttime, cmdline.toBool("rebuild"));
        else if (cmdline.toBool("rebuild"))
            result = RebuildSeekTable(chanid, starttime, -1);
        else
            result = FlagCommercials(chanid, starttime, -1,
                                     cmdline.toString("outputfile"), true);
    }
    else if (cmdline.toBool("jobid"))
    {
        jobID = cmdline.toInt("jobid");
        uint chanid = 0;
        QDateTime starttime;

        if (!JobQueue::GetJobInfoFromID(jobID, jobType, chanid, starttime))
        {
            std::cerr << "mythcommflag: ERROR: Unable to find DB info for "
                      << "JobQueue ID# " << jobID << std::endl;
            return GENERIC_EXIT_NO_RECORDING_DATA;
        }
        force = true;
        int jobQueueCPU = gCoreContext->GetNumSetting("JobQueueCPU", 0);

        if (jobQueueCPU < 2)
        {
            myth_nice(17);
            myth_ioprio((0 == jobQueueCPU) ? 8 : 7);
        }

        progress = false;

        int ret = 0;

        if (JobQueue::GetJobFlags(jobID) & JOB_REBUILD)
            RebuildSeekTable(chanid, starttime, jobID);
        else
            ret = FlagCommercials(chanid, starttime, jobID, "", jobQueueCPU != 0);

        if (ret > GENERIC_EXIT_NOT_OK)
        {
            JobQueue::ChangeJobStatus(jobID, JOB_ERRORED,
                QCoreApplication::translate("(mythcommflag)",
                                            "Failed with exit status %1",
                                            "Job status").arg(ret));
        }
        else
        {
            JobQueue::ChangeJobStatus(jobID, JOB_FINISHED,
                QCoreApplication::translate("(mythcommflag)",
                                            "%n commercial break(s)",
                                            "Job status",
                                            ret));
        }
    }
    else if (cmdline.toBool("video"))
    {
        // build skiplist for video file
        return RebuildSeekTable(cmdline.toString("video"), -1);
    }
    else if (cmdline.toBool("file"))
    {
        if (cmdline.toBool("skipdb"))
        {
            if (cmdline.toBool("rebuild"))
            {
                std::cerr << "The --rebuild parameter builds the seektable for "
                             "internal MythTV use only. It cannot be used in "
                             "combination with --skipdb." << std::endl;
                return GENERIC_EXIT_INVALID_CMDLINE;
            }

            if (!cmdline.toBool("outputfile"))
                cmdline.SetValue("outputfile", "-");

            // perform commercial flagging on file outside the database
            FlagCommercials(cmdline.toString("file"), -1,
                            cmdline.toString("outputfile"),
                            !cmdline.toBool("skipdb"),
                            true);
        }
        else
        {
            ProgramInfo pginfo(cmdline.toString("file"));
            // pass chanid and starttime
            // inefficient, but it lets the other function
            // handle sanity checking
            if (cmdline.toBool("rebuild"))
            {
                result = RebuildSeekTable(pginfo.GetChanID(),
                                          pginfo.GetRecordingStartTime(),
                                          -1, cmdline.toBool("writefile"));
            }
            else
            {
                result = FlagCommercials(pginfo.GetChanID(),
                                         pginfo.GetRecordingStartTime(),
                                         -1, cmdline.toString("outputfile"),
                                         true);
            }
        }
    }
    else if (cmdline.toBool("queue"))
    {
        // run flagging for all recordings with no skiplist
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT r.chanid, r.starttime, c.commmethod "
                        "FROM recorded AS r "
                   "LEFT JOIN channel AS c ON r.chanid=c.chanid "
//                     "WHERE startime >= :STARTTIME AND endtime <= :ENDTIME "
                    "ORDER BY starttime;");
        //query.bindValue(":STARTTIME", allStart);
        //query.bindValue(":ENDTIME", allEnd);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            QDateTime starttime;

            while (query.next())
            {
                starttime = MythDate::fromString(query.value(1).toString());
                uint chanid = query.value(0).toUInt();

                if (!cmdline.toBool("force") && !cmdline.toBool("rebuild"))
                {
                    // recording is already flagged
                    if (IsMarked(chanid, starttime))
                        continue;

                    // channel is marked as commercial free
                    if (query.value(2).toInt() == COMM_DETECT_COMMFREE)
                        continue;

                    // recording rule did not enable commflagging
#if 0
                    RecordingInfo recinfo(chanid, starttime);
                    if (!(recinfo.GetAutoRunJobs() & JOB_COMMFLAG))
                        continue;
#endif
                }

                QueueCommFlagJob(chanid, starttime, cmdline.toBool("rebuild"));
            }
        }

    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            "No valid combination of command inputs received.");
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    return result;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
