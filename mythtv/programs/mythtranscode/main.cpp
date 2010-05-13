// POSIX headers
#include <fcntl.h> // for open flags

// C++ headers
#include <iostream>
#include <fstream>
#include <cerrno>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QDir>

// MythTV headers
#include "exitcodes.h"
#include "programinfo.h"
#include "jobqueue.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "util.h"
#include "transcode.h"
#include "mpeg2fix.h"
#include "remotefile.h"
#include "langsettings.h"

void StoreTranscodeState(ProgramInfo *pginfo, int status, bool useCutlist);
void UpdatePositionMap(QMap <long long, long long> &posMap, QString mapfile,
                       ProgramInfo *pginfo);
int BuildKeyframeIndex(MPEG2fixup *m2f, QString &infile,
                       QMap <long long, long long> &posMap, int jobID);
void CompleteJob(int jobID, ProgramInfo *pginfo,
                 bool useCutlist, int &resultCode);
void UpdateJobQueue(float percent_done);
int CheckJobQueue();
static int glbl_jobID = -1;
QString recorderOptions = "";

void usage(char *progname)
{
    cerr << "Usage: " << progname << " <--chanid <channelid>>\n";
    cerr << "\t<--starttime <starttime>> <--profile <profile>>\n";
    cerr << "\t[options]\n\n";
    cerr << "\t--mpeg2          or -m: Perform MPEG2 to MPEG2 transcode.\n";
    cerr << "\t--ostream <type> or -e: Output stream type.  Options: dvd, ps.\n";
    cerr << "\t--chanid         or -c: Takes a channel id. REQUIRED\n";
    cerr << "\t--starttime      or -s: Takes a starttime for the\n";
    cerr << "\t                        recording. REQUIRED\n";
    cerr << "\t--infile         or -i: Input file (Alternative to -c and -s)\n";
    cerr << "\t--outfile        or -o: Output file\n";
    cerr << "\t--profile        or -p: Takes a profile number or 'autodetect'\n";
    cerr << "\t                        recording profile. REQUIRED\n";
    cerr << "\t--honorcutlist   or -l: Specifies whether to use the cutlist.\n";
    cerr << "                          Optionally takes a cutlist as an argument\n";
    cerr << "\t                        when used with --infile.\n";
    cerr << "\t--inversecut          : Specifies a list of frames to keep\n";
    cerr << "\t                        while cutting everything else out.\n";
    cerr << "\t                        Only works with --infile.\n";
    cerr << "\t--allkeys        or -k: Specifies that the output file\n";
    cerr << "\t                        should be made entirely of keyframes.\n";
    cerr << "\t--fifodir        or -f: Directory to write fifos to\n";
    cerr << "\t                        If --fifodir is specified, 'audout' and 'vidout'\n";
    cerr << "\t                        will be created in the specified directory\n";
    cerr << "\t--fifosync            : Enforce fifo sync\n";
    cerr << "\t--buildindex     or -b: Build a new keyframe index\n";
    cerr << "\t                        (use only if audio and video fifos are read independantly)\n";
    cerr << "\t--video               : Specifies that this is not a mythtv recording\n";
    cerr << "\t                        (must be used with --infile)\n";
    cerr << "\t--showprogress        : Display status info to the stdout\n";
    cerr << "\t--recorderOptions <OPTIONS>\n";
    cerr << "\t                or -ro: Pass a comma-separated list of\n";
    cerr << "\t                        recordingprofile options to override\n";
    cerr << "\t                        values in the database.\n";
    cerr << "\t--verbose level  or -v: Use '-v help' for level info\n";
    cerr << "\t--help           or -h: Prints this help statement.\n";
}

