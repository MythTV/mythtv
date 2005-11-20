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
#include "mpeg2trans.h"

void StoreTranscodeState(ProgramInfo *pginfo, int status, bool useCutlist);
void UpdatePositionMap(QMap <long long, long long> &posMap, QString mapfile,
                       ProgramInfo *pginfo);

void usage(char *progname) 
{
    cerr << "Usage: " << progname << " <--chanid <channelid>>\n";
    cerr << "\t<--starttime <starttime>> <--profile <profile>>\n";
    cerr << "\t[options]\n\n";
    cerr << "\t--mpeg2          or -m: Perform MPEG2 to MPEG2 transcode.\n";
    cerr << "\t--chanid         or -c: Takes a channel id. REQUIRED\n";
    cerr << "\t--starttime      or -s: Takes a starttime for the\n";
    cerr << "\t                        recording. REQUIRED\n";
    cerr << "\t--infile         or -i: Input file (Alternative to -c and -s)\n";
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
    QString chanid, starttime, infile;
    QString profilename = QString("autodetect");
    QString fifodir = NULL;
    int jobID = -1;
    bool useCutlist = false, keyframesonly = false;
    bool build_index = false, fifosync = false, showprogress = false, mpeg2 = false;
    QMap<long long, int> deleteMap;
    QMap<long long, long long> posMap;
    srand(time(NULL));

    QApplication a(argc, argv, false);

    print_verbose_messages = VB_IMPORTANT;
    verboseString = "important";

    //  Load the context
    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return TRANSCODE_EXIT_NO_MYTHCONTEXT;
    }

    int found_starttime = 0;
    int found_chanid = 0;
    int found_infile = 0;

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
        { 
            QDateTime startts;
            jobID = QString(a.argv()[++argpos]).toInt();
            int jobType = JOB_NONE;
            
            if ( !JobQueue::GetJobInfoFromID(jobID, jobType, chanid, startts))
            {
                cerr << "mythtranscode: ERROR: Unable to find DB info for "
                     << "JobQueue ID# " << jobID << endl;
                return TRANSCODE_EXIT_NO_RECORDING_DATA;
            }
            starttime = startts.toString(Qt::ISODate);
            found_starttime = 1;
            found_chanid = 1;
        }
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
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help")) 
        {
            usage(a.argv()[0]);
            return TRANSCODE_EXIT_OK;
        }
    }

    if ((! found_infile && !(found_chanid && found_starttime)) ||
        (found_infile && (found_chanid || found_starttime)) ) 
    {
         cerr << "Must specify -i OR -c AND -s options!\n";
         return TRANSCODE_EXIT_INVALID_CMDLINE;
    }
    if (found_infile && ((jobID >= 0) || build_index))
    {
         cerr << "Can't specify -j or --buildindex with --infile\n";
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

    if (verboseString != "")
        VERBOSE(VB_ALL, QString("Enabled verbose msgs :%1").arg(verboseString));

    if (!MSqlQuery::testDBConnection())
    {
        printf("couldn't open db\n");
        return TRANSCODE_EXIT_DB_ERROR;
    }

    ProgramInfo *pginfo = NULL;
    if (!found_infile)
    {
        QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
        pginfo = ProgramInfo::GetProgramFromRecorded(chanid, startts);

        if (!pginfo)
        {
            cerr << "Couldn't find recording " << chanid << " " 
                 << startts.toString() << endl;
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

    QString tmpfile;
    if (infile.left(7) == "myth://") {
        VERBOSE(VB_IMPORTANT, QString("Attempted to transcode %1. "
               "Mythtranscode is currently unable to transcode remote "
               "files.")
               .arg(infile));
        return TRANSCODE_EXIT_REMOTE_FILE;
    }

    tmpfile = infile + ".tmp";

    if (jobID >= 0)
        JobQueue::ChangeJobStatus(jobID, JOB_RUNNING);

    Transcode *transcode = new Transcode(pginfo);

    VERBOSE(VB_GENERAL, QString("Transcoding from %1 to %2")
                        .arg(infile).arg(tmpfile));

    if (showprogress)
        transcode->ShowProgress(true);
    int result = 0;
    if (!mpeg2)
    {
        result = transcode->TranscodeFile((char *)infile.ascii(),
                                          (char *)tmpfile.ascii(),
                                          profilename, useCutlist, 
                                          (fifosync || keyframesonly), jobID,
                                          fifodir);
        if ((result == REENCODE_OK) && (jobID >= 0))
            JobQueue::ChangeJobArgs(jobID, "RENAME_TO_NUV");
    }
                                              
    int exitcode = TRANSCODE_EXIT_OK;
    if ((result == REENCODE_MPEG2TRANS) || mpeg2)
    {
        if (!pginfo)
        {
            VERBOSE( VB_IMPORTANT, "MPEG2 to MPEG2 transcoding requires a program info entry.");
            return TRANSCODE_EXIT_NO_RECORDING_DATA;
        }
        
        MPEG2trans *mpeg2trans = new MPEG2trans(pginfo, jobID);
        if (build_index)
        {
            int err = mpeg2trans->BuildKeyframeIndex(infile, posMap);
            if (err)
                return err;
            UpdatePositionMap(posMap, NULL, pginfo);
        }
        else
        {
            int err = mpeg2trans->DoTranscode(infile, tmpfile, useCutlist);
            if (err)
                return err;
            err = mpeg2trans->BuildKeyframeIndex(tmpfile, posMap);
            if (err)
                return err;
            UpdatePositionMap(posMap, NULL, pginfo);
        }
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

    delete transcode;
    delete pginfo;
    delete gContext;
    return exitcode;
}

void UpdatePositionMap(QMap <long long, long long> &posMap, QString mapfile,
                       ProgramInfo *pginfo)
{
    if (pginfo)
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

/* vim: set expandtab tabstop=4 shiftwidth=4: */
