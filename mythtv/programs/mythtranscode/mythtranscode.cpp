// C++ headers
#include <cerrno>
#include <fcntl.h> // for open flags
#include <fstream>
#include <iostream>

// Qt headers
#include <QtGlobal>
#include <QCoreApplication>
#include <QDir>
#include <utility>

// MythTV headers
#include "libmyth/mythcontext.h"
#include "libmythbase/cleanupguard.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/signalhandling.h"
#include "libmythtv/HLS/httplivestream.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/recordinginfo.h"

// MythTranscode
#include "mpeg2fix.h"
#include "mythtranscode_commandlineparser.h"
#include "transcode.h"

static void CompleteJob(int jobID, ProgramInfo *pginfo, bool useCutlist,
                        frm_dir_map_t *deleteMap, int &exitCode,
                        int resultCode, bool forceDelete);

static int glbl_jobID = -1;
static QString recorderOptions = "";

static void UpdatePositionMap(frm_pos_map_t &posMap, frm_pos_map_t &durMap, const QString& mapfile,
                       ProgramInfo *pginfo)
{
    if (pginfo && mapfile.isEmpty())
    {
        pginfo->ClearPositionMap(MARK_KEYFRAME);
        pginfo->ClearPositionMap(MARK_GOP_START);
        pginfo->SavePositionMap(posMap, MARK_GOP_BYFRAME);
        pginfo->SavePositionMap(durMap, MARK_DURATION_MS);
    }
    else if (!mapfile.isEmpty())
    {
        MarkTypes keyType = MARK_GOP_BYFRAME;
        FILE *mapfh = fopen(mapfile.toLocal8Bit().constData(), "w");
        if (!mapfh)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Could not open map file '%1'")
                    .arg(mapfile) + ENO);
            return;
        }
        frm_pos_map_t::const_iterator it;
        fprintf (mapfh, "Type: %d\n", keyType);
        for (it = posMap.cbegin(); it != posMap.cend(); ++it)
        {
            QString str = QString("%1 %2\n").arg(it.key()).arg(*it);
            fprintf(mapfh, "%s", qPrintable(str));
        }
        fclose(mapfh);
    }
}

static int BuildKeyframeIndex(MPEG2fixup *m2f, const QString &infile,
                       frm_pos_map_t &posMap, frm_pos_map_t &durMap, int jobID)
{
    if (!m2f)
        return 0;

    if (jobID < 0 || JobQueue::GetJobCmd(jobID) != JOB_STOP)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobComment(jobID,
                                       QObject::tr("Generating Keyframe Index"));
        int err = m2f->BuildKeyframeIndex(infile, posMap, durMap);
        if (err)
            return err;
        if (jobID >= 0)
            JobQueue::ChangeJobComment(jobID,
                                       QObject::tr("Transcode Completed"));
    }
    return 0;
}

static void UpdateJobQueue(float percent_done)
{
    JobQueue::ChangeJobComment(glbl_jobID,
                               QString("%1% ").arg(percent_done, 0, 'f', 1) +
                               QObject::tr("Completed"));
}

static int CheckJobQueue()
{
    if (JobQueue::GetJobCmd(glbl_jobID) == JOB_STOP)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Transcoding stopped by JobQueue");
        return 1;
    }
    return 0;
}

static int QueueTranscodeJob(ProgramInfo *pginfo, const QString& profile,
                            const QString& hostname, bool usecutlist)
{
    if (!profile.isEmpty())
    {
        RecordingInfo recinfo(*pginfo);
        recinfo.ApplyTranscoderProfileChange(profile);
    }

    if (JobQueue::QueueJob(JOB_TRANSCODE, pginfo->GetChanID(),
                           pginfo->GetRecordingStartTime(),
                           hostname, "", "",
                           usecutlist ? JOB_USE_CUTLIST : 0))
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Queued transcode job for chanid %1 @ %2")
            .arg(pginfo->GetChanID())
            .arg(pginfo->GetRecordingStartTime(MythDate::ISODate)));
        return GENERIC_EXIT_OK;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Error queuing job for chanid %1 @ %2")
        .arg(pginfo->GetChanID())
        .arg(pginfo->GetRecordingStartTime(MythDate::ISODate)));
    return GENERIC_EXIT_DB_ERROR;
}

namespace
{
    void cleanup()
    {
        delete gContext;
        gContext = nullptr;
        SignalHandler::Done();
    }
}