int main(int argc, char *argv[])
{
    QString chanid, starttime, infile, outfile;
    QString profilename = QString("autodetect");
    QString fifodir = NULL;
    int jobID = -1;
    QDateTime startts;
    int jobType = JOB_NONE;
    int otype = REPLEX_MPEG2;
    bool useCutlist = false, keyframesonly = false;
    bool build_index = false, fifosync = false, showprogress = false, mpeg2 = false;
    QMap<QString, QString> settingsOverride;
    QMap<long long, int> deleteMap;
    QMap<long long, long long> posMap;
    srand(time(NULL));

    QCoreApplication a(argc, argv);

    print_verbose_messages = VB_IMPORTANT;
    verboseString = "important";

    int found_starttime = 0;
    int found_chanid = 0;
    int found_infile = 0;
    int update_index = 1;
    int isVideo = 0;

    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-s") ||
            !strcmp(a.argv()[argpos],"--starttime"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                starttime = a.argv()[argpos + 1];
                found_starttime = 1;
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -s/--starttime option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-c") ||
                 !strcmp(a.argv()[argpos],"--chanid"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                chanid = a.argv()[argpos + 1];
                found_chanid = 1;
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -c/--chanid option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos], "-j"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                jobID = QString(a.argv()[++argpos]).toInt();
            }
            else
            {
                cerr << "Missing argument to -j option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-i") ||
                 !strcmp(a.argv()[argpos],"--infile"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                infile = a.argv()[argpos + 1];
                found_infile = 1;
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -i/--infile option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"--video"))
        {
            isVideo = 1;
            //mpeg2 = true;
        }
        else if (!strcmp(a.argv()[argpos],"-o") ||
                 !strcmp(a.argv()[argpos],"--outfile"))
        {
            if ((a.argc()-1 > argpos) &&
                (a.argv()[argpos+1][0] != '-' || a.argv()[argpos+1][1] == 0x0))
            {
                outfile = a.argv()[argpos + 1];
                update_index = 0;
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -o/--outfile option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-V"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                QString temp = a.argv()[++argpos];
                print_verbose_messages = temp.toUInt();
            }
            else
            {
                cerr << "Missing argument to -V option\n";
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-v") ||
                 !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return TRANSCODE_EXIT_INVALID_CMDLINE;

                ++argpos;
            } else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-p") ||
                 !strcmp(a.argv()[argpos],"--profile"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                profilename = a.argv()[argpos + 1];
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -p/--profile option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-l") ||
                 !strcmp(a.argv()[argpos],"--honorcutlist"))
        {
            useCutlist = true;
            if (!found_infile)
                continue;

            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                QStringList cutlist = QString(a.argv()[argpos + 1])
                    .split(" ", QString::SkipEmptyParts);
                ++argpos;
                for (QStringList::Iterator it = cutlist.begin();
                     it != cutlist.end(); ++it )
                {
                    QStringList startend = (*it)
                        .split("-", QString::SkipEmptyParts);
                    if (startend.count() == 2)
                    {
                        cerr << "Cutting from: " << startend.first().toInt()
                             << " to: " << startend.last().toInt() <<"\n";
                        deleteMap[startend.first().toInt()] = 1;
                        deleteMap[startend.last().toInt()] = 0;
                    }
                }
            }
            else
            {
                cerr << "Missing argument to -l/--honorcutlist option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"--inversecut"))
        {
            useCutlist = true;
            if (!found_infile)
            {
                cerr << "--inversecut option can only be used with --infile\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }

            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                long long last = 0;
                QStringList cutlist =  QString(a.argv()[argpos + 1])
                    .split(" ", QString::SkipEmptyParts);
                ++argpos;
                deleteMap[0] = 1;
                for (QStringList::Iterator it = cutlist.begin();
                     it != cutlist.end(); ++it )
                {
                    QStringList startend = (*it).split(
                        "-", QString::SkipEmptyParts);
                    if (startend.count() == 2)
                    {
                        cerr << "Cutting from: " << last
                             << " to: " << startend.first().toInt() <<"\n";
                        deleteMap[startend.first().toInt()] = 0;
                        deleteMap[startend.last().toInt()] = 1;
                        last = startend.last().toInt();
                    }
                }
                cerr << "Cutting from: " << last
                     << " to the end\n";
                deleteMap[999999999] = 0;
            }
            else
            {
                cerr << "Missing argument to --inversecut option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-k") ||
                 !strcmp(a.argv()[argpos],"--allkeys"))
        {
            keyframesonly = true;
        }
        else if (!strcmp(a.argv()[argpos],"-b") ||
                 !strcmp(a.argv()[argpos],"--buildindex"))
        {
            build_index = true;
        }
        else if (!strcmp(a.argv()[argpos],"-f") ||
                 !strcmp(a.argv()[argpos],"--fifodir"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                fifodir = a.argv()[argpos + 1];
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -f/--fifodir option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-ro") ||
                 !strcmp(a.argv()[argpos],"--recorderOptions"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                recorderOptions = a.argv()[argpos + 1];
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -ro/--recorderOptions option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"--fifosync"))
        {
            fifosync = true;
        }
        else if (!strcmp(a.argv()[argpos],"--showprogress"))
        {
            showprogress = true;
        }
        else if (!strcmp(a.argv()[argpos],"-m") ||
                 !strcmp(a.argv()[argpos],"--mpeg2"))
        {
            mpeg2 = true;
        }
        else if (!strcmp(a.argv()[argpos],"-e") ||
                 !strcmp(a.argv()[argpos],"--ostream"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                if(!strcmp(a.argv()[argpos + 1], "dvd"))
                    otype = REPLEX_DVD;
                if(!strcmp(a.argv()[argpos + 1], "ts"))
                    otype = REPLEX_TS_SD;

                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -e/--ostream option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-O") ||
                 !strcmp(a.argv()[argpos],"--override-setting"))
        {
            if (a.argc()-1 > argpos && a.argv()[argpos+1][0] != '-')
            {
                QStringList pairs = QString(a.argv()[argpos+1]).split(
                    ",", QString::SkipEmptyParts);
                for (int index = 0; index < pairs.size(); ++index)
                {
                    QStringList tokens = pairs[index].split(
                        "=", QString::SkipEmptyParts);
                    tokens[0].replace(QRegExp("^[\"']"), "");
                    tokens[0].replace(QRegExp("[\"']$"), "");
                    tokens[1].replace(QRegExp("^[\"']"), "");
                    tokens[1].replace(QRegExp("[\"']$"), "");
                    settingsOverride[tokens[0]] = tokens[1];
                }
            }
            else
            {
                cerr << "Invalid or missing argument to -O/--override-setting "
                        "option\n";
                usage(a.argv()[0]);
                return TRANSCODE_EXIT_INVALID_CMDLINE;
            }

            ++argpos;
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help"))
        {
            usage(a.argv()[0]);
            return TRANSCODE_EXIT_OK;
        }
        else
        {
            cerr << "Unknown option: " << a.argv()[argpos] << endl;
            usage(a.argv()[0]);
            return TRANSCODE_EXIT_INVALID_CMDLINE;
        }
    }

    if (outfile == "-")
        print_verbose_messages = VB_NONE;

    //  Load the context
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return TRANSCODE_EXIT_NO_MYTHCONTEXT;
    }

    LanguageSettings::load("mythfrontend");

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

    if (jobID != -1)
    {
        if (JobQueue::GetJobInfoFromID(jobID, jobType, chanid, startts))
        {
            starttime = startts.toString(Qt::ISODate);
            found_starttime = 1;
            found_chanid = 1;
        }
        else
        {
            cerr << "mythtranscode: ERROR: Unable to find DB info for "
                 << "JobQueue ID# " << jobID << endl;
            return TRANSCODE_EXIT_NO_RECORDING_DATA;
        }
    }

    if ((! found_infile && !(found_chanid && found_starttime)) ||
        (found_infile && (found_chanid || found_starttime)) )
    {
         cerr << "Must specify -i OR -c AND -s options!\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if (isVideo && !found_infile)
    {
         cerr << "Must specify --infile to use --video\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if (jobID >= 0 && (found_infile || build_index))
    {
         cerr << "Can't specify -j with --buildindex, --video or --infile\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if ((jobID >= 0) && build_index)
    {
         cerr << "Can't specify both -j and --buildindex\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if (keyframesonly && !fifodir.isEmpty())
    {
         cerr << "Cannot specify both --fifodir and --allkeys\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if (fifosync && fifodir.isEmpty())
    {
         cerr << "Must specify --fifodir to use --fifosync\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }

    VERBOSE(VB_IMPORTANT, QString("Enabled verbose msgs: %1").arg(verboseString));

    if (!MSqlQuery::testDBConnection())
    {
        printf("couldn't open db\n");
        return TRANSCODE_EXIT_DB_ERROR;
    }

    ProgramInfo *pginfo = NULL;
    if (isVideo)
    {
        // We want the absolute file path for the filemarkup table
        QFileInfo inf(infile);
        infile = inf.absoluteFilePath();

        // Create a new, empty ProgramInfo object
        pginfo = new ProgramInfo;
        pginfo->isVideo = 1;
        pginfo->pathname = infile;
    }
    else if (!found_infile)
    {
        pginfo = ProgramInfo::GetProgramFromRecorded(chanid, starttime);

        if (!pginfo)
        {
            QString msg = QString("Couldn't find recording for chanid %1 @ %2")
                .arg(chanid).arg(starttime);
            cerr << msg.toLocal8Bit().constData() << endl;
            return TRANSCODE_EXIT_NO_RECORDING_DATA;
        }

        infile = pginfo->GetPlaybackURL(false, true);
    }
    else
    {
        QFileInfo inf(infile);
        pginfo = ProgramInfo::GetProgramFromBasename(inf.fileName());
        if (!pginfo)
        {
            QString base = inf.baseName();
            QRegExp r(
               "(\\d*)_(\\d\\d\\d\\d)(\\d\\d)(\\d\\d)(\\d\\d)(\\d\\d)(\\d\\d)");
            int pos = r.indexIn(base);
            if (pos > -1)
            {
                chanid = r.cap(1);
                QDateTime startts(
                   QDate(r.cap(2).toInt(), r.cap(3).toInt(), r.cap(4).toInt()),
                   QTime(r.cap(5).toInt(), r.cap(6).toInt(), r.cap(7).toInt()));
                pginfo = ProgramInfo::GetProgramFromRecorded(chanid, startts);

                if (!pginfo)
                {
                    VERBOSE(VB_IMPORTANT,
                        QString("Couldn't find a recording on channel %1 "
                                "starting at %2 in the database.")
                                .arg(chanid).arg(startts.toString()));
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Couldn't deduce channel and start time from "
                                "%1 ").arg(base));
            }
        }
    }

    if (infile.left(7) == "myth://" && (outfile.isNull() || outfile != "-"))
    {
        VERBOSE(VB_IMPORTANT, QString("Attempted to transcode %1. "
               "Mythtranscode is currently unable to transcode remote "
               "files.")
               .arg(infile));
        return TRANSCODE_EXIT_REMOTE_FILE;
    }

    if (outfile.isNull() && !build_index)
        outfile = infile + ".tmp";

    if (jobID >= 0)
        JobQueue::ChangeJobStatus(jobID, JOB_RUNNING);

    Transcode *transcode = new Transcode(pginfo);

    if (!build_index)
        VERBOSE(VB_GENERAL, QString("Transcoding from %1 to %2")
                            .arg(infile).arg(outfile));

    if (showprogress)
        transcode->ShowProgress(true);
    if (!recorderOptions.isEmpty())
        transcode->SetRecorderOptions(recorderOptions);
    int result = 0;
    if (!mpeg2 && !build_index)
    {
        result = transcode->TranscodeFile(infile, outfile,
                                          profilename, useCutlist,
                                          (fifosync || keyframesonly), jobID,
                                          fifodir, deleteMap);
        if ((result == REENCODE_OK) && (jobID >= 0))
            JobQueue::ChangeJobArgs(jobID, "RENAME_TO_NUV");
    }

    int exitcode = TRANSCODE_EXIT_OK;
    if ((result == REENCODE_MPEG2TRANS) || mpeg2 || build_index)
    {
        void (*update_func)(float) = NULL;
        int (*check_func)() = NULL;
        if (useCutlist && !found_infile)
            pginfo->GetCutList(deleteMap);
        if (jobID >= 0)
        {
           glbl_jobID = jobID;
           update_func = &UpdateJobQueue;
           check_func = &CheckJobQueue;
        }

        MPEG2fixup *m2f = new MPEG2fixup(
            infile, outfile,
            &deleteMap, NULL, false, false, 20,
            showprogress, otype, update_func,
            check_func);

        if (build_index)
        {
            int err = BuildKeyframeIndex(m2f, infile, posMap, jobID);
            if (err)
                return err;
            if (update_index)
                UpdatePositionMap(posMap, NULL, pginfo);
            else
                UpdatePositionMap(posMap, outfile + QString(".map"), pginfo);
        }
        else
        {
            result = m2f->Start();
            if (result == REENCODE_OK)
            {
                result = BuildKeyframeIndex(m2f, outfile, posMap, jobID);
                if (result == REENCODE_OK)
                {
                    if (update_index)
                        UpdatePositionMap(posMap, NULL, pginfo);
                    else
                        UpdatePositionMap(posMap, outfile + QString(".map"),
                                          pginfo);
                }
            }
        }
        delete m2f;
    }

    if (result == REENCODE_OK)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_STOPPING);
        VERBOSE(VB_GENERAL, QString("%1 %2 done")
                .arg(build_index ? "Building Index for" : "Transcoding")
                .arg(infile));
    }
    else if (result == REENCODE_CUTLIST_CHANGE)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_RETRY);
        VERBOSE(VB_GENERAL, QString("Transcoding %1 aborted because of "
                                    "cutlist update").arg(infile));
        exitcode = TRANSCODE_EXIT_ERROR_CUTLIST_UPDATE;
    }
    else if (result == REENCODE_STOPPED)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_ABORTING);
        VERBOSE(VB_GENERAL, QString("Transcoding %1 stopped because of "
                                    "stop command").arg(infile));
        exitcode = TRANSCODE_EXIT_STOPPED;
    }
    else
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_ERRORING);
        VERBOSE(VB_GENERAL, QString("Transcoding %1 failed").arg(infile));
        exitcode = result;
    }

    if (jobID >= 0)
        CompleteJob(jobID, pginfo, useCutlist, exitcode);

    transcode->deleteLater();

    delete gContext;
    return exitcode;
}

