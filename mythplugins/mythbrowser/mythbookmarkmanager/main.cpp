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

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythbookmarks", libversion, MYTH_BINARY_VERSION))
        return -1;
    LanguageSettings::load("mythbrowser");

    UpgradeBrowserDatabaseSchema();

    return 0;
}

int mythplugin_run(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BookmarkManager *manager = new BookmarkManager(mainStack, "bookmarkmanager");

    if (manager->Create())
        mainStack->AddScreen(manager);

    return 0;
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BrowserConfig *config = new BrowserConfig(mainStack, "browserconfig");

    if (config->Create())
        mainStack->AddScreen(config);

    return 0;
}