int main(int argc, char *argv[])
{
    uint chanid = 0;
    QDateTime starttime;
    QString infile;
    QString outfile;
    QString profilename = QString("autodetect");
    QString fifodir = nullptr;
    int jobID = -1;
    int jobType = JOB_NONE;
    int otype = REPLEX_MPEG2;
    bool useCutlist = false;
    bool keyframesonly = false;
    bool build_index = false;
    bool fifosync = false;
    bool mpeg2 = false;
    bool fifo_info = false;
    bool cleanCut = false;
    frm_dir_map_t deleteMap;
    frm_pos_map_t posMap; ///< position of keyframes
    frm_pos_map_t durMap; ///< duration from beginning of keyframes
    int AudioTrackNo = -1;

    bool found_starttime = false;
    bool found_chanid = false;
    bool found_infile = false;
    int update_index = 1;
    bool isVideo = false;
    bool passthru = false;

    MythTranscodeCommandLineParser cmdline;
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
        MythTranscodeCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHTRANSCODE);

    if (cmdline.toBool("outputfile"))
    {
        outfile = cmdline.toString("outputfile");
        update_index = 0;
    }

    bool showprogress = cmdline.toBool("showprogress");

    QString mask("general");
    bool quiet = (outfile == "-") || showprogress;
    int retval = cmdline.ConfigureLogging(mask, quiet);
    if (retval != GENERIC_EXIT_OK)
        return retval;

    if (cmdline.toBool("starttime"))
    {
        starttime = cmdline.toDateTime("starttime");
        found_starttime = true;
    }
    if (cmdline.toBool("chanid"))
    {
        chanid = cmdline.toUInt("chanid");
        found_chanid = true;
    }
    if (cmdline.toBool("jobid"))
        jobID = cmdline.toInt("jobid");
    if (cmdline.toBool("inputfile"))
    {
        infile = cmdline.toString("inputfile");
        found_infile = true;
    }
    if (cmdline.toBool("video"))
        isVideo = true;
    if (cmdline.toBool("profile"))
        profilename = cmdline.toString("profile");

    if (cmdline.toBool("usecutlist"))
    {
        useCutlist = true;
        if (!cmdline.toString("usecutlist").isEmpty())
        {
            if (!cmdline.toBool("inputfile") && !cmdline.toBool("hls"))
            {
                LOG(VB_GENERAL, LOG_CRIT, "External cutlists are only allowed "
                                          "when using the --infile option.");
                return GENERIC_EXIT_INVALID_CMDLINE;
            }

            uint64_t last = 0;
            QStringList cutlist = cmdline.toStringList("usecutlist", " ");
            for (const auto & cut : std::as_const(cutlist))
            {
                QStringList startend = cut.split("-", Qt::SkipEmptyParts);
                if (startend.size() == 2)
                {
                    uint64_t start = startend.first().toULongLong();
                    uint64_t end = startend.last().toULongLong();

                    if (cmdline.toBool("inversecut"))
                    {
                        LOG(VB_GENERAL, LOG_DEBUG,
                                QString("Cutting section %1-%2.")
                                    .arg(last).arg(start));
                        deleteMap[start] = MARK_CUT_END;
                        deleteMap[end] = MARK_CUT_START;
                        last = end;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_DEBUG,
                                QString("Cutting section %1-%2.")
                                    .arg(start).arg(end));
                        deleteMap[start] = MARK_CUT_START;
                        deleteMap[end] = MARK_CUT_END;
                    }
                }
            }

            if (cmdline.toBool("inversecut"))
            {
                if (deleteMap.contains(0) && (deleteMap[0] == MARK_CUT_END))
                    deleteMap.remove(0);
                else
                    deleteMap[0] = MARK_CUT_START;
                deleteMap[999999999] = MARK_CUT_END;
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("Cutting section %1-999999999.")
                                    .arg(last));
            }

            // sanitize cutlist
            if (deleteMap.count() >= 2)
            {
                frm_dir_map_t::iterator cur = deleteMap.begin();
                frm_dir_map_t::iterator prev;
                prev = cur++;
                while (cur != deleteMap.end())
                {
                    if (prev.value() == cur.value())
                    {
                        // two of the same type next to each other
                        QString err("Cut %1points found at %3 and %4, with no "
                                    "%2 point in between.");
                        if (prev.value() == MARK_CUT_END)
                            err = err.arg("end", "start");
                        else
                            err = err.arg("start", "end");
                        LOG(VB_GENERAL, LOG_CRIT, "Invalid cutlist defined!");
                        LOG(VB_GENERAL, LOG_CRIT, err.arg(prev.key())
                                                     .arg(cur.key()));
                        return GENERIC_EXIT_INVALID_CMDLINE;
                    }
                    if ( (prev.value() == MARK_CUT_START) &&
                         ((cur.key() - prev.key()) < 2) )
                    {
                        LOG(VB_GENERAL, LOG_WARNING, QString("Discarding "
                                          "insufficiently long cut: %1-%2")
                                            .arg(prev.key()).arg(cur.key()));
                        prev = deleteMap.erase(prev);
                        cur  = deleteMap.erase(cur);

                        if (cur == deleteMap.end())
                            continue;
                    }
                    prev = cur++;
                }
            }
        }
        else if (cmdline.toBool("inversecut"))
        {
            std::cerr << "Cutlist inversion requires an external cutlist be" << std::endl
                      << "provided using the --honorcutlist option." << std::endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (cmdline.toBool("cleancut"))
        cleanCut = true;

    if (cmdline.toBool("allkeys"))
        keyframesonly = true;
    if (cmdline.toBool("reindex"))
        build_index = true;
    if (cmdline.toBool("fifodir"))
        fifodir = cmdline.toString("fifodir");
    if (cmdline.toBool("fifoinfo"))
        fifo_info = true;
    if (cmdline.toBool("fifosync"))
        fifosync = true;
    if (cmdline.toBool("recopt"))
        recorderOptions = cmdline.toString("recopt");
    if (cmdline.toBool("mpeg2"))
        mpeg2 = true;
    if (cmdline.toBool("ostream"))
    {
        if (cmdline.toString("ostream") == "dvd")
            otype = REPLEX_DVD;
        else if (cmdline.toString("ostream") == "ps")
            otype = REPLEX_MPEG2;
        else if (cmdline.toString("ostream") == "ts")
            otype = REPLEX_TS_SD;
        else
        {
            std::cerr << "Invalid 'ostream' type: "
                      << cmdline.toString("ostream").toLocal8Bit().constData()
                      << std::endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }
    if (cmdline.toBool("audiotrack"))
        AudioTrackNo = cmdline.toInt("audiotrack");
    if (cmdline.toBool("passthru"))
        passthru = true;
    // Set if we want to delete the original file once conversion succeeded.
    bool deleteOriginal = cmdline.toBool("delete");

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    SignalHandler::Init();
#endif

    //  Load the context
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    MythTranslation::load("mythfrontend");

    cmdline.ApplySettingsOverride();

    if (jobID != -1)
    {
        if (JobQueue::GetJobInfoFromID(jobID, jobType, chanid, starttime))
        {
            found_starttime = true;
            found_chanid = true;
        }
        else
        {
            std::cerr << "mythtranscode: ERROR: Unable to find DB info for "
                      << "JobQueue ID# " << jobID << std::endl;
            return GENERIC_EXIT_NO_RECORDING_DATA;
        }
    }

    if (((!found_infile && !(found_chanid && found_starttime)) ||
         (found_infile && (found_chanid || found_starttime))) &&
        (!cmdline.toBool("hls")))
    {
         std::cerr << "Must specify -i OR -c AND -s options!" << std::endl;
         return GENERIC_EXIT_INVALID_CMDLINE;
    }
    if (isVideo && !found_infile && !cmdline.toBool("hls"))
    {
         std::cerr << "Must specify --infile to use --video" << std::endl;
         return GENERIC_EXIT_INVALID_CMDLINE;
    }
    if (jobID >= 0 && (found_infile || build_index))
    {
         std::cerr << "Can't specify -j with --buildindex, --video or --infile"
                   << std::endl;
         return GENERIC_EXIT_INVALID_CMDLINE;
    }
    if ((jobID >= 0) && build_index)
    {
         std::cerr << "Can't specify both -j and --buildindex" << std::endl;
         return GENERIC_EXIT_INVALID_CMDLINE;
    }
    if (keyframesonly && !fifodir.isEmpty())
    {
         std::cerr << "Cannot specify both --fifodir and --allkeys" << std::endl;
         return GENERIC_EXIT_INVALID_CMDLINE;
    }
    if (fifosync && fifodir.isEmpty())
    {
         std::cerr << "Must specify --fifodir to use --fifosync" << std::endl;
         return GENERIC_EXIT_INVALID_CMDLINE;
    }
    if (fifo_info && !fifodir.isEmpty())
    {
        std::cerr << "Cannot specify both --fifodir and --fifoinfo" << std::endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    if (cleanCut && fifodir.isEmpty() && !fifo_info)
    {
        std::cerr << "Clean cutting works only in fifodir mode" << std::endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    if (cleanCut && !useCutlist)
    {
        std::cerr << "--cleancut is pointless without --honorcutlist" << std::endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (fifo_info)
    {
        // Setup a dummy fifodir path, so that the "fifodir" code path
        // is taken. The path wont actually be used.
        fifodir = "DummyFifoPath";
    }

    if (!MSqlQuery::testDBConnection())
    {
        LOG(VB_GENERAL, LOG_ERR, "couldn't open db");
        return GENERIC_EXIT_DB_ERROR;
    }

    ProgramInfo *pginfo = nullptr;
    if (cmdline.toBool("hls"))
    {
        if (cmdline.toBool("hlsstreamid"))
        {
            HTTPLiveStream hls(cmdline.toInt("hlsstreamid"));
            pginfo = new ProgramInfo(hls.GetSourceFile());
        }
        if (pginfo == nullptr)
            pginfo = new ProgramInfo();
    }
    else if (isVideo)
    {
        // We want the absolute file path for the filemarkup table
        QFileInfo inf(infile);
        infile = inf.absoluteFilePath();
        pginfo = new ProgramInfo(infile);
    }
    else if (!found_infile)
    {
        pginfo = new ProgramInfo(chanid, starttime);

        if (!pginfo->GetChanID())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Couldn't find recording for chanid %1 @ %2")
                .arg(chanid).arg(starttime.toString(Qt::ISODate)));
            delete pginfo;
            return GENERIC_EXIT_NO_RECORDING_DATA;
        }

        infile = pginfo->GetPlaybackURL(false, true);
    }
    else
    {
        pginfo = new ProgramInfo(infile);
        if (!pginfo->GetChanID())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Couldn't find a recording for filename '%1'")
                    .arg(infile));
            delete pginfo;
            return GENERIC_EXIT_NO_RECORDING_DATA;
        }
    }

    if (!pginfo)
    {
        LOG(VB_GENERAL, LOG_ERR, "No program info found!");
        return GENERIC_EXIT_NO_RECORDING_DATA;
    }

    if (cmdline.toBool("queue"))
    {
        QString hostname = cmdline.toString("queue");
        return QueueTranscodeJob(pginfo, profilename, hostname, useCutlist);
    }

    if (infile.startsWith("myth://") && (outfile.isEmpty() || outfile != "-") &&
        fifodir.isEmpty() && !cmdline.toBool("hls") && !cmdline.toBool("avf"))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Attempted to transcode %1. Mythtranscode is currently "
                    "unable to transcode remote files.") .arg(infile));
        delete pginfo;
        return GENERIC_EXIT_REMOTE_FILE;
    }

    if (outfile.isEmpty() && !build_index && fifodir.isEmpty())
        outfile = infile + ".tmp";

    if (jobID >= 0)
        JobQueue::ChangeJobStatus(jobID, JOB_RUNNING);

    auto *transcode = new Transcode(pginfo);

    if (!build_index)
    {
        if (cmdline.toBool("hlsstreamid"))
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Transcoding HTTP Live Stream ID %1")
                        .arg(cmdline.toInt("hlsstreamid")));
        }
        else if (fifodir.isEmpty())
        {
            LOG(VB_GENERAL, LOG_NOTICE, QString("Transcoding from %1 to %2")
                    .arg(infile, outfile));
        }
        else
        {
            LOG(VB_GENERAL, LOG_NOTICE, QString("Transcoding from %1 to FIFO")
                    .arg(infile));
        }
    }

    if (cmdline.toBool("avf"))
    {
        transcode->SetAVFMode();

        if (cmdline.toBool("container"))
            transcode->SetCMDContainer(cmdline.toString("container"));
        if (cmdline.toBool("acodec"))
            transcode->SetCMDAudioCodec(cmdline.toString("acodec"));
        if (cmdline.toBool("vcodec"))
            transcode->SetCMDVideoCodec(cmdline.toString("vcodec"));
    }
    else if (cmdline.toBool("hls"))
    {
        transcode->SetHLSMode();

        if (cmdline.toBool("hlsstreamid"))
            transcode->SetHLSStreamID(cmdline.toInt("hlsstreamid"));
        if (cmdline.toBool("maxsegments"))
            transcode->SetHLSMaxSegments(cmdline.toInt("maxsegments"));
        if (cmdline.toBool("noaudioonly"))
            transcode->DisableAudioOnlyHLS();
    }

    if (cmdline.toBool("avf") || cmdline.toBool("hls"))
    {
        if (cmdline.toBool("width"))
            transcode->SetCMDWidth(cmdline.toInt("width"));
        if (cmdline.toBool("height"))
            transcode->SetCMDHeight(cmdline.toInt("height"));
        if (cmdline.toBool("bitrate"))
            transcode->SetCMDBitrate(cmdline.toInt("bitrate") * 1000);
        if (cmdline.toBool("audiobitrate"))
            transcode->SetCMDAudioBitrate(cmdline.toInt("audiobitrate") * 1000);
    }

    if (showprogress)
        transcode->ShowProgress(true);
    if (!recorderOptions.isEmpty())
        transcode->SetRecorderOptions(recorderOptions);
    int result = 0;
    if ((!mpeg2 && !build_index) || cmdline.toBool("hls"))
    {
        result = transcode->TranscodeFile(infile, outfile,
                                          profilename, useCutlist,
                                          (fifosync || keyframesonly), jobID,
                                          fifodir, fifo_info, cleanCut, deleteMap,
                                          AudioTrackNo, passthru);

        if ((result == REENCODE_OK) && (jobID >= 0))
        {
            JobQueue::ChangeJobArgs(jobID, "RENAME_TO_NUV");
            RecordingInfo recInfo(pginfo->GetRecordingID());
            RecordingFile *recFile = recInfo.GetRecordingFile();
            recFile->m_containerFormat = formatNUV;
            recFile->Save();
        }
    }

    if (fifo_info)
    {
        delete transcode;
        return GENERIC_EXIT_OK;
    }

    int exitcode = GENERIC_EXIT_OK;
    if ((result == REENCODE_MPEG2TRANS) || mpeg2 || build_index)
    {
        void (*update_func)(float) = nullptr;
        int (*check_func)() = nullptr;
        if (useCutlist)
        {
            LOG(VB_GENERAL, LOG_INFO, "Honoring the cutlist while transcoding");
            if (deleteMap.isEmpty())
                pginfo->QueryCutList(deleteMap);
        }
        if (jobID >= 0)
        {
           glbl_jobID = jobID;
           update_func = &UpdateJobQueue;
           check_func = &CheckJobQueue;
        }

        auto *m2f = new MPEG2fixup(infile, outfile,
                                         &deleteMap, nullptr, false, false, 20,
                                         showprogress, otype, update_func,
                                         check_func);

        if (cmdline.toBool("allaudio"))
        {
            m2f->SetAllAudio(true);
        }

        if (build_index)
        {
            int err = BuildKeyframeIndex(m2f, infile, posMap, durMap, jobID);
            if (err)
            {
                delete m2f;
                m2f = nullptr;
                return err;
            }
            if (update_index)
                UpdatePositionMap(posMap, durMap, nullptr, pginfo);
            else
                UpdatePositionMap(posMap, durMap, outfile + QString(".map"), pginfo);
        }
        else
        {
            result = m2f->Start();
            if (result == REENCODE_OK)
            {
                result = BuildKeyframeIndex(m2f, outfile, posMap, durMap, jobID);
                if (result == REENCODE_OK)
                {
                    if (update_index)
                        UpdatePositionMap(posMap, durMap, nullptr, pginfo);
                    else
                        UpdatePositionMap(posMap, durMap, outfile + QString(".map"),
                                          pginfo);
                }
                RecordingInfo recInfo(*pginfo);
                RecordingFile *recFile = recInfo.GetRecordingFile();
                if (otype == REPLEX_DVD || otype == REPLEX_MPEG2 ||
                    otype == REPLEX_HDTV)
                {
                    recFile->m_containerFormat = formatMPEG2_PS;
                    JobQueue::ChangeJobArgs(jobID, "RENAME_TO_MPG");
                }
                else
                {
                    recFile->m_containerFormat = formatMPEG2_TS;
                }
                recFile->Save();
            }
        }
        delete m2f;
        m2f = nullptr;
    }

    if (result == REENCODE_OK)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_STOPPING);
        LOG(VB_GENERAL, LOG_NOTICE, QString("%1 %2 done")
            .arg(build_index ? "Building Index for" : "Transcoding", infile));
    }
    else if (result == REENCODE_CUTLIST_CHANGE)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_RETRY);
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Transcoding %1 aborted because of cutlist update")
                .arg(infile));
        exitcode = GENERIC_EXIT_RESTART;
    }
    else if (result == REENCODE_STOPPED)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_ABORTING);
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Transcoding %1 stopped because of stop command")
                .arg(infile));
        exitcode = GENERIC_EXIT_KILLED;
    }
    else
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_ERRORING);
        LOG(VB_GENERAL, LOG_ERR, QString("Transcoding %1 failed").arg(infile));
        exitcode = result;
    }

    if (deleteOriginal || jobID >= 0)
        CompleteJob(jobID, pginfo, useCutlist, &deleteMap, exitcode, result, deleteOriginal);

    transcode->deleteLater();

    return exitcode;
}