void UpdatePositionMap(QMap <long long, long long> &posMap, QString mapfile,
                       ProgramInfo *pginfo)
{
    if (pginfo && mapfile.isEmpty())
    {
        pginfo->ClearPositionMap(MARK_KEYFRAME);
        pginfo->ClearPositionMap(MARK_GOP_START);
        pginfo->SetPositionMap(posMap, MARK_GOP_BYFRAME);
    }
    else if (!mapfile.isEmpty())
    {
        FILE *mapfh = fopen(mapfile.toLocal8Bit().constData(), "w");
        if (!mapfh)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Could not open map file '%1'")
                    .arg(mapfile) + ENO);
            return;
        }
        QMap<long long, long long>::const_iterator it;
        fprintf (mapfh, "Type: %d\n", MARK_GOP_BYFRAME);
        for (it = posMap.begin(); it != posMap.end(); ++it)
            fprintf(mapfh, "%lld %lld\n", it.key(), *it);
        fclose(mapfh);
    }
}

int BuildKeyframeIndex(MPEG2fixup *m2f, QString &infile,
                        QMap <long long, long long> &posMap, int jobID)
{
    if (jobID < 0 || JobQueue::GetJobCmd(jobID) != JOB_STOP)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobComment(jobID,
                QString(QObject::tr("Generating Keyframe Index")));
        int err = m2f->BuildKeyframeIndex(infile, posMap);
        if (err)
            return err;
        if (jobID >= 0)
            JobQueue::ChangeJobComment(jobID,
                QString(QObject::tr("Transcode Completed")));
    }
    return 0;
}

