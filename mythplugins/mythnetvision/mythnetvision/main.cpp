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

// MythNetVision headers
#include "grabbermanager.h"
#include "rssmanager.h"
#include "netsearch.h"
#include "nettree.h"
#include "treeeditor.h"
#include "dbcheck.h"

using namespace std;

GrabberManager *grabMan = 0;
RSSManager *rssMan = 0;

void runNetVision(void);
int  RunNetVision(void);
void runNetTree(void);
int  RunNetTree(void);
void runTreeEditor(void);
int  RunTreeEditor(void);

void setupKeys(void)
{
    REG_JUMP("MythNetSearch", QT_TRANSLATE_NOOP("MythControls",
        "Internet Television Client - Search"), "", runNetVision);
    REG_JUMP("MythNetTree", QT_TRANSLATE_NOOP("MythControls",
        "Internet Television Client - Site/Tree View"), "", runNetTree);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythnetvision",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gCoreContext->ActivateSettingsCache(false);
    if (!UpgradeNetvisionDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade netvision database to new schema, exiting.");
        return -1;
    }
    gCoreContext->ActivateSettingsCache(false);

    setupKeys();

    if (gCoreContext->GetNumSetting("mythnetvision.backgroundFetch", 0))
    {
        grabMan = new GrabberManager();
        grabMan->startTimer();
        grabMan->doUpdate();
    }

    if (gCoreContext->GetNumSetting("mythnetvision.rssBackgroundFetch", 0))
    {
//        rssMan = new RSSManager();
//        rssMan->startTimer();
//        rssMan->doUpdate();
    }

    return 0;
}

void runNetVision(void)
{
    RunNetVision();
}

void runNetTree(void)
{
    RunNetTree();
}

void runTreeEditor(void)
{
    RunTreeEditor();
}

int RunNetVision(void)
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

int RunNetTree(void)
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

int RunTreeEditor(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    TreeEditor *treeedit = new TreeEditor(mainStack, "mythnettreeeditor");

    if (treeedit->Create())
    {
        mainStack->AddScreen(treeedit);
        return 0;
    }
    else
    {
        delete treeedit;
        return -1;
    }
}

int mythplugin_run(void)
{
    return RunNetVision();
}

