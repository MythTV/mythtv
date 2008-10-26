
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

void setupKeys(void)
{
    REG_JUMP("MythNews", "RSS News feed reader", "", runNews);

    REG_KEY("News", "RETRIEVENEWS", "Update news items", "I");
    REG_KEY("News", "FORCERETRIEVE", "Force update news items", "M");
    REG_KEY("News", "CANCEL", "Cancel news item updating", "C");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythnews",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gContext->ActivateSettingsCache(false);
    if (!UpgradeNewsDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gContext->ActivateSettingsCache(false);

    setupKeys();

    return 0;
}

void runNews(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNews *mythnews = new MythNews(mainStack, "mythnews");

    if (mythnews->Create())
        mainStack->AddScreen(mythnews);
}

int mythplugin_run(void)
{
    runNews();
    return 0;
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNewsConfig *mythnewsconfig = new MythNewsConfig(mainStack, "mythnewsconfig");

    if (mythnewsconfig->Create())
        mainStack->AddScreen(mythnewsconfig);

    return 0;
}



