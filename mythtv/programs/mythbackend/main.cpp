#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <iostream>
#include <fstream>
using namespace std;

#include "tv.h"
#include "autoexpire.h"
#include "scheduler.h"
#include "transcoder.h"
#include "mainserver.h"
#include "encoderlink.h"
#include "remoteutil.h"

#include "libmythtv/programinfo.h"
#include "libmyth/mythcontext.h"
#include "libmythtv/dbcheck.h"

QMap<int, EncoderLink *> tvList;
MythContext *gContext;
AutoExpire *expirer = NULL;
Scheduler *sched = NULL;
Transcoder *trans = NULL;
QString pidfile;
QString lockfile_location;

bool setupTVs(bool ismaster)
{
    QString localhostname = gContext->GetHostName();

    QSqlQuery query;

    query.exec("SELECT cardid,hostname FROM capturecard ORDER BY cardid;");

    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            int cardid = query.value(0).toInt();
            QString host = query.value(1).toString();

            if (host.isNull() || host.isEmpty())
            {
                cerr << "One of your capturecard entries does not have a "
                     << "hostname.\n  Please run setup and confirm all of the "
                     << "capture cards.\n";
                exit(-1);
            }

            if (!ismaster)
            {
                if (host == localhostname)
                {
                    TVRec *tv = new TVRec(cardid);
                    tv->Init();
                    EncoderLink *enc = new EncoderLink(cardid, tv);
                    tvList[cardid] = enc;
                }
            }
            else
            {
                if (host == localhostname)
                {
                    TVRec *tv = new TVRec(cardid);
                    tv->Init();
                    EncoderLink *enc = new EncoderLink(cardid, tv);
                    tvList[cardid] = enc;
                }
                else
                {
                    EncoderLink *enc = new EncoderLink(cardid, NULL, host);
                    tvList[cardid] = enc;
                }
            }
        }
    }
    else
    {
        cerr << "ERROR: no capture cards are defined in the database.\n";
        cerr << "Perhaps you should read the installation instructions?\n";
        return false;
    }

    return true;
}

void cleanup(void) 
{
    delete gContext;

    if (sched)
        delete sched;

    if (trans)
        delete trans;

    if (pidfile != "")
        unlink(pidfile.ascii());

    unlink(lockfile_location.ascii());
}
    
