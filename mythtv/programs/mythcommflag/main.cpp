#include <qapplication.h>
#include <qstring.h>
#include <qregexp.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include "libmyth/mythcontext.h"
#include "libmythtv/commercial_skip.h"
#include "libmythtv/NuppelVideoPlayer.h"
#include "libmythtv/programinfo.h"

using namespace std;

MythContext *gContext;

int testmode = 0;
int show_blanks = 0;
int just_blanks = 0;
int quiet = 0;
int force = 0;

bool show_percentage = true;
bool full_speed = true;
bool be_nice = true;

double fps = 29.97; 

void FlagCommercials(QSqlDatabase *db, QString chanid, QString starttime)
{
    QMap<long long, int> blanks;
    ProgramInfo *program_info =
        ProgramInfo::GetProgramFromRecorded(db, chanid, starttime);

    if (!program_info)
    {
        printf( "No program data exists for channel %s at %s\n",
            chanid.ascii(), starttime.ascii());
        return;
    }

    QString filename = program_info->GetRecordFilename("...");

    if (!quiet)
    {
        printf( "%-6.6s  %-14.14s  %-44.44s  ",
            chanid.ascii(), starttime.ascii(), program_info->title.ascii());
        fflush( stdout );
    
        if ((!force) && (program_info->CheckMarkupFlag(MARK_PROCESSING, db)))
        {
            printf( "IN USE\n" );
            printf( "                        "
                        "(the program is already being flagged elsewhere)\n" );
            return;
        }
    }
    else
    {
        if ((!force) && (program_info->CheckMarkupFlag(MARK_PROCESSING, db)))
            return;
    }

    filename = program_info->GetRecordFilename(
                            gContext->GetSetting("RecordFilePrefix"));

    if (testmode)
    {
        if (!quiet)
            printf( "0 (test)\n" );
        return;
    }

    blanks.clear();
    if (show_blanks)
    {
        program_info->GetBlankFrameList(blanks, db);
        if (!blanks.empty())
        {
            QMap<long long, int> comms;
            QMap<long long, int> breaks;
            QMap<long long, int>::Iterator i;
            long long last_blank = -1;

            printf( "\n" );

            program_info->GetCommBreakList(breaks, db);
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
            CommDetect *commDetect = new CommDetect(0, 0, fps);
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

            if (just_blanks)
            {
                printf( "Saving new commercial break list to database.\n" );
                program_info->SetCommBreakList(breaks, db);
            }

            delete commDetect;
        }
        delete program_info;
        return;
    }
    else if (just_blanks)
    {
        program_info->GetBlankFrameList(blanks, db);
        if (!blanks.empty())
        {
            QMap<long long, int> comms;
            QMap<long long, int> breaks;
            QMap<long long, int> orig_breaks;
            CommDetect *commDetect = new CommDetect(0, 0, fps);

            comms.clear();
            breaks.clear();
            orig_breaks.clear();

            program_info->GetCommBreakList(orig_breaks, db);

            commDetect->SetBlankFrameMap(blanks);
            commDetect->GetCommBreakMap(breaks);
            program_info->SetCommBreakList(breaks, db);

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
        return;
    }

    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer(db, program_info);
    nvp->SetRingBuffer(tmprbuf);

    int comms_found = nvp->FlagCommercials(show_percentage, full_speed);

    if (!quiet)
        printf( "%d\n", comms_found );

    delete nvp;
    delete tmprbuf;
    delete program_info;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);
    int argpos = 1;
    QString chanid;
    QString starttime;
    time_t time_now;
    int all_recorded = 0;

    while (argpos < a.argc())
    {
        if (!strcmp(a.argv()[argpos], "--chanid"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                printf("missing or invalid parameters for --chanid option\n");
                exit(-1);
            }
            
            chanid += a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos], "--starttime"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "--", 2))
            {
                printf("missing or invalid parameters for --starttime option\n");
                exit(-1);
            }
            
            starttime += a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos], "--blanks"))
        {
            if (!quiet)
                show_blanks = 1;
        }
        else if (!strcmp(a.argv()[argpos], "--justblanks"))
        {
            just_blanks = 1;
        }
        else if (!strcmp(a.argv()[argpos], "--all"))
        {
            all_recorded = 1;
        }
        else if (!strcmp(a.argv()[argpos], "--fps"))
        {
            fps = atof(a.argv()[++argpos]);
        }
        else if (!strcmp(a.argv()[argpos], "--test"))
        {
            testmode = 1;
        }
        else if (!strcmp(a.argv()[argpos], "--quiet"))
        {
            if (!show_blanks)
            {
                quiet = 1;
                show_percentage = false;
            }
        }
        else if (!strcmp(a.argv()[argpos], "--sleep"))
        {
            full_speed = false;
        }
        else if (!strcmp(a.argv()[argpos], "--force"))
        {
            force = 1;
        }
        else if (!strcmp(a.argv()[argpos], "--hogcpu"))
        {
            be_nice = false;
        }
        else if (!strcmp(a.argv()[argpos], "--help"))
        {
            cout << "usage:\n";
            cout << "--chanid <chanid>\n";
            cout << "   Run Commercial Flagging routine only for recordings\n";
            cout << "   from given channel ID.\n";
            cout << "\n";
            cout << "--starttime <starttime>\n";
            cout << "   Run Commercial Flagging routine only for recordings\n";
            cout << "   starting at starttime.\n";
            cout << "\n";
            cout << "--sleep\n";
            cout << "   Give up some cpu time after processing each frame.\n";
            cout << "\n";
            cout << "--quiet\n";
            cout << "   Turn OFF display (also causes the program to sleep\n";
            cout << "   a little every frame so it doesn't hog cpu)\n";
            cout << "   (takes precedence over --blanks if specified first)\n";
            cout << "\n";
            cout << "--blanks\n";
            cout << "   Show list of blank frames if already in database\n";
            cout << "   (takes precedence over --quiet if specified first)\n";
            cout << "\n";
            cout << "--fps <fps>\n";
            cout << "   Set the Frames Per Second.  Only necessary when\n";
            cout << "   running with the '--blanks' option since all other\n";
            cout << "   processing modes get the frame rate from the player.\n";
            cout << "\n";
            cout << "--all\n";
            cout << "   Re-run commercial flagging for all recorded\n";
            cout << "   programs using current detection method.\n";
            cout << "\n";
            cout << "--force\n";
            cout << "   Force flagging of a video even if mythcommflag\n";
            cout << "   thinks it is already in use by another instance.\n";
            cout << "\n";
            cout << "--hogcpu\n";
            cout << "   Do not nice the flagging process.\n";
            cout << "   WARNING: This will consume all free cpu time.\n";
            cout << "\n";
            cout << "--help\n";
            cout << "   This text\n";
            cout << "\n";
            cout << "Note: both --chanid and --starttime must be used together\n";
            cout << "      if either is used.\n";
            cout << "\n";
            cout << "If no command line arguments are specified, all\n";
            cout << "unflagged videos will be flagged.\n";
            cout << "\n";
            exit(-1);
        }
        else
        {
            printf("illegal option: '%s' (use --help)\n", a.argv()[argpos]);
            exit(-1);
        }

        ++argpos;
    }

    gContext = new MythContext(MYTH_BINARY_VERSION, false);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        exit(-1);
    }

    if ((chanid.isEmpty() && !starttime.isEmpty()) ||
        (!chanid.isEmpty() && starttime.isEmpty()))
    {
        printf( "You must specify both the Channel ID and the Start Time\n");
        exit(-1);
    }

    // be nice to other programs since FlagCommercials() can consume 100% cpu
    if (be_nice)
        nice(19);

    time_now = time(NULL);
    if (!quiet)
    {
        printf( "\n" );
        printf( "MythTV Commercial Flagging, started at %s", ctime(&time_now));

        printf( "\n" );
        printf( "Flagging commercial breaks for:\n" );
        if (a.argc() == 1)
            printf( "ALL Un-flagged programs\n" );
        printf( "ChanID  Start Time      "
                "Title                                         Breaks\n" );
        printf( "------  --------------  "
                "--------------------------------------------  ------\n" );
    }

    if (!chanid.isEmpty() && !starttime.isEmpty())
    {
        FlagCommercials(db, chanid, starttime);
    }
    else
    {
        QSqlQuery recordedquery;
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

                if ( all_recorded )
                {
                    FlagCommercials(db, chanid, starttime);
                }
                else
                {
                    // check to see if this show is already marked
                    QSqlQuery markupquery;
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
                                FlagCommercials(db, chanid, starttime);
                        }
                    }
                }
            }
        }
        else
        {
             MythContext::DBError("Querying recorded programs", recordedquery);
             exit(-1);
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
