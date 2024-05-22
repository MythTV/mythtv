#include <iostream>
#include <unistd.h>

// qt
#include <QApplication>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythpluginapi.h>
#include <libmythbase/mythversion.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythscreenstack.h>

// mythbrowser
#include "bookmarkmanager.h"
#include "browserdbutil.h"
#include "mythbrowser.h"
#include "mythflashplayer.h"

// Based on MediaPlayCallback.  Parameters only seem to have local significance.
static int handleMedia(const QString &url, const QString &directory, const QString &filename,
                       const QString & /*unused*/, const QString & /*unused*/, int /*unused*/,
                       int /*unused*/, const QString & /*unused*/, std::chrono::minutes /*unused*/,
                       const QString & /*unused*/, const QString & /*unused*/, bool /*unused*/)
{
    if (url.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "MythBrowser: handleMedia got empty url!");
        return 1;
    }

    QStringList urls = url.split(" ", Qt::SkipEmptyParts);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (urls[0].startsWith("mythflash://"))
    {
        auto *flashplayer = new MythFlashPlayer(mainStack, urls);
        if (flashplayer->Create())
            mainStack->AddScreen(flashplayer);
        else
            delete flashplayer;
    }
    else
    {
        auto *mythbrowser = new MythBrowser(mainStack, urls);

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

static void runBookmarkManager()
{
    mythplugin_run();
}

/** \fn runHomepage()
 *  \brief Loads the specified homepage from the database (the name starts
 *         with an underscore) and calls handleMedia() if it exists.
 *  \return void.
 */
static void runHomepage()
{
    // Get the homepage from the database. The url
    // that is set as a homepage starts with a space.
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.exec("SELECT url FROM `websites` WHERE `homepage` = true;"))
        LOG(VB_GENERAL, LOG_ERR, "Error loading homepage from DB");

    if (query.size() > 0)
    {
        query.next();
        handleMedia( query.value(0).toString(), "", "", "", "", 0, 0, "", 0min, "", "", false);
    }
    else
    {
        // show a dialog that no homepage is specified
        QString message = "No homepage was specified.\n"
                          "If required you can do this in the bookmark manager";

        MythScreenStack *m_popupStack =
                GetMythMainWindow()->GetStack("popup stack");

        auto *okPopup = new MythConfirmationDialog(m_popupStack, message, false);

        if (okPopup->Create())
            m_popupStack->AddScreen(okPopup);
    }
}

static void setupKeys(void)
{
    REG_KEY("Browser", "NEXTTAB", QT_TRANSLATE_NOOP("MythControls",
        "Move to next browser tab"), "P,Media Play");
    REG_KEY("Browser", "PREVTAB", QT_TRANSLATE_NOOP("MythControls",
        "Move to previous browser tab"), "");

    REG_JUMP("Bookmarks", QT_TRANSLATE_NOOP("MythControls",
        "Show the bookmark manager"), "", runBookmarkManager);
    REG_JUMP("Homepage", QT_TRANSLATE_NOOP("MythControls",
        "Show the webbrowser homepage"), "", runHomepage);

    REG_MEDIAPLAYER("WebBrowser", QT_TRANSLATE_NOOP("MythControls",
        "Internal Web Browser"), handleMedia);
}

int mythplugin_init(const char *libversion)
{
    if (!MythCoreContext::TestPluginVersion("mythbrowser", libversion,
                                            MYTH_BINARY_VERSION))
        return -1;

    UpgradeBrowserDatabaseSchema();

    gCoreContext->ActivateSettingsCache(false);

    if (gCoreContext->GetSetting("WebBrowserCommand").isEmpty())
        gCoreContext->SaveSetting("WebBrowserCommand", "Internal");

    if (gCoreContext->GetSetting("WebBrowserZoomLevel").isEmpty())
        gCoreContext->SaveSetting("WebBrowserZoomLevel", "1.0");

    gCoreContext->ActivateSettingsCache(true);

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *manager = new BookmarkManager(mainStack, "bookmarkmanager");

    if (manager->Create())
    {
        mainStack->AddScreen(manager);
        return 0;
    }
    delete manager;
    return -1;
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *config = new BrowserConfig(mainStack, "browserconfig");

    if (config->Create())
    {
        mainStack->AddScreen(config);
        return 0;
    }
    delete config;
    return -1;
}