static int transUnlink(const QString& filename, ProgramInfo *pginfo)
{
    QString hostname = pginfo->GetHostname();

    if (!pginfo->GetStorageGroup().isEmpty() &&
        !hostname.isEmpty())
    {
        int port = gCoreContext->GetBackendServerPort(hostname);
        QString basename = filename.section('/', -1);
        QString uri = MythCoreContext::GenMythURL(hostname, port, basename,
                                                  pginfo->GetStorageGroup());

        LOG(VB_GENERAL, LOG_NOTICE, QString("Requesting delete for file '%1'.")
                .arg(uri));
        bool ok = RemoteFile::DeleteFile(uri);
        if (ok)
            return 0;
    }

    LOG(VB_GENERAL, LOG_NOTICE, QString("Deleting file '%1'.").arg(filename));
    return unlink(filename.toLocal8Bit().constData());
}

static uint64_t ComputeNewBookmark(uint64_t oldBookmark,
                                   frm_dir_map_t *deleteMap)
{
    if (deleteMap == nullptr)
        return oldBookmark;

    uint64_t subtraction = 0;
    uint64_t startOfCutRegion = 0;
    frm_dir_map_t delMap = *deleteMap;
    bool withinCut = false;
    bool firstMark = true;
    while (!delMap.empty() && delMap.begin().key() <= oldBookmark)
    {
        uint64_t key = delMap.begin().key();
        MarkTypes mark = delMap.begin().value();

        if (mark == MARK_CUT_START && !withinCut)
        {
            withinCut = true;
            startOfCutRegion = key;
        }
        else if (mark == MARK_CUT_END && firstMark)
        {
            subtraction += key;
        }
        else if (mark == MARK_CUT_END && withinCut)
        {
            withinCut = false;
            subtraction += (key - startOfCutRegion);
        }
        delMap.remove(key);
        firstMark = false;
    }
    if (withinCut)
        subtraction += (oldBookmark - startOfCutRegion);
    return oldBookmark - subtraction;
}

