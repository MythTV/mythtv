// C++ headers
#include <unistd.h>

// QT headers
#include <QApplication>

// MythTV headers
#include <mythcontext.h>
#include <mythplugin.h>
#include <mythpluginapi.h>
#include <mythversion.h>
#include <mythmainwindow.h>
#include <netgrabbermanager.h>
#include <mythrssmanager.h>

// MythNetVision headers
#include "netsearch.h"
#include "nettree.h"
#include "treeeditor.h"

using namespace std;

GrabberManager *grabMan = 0;
RSSManager *rssMan = 0;

static int RunNetVision(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    NetSearch *netsearch = new NetSearch(mainStack, "mythnetsearch");

    if (netsearch->Create())
    {
        mainStack->AddScreen(netsearch);
        return 0;
    }
    else
    {
        delete netsearch;
        return -1;
    }
}

static int RunNetTree(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    DialogType type = static_cast<DialogType>(gCoreContext->GetNumSetting(
                       "mythnetvision.ViewMode", DLG_TREE));

    NetTree *nettree = new NetTree(type, mainStack, "mythnettree");

    if (nettree->Create())
    {
        mainStack->AddScreen(nettree);
        return 0;
    }
    else
    {
        delete nettree;
        return -1;
    }
}

static void runNetVision(void)
{
    RunNetVision();
}

static void runNetTree(void)
{
    RunNetTree();
}

static void setupKeys(void)
{
    REG_JUMP("MythNetSearch", QT_TRANSLATE_NOOP("MythControls",
        "Internet Television Client - Search"), "", runNetVision);
    REG_JUMP("MythNetTree", QT_TRANSLATE_NOOP("MythControls",
        "Internet Television Client - Site/Tree View"), "", runNetTree);
}

int mythplugin_init(const char *libversion)
{
    if (!gCoreContext->TestPluginVersion("mythnetvision",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    return RunNetVision();
}

