#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <qfileinfo.h>
#include <qregexp.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
using namespace std;

#include "exitcodes.h"
#include "programinfo.h"
#include "jobqueue.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "transcode.h"
#include "mpeg2fix.h"

void StoreTranscodeState(ProgramInfo *pginfo, int status, bool useCutlist);
void UpdatePositionMap(QMap <long long, long long> &posMap, QString mapfile,
                       ProgramInfo *pginfo);
int BuildKeyframeIndex(MPEG2fixup *m2f, QString &infile,
                        QMap <long long, long long> &posMap, int jobID);
void CompleteJob(int jobID, ProgramInfo *pginfo, bool useCutlist, int &resultCode);

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
    cerr << "\t--allkeys        or -k: Specifies that the output file\n";
    cerr << "\t                        should be made entirely of keyframes.\n";
    cerr << "\t--fifodir        or -f: Directory to write fifos to\n";
    cerr << "\t                        If --fifodir is specified, 'audout' and 'vidout'\n";
    cerr << "\t                        will be created in the specified directory\n";
    cerr << "\t--fifosync            : Enforce fifo sync\n";
    cerr << "\t--buildindex     or -b: Build a new keyframe index\n";
    cerr << "\t                        (use only if audio and video fifos are read independantly)\n";
    cerr << "\t--showprogress        : Display status info to the stdout\n";
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

    QApplication a(argc, argv, false);

    print_verbose_messages = VB_IMPORTANT;
    verboseString = "important";

    int found_starttime = 0;
    int found_chanid = 0;
    int found_infile = 0;
    int update_index = 1;

    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-s") ||
            !strcmp(a.argv()[argpos],"--starttime")) 
        {
            if (a.argc() > argpos) 
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
            if (a.argc() > argpos) 
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
            jobID = QString(a.argv()[++argpos]).toInt();
        else if (!strcmp(a.argv()[argpos],"-i") ||
                 !strcmp(a.argv()[argpos],"--infile")) 
        {
            if (a.argc() > argpos) 
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
        else if (!strcmp(a.argv()[argpos],"-o") ||
                 !strcmp(a.argv()[argpos],"--outfile"))
        {
            if(a.argc() > argpos)
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
            if (a.argc() > argpos)
            {
                QString temp = a.argv()[++argpos];
                print_verbose_messages = temp.toInt();
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
            if (a.argc()-1 > argpos)
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
            if (a.argc() > argpos) 
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
            if (found_infile)
            {
                QStringList cutlist;
                cutlist = QStringList::split( " ", a.argv()[argpos + 1]);
                for (QStringList::Iterator it = cutlist.begin(); 
                     it != cutlist.end(); ++it )
                {
                    QStringList startend;
                    startend = QStringList::split("-", *it);
                    if (startend.count() == 2)
                    {
                        cerr << "Cutting from: " << startend.first().toInt()
                             << " to: " << startend.last().toInt() <<"\n";
                        deleteMap[startend.first().toInt()] = 1;
                        deleteMap[startend.last().toInt()] = 0;
                    }
                }
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
            if (a.argc() > argpos)
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
            if (a.argc() > argpos)
            {
                if(!strcmp(a.argv()[argpos + 1], "dvd"))
                    otype = REPLEX_DVD;
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
            if ((a.argc() - 1) > argpos)
            {
                QString tmpArg = a.argv()[argpos+1];
                if (tmpArg.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to "
                            "-O/--override-setting option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                } 
 
                QStringList pairs = QStringList::split(",", tmpArg);
                for (unsigned int index = 0; index < pairs.size(); ++index)
                {
                    QStringList tokens = QStringList::split("=", pairs[index]);
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
                return GENERIC_EXIT_INVALID_CMDLINE;
            }

            ++argpos;
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help")) 
        {
            usage(a.argv()[0]);
            return TRANSCODE_EXIT_OK;
        }
    }

    //  Load the context
    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return TRANSCODE_EXIT_NO_MYTHCONTEXT;
    }

    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(it.data()));
            gContext->OverrideSettingForSession(it.key(), it.data());
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
    if (jobID >= 0 && (found_infile || build_index))
    {
         cerr << "Can't specify -j with --buildindex or --infile\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if ((jobID >= 0) && build_index)
    {
         cerr << "Can't specify both -j and --buildindex\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if (keyframesonly && fifodir != NULL)
    {
         cerr << "Cannot specify both --fifodir and --allkeys\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if (fifosync && fifodir == NULL)
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
    if (!found_infile)
    {
        pginfo = ProgramInfo::GetProgramFromRecorded(chanid, starttime);

        if (!pginfo)
        {
            cerr << "Couldn't find recording for chanid " << chanid << " @ " 
                 << starttime << endl;
            return TRANSCODE_EXIT_NO_RECORDING_DATA;
        }

        infile = pginfo->GetPlaybackURL();
    }
    else
    {
        QFileInfo inf(infile);
        QString base = inf.baseName();
        QRegExp r("(\\d*)_(\\d\\d\\d\\d)(\\d\\d)(\\d\\d)(\\d\\d)(\\d\\d)(\\d\\d)");
        int pos = r.search(base);
        if (pos > -1)
        {
            
            chanid = r.cap(1);
            QDateTime startts(QDate(r.cap(2).toInt(), r.cap(3).toInt(), r.cap(4).toInt()),
                              QTime(r.cap(5).toInt(), r.cap(6).toInt(), r.cap(7).toInt()));
            pginfo = ProgramInfo::GetProgramFromRecorded(chanid, startts);
            
            if (!pginfo)
            {
                VERBOSE(VB_IMPORTANT, QString("Couldn't find a recording on channel %1 "
                                              "starting at %2 in the database.")
                                              .arg(chanid).arg(startts.toString()));
            }
        }            
        else
        {
            VERBOSE(VB_IMPORTANT, QString("Couldn't deduce channel and start time from %1 ")
                                          .arg(base));
        }
        
        
    }

    if (infile.left(7) == "myth://") {
        VERBOSE(VB_IMPORTANT, QString("Attempted to transcode %1. "
               "Mythtranscode is currently unable to transcode remote "
               "files.")
               .arg(infile));
        return TRANSCODE_EXIT_REMOTE_FILE;
    }

    if (outfile.isNull())
        outfile = infile + ".tmp";

    if (jobID >= 0)
        JobQueue::ChangeJobStatus(jobID, JOB_RUNNING);

    Transcode *transcode = new Transcode(pginfo);

    VERBOSE(VB_GENERAL, QString("Transcoding from %1 to %2")
                        .arg(infile).arg(outfile));

    if (showprogress)
        transcode->ShowProgress(true);
    int result = 0;
    if (!mpeg2)
    {
        result = transcode->TranscodeFile((char *)infile.ascii(),
                                          (char *)outfile.ascii(),
                                          profilename, useCutlist, 
                                          (fifosync || keyframesonly), jobID,
                                          fifodir);
        if ((result == REENCODE_OK) && (jobID >= 0))
            JobQueue::ChangeJobArgs(jobID, "RENAME_TO_NUV");
    }
                                              
    int exitcode = TRANSCODE_EXIT_OK;
    if ((result == REENCODE_MPEG2TRANS) || mpeg2)
    {
        if (useCutlist && !found_infile)
            pginfo->GetCutList(deleteMap);
       
        MPEG2fixup *m2f = new MPEG2fixup(infile.ascii(), outfile.ascii(),
                                         &deleteMap, NULL, false, false, 20,
                                         showprogress, otype);
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
            int err = m2f->Start();
            if (err)
                return err;
            err = BuildKeyframeIndex(m2f, outfile, posMap, jobID);
            if (err)
                return err;
            if (update_index)
                UpdatePositionMap(posMap, NULL, pginfo);
            else
                UpdatePositionMap(posMap, outfile + QString(".map"), pginfo);
        }
        delete m2f;
        result = REENCODE_OK;
    }
    
    if (result == REENCODE_OK)
    {
        if (jobID >= 0)
            JobQueue::ChangeJobStatus(jobID, JOB_STOPPING);
        VERBOSE(VB_GENERAL, QString("Transcoding %1 done").arg(infile));
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
        exitcode = TRANSCODE_EXIT_UNKNOWN_ERROR;
    }

    if (jobID >= 0)
        CompleteJob(jobID, pginfo, useCutlist, exitcode);

    delete transcode;
    delete pginfo;
    delete gContext;
    return exitcode;
}

void UpdatePositionMap(QMap <long long, long long> &posMap, QString mapfile,
                       ProgramInfo *pginfo)
{
    if (pginfo && ! mapfile)
    {
        pginfo->ClearPositionMap(MARK_KEYFRAME);
        pginfo->ClearPositionMap(MARK_GOP_START);
        pginfo->SetPositionMap(posMap, MARK_GOP_BYFRAME);
    }
    else if (mapfile)
    {
        FILE *mapfh = fopen(mapfile, "w");
        QMap<long long, long long>::Iterator i;
        fprintf (mapfh, "Type: %d\n", MARK_GOP_BYFRAME);
        for (i = posMap.begin(); i != posMap.end(); ++i)
            fprintf(mapfh, "%lld %lld\n", i.key(), i.data());
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

void CompleteJob(int jobID, ProgramInfo *pginfo, bool useCutlist, int &resultCode)
{
    int status = JobQueue::GetJobStatus(jobID);

    if (!pginfo)
    {
        JobQueue::ChangeJobStatus(jobID, JOB_ERRORED,
                  "Job errored, unable to find Program Info for job");
        return;
    }

    QString filename = pginfo->GetPlaybackURL();

    if (status == JOB_STOPPING)
    {
        QString tmpfile = filename + ".tmp";

        // To save the original file...
        QString oldfile = filename + ".old";
        QString newfile = filename;
        QString jobArgs = JobQueue::GetJobArgs(jobID);

        if ((jobArgs == "RENAME_TO_NUV") &&
            (filename.contains(QRegExp("mpg$"))))
        {
            QString newbase = pginfo->GetRecordBasename();

            newfile.replace(QRegExp("mpg$"), "nuv");
            newbase.replace(QRegExp("mpg$"), "nuv");
            pginfo->SetRecordBasename(newbase);
        }

        if (rename(filename, oldfile) == -1)
            perror(QString("mythtranscode: Error Renaming '%1' to '%2'")
                   .arg(filename).arg(oldfile).ascii());

        if (rename(tmpfile, newfile) == -1)
            perror(QString("mythtranscode: Error Renaming '%1' to '%2'")
                   .arg(tmpfile).arg(newfile).ascii());

        if (!gContext->GetNumSetting("SaveTranscoding", 0))
        {
            int err;
            bool followLinks = gContext->GetNumSetting("DeletesFollowLinks", 0);

            VERBOSE(VB_FILE, QString("mythtranscode: About to unlink/delete "
                                     "file: %1").arg(oldfile));
            if (followLinks)
            {
                QFileInfo finfo(oldfile);
                if ((finfo.isSymLink()) &&
                    (err = unlink(finfo.readLink().local8Bit())))
                {
                     VERBOSE(VB_IMPORTANT,
                             QString("Error deleting '%1' link pointing to "
                                     "'%2', %3").arg(oldfile)
                                     .arg(finfo.readLink().local8Bit())
                                     .arg(strerror(errno)));
                }
            }
 
            if ((err = unlink(oldfile.local8Bit())))
                VERBOSE(VB_IMPORTANT, QString("mythtranscode: Error deleting "
                                              "'%1', %2").arg(oldfile)
                                              .arg(strerror(errno)));
        }

        oldfile = filename + ".png";
        newfile += ".png";

        QFile checkFile(oldfile);
        if ((oldfile != newfile) && (checkFile.exists()))
            rename(oldfile, newfile);

        MSqlQuery query(MSqlQuery::InitCon());

        if (useCutlist)
        {
            query.prepare("DELETE FROM recordedmarkup "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :STARTTIME ");
            query.bindValue(":CHANID", pginfo->chanid);
            query.bindValue(":STARTTIME", pginfo->recstartts);
            query.exec();

            if (!query.isActive())
                MythContext::DBError("Error in mythtranscode", query);

            query.prepare("UPDATE recorded "
                          "SET cutlist = :CUTLIST, bookmark = :BOOKMARK "
                          "WHERE chanid = :CHANID "
                          "AND starttime = :STARTTIME ;");
            query.bindValue(":CUTLIST", "0");
            query.bindValue(":BOOKMARK", "0");
            query.bindValue(":CHANID", pginfo->chanid);
            query.bindValue(":STARTTIME", pginfo->recstartts);
            query.exec();

            if (!query.isActive())
                MythContext::DBError("Error in mythtranscode", query);

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
            query.exec();

            if (!query.isActive())
                MythContext::DBError("Error in mythtranscode", query);
        }

        JobQueue::ChangeJobStatus(jobID, JOB_FINISHED);

    } else {
        // Not a successful run, so remove the files we created
        filename += ".tmp";
        VERBOSE(VB_IMPORTANT, QString("Deleting %1").arg(filename));
        unlink(filename);

        filename += ".map";
        unlink(filename);

        if (status == JOB_ABORTING)                     // Stop command was sent
            JobQueue::ChangeJobStatus(jobID, JOB_ABORTED, "Job Aborted");
        else if (status != JOB_ERRORING)                // Recoverable error
            resultCode = TRANSCODE_EXIT_RESTART;
        else                                            // Unrecoverable error
            JobQueue::ChangeJobStatus(jobID, JOB_ERRORED,
                                      "Unrecoverable error");
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
