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

#include "libmyth/mythcontext.h"
#include "libmythtv/commercial_skip.h"
#include "libmythtv/NuppelVideoPlayer.h"
#include "libmythtv/programinfo.h"
#include "libmyth/mythdbcon.h"

using namespace std;

bool testMode = false;
bool showBlanks = false;
bool justBlanks = false;
bool quiet = false;
bool force = false;

bool showPercentage = true;
bool fullSpeed = true;
bool rebuildSeekTable = false;
bool beNice = true;
bool inJobQueue = false;
bool copyToCutlist = false;

double fps = 29.97; 

void BuildVideoMarkup(QString& filename)
{
    ProgramInfo* program_info = new ProgramInfo;
    program_info->recstartts = QDateTime::currentDateTime().addSecs( -180 * 60);
    program_info->recendts = QDateTime::currentDateTime().addSecs(-1);
    program_info->isVideo = true;
    program_info->pathname = filename;

    RingBuffer *tmprbuf = new RingBuffer(filename, false);


    if (!MSqlQuery::testDBConnection())
    {
        cerr << "Unable to open commflag db connection\n";
        delete tmprbuf;
        delete program_info;
        return;
    }

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer(program_info);
    nvp->SetRingBuffer(tmprbuf);

    nvp->RebuildSeekTable(!quiet);

    if (!quiet)
        printf( "Rebuilt\n" );

    delete nvp;
    delete tmprbuf;
    delete program_info;
}

void CopySkipListToCutList(QString chanid, QString starttime)
{
    QMap<long long, int> cutlist;
    QMap<long long, int>::Iterator it;

    ProgramInfo *pginfo =
        ProgramInfo::GetProgramFromRecorded(chanid, starttime);

    if (!pginfo)
    {
        cerr << "No program data exists for channel " <<
            chanid.ascii() << "at " << starttime.ascii() << endl;
        exit(-1);
    }

    pginfo->GetCommBreakList(cutlist);
    for (it = cutlist.begin(); it != cutlist.end(); ++it)
        if (it.data() == MARK_COMM_START)
            cutlist[it.key()] = MARK_CUT_START;
        else
            cutlist[it.key()] = MARK_CUT_END;
    pginfo->SetCutList(cutlist);
}

