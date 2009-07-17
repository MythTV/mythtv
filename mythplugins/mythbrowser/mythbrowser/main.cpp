#include <iostream>
#include <unistd.h>

// qt
#include <QApplication>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythversion.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>
#include <mythtv/langsettings.h>
#include <mythtv/mythpluginapi.h>

// mythbrowser
#include "bookmarkmanager.h"
#include "browserdbutil.h"

using namespace std;

void setupKeys(void)
{
    REG_KEY("Browser", "NEXTTAB",   "Move to next browser tab",       "P");
    REG_KEY("Browser", "PREVTAB",   "Move to previous browser tab",   "");
    REG_KEY("Browser", "DELETETAB", "Delete the current browser tab", "D");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythbrowser", libversion, MYTH_BINARY_VERSION))
        return -1;
    LanguageSettings::load("mythbrowser");

    UpgradeBrowserDatabaseSchema();

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BookmarkManager *manager = new BookmarkManager(mainStack, "bookmarkmanager");

    if (manager->Create())
    {
        mainStack->AddScreen(manager);
        return 0;
    }
    else
    {
        delete manager;
        return -1;
    }
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BrowserConfig *config = new BrowserConfig(mainStack, "browserconfig");

    if (config->Create())
    {
        mainStack->AddScreen(config);
        return 0;
    }
    else
    {
        delete config;
        return -1;
    }
}