int transUnlink(QString filename, ProgramInfo *pginfo)
{
    if (pginfo != NULL && !pginfo->storagegroup.isEmpty() &&
        !pginfo->hostname.isEmpty())
    {
        QString ip = gCoreContext->GetSettingOnHost("BackendServerIP",
                                                pginfo->hostname);
        QString port = gCoreContext->GetSettingOnHost("BackendServerPort",
                                                  pginfo->hostname);
        QString basename = filename.section('/', -1);
        QString uri = QString("myth://%1@%2:%3/%4").arg(pginfo->storagegroup)
                              .arg(ip).arg(port).arg(basename);
        VERBOSE(VB_IMPORTANT, QString("Requesting delete for file '%1'.")
                                      .arg(uri));
        bool ok = RemoteFile::DeleteFile(uri);
        if (ok)
            return 0;
    }

    VERBOSE(VB_IMPORTANT, QString("Deleting file '%1'.").arg(filename));
    return unlink(filename.toLocal8Bit().constData());
}

void CompleteJob(int jobID, ProgramInfo *pginfo, bool useCutlist, int &resultCode)
{
    int status = JobQueue::GetJobStatus(jobID);

    if (!pginfo)
    {
        JobQueue::ChangeJobStatus(jobID, JOB_ERRORED,
                  "Job errored, unable to find Program Info for job");
        return;
    }

    const QString filename = pginfo->GetPlaybackURL(false, true);
    const QByteArray fname = filename.toLocal8Bit();

    if (status == JOB_STOPPING)
    {
        const QString jobArgs = JobQueue::GetJobArgs(jobID);

        const QString tmpfile = filename + ".tmp";
        const QByteArray atmpfile = tmpfile.toLocal8Bit();

        // To save the original file...
        const QString oldfile = filename + ".old";
        const QByteArray aoldfile = oldfile.toLocal8Bit();

        QString cnf = filename;
        if ((jobArgs == "RENAME_TO_NUV") &&
            (filename.contains(QRegExp("mpg$"))))
        {
            QString newbase = pginfo->GetRecordBasename();

            cnf.replace(QRegExp("mpg$"), "nuv");
            newbase.replace(QRegExp("mpg$"), "nuv");
            pginfo->SetRecordBasename(newbase);
        }

        const QString newfile = cnf;
        const QByteArray anewfile = newfile.toLocal8Bit();

        if (rename(fname.constData(), aoldfile.constData()) == -1)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("mythtranscode: Error Renaming '%1' to '%2'")
                    .arg(filename).arg(oldfile) + ENO);
        }

        if (rename(atmpfile.constData(), anewfile.constData()) == -1)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("mythtranscode: Error Renaming '%1' to '%2'")
                    .arg(tmpfile).arg(newfile) + ENO);
        }

        if (!gCoreContext->GetNumSetting("SaveTranscoding", 0))
        {
            int err;
            bool followLinks = gCoreContext->GetNumSetting("DeletesFollowLinks", 0);

            VERBOSE(VB_FILE, QString("mythtranscode: About to unlink/delete "
                                     "file: %1").arg(oldfile));
            QFileInfo finfo(oldfile);
            if (followLinks && finfo.isSymLink())
            {
                QString link = getSymlinkTarget(oldfile);
                QByteArray alink = link.toLocal8Bit();
                err = transUnlink(alink.constData(), pginfo);

                if (err)
                {
                    VERBOSE(VB_IMPORTANT, QString(
                                "mythtranscode: Error deleting '%1' "
                                "pointed to by '%2'")
                            .arg(alink.constData())
                            .arg(aoldfile.constData()) + ENO);
                }

                err = unlink(aoldfile.constData());
                if (err)
                {
                    VERBOSE(VB_IMPORTANT, QString(
                                "mythtranscode: Error deleting '%1', "
                                "a link pointing to '%2'")
                            .arg(aoldfile.constData())
                            .arg(alink.constData()) + ENO);
                }
            }
            else
            {
                if ((err = transUnlink(aoldfile.constData(), pginfo)))
                    VERBOSE(VB_IMPORTANT, QString(
                            "mythtranscode: Error deleting '%1', %2")
                            .arg(oldfile).arg(strerror(errno)));
            }
        }

        // Delete previews if cutlist was applied.  They will be re-created as
        // required.  This prevents the user from being stuck with a preview
        // from a cut area and ensures that the "dimensioned" previews
        // correspond to the new timeline
        if (useCutlist)
        {
            QFileInfo fInfo(filename);
            QString nameFilter = fInfo.fileName() + "*.png";
            // QDir's nameFilter uses spaces or semicolons to separate globs,
            // so replace them with the "match any character" wildcard
            // since mythrename.pl may have included them in filenames
            nameFilter.replace(QRegExp("( |;)"), "?");
            QDir dir (fInfo.path(), nameFilter);

            for (uint nIdx = 0; nIdx < dir.count(); nIdx++)
            {
                // If unlink fails, keeping the old preview is not a problem.
                // The RENAME_TO_NUV check below will attempt to rename the
                // file, if required.
                const QString oldfileop = QString("%1/%2")
                    .arg(fInfo.path()).arg(dir[nIdx]);
                const QByteArray aoldfileop = oldfileop.toLocal8Bit();
                transUnlink(aoldfileop.constData(), pginfo);
            }
        }

        /* Rename all preview thumbnails. */
        if (jobArgs == "RENAME_TO_NUV")
        {
            QFileInfo fInfo(filename);
            QString nameFilter = fInfo.fileName() + "*.png";
            // QDir's nameFilter uses spaces or semicolons to separate globs,
            // so replace them with the "match any character" wildcard
            // since mythrename.pl may have included them in filenames
            nameFilter.replace(QRegExp("( |;)"), "?");

            QDir dir (fInfo.path(), nameFilter);

            for (uint nIdx = 0; nIdx < dir.count(); nIdx++)
            {
                const QString oldfileprev = QString("%1/%2")
                    .arg(fInfo.path()).arg(dir[nIdx]);
                const QByteArray aoldfileprev = oldfileprev.toLocal8Bit();

                QString newfileprev = oldfileprev;
                QRegExp re("mpg(\\..*)?\\.png$");
                if (re.indexIn(newfileprev))
                {
                    newfileprev.replace(
                        re, QString("nuv%1.png").arg(re.cap(1)));
                }
                const QByteArray anewfileprev = newfileprev.toLocal8Bit();

                QFile checkFile(oldfileprev);

                if ((oldfileprev != newfileprev) && (checkFile.exists()))
                    rename(aoldfileprev.constData(), anewfileprev.constData());
            }
        }

        MSqlQuery query(MSqlQuery::InitCon());

        if (useCutlist)
        {
            query.prepare("DELETE FROM recordedmarkup "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :STARTTIME ");
            query.bindValue(":CHANID", pginfo->chanid);
            query.bindValue(":STARTTIME", pginfo->recstartts);

            if (!query.exec())
                MythDB::DBError("Error in mythtranscode", query);

            query.prepare("UPDATE recorded "
                          "SET cutlist = :CUTLIST, bookmark = :BOOKMARK "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :STARTTIME ;");
            query.bindValue(":CUTLIST", "0");
            query.bindValue(":BOOKMARK", "0");
            query.bindValue(":CHANID", pginfo->chanid);
            query.bindValue(":STARTTIME", pginfo->recstartts);

            if (!query.exec())
                MythDB::DBError("Error in mythtranscode", query);

            pginfo->SetCommFlagged(COMM_FLAG_NOT_FLAGGED);
        }
        else
        {
            query.prepare("DELETE FROM recordedmarkup "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :STARTTIME "
                          "AND type not in ( :COMM_START, "
                          "    :COMM_END, :BOOKMARK, "
                          "    :CUTLIST_START, :CUTLIST_END) ;");
            query.bindValue(":CHANID", pginfo->chanid);
            query.bindValue(":STARTTIME", pginfo->recstartts);
            query.bindValue(":COMM_START", MARK_COMM_START);
            query.bindValue(":COMM_END", MARK_COMM_END);
            query.bindValue(":BOOKMARK", MARK_BOOKMARK);
            query.bindValue(":CUTLIST_START", MARK_CUT_START);
            query.bindValue(":CUTLIST_END", MARK_CUT_END);

            if (!query.exec())
                MythDB::DBError("Error in mythtranscode", query);
        }

        JobQueue::ChangeJobStatus(jobID, JOB_FINISHED);

    } else {
        // Not a successful run, so remove the files we created
        QString filename_tmp = filename + ".tmp";
        QByteArray fname_tmp = filename_tmp.toLocal8Bit();
        VERBOSE(VB_IMPORTANT, QString("Deleting %1").arg(filename_tmp));
        transUnlink(fname_tmp.constData(), pginfo);

        QString filename_map = filename + ".tmp.map";
        QByteArray fname_map = filename_map.toLocal8Bit();
        unlink(fname_map.constData());

        if (status == JOB_ABORTING)                     // Stop command was sent
            JobQueue::ChangeJobStatus(jobID, JOB_ABORTED, "Job Aborted");
        else if (status != JOB_ERRORING)                // Recoverable error
            resultCode = TRANSCODE_EXIT_RESTART;
        else                                            // Unrecoverable error
            JobQueue::ChangeJobStatus(jobID, JOB_ERRORED,
                                      "Unrecoverable error");
    }
}

void UpdateJobQueue(float percent_done)
{
    JobQueue::ChangeJobComment(glbl_jobID,
                               QString("%1% " + QObject::tr("Completed"))
                               .arg(percent_done, 0, 'f', 1));
}

int CheckJobQueue()
{
    if (JobQueue::GetJobCmd(glbl_jobID) == JOB_STOP)
    {
        VERBOSE(VB_IMPORTANT, "Transcoding stopped by JobQueue");
        return 1;
    }
    return 0;
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