int FlagCommercials(QString chanid, QString starttime)
{
    int breaksFound = 0;
    int commDetectMethod = gContext->GetNumSetting("CommercialSkipMethod",
                                                   COMM_DETECT_BLANKS);
    QMap<long long, int> blanks;
    ProgramInfo *program_info =
        ProgramInfo::GetProgramFromRecorded(chanid, starttime);

    if (!program_info)
    {
        printf( "No program data exists for channel %s at %s\n",
            chanid.ascii(), starttime.ascii());
        return(0);
    }

    QString filename = program_info->GetRecordFilename("...");

    if (!quiet)
    {
        printf( "%-6.6s  %-14.14s  %-41.41s  ",
            chanid.ascii(), starttime.ascii(), program_info->title.ascii());
        fflush( stdout );
    
        if ((!force) && (program_info->IsCommProcessing()))
        {
            printf( "IN USE\n" );
            printf( "                        "
                        "(the program is already being flagged elsewhere)\n" );
            return(0);
        }
    }
    else
    {
        if ((!force) && (program_info->IsCommProcessing()))
            return(0);
    }

    filename = program_info->GetPlaybackURL();

    if (testMode)
    {
        if (!quiet)
            printf( "0 (test)\n" );
        return(0);
    }

    blanks.clear();
    if (showBlanks)
    {
        program_info->GetBlankFrameList(blanks);
        if (!blanks.empty())
        {
            QMap<long long, int> comms;
            QMap<long long, int> breaks;
            QMap<long long, int>::Iterator i;
            long long last_blank = -1;

            printf( "\n" );

            program_info->GetCommBreakList(breaks);
            if (!breaks.empty())
            {
                printf( "Breaks (from recordedmarkup table)\n" );
                for (i = breaks.begin(); i != breaks.end(); ++i)
                {
                    long long frame = i.key();
                    int my_fps = (int)ceil(fps);
                    int hour = (frame / my_fps) / 60 / 60;
                    int min = (frame / my_fps) / 60 - (hour * 60);
                    int sec = (frame / my_fps) - (min * 60) - (hour * 60 * 60);
                    int frm = frame - ((sec * my_fps) + (min * 60 * my_fps) +
                                (hour * 60 * 60 * my_fps));
                    printf( "%7lld : %d (%02d:%02d:%02d.%02d) (%d)\n",
                        i.key(), i.data(), hour, min, sec, frm,
                        (int)(frame / my_fps));
                }
            }

            for (i = blanks.begin(); i != blanks.end(); ++i)
            {
                long long frame = i.key();

                if ((frame - 1) != last_blank)
                {
                    int my_fps = (int)ceil(fps);
                    int hour = (frame / my_fps) / 60 / 60;
                    int min = (frame / my_fps) / 60 - (hour * 60);
                    int sec = (frame / my_fps) - (min * 60) - (hour * 60 * 60);
                    int frm = frame - ((sec * my_fps) + (min * 60 * my_fps) +
                                (hour * 60 * 60 * my_fps));
                    printf( "(skip %lld)\nBlank: (%02d:%02d:%02d.%02d) (%d) ",
                        frame - last_blank, hour, min, sec, frm,
                        (int)(frame / my_fps));
                }
                printf( "%lld ", frame );
                last_blank = frame;
            }
            printf( "\n" );

            // build break list and print what it would be
            CommDetect *commDetect = new CommDetect(0, 0, fps,
                                                    commDetectMethod);
            commDetect->SetBlankFrameMap(blanks);
            commDetect->GetBlankCommMap(comms);
            printf( "Commercials (computed using only blank frame detection)\n" );
            for (i = comms.begin(); i != comms.end(); ++i)
            {
                long long frame = i.key();
                int my_fps = (int)ceil(fps);
                int hour = (frame / my_fps) / 60 / 60;
                int min = (frame / my_fps) / 60 - (hour * 60);
                int sec = (frame / my_fps) - (min * 60) - (hour * 60 * 60);
                int frm = frame - ((sec * my_fps) + (min * 60 * my_fps) +
                            (hour * 60 * 60 * my_fps));
                printf( "%7lld : %d (%02d:%02d:%02d.%02d) (%d)\n",
                    i.key(), i.data(), hour, min, sec, frm,
                    (int)(frame / my_fps));
            }

            commDetect->GetBlankCommBreakMap(breaks);

            printf( "Breaks (computed using only blank frame detection)\n" );
            for (i = breaks.begin(); i != breaks.end(); ++i)
            {
                long long frame = i.key();
                int my_fps = (int)ceil(fps);
                int hour = (frame / my_fps) / 60 / 60;
                int min = (frame / my_fps) / 60 - (hour * 60);
                int sec = (frame / my_fps) - (min * 60) - (hour * 60 * 60);
                int frm = frame - ((sec * my_fps) + (min * 60 * my_fps) +
                            (hour * 60 * 60 * my_fps));
                printf( "%7lld : %d (%02d:%02d:%02d.%02d) (%d)\n",
                    i.key(), i.data(), hour, min, sec, frm,
                    (int)(frame / my_fps));
            }

            if (justBlanks)
            {
                printf( "Saving new commercial break list to database.\n" );
                program_info->SetCommBreakList(breaks);
            }

            delete commDetect;
        }
        delete program_info;
        return(0);
    }
    else if (justBlanks)
    {
        program_info->GetBlankFrameList(blanks);
        if (!blanks.empty())
        {
            QMap<long long, int> comms;
            QMap<long long, int> breaks;
            QMap<long long, int> orig_breaks;
            CommDetect *commDetect = new CommDetect(0, 0, fps,
                                                    commDetectMethod);

            comms.clear();
            breaks.clear();
            orig_breaks.clear();

            program_info->GetCommBreakList(orig_breaks);

            commDetect->SetBlankFrameMap(blanks);
            commDetect->GetCommBreakMap(breaks);
            program_info->SetCommBreakList(breaks);

            if (!quiet)
                printf( "%d -> %d\n",
                    orig_breaks.count() / 2, breaks.count() / 2);

            delete commDetect;
        }
        else
        {
            if (!quiet)
                printf( "no blanks\n" );
        }
        delete program_info;
        return(0);
    }

    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    if (!MSqlQuery::testDBConnection())
    {
        cerr << "Unable to open commflag db connection\n";
        delete tmprbuf;
        delete program_info;
        return(0);
    }
    
    NuppelVideoPlayer *nvp = new NuppelVideoPlayer(program_info);
    nvp->SetRingBuffer(tmprbuf);

    if (rebuildSeekTable)
    {
        nvp->RebuildSeekTable();

        if (!quiet)
            printf( "Rebuilt\n" );
    }
    else
    {
        breaksFound = nvp->FlagCommercials(showPercentage, fullSpeed,
                                               NULL, inJobQueue);

        if (!quiet)
            printf( "%d\n", breaksFound );
    }

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
    time_t time_now;
    bool allRecorded = false;
    QString verboseString = QString("");

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
                printf("missing or invalid parameters for --chanid option\n");
                exit(7);
            }
            
            chanid += a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos],"-s") ||
                 !strcmp(a.argv()[argpos],"--starttime"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                printf("missing or invalid parameters for --starttime option\n");
                exit(8);
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
            cerr << filename << endl;
            isVideo = true;
            rebuildSeekTable = true;
            beNice = false;
        }
        else if (!strcmp(a.argv()[argpos], "--blanks"))
        {
            if (!quiet)
                showBlanks = true;
        }
        else if (!strcmp(a.argv()[argpos], "--gencutlist"))
        {
            copyToCutlist = true;
        }
        else if (!strcmp(a.argv()[argpos], "--justblanks"))
        {
            justBlanks = true;
        }
        else if (!strcmp(a.argv()[argpos], "-j"))
        {
            inJobQueue = true;
        }
        else if (!strcmp(a.argv()[argpos], "--all"))
        {
            allRecorded = true;
        }
        else if (!strcmp(a.argv()[argpos], "--fps"))
        {
            fps = atof(a.argv()[++argpos]);
        }
        else if (!strcmp(a.argv()[argpos], "--test"))
        {
            testMode = true;
        }
        else if (!strcmp(a.argv()[argpos], "--quiet"))
        {
            if (!showBlanks)
            {
                quiet = true;
                showPercentage = false;
            }
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
        else if (!strcmp(a.argv()[argpos],"-v") ||
                 !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc() > argpos)
            {
                QString temp = a.argv()[argpos+1];
                if (temp.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -v/--verbose option\n";
                    return -1;
                }
                else
                {
                    QStringList verboseOpts;
                    verboseOpts = QStringList::split(',',a.argv()[argpos+1]);
                    ++argpos;
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
                            cerr << "Unknown argument for -v/--verbose: "
                                 << *it << endl;;
                        }
                    }
                }
            } else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return -1;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help"))
        {
            cerr << "Valid Options are:" << endl <<
                    "-c OR --chanid chanid        Flag recording with given channel ID" << endl <<
                    "-s OR --starttime starttime  Flag recording with given starttime" << endl <<
                    "-f OR --file filename        Flag recording with specific filename" << endl <<
                    "--video filename             Rebuild the seektable for a video (non-recording) file" << endl <<
                    "--sleep                      Give up some CPU time after processing" << endl <<
                    "--rebuild                    Do not flag commercials, just rebuild seektable" << endl <<
                    "--gencutlist                 Copy the commercial skip list to the cutlist" << endl <<
                    "                             each frame." << endl <<
                    "-v OR --verbose debug-level  Prints more information" << endl <<
                    "                             Accepts any combination (separated by comma)" << endl << 
                    "                             of all,none,quiet,record,playback," << endl <<
                    "                             channel,osd,file,schedule,network,commflag" << endl <<
                    "--quiet                      Turn OFF display (also causes the program to" << endl <<
                    "                             sleep a little every frame so it doesn't hog CPU)" << endl <<
                    "                             (takes precedence over --blanks if given first)" << endl <<
                    "--blanks                     Show list of blank frames if already in database" << endl <<
                    "                             (takes precedence over --quiet if given first)" << endl <<
                    "--fps fps                    Set the Frames Per Second.  Only necessary when" << endl <<
                    "                             running with the '--blanks' option since all" << endl <<
                    "                             other processing modes get the frame rate from" << endl <<
                    "                             the player." << endl <<
                    "--all                        Re-run commercial flagging for all recorded" << endl <<
                    "                             programs using current detection method." << endl <<
                    "--force                      Force flagging of a video even if mythcommflag" << endl <<
                    "                             thinks it is already in use by another instance." << endl <<
                    "--hogcpu                     Do not nice the flagging process." << endl <<
                    "                             WARNING: This will consume all free CPU time." << endl <<
                    "-h OR --help                 This text" << endl << endl <<
                    "Note: both --chanid and --starttime must be used together" << endl <<
                    "      if either is used." << endl << endl <<
                    "If no command line arguments are specified, all" << endl <<
                    "unflagged videos will be flagged." << endl << endl;
            exit(9);
        }
        else
        {
            printf("illegal option: '%s' (use --help)\n", a.argv()[argpos]);
            exit(10);
        }

        ++argpos;
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    gContext->Init(false);


    if (!MSqlQuery::testDBConnection())
    {
        printf("couldn't open db\n");
        exit(11);
    }

    if ((chanid.isEmpty() && !starttime.isEmpty()) ||
        (!chanid.isEmpty() && starttime.isEmpty()))
    {
        printf( "You must specify both the Channel ID and the Start Time\n");
        exit(12);
    }

    if (copyToCutlist) {
        CopySkipListToCutList(chanid, starttime);
        exit(0);
    }

    if (inJobQueue) {
        int jobQueueCPU = gContext->GetNumSetting("JobQueueCPU", 0);

        if (beNice || jobQueueCPU < 2)
            nice(17);

        if (jobQueueCPU)
            fullSpeed = true;
        else
            fullSpeed = false;

        quiet = true;
        isVideo = false;
        showBlanks = false;
        justBlanks = false;
        testMode = false;
        showPercentage = false;

        int breaksFound = FlagCommercials(chanid, starttime);

        delete gContext;

        exit(breaksFound);
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

        printf( "\n" );
        printf( "MythTV Commercial Flagger, started at %s", ctime(&time_now));

        printf( "\n" );
        if (!isVideo)
        {
            if (!rebuildSeekTable)
            {
                printf( "Flagging commercial breaks for:\n" );
                if (a.argc() == 1)
                    printf( "ALL Un-flagged programs\n" );
            }
            else
            {
                printf( "Rebuilding SeekTable(s) for:\n" );
            }

            printf( "ChanID  Start Time      "
                    "Title                                      " );
            if (rebuildSeekTable)
                printf("Status\n");
            else
                printf("Breaks\n");

            printf( "------  --------------  "
                    "-----------------------------------------  ------\n" );
        }
        else
        {
            printf( "Building seek table for: %s\n", (const char*)filename);
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
                                    "WHERE endtime < now() "
                                   "ORDER BY starttime;");
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
                    QString markupstr = QString("SELECT count(*) "
                                                "FROM recordedmarkup "
                                                "WHERE chanid = %1 "
                                                "AND starttime = %2 "
                                                "AND type in (%3,%4);")
                                                .arg(chanid).arg(starttime)
                                                .arg(MARK_COMM_START)
                                                .arg(MARK_COMM_END);

                    markupquery.exec(markupstr);
                    if (markupquery.isActive() && markupquery.numRowsAffected() > 0)
                    {
                        if (markupquery.next())
                        {
                            if (markupquery.value(0).toInt() == 0)
                                FlagCommercials(chanid, starttime);
                        }
                    }
                }
            }
        }
        else
        {
             MythContext::DBError("Querying recorded programs", recordedquery);
             exit(13);
        }
    }

    delete gContext;

    time_now = time(NULL);

    if (!quiet)
    {
        printf( "\n" );
        printf( "Finished commercial break flagging at %s", ctime(&time_now));
    }

    exit(0);
}
