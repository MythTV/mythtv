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
#include "libmythtv/jobqueue.h"
#include "libmyth/mythdbcon.h"

using namespace std;

JobQueue *jobqueue = NULL;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);
    int argpos = 1;

    QString filename;
        
    QString verboseString = QString(" important general");

    QFileInfo finfo(a.argv()[0]);

    QString binname = finfo.baseName();

    while (argpos < a.argc())
    {
        if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc() > argpos)
            {
                QString temp = a.argv()[argpos+1];
                if (temp.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -v/--verbose option\n";
                    return -1;
                } else
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
                        else if (!strcmp(*it,"commflag"))
                        {
                            print_verbose_messages |= VB_COMMFLAG;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"jobqueue"))
                        {
                            print_verbose_messages |= VB_JOBQUEUE;
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
                    "-v OR --verbose debug-level  Prints more information" << endl <<
                    "                             Accepts any combination (separated by comma)" << endl << 
                    "                             of all,none,quiet,record,playback," << endl <<
                    "                             channel,osd,file,schedule,jobqueue," << endl <<
                    "                             network,commflag" << endl <<
                    endl;
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
    if(!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return -1;
    }

    jobqueue = new JobQueue(false);

    a.exec();

    delete gContext;

    exit(0);
}
