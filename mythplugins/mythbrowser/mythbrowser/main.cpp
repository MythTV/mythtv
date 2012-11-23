#include <iostream>
#include <unistd.h>

// qt
#include <QApplication>

// myth
#include <mythcontext.h>
#include <mythversion.h>
#include <mythscreenstack.h>
#include <mythmainwindow.h>
#include <mythpluginapi.h>

// mythbrowser
#include "bookmarkmanager.h"
#include "browserdbutil.h"
#include "mythbrowser.h"
#include "mythflashplayer.h"

using namespace std;

static int handleMedia(const QString &url, const QString &directory, const QString &filename,
                       const QString &, const QString &, int, int, const QString &, int,
                       const QString &, const QString &, bool)
{
    if (url.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "MythBrowser: handleMedia got empty url!");
        return 1;
    }

    QStringList urls = url.split(" ", QString::SkipEmptyParts);
    float zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.4").toFloat();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (urls[0].startsWith("mythflash://"))
    {
        MythFlashPlayer *flashplayer = new MythFlashPlayer(mainStack, urls);
        if (flashplayer->Create())
            mainStack->AddScreen(flashplayer);
        else
            delete flashplayer;
    }
    else
    {
        MythBrowser *mythbrowser = new MythBrowser(mainStack, urls, zoom);

        if (!directory.isEmpty())
            mythbrowser->setDefaultSaveDirectory(directory);

        if (!filename.isEmpty())
            mythbrowser->setDefaultSaveFilename(filename);

        if (mythbrowser->Create())
            mainStack->AddScreen(mythbrowser);
        else
            delete mythbrowser;
    }

    return 0;
}

static void setupKeys(void)
{
    REG_KEY("Browser", "NEXTTAB", QT_TRANSLATE_NOOP("MythControls",
        "Move to next browser tab"), "P");
    REG_KEY("Browser", "PREVTAB", QT_TRANSLATE_NOOP("MythControls",
        "Move to previous browser tab"), "");

    REG_MEDIAPLAYER("WebBrowser", QT_TRANSLATE_NOOP("MythControls",
        "Internal Web Browser"), handleMedia);
}

int mythplugin_init(const char *libversion)
{
    if (!gCoreContext->TestPluginVersion("mythbrowser", libversion, MYTH_BINARY_VERSION))
        return -1;

    UpgradeBrowserDatabaseSchema();

    gCoreContext->ActivateSettingsCache(false);

    if (gCoreContext->GetSetting("WebBrowserCommand").isEmpty())
        gCoreContext->SaveSetting("WebBrowserCommand", "Internal");

    if (gCoreContext->GetSetting("WebBrowserZoomLevel").isEmpty())
        gCoreContext->SaveSetting("WebBrowserZoomLevel", "1.4");

    gCoreContext->ActivateSettingsCache(true);

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
