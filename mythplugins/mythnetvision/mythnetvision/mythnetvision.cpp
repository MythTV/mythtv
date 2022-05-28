// C++ headers
#include <unistd.h>

// QT headers
#include <QApplication>

// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythbase/mythplugin.h>
#include <libmythbase/mythpluginapi.h>
#include <libmythbase/mythversion.h>
#include <libmythbase/netgrabbermanager.h>
#include <libmythbase/rssmanager.h>
#include <libmythui/mythmainwindow.h>

// MythNetVision headers
#include "netsearch.h"
#include "nettree.h"
#include "treeeditor.h"

GrabberManager *grabMan = nullptr;
RSSManager *rssMan = nullptr;

static int RunNetVision(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *netsearch = new NetSearch(mainStack, "mythnetsearch");

    if (netsearch->Create())
    {
        mainStack->AddScreen(netsearch);
        return 0;
    }
    delete netsearch;
    return -1;
}

static int RunNetTree(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    DialogType type = static_cast<DialogType>(gCoreContext->GetNumSetting(
                       "mythnetvision.ViewMode", DLG_TREE));

    auto *nettree = new NetTree(type, mainStack, "mythnettree");

    if (nettree->Create())
    {
        mainStack->AddScreen(nettree);
        return 0;
    }
    delete nettree;
    return -1;
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

    REG_KEY("Internet Video", "PAGELEFT", QT_TRANSLATE_NOOP("MythControls",
            "Previous Page"), "");
    REG_KEY("Internet Video", "PAGERIGHT", QT_TRANSLATE_NOOP("MythControls",
            "Next Page"), "");
}

int mythplugin_init(const char *libversion)
{
    if (!MythCoreContext::TestPluginVersion("mythnetvision",
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