static uint64_t ReloadBookmark(ProgramInfo *pginfo)
{
    MSqlQuery query(MSqlQuery::InitCon());
    uint64_t currentBookmark = 0;
    query.prepare("SELECT DISTINCT mark FROM recordedmarkup "
                  "WHERE chanid = :CHANID "
                  "AND starttime = :STARTIME "
                  "AND type = :MARKTYPE ;");
    query.bindValue(":CHANID", pginfo->GetChanID());
    query.bindValue(":STARTTIME", pginfo->GetRecordingStartTime());
    query.bindValue(":MARKTYPE", MARK_BOOKMARK);
    if (query.exec() && query.next())
    {
        currentBookmark = query.value(0).toLongLong();
    }
    return currentBookmark;
}

static void WaitToDelete(ProgramInfo *pginfo)
{
    LOG(VB_GENERAL, LOG_NOTICE,
        "Transcode: delete old file: waiting while program is in use.");

    bool inUse = true;
    MSqlQuery query(MSqlQuery::InitCon());
    while (inUse)
    {
        query.prepare("SELECT count(*) FROM inuseprograms "
                      "WHERE chanid = :CHANID "
                      "AND starttime = :STARTTIME "
                      "AND recusage = 'player' ;");
        query.bindValue(":CHANID", pginfo->GetChanID());
        query.bindValue(":STARTTIME", pginfo->GetRecordingStartTime());
        if (!query.exec() || !query.next())
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Transcode: delete old file: in-use query failed;");
            inUse = false;
        }
        else
        {
            inUse = (query.value(0).toUInt() != 0);
        }

        if (inUse)
        {
            const unsigned kSecondsToWait = 10;
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Transcode: program in use, rechecking in %1 seconds.")
                    .arg(kSecondsToWait));
            sleep(kSecondsToWait);
        }
    }
    LOG(VB_GENERAL, LOG_NOTICE, "Transcode: program is no longer in use.");
}