int main(int argc, char **argv)
{
    for(int i = 3; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
        close(i);

    QApplication a(argc, argv, false);

    QString logfile = "";
    QString binname = basename(a.argv()[0]);
    QString verboseString = QString(" important general");

    bool daemonize = false;
    bool printsched = false;
    bool printexpire = false;
    bool noAutoShutdown = false;
    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile"))
        {
            if (a.argc() > argpos)
            {
                logfile = a.argv()[argpos+1];
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return -1;
                }
                else
                {
                    ++argpos;
                }
            }
        } 
        else if (!strcmp(a.argv()[argpos],"-p") ||
                 !strcmp(a.argv()[argpos],"--pidfile"))
        {
            if (a.argc() > argpos)
            {
                pidfile = a.argv()[argpos+1];
                if (pidfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -p/--pidfile option\n";
                    return -1;
                } 
                else
                {
                   ++argpos;
                }
            }
        } 
        else if (!strcmp(a.argv()[argpos],"-d") ||
                 !strcmp(a.argv()[argpos],"--daemon"))
        {
            daemonize = true;

        }
        else if (!strcmp(a.argv()[argpos],"-n") ||
                 !strcmp(a.argv()[argpos],"--noautoshutdown"))
        {
            noAutoShutdown = true;
            VERBOSE(VB_ALL, "Auto shutdown procedure disabled by commandline"); 
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
                        if(!strcmp(*it,"none"))
                        {
                            print_verbose_messages = VB_NONE;
                            verboseString = "";
                        }
                        else if(!strcmp(*it,"all"))
                        {
                            print_verbose_messages = VB_ALL;
                            verboseString = "all";
                        }
                        else if(!strcmp(*it,"quiet"))
                        {
                            print_verbose_messages = VB_IMPORTANT;
                            verboseString = "important";
                        }
                        else if(!strcmp(*it,"record"))
                        {
                            print_verbose_messages |= VB_RECORD;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"playback"))
                        {
                            print_verbose_messages |= VB_PLAYBACK;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"channel"))
                        {
                            print_verbose_messages |= VB_CHANNEL;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"osd"))
                        {
                            print_verbose_messages |= VB_OSD;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"file"))
                        {
                            print_verbose_messages |= VB_FILE;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"schedule"))
                        {
                            print_verbose_messages |= VB_SCHEDULE;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"network"))
                        {
                            print_verbose_messages |= VB_NETWORK;
                            verboseString += " " + *it;
                        }
                        else
                        {
                            cerr << "Unknown argument for -v/--verbose: "
                                 << *it << endl;;
                        }
                    }
                }
            } 
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return -1;
            }
        } 
        else if (!strcmp(a.argv()[argpos],"--printsched"))
        {
            printsched = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--printexpire"))
        {
            printexpire = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--version"))
        {
            cout << MYTH_BINARY_VERSION << endl;
            exit(0);
        }
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||
                !strcmp(a.argv()[argpos],"--help")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            cerr << "Valid options are: " << endl <<
                    "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
                    "-p or --pidfile filename       Write PID of mythbackend " <<
                                                    "to filename" << endl <<
                    "-d or --daemon                 Runs mythbackend as a daemon" << endl <<
                    "-n or --noautoshutdonw         Blocks the auto shutdown routine, so that the" << endl <<
                    "                               backend will stay alive"<<endl <<
                    "-v or --verbose debug-level    Prints more information" << endl <<
                    "                               Accepts any combination (separated by comma)" << endl << 
                    "                               of all,none,quiet,record,playback," << endl <<
                    "                               channel,osd,file,schedule,network" << endl <<
                    "--printexpire                  List of auto-expire programs" << endl <<
                    "--printsched                   Upcoming scheduled programs" << endl <<
                    "--version                      Version information" << endl;
            return -1;
        }
    }

    int logfd = -1;

    if (logfile != "")
    {
        logfd = open(logfile.ascii(), O_WRONLY|O_CREAT|O_APPEND|O_SYNC, 0664);
         
        if (logfd < 0)
        {
            perror("open(logfile)");
            return -1;
        }
    }
    
    ofstream pidfs;
    if (pidfile != "")
    {
        pidfs.open(pidfile.ascii());
        if (!pidfs)
        {
            perror("open(pidfile)");
            return -1;
        }
    }

    close(0);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Unable to ignore SIGPIPE\n";

    if (daemonize)
        if (daemon(0, 1) < 0)
        {
            perror("daemon");
            return -1;
        }


    if (pidfs)
    {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    if (logfd != -1)
    {
        // Send stdout and stderr to the logfile
        dup2(logfd, 1);
        dup2(logfd, 2);
    }

    gContext = new MythContext(MYTH_BINARY_VERSION, false, false);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    QSqlDatabase *subthread = QSqlDatabase::addDatabase("QMYSQL3", "SUBDB");
    if (!subthread)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    QSqlDatabase *expthread = QSqlDatabase::addDatabase("QMYSQL3", "EXPDB");
    if (!expthread)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    QSqlDatabase *transthread = QSqlDatabase::addDatabase("QMYSQL3", "TRANSDB");
    if (!transthread)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    QSqlDatabase *msdb = QSqlDatabase::addDatabase("QMYSQL3", "MSDB");
    if (!msdb)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    if (!gContext->OpenDatabase(db) || !gContext->OpenDatabase(subthread) ||
        !gContext->OpenDatabase(expthread) ||
        !gContext->OpenDatabase(transthread) ||
        !gContext->OpenDatabase(msdb))
    {
        printf("couldn't open db\n");
        return -1;
    }

    UpgradeTVDatabaseSchema();

    if (printsched)
    {
        sched = new Scheduler(false, &tvList, db);
        if (gContext->ConnectToMasterServer())
        {
            cout << "Retrieving Schedule from Master backend.\n";
            sched->FillRecordListFromMaster();
        }
        else
        {
            cout << "Calculating Schedule from database.\n" <<
                    "Inputs, Card IDs, and Conflict info may be invalid "
                    "if you have multiple tuners.\n";
            sched->FillRecordLists(false);
        }

        sched->PrintList();
        cleanup();
        exit(0);
    }

    if (printexpire)
    {
        expirer = new AutoExpire(false, false, db);
        expirer->FillExpireList();
        expirer->PrintExpireList();
        cleanup();
        exit(0);
    }

    int port = gContext->GetNumSetting("BackendServerPort", 6543);
    int statusport = gContext->GetNumSetting("BackendStatusPort", 6544);

    QString myip = gContext->GetSetting("BackendServerIP");
    QString masterip = gContext->GetSetting("MasterServerIP");

    bool ismaster = false;

    if (myip.isNull() || myip.isEmpty())
    {
        cerr << "No setting found for this machine's BackendServerIP.\n"
             << "Please run setup on this machine and modify the first page\n"
             << "of the general settings.\n";
        exit(-1);
    }

    if (masterip == myip)
    {
        cerr << "Starting up as the master server.\n";
        ismaster = true;
    }
    else
    {
        cerr << "Running as a slave backend.\n";
    }
 
    bool runsched = setupTVs(ismaster);

    if (ismaster && runsched)
    {
        QSqlDatabase *scdb = QSqlDatabase::database("SUBDB");
        sched = new Scheduler(true, &tvList, scdb, noAutoShutdown);
    }

    QSqlDatabase *expdb = QSqlDatabase::database("EXPDB");
    expirer = new AutoExpire(true, ismaster, expdb);

    QSqlDatabase *trandb = QSqlDatabase::database("TRANSDB");
    trans = new Transcoder(&tvList, trandb);

    VERBOSE(VB_ALL, QString("%1 version: %2 www.mythtv.org")
                            .arg(binname).arg(MYTH_BINARY_VERSION));

    VERBOSE(VB_ALL, QString("Enabled verbose msgs :%1").arg(verboseString));

    lockfile_location = gContext->GetSetting("RecordFilePrefix") + "/nfslockfile.lock";
    int nfsfd = -1;

    if (ismaster)
// Create a file in the recording directory.  A slave encoder will 
// look for this file and return 0 bytes free if it finds it when it's
// queried about its available/used diskspace.  This will prevent double (or
// more) counting of available disk space.
// If the slave doesn't find this file then it will assume that it has
// a seperate store.
    {
       nfsfd = open(lockfile_location.ascii(), O_WRONLY|O_CREAT|O_APPEND, 0664);
       if (nfsfd < 0) 
       {
           perror("open lockfile"); 
           cerr << "Unable to create \'" << lockfile_location << "\'!\n"
                << "Be sure that \'" << gContext->GetSetting("RecordFilePrefix")
                << "\' exists and that both \nthe directory and that "
                << "file are writeble by this user.\n";
          return -1;
       }
       close(nfsfd);
    }

    new MainServer(ismaster, port, statusport, &tvList, msdb, sched);

    if (ismaster)
    {
        QString WOLslaveBackends;
        WOLslaveBackends = gContext->GetSetting("WOLslaveBackendsCommand","");
        if (!WOLslaveBackends.isEmpty())
        {
            VERBOSE(VB_ALL, "Waking slave Backends now.");
            system(WOLslaveBackends.ascii());
        }
    }

    a.exec();

    // delete trans;

    cleanup();

    return 0;
}
