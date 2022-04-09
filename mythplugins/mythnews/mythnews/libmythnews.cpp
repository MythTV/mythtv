
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

// MythNews headers
#include "newsdbcheck.h"
#include "mythnews.h"
#include "mythnewsconfig.h"

static int RunNews(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *mythnews = new MythNews(mainStack, "mythnews");

    if (mythnews->Create())
    {
        mainStack->AddScreen(mythnews);
        return 0;
    }
    delete mythnews;
    return -1;
}

static void runNews(void)
{
    RunNews();
}

static void setupKeys(void)
{
    REG_JUMP("MythNews", QT_TRANSLATE_NOOP("MythControls",
        "RSS News feed reader"), "", runNews);

    REG_KEY("News", "RETRIEVENEWS",
        QT_TRANSLATE_NOOP("MythControls", "Update news items"), "I");
    REG_KEY("News", "FORCERETRIEVE",
        QT_TRANSLATE_NOOP("MythControls", "Force update news items"), "M");
    REG_KEY("News", "CANCEL",
        QT_TRANSLATE_NOOP("MythControls", "Cancel news item updating"), "C");
}

int mythplugin_init(const char *libversion)
{
    if (!MythCoreContext::TestPluginVersion("mythnews",
                                            libversion,
                                            MYTH_BINARY_VERSION))
        return -1;

    gCoreContext->ActivateSettingsCache(false);
    if (!UpgradeNewsDatabaseSchema())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gCoreContext->ActivateSettingsCache(true);

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    return RunNews();
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *mythnewsconfig = new MythNewsConfig(mainStack, "mythnewsconfig");

    if (mythnewsconfig->Create())
    {
        mainStack->AddScreen(mythnewsconfig);
        return 0;
    }
    delete mythnewsconfig;
    return -1;
}