static void CompleteJob(int jobID, ProgramInfo *pginfo, bool useCutlist,
                 frm_dir_map_t *deleteMap, int &exitCode, int resultCode, bool forceDelete)
{
    int status = JOB_UNKNOWN;
    if (jobID >= 0)
        status = JobQueue::GetJobStatus(jobID);

    if (!pginfo)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_ERRORED,
                QObject::tr("Job errored, unable to find Program Info for job"));
        LOG(VB_GENERAL, LOG_CRIT, "MythTranscode: Cleanup errored, unable to find Program Info");
        return;
    }

    const QString filename = pginfo->GetPlaybackURL(false, true);
    const QByteArray fname = filename.toLocal8Bit();

    if (resultCode == REENCODE_OK)
    {
        WaitToDelete(pginfo);

        // Transcoding may take several minutes.  Reload the bookmark
        // in case it changed, then save its translated value back.
        uint64_t previousBookmark =
            ComputeNewBookmark(ReloadBookmark(pginfo), deleteMap);
        pginfo->SaveBookmark(previousBookmark);

        QString jobArgs;
        if (jobID >= 0)
            jobArgs = JobQueue::GetJobArgs(jobID);

        const QString tmpfile = filename + ".tmp";
        const QByteArray atmpfile = tmpfile.toLocal8Bit();

        // To save the original file...
        const QString oldfile = filename + ".old";
        const QByteArray aoldfile = oldfile.toLocal8Bit();

        QFileInfo st(tmpfile);
        qint64 newSize = 0;
        if (st.exists())
            newSize = st.size();

        QString cnf = filename;
        if (jobID >= 0)
        {
            if (filename.endsWith(".mpg") && jobArgs == "RENAME_TO_NUV")
            {
                QString newbase = pginfo->QueryBasename();
                cnf.replace(".mpg", ".nuv");
                newbase.replace(".mpg", ".nuv");
                pginfo->SaveBasename(newbase);
            }
            else if (filename.endsWith(".ts") &&
                    (jobArgs == "RENAME_TO_MPG"))
            {
                QString newbase = pginfo->QueryBasename();
                // MPEG-TS to MPEG-PS
                cnf.replace(".ts", ".mpg");
                newbase.replace(".ts", ".mpg");
                pginfo->SaveBasename(newbase);
            }
        }

        const QString newfile = cnf;
        const QByteArray anewfile = newfile.toLocal8Bit();

        if (rename(fname.constData(), aoldfile.constData()) == -1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("mythtranscode: Error Renaming '%1' to '%2'")
                    .arg(filename, oldfile) + ENO);
        }

        if (rename(atmpfile.constData(), anewfile.constData()) == -1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("mythtranscode: Error Renaming '%1' to '%2'")
                    .arg(tmpfile, newfile) + ENO);
        }

        if (!gCoreContext->GetBoolSetting("SaveTranscoding", false) || forceDelete)
        {
            bool followLinks =
                gCoreContext->GetBoolSetting("DeletesFollowLinks", false);

            LOG(VB_FILE, LOG_INFO,
                QString("mythtranscode: About to unlink/delete file: %1")
                    .arg(oldfile));

            QFileInfo finfo(oldfile);
            if (followLinks && finfo.isSymLink())
            {
                QString link = getSymlinkTarget(oldfile);
                QByteArray alink = link.toLocal8Bit();
                int err = transUnlink(alink, pginfo);
                if (err)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("mythtranscode: Error deleting '%1' "
                                "pointed to by '%2'")
                            .arg(alink.constData(), aoldfile.constData()) + ENO);
                }

                err = unlink(aoldfile.constData());
                if (err)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("mythtranscode: Error deleting '%1', "
                                "a link pointing to '%2'")
                            .arg(aoldfile.constData(), alink.constData()) + ENO);
                }
            }
            else
            {
                int err = transUnlink(aoldfile.constData(), pginfo);
                if (err)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("mythtranscode: Error deleting '%1': ")
                            .arg(oldfile) + ENO);
                }
            }
        }

        // Rename or delete all preview thumbnails.
        //
        // TODO: This cleanup should be moved to RecordingInfo, and triggered
        //       when SaveBasename() is called
        QFileInfo fInfo(filename);
        QStringList nameFilters;
        nameFilters.push_back(fInfo.fileName() + "*.png");
        nameFilters.push_back(fInfo.fileName() + "*.jpg");

        QDir dir (fInfo.path());
        QFileInfoList previewFiles = dir.entryInfoList(nameFilters);

        for (const auto & previewFile : std::as_const(previewFiles))
        {
            QString oldFileName = previewFile.absoluteFilePath();

            // Delete previews if cutlist was applied.  They will be re-created as
            // required.  This prevents the user from being stuck with a preview
            // from a cut area and ensures that the "dimensioned" previews
            // correspond to the new timeline
            if (useCutlist)
            {
                // If unlink fails, keeping the old preview is not a problem.
                // The RENAME_TO_NUV check below will attempt to rename the
                // file, if required.
                if (transUnlink(oldFileName.toLocal8Bit().constData(), pginfo) != -1)
                    continue;
            }

            if (jobArgs == "RENAME_TO_NUV" || jobArgs == "RENAME_TO_MPG")
            {
                QString newExtension = "mpg";
                if (jobArgs == "RENAME_TO_NUV")
                    newExtension = "nuv";

                QString oldSuffix = previewFile.completeSuffix();

                if (!oldSuffix.startsWith(newExtension))
                {
                    QString newSuffix = oldSuffix;
                    QString oldExtension = oldSuffix.section(".", 0, 0);
                    newSuffix.replace(oldExtension, newExtension);

                    QString newFileName = oldFileName;
                    newFileName.replace(oldSuffix, newSuffix);

                    if (!QFile::rename(oldFileName, newFileName))
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("mythtranscode: Error renaming %1 to %2")
                                    .arg(oldFileName, newFileName));
                    }
                }
            }
        }

        MSqlQuery query(MSqlQuery::InitCon());

        if (useCutlist)
        {
            query.prepare("DELETE FROM recordedmarkup "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :STARTTIME "
                          "AND type != :BOOKMARK ");
            query.bindValue(":CHANID", pginfo->GetChanID());
            query.bindValue(":STARTTIME", pginfo->GetRecordingStartTime());
            query.bindValue(":BOOKMARK", MARK_BOOKMARK);

            if (!query.exec())
                MythDB::DBError("Error in mythtranscode", query);

            query.prepare("UPDATE recorded "
                          "SET cutlist = :CUTLIST "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :STARTTIME ;");
            query.bindValue(":CUTLIST", "0");
            query.bindValue(":CHANID", pginfo->GetChanID());
            query.bindValue(":STARTTIME", pginfo->GetRecordingStartTime());

            if (!query.exec())
                MythDB::DBError("Error in mythtranscode", query);

            pginfo->SaveCommFlagged(COMM_FLAG_NOT_FLAGGED);
        }
        else
        {
            query.prepare("DELETE FROM recordedmarkup "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :STARTTIME "
                          "AND type not in ( :COMM_START, "
                          "    :COMM_END, :BOOKMARK, "
                          "    :CUTLIST_START, :CUTLIST_END) ;");
            query.bindValue(":CHANID", pginfo->GetChanID());
            query.bindValue(":STARTTIME", pginfo->GetRecordingStartTime());
            query.bindValue(":COMM_START", MARK_COMM_START);
            query.bindValue(":COMM_END", MARK_COMM_END);
            query.bindValue(":BOOKMARK", MARK_BOOKMARK);
            query.bindValue(":CUTLIST_START", MARK_CUT_START);
            query.bindValue(":CUTLIST_END", MARK_CUT_END);

            if (!query.exec())
                MythDB::DBError("Error in mythtranscode", query);
        }

        if (newSize)
            pginfo->SaveFilesize(newSize);

        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_FINISHED);
    }
    else
    {
        // Not a successful run, so remove the files we created
        QString filename_tmp = filename + ".tmp";
        QByteArray fname_tmp = filename_tmp.toLocal8Bit();
        LOG(VB_GENERAL, LOG_NOTICE, QString("Deleting %1").arg(filename_tmp));
        transUnlink(fname_tmp.constData(), pginfo);

        QString filename_map = filename + ".tmp.map";
        QByteArray fname_map = filename_map.toLocal8Bit();
        unlink(fname_map.constData());

        if (jobID >= 0)
        {
            if (status == JOB_ABORTING)                     // Stop command was sent
            {
                JobQueue::ChangeJobStatus(jobID, JOB_ABORTED,
                                        QObject::tr("Job Aborted"));
            }
            else if (status != JOB_ERRORING)                // Recoverable error
            {
                exitCode = GENERIC_EXIT_RESTART;
            }
            else                                            // Unrecoverable error
            {
                JobQueue::ChangeJobStatus(jobID, JOB_ERRORED,
                                        QObject::tr("Unrecoverable error"));
            }
        }
    }
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
