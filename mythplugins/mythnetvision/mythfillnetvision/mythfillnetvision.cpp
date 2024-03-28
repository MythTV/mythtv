// C headers
#include <unistd.h>

// C++ headers
#include <iostream>

// Qt headers
#include <QCoreApplication>
#include <QEventLoop>

// MythTV headers
#include <mythconfig.h>
#include <libmyth/mythcontext.h>
#include <libmythbase/exitcodes.h>
#include <libmythbase/mythcommandlineparser.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythmiscutil.h>
#include <libmythbase/mythpluginexport.h>
#include <libmythbase/mythtranslation.h>
#include <libmythbase/mythversion.h>
#include <libmythbase/netgrabbermanager.h>
#include <libmythbase/netutils.h>
#include <libmythbase/remoteutil.h>
#include <libmythbase/rssmanager.h>

GrabberDownloadThread *gdt = nullptr;
RSSManager *rssMan = nullptr;

class MPLUGIN_PUBLIC MythFillNVCommandLineParser : public MythCommandLineParser
{
  public:
    MythFillNVCommandLineParser(); 
    void LoadArguments(void) override; // MythCommandLineParser
};

MythFillNVCommandLineParser::MythFillNVCommandLineParser() :
    MythCommandLineParser("mythfillnetvision")
{ MythFillNVCommandLineParser::LoadArguments(); }

void MythFillNVCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
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
        MythFillNVCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("mythfillnetvision");

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;
    
    ///////////////////////////////////////////////////////////////////////
    // Don't listen to console input
    close(0);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        delete gContext;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    bool refreshall  = cmdline.toBool("refresh-all");
    bool refreshrss  = cmdline.toBool("refresh-rss");
    bool refreshtree = cmdline.toBool("refresh-tree");

    if (refreshall && (refreshrss || refreshtree))
    {
        LOG(VB_GENERAL, LOG_ERR, "--refresh-all must not be accompanied by "
                                 "--refresh-rss or --refresh-tree");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (refreshrss && refreshtree)
    {
        LOG(VB_GENERAL, LOG_ERR, "--refresh-rss and --refresh-tree are "
                                 "mutually exclusive options");
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

        gdt = new GrabberDownloadThread(nullptr);
        if (refreshall)
            gdt->refreshAll();
        gdt->start();

        QObject::connect(gdt, &GrabberDownloadThread::finished, &treeloop, &QEventLoop::quit);
        treeloop.exec();
    }

    if ((refreshall || refreshrss) && !findAllDBRSS().empty())
    {
        QEventLoop rssloop;

        rssMan = new RSSManager();
        rssMan->doUpdate();

        QObject::connect(rssMan, &RSSManager::finished, &rssloop, &QEventLoop::quit);
        rssloop.exec();
    }

    delete gdt;
    delete rssMan;
    delete gContext;

    LOG(VB_GENERAL, LOG_INFO, "MythFillNetvision run complete.");

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
