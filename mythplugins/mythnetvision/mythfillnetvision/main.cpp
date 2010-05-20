// C headers
#include <unistd.h>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QEventLoop>

// libmyth headers
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "remoteutil.h"
#include "util.h"
#include "langsettings.h"

#include "mythconfig.h"

#include "../mythnetvision/grabbermanager.h"

GrabberDownloadThread *gdt = 0;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    int argpos = 1;
    bool refreshall = false;

    myth_nice(19);

    while (argpos < a.argc())
    {
        // The manual and update flags should be mutually exclusive.
        if (!strcmp(a.argv()[argpos], "--refresh-all"))
        {
            cout << "###\n";
            cout << "### Refreshing ALL configured and installed tree grabbers.\n";
            cout << "###\n";
            refreshall = true;
        }
        else if (!strcmp(a.argv()[argpos], "-h") ||
                 !strcmp(a.argv()[argpos], "--help"))
        {
            cout << "usage:\n";
            cout << "--refresh-all\n";
            cout << "   Refresh all tree views, regardless of whether they need it.\n";
            cout << "\n";
            cout << "Run with no options to only update trees which need update.\n";
            return FILLDB_EXIT_INVALID_CMDLINE;
        }
        else
        {
            fprintf(stderr, "illegal option: '%s' (use --help)\n",
                    a.argv()[argpos]);
            return FILLDB_EXIT_INVALID_CMDLINE;
        }

        ++argpos;
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        delete gContext;
        return FILLDB_EXIT_NO_MYTHCONTEXT;
    }

    LanguageSettings::load("mythfrontend");

    gCoreContext->LogEntry("mythfillnetvision", LP_INFO,
                       "Listings Download Started", "");

    QEventLoop loop;

    gdt = new GrabberDownloadThread(NULL);
    if (refreshall)
        gdt->refreshAll();
    gdt->start();

    QObject::connect(gdt, SIGNAL(finished(void)), &loop, SLOT(quit()));

    loop.exec();

    delete gContext;

    VERBOSE(VB_IMPORTANT, "mythfillnetvision run complete.");

    return FILLDB_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
