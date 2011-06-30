// C headers
#include <unistd.h>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QEventLoop>

// libmyth headers
#include <exitcodes.h>
#include <mythcontext.h>
#include <mythdb.h>
#include <mythversion.h>
#include <remoteutil.h>
#include <util.h>
#include <mythtranslation.h>
#include <mythconfig.h>
#include <netgrabbermanager.h>
#include <mythrssmanager.h>
#include <mythcommandlineparser.h>
#include <mythlogging.h>

GrabberDownloadThread *gdt = 0;
RSSManager *rssMan = 0;

class MPUBLIC MythFillNVCommandLineParser : public MythCommandLineParser
{
  public:
    MythFillNVCommandLineParser(); 
    void LoadArguments(void);
};

MythFillNVCommandLineParser::MythFillNVCommandLineParser() :
    MythCommandLineParser("mythfillnetvision")
{ LoadArguments(); }

void MythFillNVCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addVerbose();
    addLogging();

    add("--refresh-all", "refresh-all", false,
            "Refresh ALL configured and installed tree grabbers", "");
    add("--refresh-rss", "refresh-rss", false,
            "Refresh RSS feeds only", "");
    add("--refresh-tree", "refresh-tree", false,
            "Refresh trees only", "");
}



int main(int argc, char *argv[])
{
    MythFillNVCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("mythfillnetvision");

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;
    
    ///////////////////////////////////////////////////////////////////////
    // Don't listen to console input
    close(0);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        delete gContext;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    bool refreshall  = cmdline.toBool("refresh-all");
    bool refreshrss  = cmdline.toBool("refresh-rss");
    bool refreshtree = cmdline.toBool("refresh-tree");

    if (refreshall && (refreshrss || refreshtree))
    {
        VERBOSE(VB_IMPORTANT, "--refresh-all must not be accompanied by "
                              "--refresh-rss or --refresh-tree");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (refreshrss && refreshtree)
    {
        VERBOSE(VB_IMPORTANT, "--refresh-rss and --refresh-tree are mutually "
                              "exclusive options");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!refreshall && !refreshrss && !refreshtree)
    {
        // Default is to do rss & tree, but not all
        refreshtree = true;
        refreshrss  = true;
    }

    myth_nice(19);

    MythTranslation::load("mythfrontend");

    if (refreshall || refreshtree)
    {
        QEventLoop treeloop;

        gdt = new GrabberDownloadThread(NULL);
        if (refreshall)
            gdt->refreshAll();
        gdt->start();

        QObject::connect(gdt, SIGNAL(finished(void)), &treeloop, SLOT(quit()));
        treeloop.exec();
    }

    if (refreshall || refreshrss)
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

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
