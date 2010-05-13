
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
#include "dbcheck.h"
#include "mythnews.h"
#include "mythnewsconfig.h"

using namespace std;

void runNews(void);
int  RunNews(void);

void setupKeys(void)
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
    if (!gContext->TestPopupVersion("mythnews",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gCoreContext->ActivateSettingsCache(false);
    if (!UpgradeNewsDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gCoreContext->ActivateSettingsCache(false);

    setupKeys();

    return 0;
}

void runNews(void)
{
    RunNews();
}

int RunNews(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNews *mythnews = new MythNews(mainStack, "mythnews");

    if (mythnews->Create())
    {
        mainStack->AddScreen(mythnews);
        return 0;
    }
    else
    {
        delete mythnews;
        return -1;
    }
}

int mythplugin_run(void)
{
    return RunNews();
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNewsConfig *mythnewsconfig = new MythNewsConfig(mainStack, "mythnewsconfig");

    if (mythnewsconfig->Create())
    {
        mainStack->AddScreen(mythnewsconfig);
        return 0;
    }
    else
    {
        delete mythnewsconfig;
        return -1;
    }
}



