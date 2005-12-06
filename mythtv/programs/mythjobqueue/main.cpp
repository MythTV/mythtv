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

#include "libmyth/exitcodes.h"
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
        
    QFileInfo finfo(a.argv()[0]);

    QString binname = finfo.baseName();

    while (argpos < a.argc())
    {
        if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return JOBQUEUE_EXIT_INVALID_CMDLINE;

                ++argpos;
            } else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return JOBQUEUE_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help"))
        {
            cerr << "Valid Options are:" << endl <<
                    "-v or --verbose debug-level    Use '-v help' for level info" << endl <<
                    endl;
            return JOBQUEUE_EXIT_INVALID_CMDLINE;
        }
        else
        {
            printf("illegal option: '%s' (use --help)\n", a.argv()[argpos]);
            return JOBQUEUE_EXIT_INVALID_CMDLINE;
        }

        ++argpos;
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return JOBQUEUE_EXIT_NO_MYTHCONTEXT;
    }

    gContext->ConnectToMasterServer();

    jobqueue = new JobQueue(false);

    a.exec();

    delete gContext;

    return JOBQUEUE_EXIT_OK;
}
