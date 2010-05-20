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
#include "../mythnetvision/rssmanager.h"

GrabberDownloadThread *gdt = 0;
RSSManager *rssMan = 0;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    int argpos = 1;
    bool refreshall = false;
    bool refreshrss = true;
    bool refreshtree = true;

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
        else if (!strcmp(a.argv()[argpos], "--refresh-rss"))
        {
            cout << "###\n";
            cout << "### Refreshing RSS Only.\n";
            cout << "###\n";
            refreshtree = false;
        }
        else if (!strcmp(a.argv()[argpos], "--refresh-tree"))
        {
            cout << "###\n";
            cout << "### Refreshing Trees Only.\n";
            cout << "###\n";
            refreshrss = false;
        }
        else if (!strcmp(a.argv()[argpos], "-h") ||
                 !strcmp(a.argv()[argpos], "--help"))
        {
            cout << "usage:\n";
            cout << "--refresh-all\n";
            cout << "   Refresh all tree views and RSS feeds, regardless of whether they need it.\n";
            cout << "--refresh-rss\n";
            cout << "   Refresh RSS feeds only.\n";
            cout << "--refresh-tree\n";
            cout << "   Refresh Tree views only.\n";
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
                       "Online Source Listing Download Started", "");

    if (refreshtree)
    {
        QEventLoop treeloop;

        gdt = new GrabberDownloadThread(NULL);
        if (refreshall)
            gdt->refreshAll();
        gdt->start();

        QObject::connect(gdt, SIGNAL(finished(void)), &treeloop, SLOT(quit()));
        treeloop.exec();
    }

    if (refreshrss)
    {
        QEventLoop rssloop;

        rssMan = new RSSManager();
        rssMan->doUpdate();

        QObject::connect(rssMan, SIGNAL(finished(void)), &rssloop, SLOT(quit()));
        rssloop.exec();
    }

    delete gdt;
    delete rssMan;
    delete gContext;

    VERBOSE(VB_IMPORTANT, "MythFillNetvision run complete.");

    return FILLDB_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
