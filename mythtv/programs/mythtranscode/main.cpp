#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
using namespace std;

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
    cerr << "\t--chanid       or -c: Takes a channel id. REQUIRED\n";
    cerr << "\t--starttime    or -s: Takes a starttime for the\n";
    cerr << "\t\trecording. REQUIRED\n";
    cerr << "\t--infile       or -i: Input file (Alternative to -c and -s)\n";
    cerr << "\t--profile      or -p: Takes a profile number of 'autodetect'\n";
    cerr << "\t\trecording profile. REQUIRED\n";
    cerr << "\t--honorcutlist or -l: Specifies whether to use the cutlist.\n";
    cerr << "\t--allkeys      or -k: Specifies that the output file\n";
    cerr << "\t\tshould be made entirely of keyframes.\n";
    cerr << "\t--database     or -d: Store status in the db\n";
    cerr << "\t--fifodir      or -f: Directory to write fifos to\n";
    cerr << "\t\tIf --fifodir is specified, 'audout' and 'vidout'\n";
    cerr << "\t\twill be created in the specified directory\n";
    cerr << "\t--fifosync          : Enforce fifo sync\n";
    cerr << "\t--buildindex   or -b: Build a new keyframe index\n";
    cerr << "\t\t(use only if audio and video fifos are read independantly)\n";
    cerr << "\t--showprogress      : Display status info to the stdout\n";
    cerr << "\t--help         or -h: Prints this help statement.\n";
}

int main(int argc, char *argv[])
{
    QString chanid, starttime, infile;
    QString profilename = QString("autodetect");
    QString fifodir = NULL;
    bool useCutlist = false, keyframesonly = false, use_db = false;
    bool build_index = false, fifosync = false, showprogress = false;
    QMap<long long, int> deleteMap;
    QMap<long long, long long> posMap;
    srand(time(NULL));

    QApplication a(argc, argv, false);

    //  Load the context
    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    gContext->Init(false);

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
                return -1;
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
                return -1;
            }
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
                return -1;
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
                cerr << "Missing argument to -c/--chanid option\n";
                usage(a.argv()[0]);
                return -1;
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
        else if (!strcmp(a.argv()[argpos],"-d") ||
                 !strcmp(a.argv()[argpos],"--database"))
        {
            use_db = true;
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
                return -1;
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
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help")) 
        {
            usage(a.argv()[0]);
            return(0);
        }
    }

    if ((! found_infile && !(found_chanid && found_starttime)) ||
        (found_infile && (found_chanid || found_starttime)) ) 
    {
         cerr << "Must specify -i OR -c AND -s options!\n";
         return -1;
    }
    if (found_infile && (use_db || build_index))
    {
         cerr << "Can't specify --database or --buildindex with --infile\n";
         return -1;
    }
    if (use_db && build_index)
    {
         cerr << "Can't specify both --database and --buildindex\n";
         return -1;
    }
    if (keyframesonly && fifodir != NULL)
    {
         cerr << "Cannot specify both --fifodir and --allkeys\n";
         return -1;
    }
    if (fifosync && fifodir == NULL)
    {
         cerr << "Must specify --fifodir to use --fifosync\n";
         return -1;
    }

    //if (!db)
    //{
    //    printf("Couldn't connect to database\n");
    //    return -1;
    //}

    if (!MSqlQuery::testDBConnection())
    {
        printf("couldn't open db\n");
        return -1;
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
            return -1;
        }

        infile = pginfo->GetPlaybackURL();
    }

    QString tmpfile;
    if (infile.left(7) == "myth://") {
        VERBOSE(VB_IMPORTANT, QString("Attempted to transcode %1. "
               "Mythtranscode is currently unable to transcode remote "
               "files.")
               .arg(infile));
        exit(0);
    }

    tmpfile = infile + ".tmp";

    int jobID = -1;
    if (use_db)
    {
        jobID = JobQueue::GetJobID(JOB_TRANSCODE, pginfo->chanid,
                                   pginfo->startts);
        JobQueue::ChangeJobStatus(jobID, JOB_RUNNING);
    }

    if (found_infile)
    {
        MPEG2trans *mpeg2trans = new MPEG2trans(&deleteMap);
        mpeg2trans->DoTranscode(infile, tmpfile);
        mpeg2trans->BuildKeyframeIndex(tmpfile, posMap);
        UpdatePositionMap(posMap, tmpfile + ".map", NULL);
        exit(0);
    }
    Transcode *transcode = new Transcode(pginfo);

    VERBOSE(VB_GENERAL, QString("Transcoding from %1 to %2")
                        .arg(infile).arg(tmpfile));

    if (showprogress)
        transcode->ShowProgress(true);
    int result = 0;
    result = transcode->TranscodeFile((char *)infile.ascii(),
                                   (char *)tmpfile.ascii(),
                                   profilename, useCutlist, 
                                   (fifosync || keyframesonly), use_db,
                                   fifodir);
    int retval;
    if (result == REENCODE_MPEG2TRANS)
    {
        if (useCutlist)
            pginfo->GetCutList(deleteMap);
        MPEG2trans *mpeg2trans = new MPEG2trans(&deleteMap);
        if (build_index)
        {
            mpeg2trans->BuildKeyframeIndex(infile, posMap);
            UpdatePositionMap(posMap, NULL, pginfo);
        }
        else
        {
            mpeg2trans->DoTranscode(infile, tmpfile);
            mpeg2trans->BuildKeyframeIndex(tmpfile, posMap);
            UpdatePositionMap(posMap, tmpfile + ".map", NULL);
        }
        // this is a hack, since the mpeg2 transcoder doesn't return anything
        // useful yet.
        result = REENCODE_OK;
    }
    if (result == REENCODE_OK)
    {
        if (use_db)
            JobQueue::ChangeJobStatus(jobID, JOB_STOPPING);
        VERBOSE(VB_GENERAL, QString("Transcoding %1 done").arg(infile));
        retval = 0;
    } 
    else if (result == REENCODE_CUTLIST_CHANGE)
    {
        if (use_db)
            JobQueue::ChangeJobStatus(jobID, JOB_RETRY);
        VERBOSE(VB_GENERAL, QString("Transcoding %1 aborted because of "
                                    "cutlist update").arg(infile));
        retval = 1;
    }
    else if (result == REENCODE_STOPPED)
    {
        if (use_db)
            JobQueue::ChangeJobStatus(jobID, JOB_ABORTING);
        VERBOSE(VB_GENERAL, QString("Transcoding %1 stopped because of "
                                    "stop command").arg(infile));
        retval = 1;
    }
    else
    {
        if (use_db)
            JobQueue::ChangeJobStatus(jobID, JOB_ERRORING);
        VERBOSE(VB_GENERAL, QString("Transcoding %1 failed").arg(infile));
        retval = -1;
    }

    delete transcode;
    delete pginfo;
    delete gContext;
    return retval;
}
void UpdatePositionMap(QMap <long long, long long> &posMap, QString mapfile,
                       ProgramInfo *pginfo)
{
    if (pginfo)
    {
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
